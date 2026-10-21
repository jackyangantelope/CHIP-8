# CHIP-8 / Super-CHIP for RP2350B

Source release of a CHIP-8 and modern Super-CHIP interpreter for the public Adafruit Fruit Jam (RP2350B). The UF2 links at the loader application address `0x10080000`; it is **not** a standalone firmware image. No ROM, game, save, or prebuilt UF2 is included.

## Status

The project passed basic testing as reported by the release requester on 2026-10-21. The recorded host tests include four games for 1,800 frames each under ASan/UBSan, instruction/pixel checks, and a 600-second simulated audio-scheduling regression. The original version 0.1.1 build passed ARM compilation. This public Fruit Jam variant compiles but has **not** been tested on a Fruit Jam. Long-run physical audio stability, complete key coverage, other games, and exact per-frame timing remain unverified. XO-CHIP and HP48 half-pixel scrolling are outside scope.

## Build

Requirements: CMake 3.20+, Ninja, an ARM embedded GCC toolchain, Raspberry Pi Pico SDK 2.3.x with RP2350/Fruit Jam support, Pico-PIO-USB, and TinyUSB supplied by the SDK. Set `PICO_SDK_PATH` and `PICO_PIO_USB_PATH` to local directories:

```sh
cmake -S . -B build/fruit-jam-release -G Ninja \
  -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=adafruit_fruit_jam \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/fruit-jam-release --target picoChip8
```

On Windows, `scripts/build-release.ps1` accepts explicit paths for the SDK, Pico-PIO-USB, toolchain and Ninja. Output: `build/fruit-jam-release/picoChip8.uf2`. Use it only with a compatible loader after checking target hardware.

## Games and controls

Supply your own legally obtained `.ch8` files in `/roms/CHIP8/`. An optional file named `<game>.ch8.cfg` in the same directory can set `tickrate` (1–10000), boolean quirks `shift`, `loadstore`, `jump0`, `logic`, `clip`, `vblank` (0–2), and `pad0`–`pad11` (0–15) mappings. The 16-key USB keyboard map is implemented in `firmware/chip8.c`; arrow keys map to each game's configured directions. Escape returns to the menu. The Fruit Jam source build supports USB keyboard input; the board-specific NES/SNES adapter is disabled.

The CHIP-8 sound timer drives the original square-wave buzzer at 44.1 kHz. Audio buffering does not change the 60 Hz emulation timer. Games that never set the sound timer remain silent.

## Source and licenses

The interpreter is the unchanged `src/octo_emulator.h` from [JohnEarnest/c-octo](https://github.com/JohnEarnest/c-octo), commit `102994e6166236045a067efa930a88532510e03e`, MIT license in `deps/c-octo/LICENSE.txt`. `core/jack_chip8.c` and `.h` add our ROM/config validation, opcode safety checks, key state, 60 Hz stepping, pixels and buzzer. `firmware/chip8.c` adds SD selection, USB input, HDMI rendering and audio scheduling. `platform/input/` and `platform/hdmi/` provide shared adapters; the HDMI files adapt [fhoedemakers/pico_shared](https://github.com/fhoedemakers/pico_shared) revision `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` to frame handoff. The release retargets public Fruit Jam pins and disables the gamepad adapter. See [THIRD_PARTY.md](THIRD_PARTY.md) for component licenses.

The combined frontend is distributed under the root GPL-3.0-only [LICENSE](LICENSE). C-Octo and other components retain their own licenses. Pico SDK, Pico-PIO-USB and TinyUSB are external build dependencies.
