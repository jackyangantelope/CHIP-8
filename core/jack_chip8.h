// SPDX-License-Identifier: BSD-3-Clause
#ifndef JACK_CHIP8_H
#define JACK_CHIP8_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    unsigned tickrate;
    unsigned shift, loadstore, jump0, logic, clip, vblank;
    uint8_t pad[12];
} jack_chip8_config;
void jack_chip8_defaults(jack_chip8_config *c);
bool jack_chip8_option(jack_chip8_config *c, const char *line);
bool jack_chip8_load(const uint8_t *rom, size_t size, const jack_chip8_config *c);
void jack_chip8_keys(uint16_t keys);

// One 60 Hz frame at 44.1 kHz.
void jack_chip8_frame(int16_t pcm[735]);
const uint8_t *jack_chip8_pixels(unsigned *width, unsigned *height);
const char *jack_chip8_error(void);
bool jack_chip8_halted(void);
#endif
