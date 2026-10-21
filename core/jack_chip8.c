// SPDX-License-Identifier: BSD-3-Clause
#include "jack_chip8.h"
#include "octo_emulator.h"
static octo_emulator emu;
static uint32_t tone_phase;
void jack_chip8_defaults(jack_chip8_config *c) {
    *c=(jack_chip8_config){.tickrate=20,.pad={5,0,7,9,15,5,2,8,4,6,1,3}};
}
bool jack_chip8_option(jack_chip8_config *c,const char *line) {
    char key[32], tail;
    unsigned v;
    if(!line[0]||line[0]=='#'||line[0]=='\n'||line[0]=='\r')return true;
    if(sscanf(line," %31[^=]=%u %c",key,&v,&tail)!=2)return false;
    if(!strcmp(key,"tickrate")){if(v<1||v>10000)return false;c->tickrate=v;return true;}
    unsigned *q=NULL;
    if(!strcmp(key,"shift"))q=&c->shift;
    if(!strcmp(key,"loadstore"))q=&c->loadstore;
    if(!strcmp(key,"jump0"))q=&c->jump0;
    if(!strcmp(key,"logic"))q=&c->logic;
    if(!strcmp(key,"clip"))q=&c->clip;
    if(!strcmp(key,"vblank")){if(v>2)return false;c->vblank=v;return true;}
    if(q){if(v>1)return false;*q=v;return true;}
    unsigned index;
    if(sscanf(key,"pad%u%c",&index,&tail)==1 && index<12 && v<16){
        c->pad[index]=(uint8_t)v;return true;
    }
    return false;
}
static void fault(const char *s){emu.halt=1;snprintf(emu.halt_message,sizeof(emu.halt_message),"%s",s);}
bool jack_chip8_load(const uint8_t *rom,size_t size,const jack_chip8_config *c){
    if(!rom||!c||size<2||size>3584||c->tickrate<1||c->tickrate>10000){
        memset(&emu,0,sizeof(emu));fault("ROM must contain 2..3584 bytes");return false;
    }
    octo_options o;octo_default_options(&o);
    o.tickrate=c->tickrate;o.q_shift=c->shift;o.q_loadstore=c->loadstore;
    o.q_jump0=c->jump0;o.q_logic=c->logic;o.q_clip=c->clip;o.q_vblank=c->vblank;
    octo_emulator_init(&emu,(char *)rom,size,&o,NULL);tone_phase=0;
    return true;
}
void jack_chip8_keys(uint16_t keys){
    for(unsigned k=0;k<16;k++){
        bool down=(keys&(1u<<k))!=0;
        if(emu.wait && down && !emu.keys[k] && emu.pending<0)emu.pending=(int)k;
        if(emu.wait && !down && emu.keys[k] && emu.pending==(int)k){
            emu.v[(unsigned)emu.wait_reg]=k;emu.wait=0;emu.pending=-1;
        }
        emu.keys[k]=down;
    }
}
static bool safe_instruction(void){

    if(emu.pc>65529){fault("PC outside executable memory");return false;}
    unsigned op=(emu.ram[emu.pc]<<8)|emu.ram[emu.pc+1], major=op>>12;
    if((op==0x00ee && emu.rp==0)||(major==2 && emu.rp>=12)){
        fault("Invalid return stack");return false;
    }
    if((major==13 && emu.i>65536-64)||(major==15 && emu.i>65536-16)){
        fault("Memory operand outside RAM");return false;
    }

    if(op==0xf000||(op&0xf0ff)==0xf001||(op&0xf0ff)==0xf002||
       (op&0xf0ff)==0xf03a||(op&0xfff0)==0x00d0||
       (major==5&&(op&15))||(major==9&&(op&15))){
        fault("Unsupported opcode / XO-CHIP ROM");return false;
    }
    return true;
}
void jack_chip8_frame(int16_t pcm[735]){
    for(int n=0;n<emu.options.tickrate&&!emu.halt&&!emu.wait;n++){
        if(!safe_instruction())break;
        bool draw=(emu.options.q_vblank==1||(emu.options.q_vblank==2&&!emu.hires))&&
                  (emu.ram[emu.pc]&0xf0)==0xd0;
        octo_emulator_instruction(&emu);
        if(draw)break;
    }
    bool sound=emu.st>0&&!emu.halt;
    for(unsigned n=0;n<735;n++){

        pcm[n]=sound?(tone_phase<22050?5000:-5000):0;
        tone_phase=(tone_phase+440)%44100;
    }
    if(emu.dt)emu.dt--;
    if(emu.st)emu.st--;
}
const uint8_t *jack_chip8_pixels(unsigned *w,unsigned *h){
    *w=emu.hires?128:64;*h=emu.hires?64:32;return emu.px;
}
const char *jack_chip8_error(void){return emu.halt_message;}
bool jack_chip8_halted(void){return emu.halt!=0;}
