// SPDX-License-Identifier: BSD-3-Clause
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "ff.h"
#include "tf_card.h"
#include "hstx.h"
#include "jack_chip8.h"
#include "jack_input_devices.h"
#include "jack_tinyusb_input.h"
#include "jack_nespad_input.h"
#include "font_8x8.h"
static FATFS fs;
static jack_input_queue queue;
static jack_input_devices devices;
static char names[64][128];
static uint8_t rom[3584];
static int16_t samples[4096],pcm[735];
static unsigned rd,wr,count,names_count,selected;
static bool usb_ok,pad_ok,playing,audio_buffering;
static uint64_t audio_next;
static uint32_t audio_under,audio_over,audio_rebuffer,audio_resync;
static const uint8_t keymap[16]={0x1b,0x1e,0x1f,0x20,0x14,0x1a,0x08,0x04,0x16,0x07,0x1d,0x06,0x21,0x15,0x09,0x19};
static bool key(unsigned k){for(unsigned i=0;i<6;i++)if(devices.keyboard_keys[i]==k)return true;return false;}
static void poll(void){
    if(usb_ok)jack_tinyusb_input_poll();
    if(pad_ok)(void)jack_nespad_input_poll();
    jack_input_event e;while(jack_input_pop(&queue,&e)){}
}
static void text(uint16_t *p,unsigned x,unsigned y,const char *s,uint16_t color){
    for(;*s&&x+8<=320;s++,x+=8){
        unsigned c=(unsigned char)*s;if(c<32||c>126)c='?';
        for(unsigned r=0;r<8;r++)for(unsigned b=0;b<8;b++)
            if(font_8x8[r*95+c-32]&(1u<<b))p[(y+r)*320+x+b]=color;
    }
}
static void service_audio(void){
    uint64_t now=time_us_64()*44100u;
    if(now>audio_next+1000000ull*2048){audio_next=now;audio_resync++;}

    // Two 735-sample frames before playback resumes.
    if(audio_buffering && count>=1470)audio_buffering=false;
    while(audio_next<=now){
        int s=0;
        if(playing && !audio_buffering){
            if(count){s=samples[rd];rd=(rd+1)&4095;count--;}
            else {audio_under++;audio_rebuffer++;audio_buffering=true;}
        }
        hstx_push_audio_sample(s,s);audio_next+=1000000;
    }
}
static void menu(const char *message){
    if(!hstx_framebuffer_available())return;
    uint16_t *p=(uint16_t *)hstx_getframebuffer();memset(p,0,320*240*2);
    text(p,16,12,"CHIP-8 / SUPER-CHIP 0.1.1",0x7fff);
    unsigned top=selected/10*10;
    for(unsigned n=top;n<names_count&&n<top+10;n++){
        text(p,8,36+(n-top)*16,n==selected?">":" ",0x7fff);
        text(p,24,36+(n-top)*16,names[n],n==selected?0x7fe0:0x5294);
    }
    text(p,8,204,message,0x7fff);
    text(p,8,220,"ESC / SELECT+START: menu",0x5294);
    hstx_present_frame();
}
static FRESULT scan(void){
    DIR d;FILINFO f;FRESULT r=f_opendir(&d,"/roms/CHIP8");names_count=0;
    if(r!=FR_OK)return r;
    while((r=f_readdir(&d,&f))==FR_OK&&f.fname[0]){
        size_t l=strlen(f.fname);
        if(!(f.fattrib&AM_DIR)&&l>4&&l<128&&!strcasecmp(f.fname+l-4,".ch8")&&names_count<64)
            strcpy(names[names_count++],f.fname);
    }
    f_closedir(&d);
    for(unsigned i=1;i<names_count;i++){
        char tmp[128];strcpy(tmp,names[i]);unsigned j=i;
        while(j&&strcmp(names[j-1],tmp)>0){strcpy(names[j],names[j-1]);j--;}strcpy(names[j],tmp);
    }
    return r;
}
static bool load(jack_chip8_config *c){
    char path[180];FIL f;UINT n;snprintf(path,sizeof(path),"/roms/CHIP8/%s",names[selected]);
    if(f_open(&f,path,FA_READ)!=FR_OK)return false;
    FSIZE_t len=f_size(&f);bool ok=len>=2&&len<=sizeof(rom);
    if(ok)ok=f_read(&f,rom,(UINT)len,&n)==FR_OK&&n==len;
    f_close(&f);if(!ok)return false;
    jack_chip8_defaults(c);strcat(path,".cfg");
    FRESULT cfg=f_open(&f,path,FA_READ);
    if(cfg==FR_OK){
        char line[128];unsigned used=0;char ch;
        for(;;){
            if(f_read(&f,&ch,1,&n)!=FR_OK){ok=false;break;}
            if(!n||ch=='\n'){
                line[used]=0;if(used&&!jack_chip8_option(c,line))ok=false;
                used=0;if(!n||!ok)break;
            }else if(ch!='\r'){
                if(used>=sizeof(line)-1){ok=false;break;}line[used++]=ch;
            }
        }
        f_close(&f);
    }else if(cfg!=FR_NO_FILE)ok=false;
    if(!ok)return false;
    return jack_chip8_load(rom,(size_t)len,c);
}
static void render(void){
    if(!hstx_framebuffer_available())return;
    uint16_t *p=(uint16_t *)hstx_getframebuffer();unsigned w,h;
    const uint8_t *src=jack_chip8_pixels(&w,&h);unsigned scale=w==64?4:2;
    memset(p,0,320*240*2);
    for(unsigned y=0;y<h*scale;y++)for(unsigned x=0;x<w*scale;x++)
        p[(56+y)*320+32+x]=src[y/scale*w+x/scale]?0x7fff:0;
    if(jack_chip8_halted())text(p,8,220,*jack_chip8_error()?jack_chip8_error():"PROGRAM ENDED - ESC FOR MENU",0x7fe0);
    hstx_present_frame();
}
int main(void){
    vreg_set_voltage(VREG_VOLTAGE_1_30);sleep_ms(10);set_sys_clock_khz(252000,true);
    stdio_init_all();printf("CHIP8_BOOT 0.1.1 Fruit Jam\n");
    pico_fatfs_spi_config_t cfg={SDCARD_SPI,CLK_SLOW_DEFAULT,CLK_FAST_DEFAULT_PIO,SDCARD_PIN_MISO,SDCARD_PIN_CS,SDCARD_PIN_SCK,SDCARD_PIN_MOSI,true};
    if(!pico_fatfs_set_config(&cfg))panic("SD pins");
    FRESULT sd=f_mount(&fs,"",1);hstx_init(false);
    jack_input_init(&queue);jack_input_devices_init(&devices,&queue);
    usb_ok=jack_tinyusb_input_init(&devices);pad_ok=false;
    FRESULT listing=sd==FR_OK?scan():sd;
    printf("CHIP8_SD result=%d count=%u usb=%u pad=%u\n",listing,names_count,usb_ok,pad_ok);
    unsigned old=0;jack_chip8_config c;const char *message="UP/DOWN + ENTER/A: play";
    uint64_t next=time_us_64()*60,report=time_us_64()+1000000;
    audio_next=time_us_64()*44100u;uint32_t frames=0;
    while(true){
        poll();service_audio();
        unsigned pad=devices.pads[0];
        unsigned buttons=(key(0x52)||(pad&(1u<<6))?1:0)|(key(0x51)||(pad&(1u<<7))?2:0)|
            (key(0x28)||(pad&1)?4:0)|(key(0x29)||((pad&0x30)==0x30)?8:0);
        unsigned edge=buttons&~old;old=buttons;
        if(playing&&(edge&8)){playing=false;rd=wr=count=0;message="UP/DOWN + ENTER/A: play";}
        if(!playing){
            if(names_count){
                if(edge&1)selected=(selected+names_count-1)%names_count;
                if(edge&2)selected=(selected+1)%names_count;
                if(edge&4){
                    playing=load(&c);message=playing?"":"ROM/config load failed";
                    rd=wr=count=0;next=time_us_64()*60;
                    audio_buffering=playing;
                    printf("CHIP8_LOAD %s ok=%u\n",names[selected],playing);
                    audio_next=time_us_64()*44100u;next=time_us_64()*60;
                }
            }else message="No ROMs in /roms/CHIP8";
        }
        uint64_t now=time_us_64();
        if(now*60>=next){
            next+=1000000;if(now*60>next+3000000)next=now*60;
            if(playing){
                uint16_t keys=0;
                for(unsigned k=0;k<16;k++)if(key(keymap[k]))keys|=1u<<k;

                unsigned gamepad=pad;
                if(key(0x52))gamepad|=1u<<6;
                if(key(0x51))gamepad|=1u<<7;
                if(key(0x50))gamepad|=1u<<8;
                if(key(0x4f))gamepad|=1u<<9;
                for(unsigned p=0;p<12;p++)if(gamepad&(1u<<p))keys|=1u<<c.pad[p];
                jack_chip8_keys(keys);jack_chip8_frame(pcm);
                for(unsigned i=0;i<735;i++){if(count==4096){audio_over++;break;}samples[wr]=pcm[i];wr=(wr+1)&4095;count++;}
                service_audio();render();service_audio();frames++;
            }else menu(message);
        }
        if(now>=report){
            service_audio();
            printf("CHIP8_ALIVE frames=%lu queue=%u under=%lu over=%lu halt=%u rebuf=%lu resync=%lu\n",(unsigned long)frames,count,(unsigned long)audio_under,(unsigned long)audio_over,playing&&jack_chip8_halted(),(unsigned long)audio_rebuffer,(unsigned long)audio_resync);
            service_audio();
            report=now+1000000;
        }
        sleep_us(100);
    }
}
