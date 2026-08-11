# SmartClock Changelog

Version history for the SmartClock firmware. Entries from 2.90 onward have
real dates; everything before that is migrated verbatim from the
commented-out changelog that used to live at the top of `src/main.cpp`
(going back to the original Arduino IDE sketch), where no dates were ever
recorded — only version numbers and short notes.

> Note: from 2.90 onward the version auto-increments on **every** build, so
> some numbers below are just iteration builds with no shipped change of their
> own. Entries are written against the version that first carried the change.

## 3.43 - 2026-08-11
- **The clock now announces a name on the network** instead of appearing as a
  bare IP. It had never called `WiFi.setHostname()`, so its DHCP lease was
  requested under the default espressif name.
  - DHCP hostname (option 12) + `myWM.setHostname()` — set while in STA mode
    and *before* associating, which is required for the lease to carry it.
  - mDNS: resolves as `Clock-XXXX.local` and advertises `_http._tcp`, so
    Bonjour-aware scanners and browsers list it by name.
  - NetBIOS (NBNS) registered as well, for scanners that resolve that way.
  - `ArduinoOTA.setHostname()` so it shows as the device name rather than
    `esp32-xxxxxx` in PlatformIO's OTA target list.
- **Fixed a latent hostname bug:** the name was built with `"Clock-%4X"`. The
  width flag pads with *spaces*, so any chip id below `0x1000` produced
  `"Clock- 2B"` — not a legal hostname. Now `%04X`.
- `GET /debug` also reports `hostname` and `ip`.
- Verified on hardware: name is `Clock-3000`, `clock-3000.local` resolves to
  192.168.1.122 from macOS, and `_http._tcp Clock-3000` is advertised. NBNS
  reports as listening but could not be confirmed from this Mac, since macOS's
  own netbiosd owns UDP 137 and absorbs the reply. Note this router does not
  publish DHCP client names over DNS (`nslookup Clock-3000` returns NXDOMAIN),
  so the DHCP name will show in the Orbi's client list rather than via DNS.

## 3.39 - 2026-08-11
- Removed the red "Safe Mode active" label under Live Refresh, and Safe Mode no
  longer disables the dropdown. It now simply selects 4s and leaves the control
  fully usable — a hint rather than a restriction.
- **Fixed page charset.** The page had no `<meta charset>` and the server sent
  `text/html` with no charset, so browsers fell back to Latin-1 and mangled
  every accented character — that label's em-dash rendered as `â€"`, and more
  importantly all 70+ umlauts in the German (ä ö ü ß) and Swedish (ä ö å)
  translations were affected. Now declared in both the header and the document.

## 3.37 - 2026-08-11
- **Browser OTA now accepts the merged `BIN/SmartClock_vX.XX.bin` files.**
  Previously only the app-only `firmware.bin` worked; picking a merged image
  silently failed. The merged file is bootloader + partitions + boot_app0 + app
  and is only valid at flash offset `0x0` over USB, whereas OTA writes into an
  app partition. The handler now detects which kind it got (an ESP-IDF app
  image carries `esp_app_desc_t`'s magic `0xABCD5432` at offset `0x20`) and for
  a merged file discards the first 64KB, flashing only the app payload — the
  part that is byte-identical to `firmware.bin`.
- **Upload is now chunked (32KB) with a real KB counter.** A single POST was
  useless for progress: the browser buffers the whole file, so the bar jumped
  straight to 100% and then sat there. Chunking also lets a chunk lost to a
  WiFi dip be retried on its own instead of restarting ~1.5MB.
- `Update.begin()` is deferred until the image type is known so it can be given
  the exact app size. With an unknown size ESP-IDF erases the whole 1.9MB
  partition up front, which stalled the first request long enough to time the
  client out.
- Failures now return the real `Update.errorString()` instead of bare "FAIL".
- Verified on hardware: OTA'd a merged `SmartClock_v3.33.bin` onto a clock
  running 3.35 and confirmed it came back reporting 3.33. ~1.5MB took ~8.5 min
  on this WiFi. Settings survived (SPIFFS is a separate partition).

## 3.29 - 2026-08-11
- **WiFi power save disabled** (`WiFi.setSleep(false)`). This had never been
  turned off, so the radio was parking between DTIM beacons and the AP could
  drop inbound packets — showing up as the web UI being unreachable for tens of
  seconds while `WiFi.status()` still said `WL_CONNECTED`. Mains-powered clock,
  so the extra current is irrelevant. Measurably steadier RSSI afterwards.
- **Weather fetch: forced HTTP/1.0** (`http.useHTTP10(true)`). With chunked
  encoding `getString()` intermittently returned empty on a 200, logged as
  `JSON parsing failed: EmptyInput`, and then burned API quota retrying a
  request that had actually succeeded. An empty body is now reported as a
  transport failure rather than malformed JSON.
- **Passive connectivity watchdog**: reboots only after 30 minutes with no
  successful network activity at all (weather / NTP / a served web request).
  Stands down entirely when the weather service is "none", since then there is
  nothing to infer liveness from.
- Added `GET /debug` (uptime, heap, min heap, max alloc, WiFi status, RSSI).
- Investigated but **removed** two earlier attempts, both of which made things
  worse and are documented in the code so they don't get retried:
  an ICMP link watchdog (couldn't allocate its task once TLS had taken internal
  DRAM, and its forced reconnects cost 1–3 min of downtime each), and a
  `WiFi.hostByName()` probe (blocked `loop()` for 30 minutes when the network
  wedged, freezing the display).
- **Not fixed:** the clock still intermittently becomes unreachable from the
  LAN. Established by serial capture that this is *not* a firmware crash — see
  HANDOFF.md for the full evidence and what to try next.

## 3.11 - 2026-08-11
- **FIXED: web UI died after ~1 minute and needed a physical reboot.** Two
  compounding causes, found by adding heartbeat logging (heap was flat and both
  FreeRTOS tasks stayed alive, so it was never a leak or a crash):
  1. `fetchSettings()`'s `.catch()` cleared `isLoadingSettings` *before* calling
     `setLanguage()`, so every failed poll fired `/language` at the device —
     and `handleLanguage()` called `saveSettings()` unconditionally, writing 8KB
     to SPIFFS. Flash writes stall the flash cache and therefore the WiFi stack,
     so each write made the next poll more likely to fail: a spiral that ended
     with the device unreachable even to ping. The `.catch()` now re-applies
     translations before clearing the guard, matching the success path.
  2. `saveSettings()` now fingerprints the serialized settings (FNV-1a) and
     **skips the flash write entirely when nothing changed**. ~65 call sites
     call it unconditionally, so this kills the whole class of problem rather
     than just the `/language` instance of it.
- Added `GET /debug` (uptime, heap, min heap, WiFi status/state) and a
  once-a-minute `[Health]` serial line for diagnosing this kind of thing
- Verified: 24/24 polls over 2 minutes with flat heap, and it now recovers on
  its own from a bad patch instead of needing a reboot

## 3.02 - 2026-08-11
- Reverted the visual redesign below — kept the original page look/CSS, per
  feedback that the redesign "looked crap"
- Kept the two functional additions from that redesign: the selectable live
  refresh dropdown (1/2/3/4s) and the working browser OTA upload button

## 3.00 - 2026-08-11
- Settings page completely redesigned: new light/dark design system (follows the
  system theme, with a manual toggle in the header), responsive grid layout
  (live preview + compact cards in a left rail, screen controls in the main
  column on desktop; single column on mobile), modern cards, toggles, sliders
  and inputs. No external fonts or CDNs — everything is self-contained
- Live preview refresh is now selectable: **1s / 2s / 3s / 4s**, defaulting to
  4s, remembered per browser in `localStorage`
- **Safe Mode** (the Reboot Guard: 3+ reboots within 60s) is now published to
  the web UI as `safe_mode` in `/settings`. When active, a badge appears in the
  header and the refresh rate is forced to 4s and locked
- **Firmware Update button now works.** It previously called
  `/startUpdatePortal`, which never existed in the firmware. Added a real
  `POST /update` OTA endpoint plus an upload UI with a progress bar. Upload the
  app-only `firmware.bin` — *not* the merged `SmartClock_*.bin`
- Live preview no longer starts a request while the previous one is in flight

## 2.96 - 2026-08-10
- Live preview polling (`/screenshot.bin`, 4 KB) reduced from every 1s to every 4s
- Preview now pauses while the browser tab is hidden, and will not start a new
  request while the previous one is still in flight (they used to pile up faster
  than they completed and saturate the single-threaded web server)
- Flashed and verified on hardware

## 2.93 - 2026-08-10
- Settings page moved out of the 93 KB `src/WEB_Settings_HTML.h` string literal
  into a normal editable file, `web/index.html`
- Page is now gzipped into the firmware at build time by `scripts/build_web.py`
  and served with `Content-Encoding: gzip`: ~93 KB → ~16 KB over the air (5.8x
  fewer bytes). Fixes the settings page stalling / rendering half-drawn on a
  weak WiFi link
- Flash usage 79.1% → 75.2%
- Weather API rate limiting fixed three ways: attempts that never reach the
  network (no GPS, no API key) no longer consume the hourly quota; the counter
  stops at 10/10 instead of climbing forever (15/10, 21/10, ...); and the retry
  loop no longer hammers every 30s on states that cannot recover on their own
  (missing config, rate limited)
- `pio run -t uploadfs` is no longer part of the workflow (no `data/` directory)

## 2.91, 2.92, 2.94, 2.95, 2.97 - 2026-08-10
- Iteration/verification builds, no shipped changes of their own

## 2.90 - 2026-08-10
- Converted from the Arduino IDE sketch (`MyClock_2.89.ino`) to a PlatformIO / VS Code project
- WiFi now actively reconnects after a router reboot (fixed a 1-hour-instead-of-1-minute
  timeout typo; retries stored credentials for ~5 min before falling back to the AP portal)
- Weather API calls no longer block the web server for up to 15s
- `/settings.json` writes are atomic (temp file + rename), so a power loss
  mid-write can no longer corrupt saved settings
- SPIFFS now mounts unconditionally (it was skipped entirely if the RTC was
  missing) and logs loudly if it ever reformats
- Added a recursive mutex around settings reads/writes shared across both cores
- Added `esp_reset_reason()` logging at boot
- Menu button no longer does a full settings flash-write on every press

## 2.89
- Added Swedish Language

## 2.88
- Disabled autobrightness fluctuations under 11%

## 2.87
- Added ability to schedule screens
- BUG FIX: City search now works with iOS26

## 2.86
- BUG FIX: After 3 reboots within 1 minute, clock will revert to 15% brightness

## 2.85
- Added Temperature offset
- BUG FIX: Webpage now shows screen brightness when auto brightness is enabled by default

## 2.84
- Added Display Type selection to swap the RGB pins to correct the colours on the P2.5 or P5.0 display

## 2.83
- Added Language option: English/German

## 2.82
- Updated MenuButtonPressed() to now cycle through screen 90 and 91 when required

## 2.81
- Updated Screen3 to include seconds hand a length slider

## 2.80
- FIXED: Screen3 tick/numbers now work

## 2.79
- Added Screen9 Nixie Tube Clock

## 2.78
- BUG: Screen3 tick/numbers not working

## 2.77
- Perfected Screen7 and renamed it to "Digital Watch"

## 2.76
- Added Screen 7 & 8

## 2.75
- Refactoring entire screen selection/creation/saving/loading
- Added screen6 = Gradient Clock

## 2.74
- Added c/f option

## 2.73
- Added wifi RSSI in Menu page (Perfect WIFI)

## 2.72
- Solid savepoint

## 2.71
- Added Pirateweather, OpenWeatherMap, WeatherAPI services

## 2.70
- Clamped temp/humidity values to +- 99

## 2.69
- (no description recorded)

## 2.68
- Changed Fetchweather from `http.getString()` to `WiFiClient& stream = http.getStream();`
- Changed Fetchweather back to `http.getString()` but this time filtered to ONLY receive the values needed
- Removed "FetchWeatherTask Stack remaining" from Serial print
- Improved fetchweather rate limits and removed canMakeAPIcall

## 2.67
- Added live image of clock to webpage with simulated grid effect

## 2.66
- Added `dma_canvas.setRotation(2);` into SETUP

## 2.65
- BUG: JSON parsing failed: IncompleteInput, however v2.35 worked flawlessly
- Fixed: reverted to the simpler `http.getString()` method from v2.35, but this brought back the SPIFFS corruption bug from v2.47
- Fixed: fixed SPIFFS corruption by moving the saveSettings call within fetchWeather to AFTER `http.end();`

## 2.64
- Added fuzziness slider to page 3 for analogue clock hands

## 2.63
- Fixed: GPS dot accuracy on 3D map
- Stars now move opposite direction when you change earth's rotation
- Updated website with Speed & Direction ranging from -50 to +50
- Fixed 3D spin during animation

## 2.62
- FIXED: Screen4 and webpage now fixed and working, but during year animation it stops spinning the earth (should continue)

## 2.61
- Website no longer flickers and all controls working again, but the 3D switch doesn't expand the box anymore

## 2.60
- Added 2 sliders: rotation speed, number of stars
- Extra switch to turn sun on/off
- BUG: Website flickers between screen4 and screen1 causing a lot of SPIFFS activity

## 2.59
- Adding working 3D switch to webpage

## 2.57
- Good sun fuzz, stars, speed but GPS dot not in line with rotation speed
- Ability to switch 2D/3D manually using "SpareSwitch"

## 2.56
- Made sun fuzzy and corrected it from suddenly disappearing

## 2.55
- Added Sun to space and star rotation

## 2.54
- Added a fully working 3D rotating earth screen with working day/night terminator

## 2.53
- Added a pageslider for each page
- Updated webpage for screen4 and 5; slider now adjusts the shadow intensity and it is saved to SPIFFS

## 2.52
- Added new animation logic "renderAnimatedValue_Down_Up" and "renderAnimatedValue_Left_Right", implemented into all screens
- Added internal and wifi temp/humidity values

## 2.51
- FIXED: stopped program from constantly calling "getInternalAHT10" after API rate limit exceeded

## 2.50
- BUG: clock will boot to clock screen when saved wifi is not available, but it starts the AP portal, even when reconnected AP portal remains
- FIXED: rewrote STATE machine logic

## 2.49
- Added anti-aliasing to minute/hour hands on Screen3

## 2.48
- Improved red dot functionality and clock now loads from RTC before NTP

## 2.47
- BUG: sometimes SPIFFS gets corrupted if reset button is used (v2.42 not affected)
- FIXED: fixed SPIFFS corruption bug by changing fetchweather to `WiFiClient& stream = http.getStream();` instead of `String payload = http.getString();`
- FIXED: moon percentage during animation up/down arrow now working

## 2.46
- Fixed date 23/5/25 on Screen2 and 5; all working perfectly

## 2.45
- Fixed Screen 6 and 7
- BUG: cannot load webpage — FIXED, was not calling server.begin

## 2.44
- Narrowed down to something wrong in choosescreen()
- SOLVED: 5v regulator only provided 4.1v; replaced board
- Red dot works good now

## 2.43
- Found cause of CAPTIVE portal not working: switch machine state in loop

## 2.42
- Added Arduino OTA
- BUG: WiFi manager no longer loads captive portal

## 2.41
- Fixed large Sunny Icon

## 2.40
- Added red dot to signify WiFi lost; clock now works without WiFi if no credentials

## 2.39
- Added large weather icons

## 2.38
- Fixed date colour and spacing for screen 2 & 5 — no longer shows "02 Mar", now shows "2 Mar"

## 2.37
- Added RTC ability

## 2.36
- Added code for AHT10 temp/humidity sensor

## 2.35
- Fully working Screen5 Moon Phase screen; implementing into year animation so moon percentage works

## 2.34
- Screen5 now has working moon phase, gets data from Weather API, but does not save/load from SPIFFS
- Year animation also works

## 2.33
- Screen5 Moon phase clock, animated terminator line working

## 2.32
- FIXED: SPI crash caused by FetchWeatherTask only having 2048 byte stack size, increased to 8192 bytes
- Added default colours and switch settings, enhanced Screen5 QR code page
- Added "colour" serial command to print the current state of all colours/switches on each page

## 2.31
- Replaced "CheckWifi" function with a state machine
- Added better splash screen and QR code to set up WiFi

## 2.30
- Updated screens 1, 2, 5, 6 by moving Day row above Time row

## 2.29
- Updated website to complete Screen 4

## 2.28
- Fully completed webpage for screen 3; all screen pages now non-global and working independently

## 2.27
- Webpage modifications, moved brightness controls to own card
- In process of making screen controls non-global to allow show/hide/edit of switches and labels

## 2.26
- Added ability to choose image or colour for screen 3; all settings saved to SPIFFS

## 2.25
- Added 11 clock images and 3 masks

## 2.24
- Added numbers/ticks to clock
- Added MSK clock bitmap for personal colour iteration

## 2.23
- Started trying to add numbers/ticks to clock

## 2.22
- Successfully added QR code generation pointing to local IP

## 2.21
- Added year-centred animation button to HTML

## 2.20
- Restored v2.17 (still had working webpage for buttons/colours)
- Added ability to turn on/off time shadow using Month switch/colour

## 2.19
- Forked

## 2.18
- Added year animation button to webpage; unable to center (forked)

## 2.17
- Added 13 images for World clock face

## 2.16
- Skipped 2.15 (poor performing); working perfectly now

## 2.142
- Fixed slow website; everything works on world map

## 2.141
- Added apparent temp

## 2.14
- Screen4 is good, but webpage slow

## 2.13
- Added ability to use a single world map mask to mask map images and/or choose colours
- Changed Serial to 115200
- Added LAND/WATER/ICE switches on HTML with expandable colour/image pickers (website sluggish)

## 2.12
- Added fully custom colourable world map

## 2.11
- Save point

## 2.10
- Added test function to "emulate" a day of the year via serial ("d135" or "YEAR" to animate a year)

## 2.095
- Added 2 pixel transition from night to day
- Working terminator line (adapted from amCharts day/night world map demo)

## 2.085
- Added terminatorOffset (value of 31 works well, hard transition)

## 2.08
- Finally got good mercator projection but wrong location

## 2.07
- Terminator line travels the wrong direction (right instead of left with time)

## 2.06
- Added Day/Night Terminator Map Clock

## 2.05
- Added Screen3: Analogue clock with calendar

## 2.04
- Animated Temp/Humidity to swap when both selected

## 2.03
- Added working animated seconds on Screen2
- Added PM indicator and all colours/switches active
- Enabled min/max temps and added % sign to Tidbyt_Numbers1

## 2.02
- Added working animated seconds on Screen2

## 2.01
- Adding Screen2 and 1 large weather icon

## 2.00
- Added webserver/fetchweather to core 0

## 1.99
- BUG: weather values not saved/read from SPIFFS — FIXED
- BUG: glitchy auto brightness — FIXED

## 1.98
- BUG: colours no longer saving to SPIFFS — FIXED
- BUG: weather API causes slow webpage — FIXED

## 1.97
- Fixed lux values on webpage; auto brightness added; webpage loads fine without weather API

## 1.96
- Fixed lux values on webpage

## 1.95
- Perfected webpage brightness sliders and now show lux, but lux pushes the slider off the page

## 1.94
- Added Screen2
- BUG: button menu no longer works — FIXED, now cycles through screens 1, 2, 3 etc

## 1.93
- Ability to save/load colours/switches for each screen (fully working Screen1)

## 1.92
- Added multiple screens

## 1.91
- Fixed non-loading webpage bug by enabling dual-core mode

## 1.90
- Updated webpage and added reboot/clear wifi to the webpage
- Clock now displays after gettime, no longer waiting for weather
- Fixed bug where weather data could be lost during an API update — values are now saved and read from SPIFFS

## 1.89
- Weather icons now update

## 1.88
- Added ability for day/date/month colours, added switch code, all centering works

## 1.87
- Weather icon switch is working, added brightness slider

## 1.86
- Added weather info from PirateWeather
- All settings now saved/loaded in SPIFFS
- PirateWeather API working and displaying temp/humidity

## 1.85
- Added custom colours, switches, and the ability to set the time zone/daylight savings info from the web server

## 1.84
- Used GROK-3 AI to create a nice web UI for the settings page
- Can now parse chosen colour to the clock

## 1.82
- Code clean-up

## 1.81
- Converted whole program to use CANVAS

## 1.8
- Changed page order; added CANVAS to remove text/images being overwritten

## 1.7
- Added basic settings menu

## 1.6
- Added button on GPIO 18

## 1.5
- Fixed bug when time switching from 12 to 1 would overwrite numbers on display

## 1.4
- Fixed crash bug

## 1.3
- Added NTP

## 1.2
- Added WiFi

## 1.1
- Fixed green and blue being mixed by swapping pins in ESP32-HUB75-MatrixPanel-I2S-DMA.h

## 1.0
- Initial working demo
