# Clock Builder for Windows

A native 64-bit Windows edition of Clock Builder. It keeps the same protected
three-stage workflow as the Mac app and uses the same dark matrix interface:

1. Discover a COM port and identify the attached ESP32.
2. Inspect a full merged SmartClock image before allowing it to be selected.
3. Update while preserving SPIFFS, or explicitly erase everything first.

The app is implemented directly against Win32 and has no .NET, Python or other
runtime requirement. It supports Windows 10 and 11 on Intel/AMD 64-bit systems.

## Build

```bash
./build.sh
```

The script cross-compiles from macOS and delivers the versioned executable to:

`/Volumes/home/Documents/Arduino/SmartClock/Software/Clock Builder/Windows/`

`VERSION` holds the number stamped into the next build. A successful build
advances it by 0.01; every such bump must be committed as its own Git save
point.

## Files to keep together

- `Clock Builder X.XX.exe` — the user interface and safety checks.
- `esptool.exe` — Espressif's official serial flashing utility.
- `ESPTOOL-LICENSE.txt` — esptool's GPL-2.0-or-later licence.

Clock Builder searches for `esptool.exe` beside itself first. The Windows
esptool binary comes unmodified from Espressif's signed v5.3.1 release:

<https://github.com/espressif/esptool/releases/tag/v5.3.1>

## Safety behaviour

- Rejects app-only `firmware.bin` files which belong at offset `0x10000`.
- Accepts merged images only after finding the bootloader at `0x1000` and the
  partition table at `0x8000`.
- Refuses non-classic ESP32 families such as S2, S3, C3 and C6.
- Refuses an image whose partition table is larger than the detected flash.
- Update mode writes at `0x0` without erasing, preserving the SPIFFS partition.
- Erase mode requires a second explicit confirmation.
- Offers 460800, 230400 and 115200 baud; intentionally omits unreliable 921600.

The binary can be cross-compiled and structurally verified on macOS, but the
COM-port and flashing paths must be exercised on a real Windows PC before it is
treated as field-tested.
