# SmartClock — firmware (v2.92)

ESP32 + HUB75 64×32 LED matrix clock: NTP time, PirateWeather / OpenWeatherMap,
day-night terminator world map, analogue faces, moon phase, and a web settings UI.

Converted from the Arduino IDE sketch `MyClock_2.89.ino` to a PlatformIO project
so it can be built and edited in VS Code. That conversion is what v2.90 is —
no functional changes to the clock itself.

The version lives in exactly one place you ever hand-edit: `float ver` near
the top of `src/main.cpp` (shown on the display and served in the web UI).
`scripts/merge_firmware.py` reads that value directly to name each build's
`.bin`, then automatically bumps it by +0.01 (in `main.cpp`, this file's
title above, and the header comment in `platformio.ini`) for the next build —
see CHANGELOG.md for a running history of what changed at each version.

---

## Getting started

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE**
   extension (VS Code will offer it automatically — see `.vscode/extensions.json`).
2. Open **this folder** (`Firmware/Other`) as the workspace root. PlatformIO
   looks for `platformio.ini` in the folder you open, so opening a parent
   folder will not work.
3. First build downloads the toolchain (~250 MB, one time).

| Task | PlatformIO toolbar | Command line |
| --- | --- | --- |
| Build | ✔ Build | `pio run` |
| Upload over USB | → Upload | `pio run -t upload` |
| Serial monitor (115200) | 🔌 Monitor | `pio device monitor` |
| Upload `data/` to SPIFFS | Platform → Upload Filesystem Image | `pio run -t uploadfs` |
| Clean | 🗑 Clean | `pio run -t clean` |

Upload over WiFi instead of USB (the sketch runs ArduinoOTA): set `upload_port`
to the clock's IP in the `[env:ota]` section of `platformio.ini`, then

```bash
pio run -e ota -t upload
```

---

## Layout

```
Other/                        <- open THIS folder in VS Code
├── platformio.ini            build configuration (board, flags, libraries)
├── src/
│   ├── main.cpp              the sketch (was MyClock_2.89.ino)
│   ├── Clock_Faces.h         clock face bitmaps
│   ├── Maps.h                world map bitmaps
│   ├── WeatherIcons.h        weather icon bitmaps
│   └── WEB_Settings_HTML.h   the settings web page
├── lib/                      libraries that must NOT come from the registry
│   ├── Adafruit_GFX/         modified + this project's fonts
│   ├── ESP32-HUB75-...-DMA/  modified R/G/B pin mapping
│   ├── EasyButton/           touch variant removed
│   └── qrcoderm/             not published on the registry
├── data/index.html           SPIFFS contents (`pio run -t uploadfs`)
├── partitions/min_spiffs.csv partition table
├── scripts/merge_firmware.py post-build single-file image builder
├── extras/                   not compiled — source art, old IDE tool
│   ├── Icons/                PNGs the header bitmaps were generated from
│   └── tools/esp32fs.jar     the old Arduino IDE SPIFFS uploader
└── ../BIN/                   every build's flashable image accumulates here
```

---

## Hardware / board settings

These are baked into `platformio.ini`; they reproduce what the Arduino IDE was set to.

| | |
| --- | --- |
| Board | ESP32 Dev Module (`esp32dev`) |
| Flash | 4 MB, QIO, 80 MHz |
| CPU | 240 MHz |
| Partitions | Minimal SPIFFS — 1.9 MB app w/ OTA, 190 KB SPIFFS |
| Arduino task core | 1 (events on core 0) |
| Core debug level | None |
| Panel | 64×32, chain of 1 |
| I²C | SDA 21, SCL 19 (AHT10 + DS3231) |

The generated partition table is byte-for-byte identical to the one the Arduino
IDE produced, so an OTA update from an existing clock will land in the right place.

---

## Flashing a blank ESP32

Each build drops `../BIN/SmartClock_v<version>.bin` — a **complete** image —
bootloader, partition table, OTA selector and application merged into one
file, named after whatever version you just built (see CHANGELOG.md for the
history). Write the one you want at offset `0x0`:

```bash
esptool.py --chip esp32 --port /dev/cu.usbserial-0001 --baud 921600 write_flash 0x0 SmartClock_v<version>.bin
```

It deliberately does **not** contain a SPIFFS image, so flashing it leaves saved
settings, WiFi credentials and the weather cache intact. On a genuinely blank
board, follow up with `pio run -t uploadfs` to write `data/`.

Every `pio run` produces a new one of these (version auto-increments each
build — see below) and adds it to `../BIN/` alongside every previous build,
so that folder is a running history, not a single file that gets overwritten.
If you need the app-only binary for a manual OTA upload instead, it is
`.pio/build/esp32dev/firmware.bin`.

---

## Notes for future work

**Stay on Arduino-ESP32 2.x.** The vendored HUB75 driver drives the I2S
peripheral through registers (`i2s_dev_t`, `I2S0`/`I2S1`, `I2S_NUM_MAX`,
`PIN_FUNC_SELECT`) that ESP-IDF 5 removed. Arduino-ESP32 3.x is built on IDF 5,
so the panel driver will not compile there. Moving to 3.x means first upgrading
`lib/ESP32-HUB75-MatrixPanel-I2S-DMA/` to a release that uses the LCD_CAM/GDMA
peripheral. `platformio.ini` pins `espressif32@6.12.0` (core 2.0.17) for this reason.

**Anything in `lib/` is modified — do not "update" it.** Replacing
`Adafruit_GFX` or the HUB75 driver with a registry version will lose the
green/blue pin-swap fix and the custom fonts.

**Adding a new function?** Add a prototype to the *Forward declarations* block
near the top of `main.cpp`. The Arduino IDE used to generate these invisibly;
plain C++ has no such step, so a function must be declared before it is called.

**Flash is 79% full** (1.55 MB of the 1.875 MB app partition). There is room,
but the bitmap headers in `src/` are what fill it.
