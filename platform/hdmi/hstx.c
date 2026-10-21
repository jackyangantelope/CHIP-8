// SPDX-License-Identifier: GPL-3.0-only
#if PICO_RP2350
#include "hstx.h"
#include "pico/multicore.h"
#include "stdio.h"

volatile bool HSTX_vblank = false;
static uint8_t FRAMEBUFFER[(MODE_H_ACTIVE_PIXELS / 2) * (MODE_V_ACTIVE_LINES / 2) * 2] __attribute__((aligned(4)));
#if JACK_HSTX_DOUBLE_BUFFER
static uint8_t BACKBUFFER[sizeof(FRAMEBUFFER)] __attribute__((aligned(4)));
static uint32_t frame_pending;
#endif

#if JACK_HSTX_DOUBLE_BUFFER
static uint8_t *WriteBuf = BACKBUFFER;
#else
static uint8_t *WriteBuf = FRAMEBUFFER;
#endif
static uint8_t *DisplayBuf = FRAMEBUFFER;
static uint8_t *LayerBuf = FRAMEBUFFER;
static uint16_t *tilefcols;
static uint16_t *tilebcols;
static volatile int enableScanLines = 0;
static volatile int enableAspectRatio87 = 0;
static volatile int scanlineType = 0;
static volatile int scanlineMode = 0;
#define HRes (MODE_H_ACTIVE_PIXELS / 2)
#define VRes (MODE_V_ACTIVE_LINES / 2)
void hstx_waitForVSync(void)
{

    uint32_t current = video_frame_count;
    while (video_frame_count == current)
    {
        tight_loop_contents();
    }
}

void hstx_paceFrame(bool init)
{

    static uint32_t target_frame = 0;
    static uint32_t pace_accum = 0;
    if (init)
    {
        target_frame = video_frame_count;
        pace_accum = 0;
    }

    pace_accum += 600000u;
    uint32_t step = pace_accum / 598261u;
    pace_accum -= step * 598261u;
    target_frame += step;
    int32_t lag = (int32_t)(video_frame_count - target_frame);
    if (lag < 0)
    {

        while ((int32_t)(video_frame_count - target_frame) < 0)
        {
            tight_loop_contents();
        }
    }
    else if (lag > 0)
    {

        target_frame = video_frame_count;
    }

}
uint8_t *hstx_getframebuffer(void)
{

    return WriteBuf;
}

bool hstx_framebuffer_available(void)
{
#if JACK_HSTX_DOUBLE_BUFFER
    return __atomic_load_n(&frame_pending, __ATOMIC_ACQUIRE) == 0;
#else
    return true;
#endif
}

bool hstx_present_frame(void)
{
#if JACK_HSTX_DOUBLE_BUFFER
    if (!hstx_framebuffer_available()) return false;

    __atomic_store_n(&frame_pending, 1, __ATOMIC_RELEASE);
#endif
    return true;
}

void hstx_setScanLines(int enable)
{

    enableScanLines = enable ? 1 : 0;
}

void hstx_setAspectRatio87(int enable)
{
    enableAspectRatio87 = enable ? 1 : 0;
}

void hstx_setScanLineType(int type)
{
    scanlineType = type;
}

uint16_t *__not_in_flash_func(hstx_getlineFromFramebuffer)(int scanline)
{
    return (uint16_t *)(WriteBuf + (scanline * HRes * 2));
}
void __not_in_flash_func(hstx_vsync_callbackfunc)(void)
{
#if JACK_HSTX_DOUBLE_BUFFER
   if (__atomic_load_n(&frame_pending, __ATOMIC_ACQUIRE)) {
       uint8_t *old_display = DisplayBuf;
       DisplayBuf = WriteBuf;
       WriteBuf = old_display;

       __atomic_store_n(&frame_pending, 0, __ATOMIC_RELEASE);
   }
#endif
   HSTX_vblank = true;
}

void __not_in_flash_func(scanline_callbackfunc)(uint32_t v_scanline, uint32_t active_line, uint32_t *buff)
{
    int load_line = v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
    if (load_line < 0 || load_line >= MODE_V_ACTIVE_LINES)
        return;

    HSTX_vblank = false;
    __dmb();

    int Line_dup = load_line >> 1;
    const uint32_t DARKEN_MASK = 0x7BDE7BDEu;
    const uint16_t DM = 0x7BDEu;
    int is_odd_line = load_line & 1;
    int do_scanline = is_odd_line && enableScanLines;
    int stype = enableScanLines ? scanlineType : 0;

    if (enableAspectRatio87) {
        const uint16_t *sp = (const uint16_t *)&DisplayBuf[Line_dup * MODE_H_ACTIVE_PIXELS] + 34;
        uint32_t *dp = buff;

        volatile uint32_t *bp = dp;
        for (int i = 0; i < 16; i++)
            bp[i] = 0;
        dp += 16;

        if (stype == 1) {
            for (int g = 0; g < 36; g++) {
                uint32_t p0=*sp++,p1=*sp++,p2=*sp++,p3=*sp++,p4=*sp++,p5=*sp++,p6=*sp++;
                if (do_scanline) {
                    p0=(p0&DM)>>1; p1=(p1&DM)>>1; p2=(p2&DM)>>1; p3=(p3&DM)>>1;
                    p4=(p4&DM)>>1; p5=(p5&DM)>>1; p6=(p6&DM)>>1;
                }
                uint32_t d0=(p0&DM)>>1,d1=(p1&DM)>>1,d2=(p2&DM)>>1,d3=(p3&DM)>>1;
                uint32_t d4=(p4&DM)>>1,d5=(p5&DM)>>1,d6=(p6&DM)>>1;

                *dp++=p0|(d0<<16); *dp++=p1|(d1<<16);
                *dp++=p2|(d2<<16); *dp++=p2|(d3<<16);
                *dp++=p3|(d4<<16); *dp++=p4|(d5<<16);
                *dp++=p5|(d5<<16); *dp++=p6|(d6<<16);
            }
        } else if (do_scanline) {
            for (int g = 0; g < 36; g++) {
                uint32_t p0=(*sp++&DM)>>1,p1=(*sp++&DM)>>1,p2=(*sp++&DM)>>1;
                uint32_t p3=(*sp++&DM)>>1,p4=(*sp++&DM)>>1,p5=(*sp++&DM)>>1;
                uint32_t p6=(*sp++&DM)>>1;
                *dp++=p0|(p0<<16); *dp++=p1|(p1<<16);
                *dp++=p2|(p2<<16); *dp++=p2|(p3<<16);
                *dp++=p3|(p4<<16); *dp++=p4|(p5<<16);
                *dp++=p5|(p5<<16); *dp++=p6|(p6<<16);
            }
        } else {
            for (int g = 0; g < 36; g++) {
                uint32_t p0=*sp++,p1=*sp++,p2=*sp++,p3=*sp++,p4=*sp++,p5=*sp++,p6=*sp++;
                *dp++=p0|(p0<<16); *dp++=p1|(p1<<16);
                *dp++=p2|(p2<<16); *dp++=p2|(p3<<16);
                *dp++=p3|(p4<<16); *dp++=p4|(p5<<16);
                *dp++=p5|(p5<<16); *dp++=p6|(p6<<16);
            }
        }

        bp = dp;
        for (int i = 0; i < 16; i++)
            bp[i] = 0;
        dp += 16;
    } else {
        const uint32_t *src = (const uint32_t *)&DisplayBuf[Line_dup * MODE_H_ACTIVE_PIXELS];
        uint32_t *dst = buff;
        const uint32_t iters = MODE_H_ACTIVE_PIXELS / 4;

        if (stype == 1) {
            for (uint32_t i = 0; i < iters; i++) {
                uint32_t pair = src[i];
                if (do_scanline) pair = (pair & DARKEN_MASK) >> 1;
                uint32_t px0 = pair & 0xFFFFu;
                uint32_t px1 = pair >> 16;

                *dst++ = px0 | (((px0 & DM) >> 1) << 16);
                *dst++ = px1 | (((px1 & DM) >> 1) << 16);
            }
        } else if (do_scanline) {
            for (uint32_t i = 0; i < iters; i++) {
                uint32_t pair = (src[i] & DARKEN_MASK) >> 1;
                uint32_t px0 = pair & 0xFFFFu;
                uint32_t px1 = pair >> 16;
                *dst++ = px0 | (px0 << 16);
                *dst++ = px1 | (px1 << 16);
            }
        } else {
            for (uint32_t i = 0; i < iters; i++) {
                uint32_t pair = src[i];
                uint32_t px0 = pair & 0xFFFFu;
                uint32_t px1 = pair >> 16;
                *dst++ = px0 | (px0 << 16);
                *dst++ = px1 | (px1 << 16);
            }
        }
    }
}

extern volatile uint32_t video_frame_count;
uint32_t hstx_getframecounter(void)
{
    return video_frame_count;
}

void __not_in_flash_func(hstx_push_audio_sample)(const int left, const int right)
{
    static int g_hdmi_audio_frame_counter = 0;
    static audio_sample_t acc_buf[4];
    static int acc_count = 0;
    static  uint32_t video_frame_count = 0;

    acc_buf[acc_count & 3].left = left;
    acc_buf[acc_count & 3].right = right;
    acc_count++;
    if (acc_count >= 4)
    {
        if (hstx_di_queue_get_level() >= HSTX_AUDIO_DI_HIGH_WATERMARK)
        {
            acc_count = 0;
            return;
        }
        hstx_packet_t packet;

        g_hdmi_audio_frame_counter = hstx_packet_set_audio_samples_cs(&packet, acc_buf, 4, g_hdmi_audio_frame_counter);

        hstx_data_island_t island;
        hstx_encode_data_island(&island, &packet, false, true);
        (void)hstx_di_queue_push(&island);
        acc_count = 0;
    }
}

static bool s_hstx_dvi_mode = false;

#if HSTX_CORE1_STACK_IN_SCRATCH_X
extern uint32_t __StackOneBottom[];
extern uint32_t __StackOneTop[];

void *hstx_default_core1_stack(size_t *out_bytes)
{
    if (out_bytes)
        *out_bytes = (size_t)((uintptr_t)__StackOneTop - (uintptr_t)__StackOneBottom);
    return __StackOneBottom;
}
#else
static uint32_t core1_stack[1024] __attribute__((aligned(8)));

void *hstx_default_core1_stack(size_t *out_bytes)
{
    if (out_bytes) *out_bytes = sizeof(core1_stack);
    return core1_stack;
}
#endif

void hstx_init(bool dviOnly)
{
    s_hstx_dvi_mode = dviOnly;
    video_output_set_dvi_mode(dviOnly);
    hstx_di_queue_init();
    video_output_set_vsync_callback(hstx_vsync_callbackfunc);
    video_output_init(640, 480);
    pico_hdmi_set_audio_sample_rate(44100);
    video_output_set_scanline_callback(scanline_callbackfunc);
    size_t stack_bytes;
    uint32_t *stack = (uint32_t *)hstx_default_core1_stack(&stack_bytes);
    multicore_launch_core1_with_stack(video_output_core1_run, stack, stack_bytes);
    printf("Pico HDMI initialized.\n");
}

void hstx_restart_core1(uint32_t *new_stack, size_t new_stack_bytes)
{

    uint32_t audio_rate = pico_hdmi_get_audio_sample_rate();

    video_output_stop();
    multicore_reset_core1();

    video_output_set_dvi_mode(s_hstx_dvi_mode);
    hstx_di_queue_init();
    video_output_set_vsync_callback(hstx_vsync_callbackfunc);
    video_output_init(640, 480);
    pico_hdmi_set_audio_sample_rate(audio_rate);
    video_output_set_scanline_callback(scanline_callbackfunc);

    multicore_launch_core1_with_stack(video_output_core1_run, new_stack, new_stack_bytes);
}
#endif
