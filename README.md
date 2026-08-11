# SmartClock — firmware (v3.54)

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
| Clean | 🗑 Clean | `pio run -t clean` |

Upload over WiFi instead of USB (the sketch runs ArduinoOTA): set `upload_port`
to the clock's IP in the `[env:ota]` section of `platformio.ini`, then

```bash
pio run -e ota -t upload
```

Or upload from a browser: open the clock's web UI → **System Settings → Upload
Firmware**. It accepts **either** kind of `.bin`:

- any `../BIN/SmartClock_vX.XX.bin` (the merged image — normally the one you want)
- `.pio/build/esp32dev/firmware.bin` (the app-only image)

The merged file also contains the bootloader and partition table, which are
only valid at flash offset `0x0` over USB. OTA writes into an app partition
instead, so the firmware detects a merged image and flashes just its app
payload — the part that is byte-identical to `firmware.bin`. The bootloader
and partition table cannot be changed over OTA at all; those still need USB.

The page posts the file in 32 KB chunks with a live KB counter, retrying any
chunk that a WiFi dip kills. Budget roughly 5–10 minutes for ~1.5 MB. Saved
settings live in a separate SPIFFS partition and are **not** touched. An
invalid image is rejected and the clock keeps running the old firmware.

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
│   └── WebPage_gz.h          GENERATED from web/index.html at build time
├── lib/                      libraries that must NOT come from the registry
│   ├── Adafruit_GFX/         modified + this project's fonts
│   ├── ESP32-HUB75-...-DMA/  modified R/G/B pin mapping
│   ├── EasyButton/           touch variant removed
│   └── qrcoderm/             not published on the registry
├── web/index.html            the settings page — EDIT THIS (gzipped into the
│                             firmware at build time by scripts/build_web.py)
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
settings, WiFi credentials and the weather cache intact. Nothing further is
needed on a blank board — the settings page is compiled into the firmware, and
the clock creates its own `/settings.json` on SPIFFS at first run.

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

**Editing the settings web page:** edit `web/index.html` — a normal HTML file
with normal syntax highlighting. On every build, `scripts/build_web.py` gzips
it into `src/WebPage_gz.h` (generated, gitignored, never edit by hand) and
`handleRoot()` serves those bytes with `Content-Encoding: gzip`. This takes the
page from ~93 KB to ~16 KB over the air — it used to be a 93 KB raw literal in
a `.h` file sent uncompressed in chunks, which on a weak WiFi link regularly
stalled or arrived half-rendered. Just rebuild and upload as usual; there is no
separate filesystem-upload step.

**Adding a new function?** Add a prototype to the *Forward declarations* block
near the top of `main.cpp`. The Arduino IDE used to generate these invisibly;
plain C++ has no such step, so a function must be declared before it is called.

**Flash is 75% full** (1.48 MB of the 1.875 MB app partition). There is room,
but the bitmap headers in `src/` are what fill it. It was 79% before the web
page moved to a gzipped blob (see above), which freed ~77 KB.
