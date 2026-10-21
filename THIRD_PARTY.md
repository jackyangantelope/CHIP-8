# Third-party sources

| Component | Source and revision | License | Included files |
|---|---|---|---|
| C-Octo | https://github.com/JohnEarnest/c-octo, `102994e6166236045a067efa930a88532510e03e` | MIT | `deps/c-octo/src/octo_emulator.h`, `deps/c-octo/LICENSE.txt` |
| pico_shared | https://github.com/fhoedemakers/pico_shared, `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` | GPL-3.0-only | `deps/pico_shared/`, including its `LICENSE` |
| FatFs within pico_shared | See `deps/pico_shared/drivers/pico_fatfs` | Component license | `deps/pico_shared/drivers/pico_fatfs/LICENSE` |

The adapted HDMI files in `platform/hdmi/` retain GPL-3.0-only SPDX notices. Game ROMs and saves are excluded. The SDK, Pico-PIO-USB and TinyUSB are external build dependencies.
