# Clock Builder

A small native macOS app for flashing SmartClock firmware onto an ESP32 over
USB. SwiftUI, single window, no Xcode project.

```
./build.sh              # builds and delivers to ../../BIN/Clock Builder.app
./build.sh /some/path   # deliver somewhere else
```

The finished `Clock Builder.app` is written to `Firmware/BIN/`, beside the
firmware images it flashes. Only the source lives in git — the built bundle
does not, for the same reason the `.bin` files do not.

## Versioning

`VERSION` holds the number the next build will stamp, and `build.sh` bumps it by
0.01 afterwards. Same convention as `scripts/merge_firmware.py`, so the app's
version behaves like the firmware's. Started at 0.1.

## What it checks before writing anything

The point of the app is that it refuses to do the two things that leave you with
a clock that will not boot.

**Is this the right chip?** *Identify* runs `esptool flash_id` and reads the
reply. It rejects an ESP32-S2/S3/C3/C6/H2 — SmartClock is built for `esp32dev`
and pinned to Arduino-ESP32 2.x, so the image cannot run on those. It also
compares the chip's reported flash size against the partition table in the
selected image and refuses if the chip is too small, rather than truncating the
partitions.

**Is this the right kind of image?** The `.bin` is inspected by reading it, not
by trusting its name:

| | byte 0x0 | byte 0x1000 | byte 0x8000 | verdict |
|---|---|---|---|---|
| Merged (`SmartClock_vX.XX.bin`) | `FF` padding | `E9` bootloader | `AA 50` partition table | flashable at 0x0 |
| App-only (`firmware.bin`) | `E9` app | — | — | **refused** |

An app-only image belongs at 0x10000 and is what the clock's own browser update
page expects. Writing it at 0x0 produces a device with no bootloader. That is
the easiest way to brick a flash, so Clock Builder will not do it.

## Write modes

- **Update** — `write_flash 0x0`. Leaves the SPIFFS partition alone, so saved
  settings, WiFi credentials and cached weather survive.
- **Erase everything, then flash** — `erase_flash` first. For a brand new ESP32,
  or to wipe a configured one back to nothing. Asks for confirmation.

## Speed

Defaults to 460800, matching `platformio.ini`. 921600 is deliberately not
offered: the CH340 on this clock cannot sustain it on every Mac and fails with
"Invalid head of packet" partway through a write. 230400 and 115200 are there
to fall back to.

## Requirements

esptool, found via PlatformIO:

```
~/.platformio/penv/bin/python
~/.platformio/packages/tool-esptoolpy/esptool.py
```

PlatformIO's virtualenv is used because it has `pyserial`; the system
`/usr/bin/python3` does not, and esptool cannot open a serial port without it.
The app falls back to other interpreters if it finds one that works, and says so
plainly if it finds none.

Built against the macOS 12 SDK with Command Line Tools only, ad-hoc signed.
Signing happens on local disk — an AFP share adds metadata that `codesign`
rejects outright, and re-adds it faster than `xattr -cr` can strip it.

## If the port is busy

"Could not talk to a chip" with the cable known good usually means something
else holds the port. The ESP Decoder VS Code extension and an open PlatformIO
Serial Monitor both do. Disconnect them and retry.
