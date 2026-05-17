# Building Life Puck with PlatformIO + VS Code

This project is configured for **PlatformIO** in **Visual Studio Code**.
The Arduino IDE is no longer required (or recommended) — all the
configuration that used to live in `Arduino/libraries/.../User_Setup.h`
and `Arduino/libraries/lv_conf.h` now lives inside this repo, where it
can be version-controlled with the rest of the code.

## Why pioarduino, not the stock PlatformIO platform?

This project uses **LVGL 9.5**, which needs the **Arduino-ESP32 3.x**
core. Espressif officially dropped PlatformIO support, so the
`platformio/espressif32` platform on the PIO registry is stuck on
Arduino-ESP32 **2.x** and will not build this code.

The community-maintained
[`pioarduino/platform-espressif32`](https://github.com/pioarduino/platform-espressif32)
fork tracks the current Arduino-ESP32 release. `platformio.ini` is
already pointed at its `stable` channel, so you don't have to do
anything extra — but be aware that this is the reason you can't just
write `platform = espressif32`.

## First-time setup

1. **Install VS Code**: https://code.visualstudio.com/
2. **Install the PlatformIO IDE extension**. Open VS Code's Extensions
   panel (`Ctrl+Shift+X`), search for **PlatformIO IDE**, and install it.
   On first install it downloads the PlatformIO core (one-time, ~5 min).
3. **Clone or unzip this project**, then **File → Open Folder…** and
   select the `life_puck/` directory (the one containing
   `platformio.ini`). VS Code will detect it as a PlatformIO project.
4. On first open VS Code will offer to install the recommended extension
   from `.vscode/extensions.json`. Accept.
5. The first time you build, PlatformIO will download the toolchain
   (xtensa-esp32s3, ~150 MB) and the three libraries (LVGL, TFT_eSPI,
   CST816S). This takes a few minutes; subsequent builds are seconds.

## Day-to-day commands

In the VS Code status bar you'll see icons for each:

| Icon | Action | CLI equivalent |
|---|---|---|
| ✓ | Build | `pio run` |
| → | Upload | `pio run -t upload` |
| 🔌 | Serial monitor | `pio device monitor` |
| 🗑 | Clean | `pio run -t clean` |

Plug the board in over USB, hit Upload, and watch the serial monitor at
**115200 baud**. The board enters bootloader mode automatically on
upload — no need to hold BOOT.

## How the libraries are configured

You don't need to copy any files into `Arduino/libraries/` anymore.
Everything is driven from `platformio.ini`:

- **TFT_eSPI** — All the pin assignments and driver selection that used
  to live in `User_Setup.h` are now `build_flags` in `platformio.ini`.
  The library sees `USER_SETUP_LOADED=1` and uses those defines
  directly, skipping its `User_Setup_Select.h`. If you ever need to
  change a pin, edit `platformio.ini`, not any file inside the library.
- **LVGL** — Your customised `lv_conf.h` lives at `src/lv_conf.h`. The
  build flag `-DLV_CONF_INCLUDE_SIMPLE` plus `-I src` tells LVGL to find
  it via `#include "lv_conf.h"`. Edit the one in `src/`; do not place
  copies anywhere else.
- **CST816S** — No configuration; pulled clean from the registry.

## Project layout

```
life_puck/
├── platformio.ini          ← single source of truth for the build
├── BUILDING.md             ← this file
├── README.md               ← project overview (hardware, architecture)
├── .vscode/                ← VS Code workspace hints
├── .gitignore
└── src/                    ← all C/C++ source compiled into the firmware
    ├── main.cpp            ← previously life_puck.ino
    ├── lv_conf.h           ← LVGL configuration (used to live at
    │                         Arduino/libraries/lv_conf.h)
    ├── App.{cpp,h}
    ├── Backlight.{cpp,h}
    ├── ...
    └── font_*.c            ← generated FontAwesome / numeric fonts
```

## Migrating an existing Arduino IDE checkout

If you previously built this in the Arduino IDE:

1. **Do not** copy your `libraries/` folder into the PIO project. PIO
   manages dependencies on its own.
2. **Do not** copy `User_Setup.h` or `lv_conf.h` from your Arduino
   `libraries/` folder. The equivalents are already baked into
   `platformio.ini` and `src/lv_conf.h`.
3. If you had a custom `Config.h` tweak (e.g. a different
   `STARTING_LIFE`), bring that one file across — but anything that was
   in a library's `User_Setup.h` belongs in `platformio.ini`'s
   `build_flags` now.

## Reproducible builds

`platformio.ini` currently uses the pioarduino `stable` channel and
caret pins (`^9.5.0`, etc.) for libraries, so you get bug-fix updates
automatically. Once your build is happy and you want byte-for-byte
reproducibility, pin everything to exact versions:

- Change `stable` → a pinned tag like
  `download/55.03.37/platform-espressif32.zip` (see the
  [pioarduino releases page](https://github.com/pioarduino/platform-espressif32/releases)).
- Change `^9.5.0` → `9.5.0`, `^2.5.43` → `2.5.43`, etc., or commit a
  `platformio.lock` after a successful build.

## Troubleshooting

- **"Cannot find LVGL header" / "lv_conf.h not found"** — verify
  `src/lv_conf.h` exists and that `platformio.ini` still has both
  `-D LV_CONF_INCLUDE_SIMPLE` *and* `-I src` in `build_flags`. They
  work together; missing either one breaks the include.
- **TFT shows garbage or wrong colours** — re-check that every
  `-D TFT_*` flag in `platformio.ini` matches your board's wiring. The
  defaults are for the Waveshare ESP32-S3-Touch-LCD-1.28-B.
- **Binary too large** — switch to an 8 MB partition table. There's a
  commented `[env:life_puck_8mb_psram]` block at the bottom of
  `platformio.ini` showing how.
- **"package incompatible" or "Arduino 3.x not supported"** — you've
  accidentally fallen back to `platform = espressif32` somewhere.
  Make sure the `platform = https://github.com/pioarduino/...` URL in
  `[env]` is intact.
