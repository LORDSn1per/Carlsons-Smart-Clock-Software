# SmartClock Changelog

Version history for the SmartClock firmware. Entries from 2.90 onward have
real dates; everything before that is migrated verbatim from the
commented-out changelog that used to live at the top of `src/main.cpp`
(going back to the original Arduino IDE sketch), where no dates were ever
recorded — only version numbers and short notes.

> Note: from 2.90 onward the version auto-increments on **every** build, so
> some numbers below are just iteration builds with no shipped change of their
> own. Entries are written against the version that first carried the change.

## 4.35 - 2026-08-20
- Screen 13's rain module gains an "Only Show If Rain Chance Above" switch
  and a 0-100% threshold slider. When on, the module is skipped in the
  screen's rotation unless the peak of the next hour's rain chance (the four
  15-minute buckets when available, otherwise the current hourly point)
  reaches the threshold - so a dry stretch drops the rain panel from the
  rotation instead of holding an unchanging low number every cycle. Screen
  13 only; not offered on Screen 10, which doesn't rotate between modules.

## 4.34 - 2026-08-14
- Rain graphs on Screen 10 and Screen 13 gain a Line/Bar dropdown, sharing one
  `rainGraphStyle` field between the two screens that draw one.
- PirateWeather's `minutely` block (per-minute rain probability, previously
  excluded from every request) is now parsed and bucketed into four 15-minute
  averages for the current hour. When present, both rain graphs plot the
  current hour as those four sub-points, sized within that hour's own column
  width, and the remaining four columns as before. Falls back to the plain
  hourly timeline - pixel-identical to the prior code - whenever the bucket
  data isn't available. **WeatherAPI is not wired up**: its literal 15-minute
  endpoint (`tp=15`) is documented as Enterprise-only and its response shape
  isn't published, so implementing it now would mean guessing at fields
  nobody could verify. OpenWeatherMap's `minutely` has no probability field
  at all, only an intensity in mm/h, so it was never a candidate.
- Fixed two real collisions on Screen 10, both visible in a user-supplied
  screenshot: the peak-% readout sat at a fixed position that a
  high-probability point could be drawn directly through (same colour, same
  pixels) - removed rather than repositioned, since the chart already shows
  where the peak is. The seconds line was drawn on y=16, the same row a
  100%-chance point occupies - moved to y=15.
- Screen 13's rain module gains a Grid switch and colour picker: 0/25/50/75/
  100% horizontal reference lines and a vertical divider between each hour's
  column, drawn behind the graph. Screen 10 does not get this control -
  narrower chart, not requested there.

## 4.26 - 2026-08-13
- **The config portal is no longer starved.** Reading the clock never blocks a
  render again. `getLocalTime(tm*, ms)` is not a "read the clock" call: it is a
  retry loop that polls until the year looks valid, and its default timeout is
  5000 ms. Every render path used that default, and the clock screens call it
  twice, so with the system clock unset a single frame cost **ten seconds**:

      [SlowLoop] total=10033ms wm=1 screen=10024 present=8 other=0 state=0
      [Health]   loop=0/s

  Normally the first poll succeeds and the cost is nil, which is why this went
  unnoticed for years. On a clock whose DS3231 had lost its time, with no WiFi
  to reach NTP, the year stayed at 2000 and both calls ran their timeouts in
  full. Everything reactive moves at the loop rate, so the portal answered DNS
  and HTTP roughly once every ten seconds and a phone's captive-portal probe
  always gave up first — the portal appeared broken when it was merely starved.
  All 22 unbounded call sites now pass a 5 ms cap. The two deliberate waits are
  untouched: the 100 ms poll in `STATE_WIFI_CONNECTING` and the explicit 5000 ms
  NTP sync. Callers already fall back to the RTC, so nothing changes when the
  clock is healthy. Confirmed working by Phillip.

## 4.25 - 2026-08-13
- Added a slow-loop detector: any `loop()` iteration over 150 ms prints a
  breakdown of `myWM.process()`, the screen draw and the DMA present. It named
  the cause of the above on the first capture, and contradicted the expectation
  that WiFiManager was at fault.

## 4.24 - 2026-08-13
- `Screen91()` built the setup QR code on **every** `loop()` iteration —
  Reed-Solomon encoding plus scoring all eight mask patterns over a 29x29 grid,
  the most expensive thing in the loop, on the screen shown for the entire
  duration of the config portal. The QR encodes `AutoChipID`, which never
  changes, so it is now built once into a 106-byte static buffer.
- Added `loop=N/s` to the `[Health]` line. It is the one number that separates
  "the firmware is busy" from "the network is at fault".

## 4.23 - 2026-08-13
- Raised WiFiManager to `WM_DEBUG_VERBOSE` so the portal logs each inbound
  request and reports when it answers a captive-portal probe. Silent in normal
  operation, since the portal only runs before WiFi is configured.

## 4.22 - 2026-08-13
- A brand new or fully erased chip could never store settings. `erase_flash`
  wipes the SPIFFS partition and nothing recreated it, because the mount uses
  `SPIFFS.begin(false)` so a transient failure can never destroy user data.
  Correct for a clock that has been running, wrong for one that has never had a
  filesystem: it came up unable to mount, unable to save anything, and stayed
  that way every boot until someone found Format SSD. NVS distinguishes the two
  — it survives a firmware update but not `erase_flash` — so a missing "created"
  flag means the chip really is fresh and formatting destroys nothing. A chip
  that has mounted before is still left strictly alone.

## 4.21 - 2026-08-13
- Screen 13's sun marker was drawn with only two satellite pixels, at 12 and 6
  o'clock, where Screen 11 draws four. Added the 3 and 9 o'clock pixels so both
  screens draw the same sun.
- Gave three elements their own colours instead of silently borrowing another
  control's: Screen 11's sun and solar times (previously `temp_col` and
  `dateBG_col`) and Screen 13's sun marker (previously `temp_col`). Defaults are
  seeded from the values they used to inherit, so nothing changes appearance
  until a new colour is chosen.

## 4.19 - 2026-08-13
- Fixed Screen 1's seconds and PM indicator. `updateCurrentHoursMins()` formats
  the hour with `%d` — no zero padding — but the parts were carved out of that
  string with fixed substring offsets assuming a two-digit hour. At 4:06:08 PM
  the string is `"4:06:08 PM"`, one character short, so every field slid:
  `hoursMins` kept a trailing colon, seconds became `"8 "` and the meridiem
  `"M"`. It only misbehaved for hours 1-9, which is why it looked intermittent.
  The identical parsing sat in `getTimeDateString()`, so Screens 7 and 9 were
  affected too. Each field is now formatted straight from `struct tm`.
- Safe Mode no longer overwrites saved settings. It called `saveSettings()`
  after forcing auto-brightness off and brightness to 40, writing those into
  `/settings.json` permanently — one trip of the guard and the real brightness
  configuration was gone with no restore path. The dimming is now applied in RAM
  only.
- Safe Mode is latched in `/reboot_guard.json` instead of the guard file being
  deleted on trigger. Deleting it meant the next boot came up at full brightness,
  straight back into the brownout loop it had just caught. The latch is released
  after ten minutes of uptime; the running session stays dim deliberately.
- Hardened the trigger: a DS3231 that has lost power reads back a fixed time,
  making every boot look simultaneous, so three ordinary power-ups would trip
  Safe Mode. Implausible timestamps now disable the guard for that boot, and the
  elapsed comparison is signed so an NTP correction backwards cannot underflow.

## 4.18 - 2026-08-13
- Format SSD actually clears settings now. It formatted and then sat in
  `delay(1000)` with both settings writers still running, so the in-RAM settings
  were written straight back onto the freshly formatted filesystem before the
  reboot. A single latch honoured by `persistSettingsNow()` now blocks every
  writer for the window, and the on-clock menu no longer calls the web handler
  (which replies on `server`, owned by a task on the other core).

## 4.16 - 2026-08-13
- Restored the two 3.91 fixes that 3.92-4.00 had undone, after Phillip
  identified v3.91 as the last build with responsive WiFi and no animation lag.
  A true rollback was impossible: versions 3.71-4.01 were built from uncommitted
  working trees and exist only as binaries.
- Screen 13 back to a flat 30 FPS. The conditional that dropped to 5 FPS
  whenever no transition was due meant the seconds bar, which is always moving,
  visibly animated only while some other animation happened to be running.
- Web session recovery, which had been deleted outright. `noteWebRequest()` had
  been reduced to feeding the 30-minute reboot watchdog; its nine call sites
  still fired but nothing consumed them, so a browser that stopped getting
  answers waited thirty minutes for a reboot instead of forty-five seconds for a
  WiFi recycle.

## 4.08 - 2026-08-13
- Reverted the OE dimming math to the hybrid-floor method that was in use right
  after the low-brightness flicker fix (161470c). The 4.00 rewrite quantised OE
  to whole HUB75 clock periods and compensated the remainder in RGB, intending
  to remove a small hand-off artefact at the 15% floor. It made dimming visibly
  steppy across the entire range instead: OE held flat for four brightness units
  then jumped four at once while RGB sawtoothed 246-254 to compensate. OE is
  duty cycle and RGB scaling is bitplane depth, so the compensation does not
  cancel perceptually. It also cut OE from 216 distinct values to 52 while
  producing the same 216 output pairs overall, so it bought no resolution for
  the stepping it introduced. `applyDisplayBrightness()` carries a comment
  recording this so it is not "improved" back.
- Auto-brightness ramp tuned by feel to a fixed one-unit step on a 25 ms tick:
  about 40 units/s, or 6.4 s for a full sweep. Three times the speed of the
  original 75 ms tick and without the 19 s crawl that started this, while
  keeping one ramp step equal to exactly one OE step - the smoothest motion the
  panel can produce now that OE is continuous again. The proportional ramp
  added in 4.03 is still in the code and re-enabled by raising
  `BRIGHTNESS_RAMP_MAX_STEP` above 1; larger steps read as visibly coarser.

## 4.03 - 2026-08-13
- Browser OTA no longer refuses the update when it cannot checkpoint settings.
  `/update/start` copies `/settings.json` to `/settings.json.ota` before opening
  the OTA partition, and a failed copy aborted the whole transfer at 0 bytes
  ("settings could not be safely checkpointed"). The SPIFFS partition is 128 KB
  and cannot hold four simultaneous copies of a settings file this size, and the
  validator also wants a 20 KB *contiguous* allocation, so the check failed
  exactly when the device was most constrained. The reasoning behind refusing
  was wrong regardless: an OTA writes the app partition only, never SPIFFS, so
  the checkpoint is redundancy against the *new* firmware mishandling settings -
  not protection for the transfer. Refusing left no way to flash the firmware
  that would fix the problem. It is now advisory: stale `.ota`/`.tmp` copies are
  reclaimed first, the checkpoint is still made when possible, and the update
  always proceeds.
- That failure now logs SPIFFS used/total and the largest free heap block
  instead of a bare message, and `/debug` reports `spiffs_used`/`spiffs_total`.
- Auto-brightness reacts at a sane speed again. The ramp stepped one brightness
  unit per 75 ms regardless of how far it had to travel - about 13 units per
  second, so a lamp being switched on (roughly 180 units) took over 13 seconds
  and a full sweep took 19. It now moves a fraction of the remaining distance
  each tick (`clamp(remaining / 6, 1, 8)` every 25 ms), which is quick while the
  gap is wide and eases in as it closes: the same swings now settle in about a
  second. The median filter, EMA and 350 ms dwell from the low-brightness
  flicker work are untouched - the ramp was the entire problem.

## 4.00 - 2026-08-13
- Fixed the render task reading weather icon names out of `String` globals that
  `fetchWeatherTask` reassigns from another task. Reassigning a `String` frees
  its buffer, so a renderer preempted mid-comparison could dereference freed
  memory. Screen 13 was the heaviest reader (up to eight icon lookups per frame,
  doubled during a module transition), which matches the crash being specific to
  it. Icon names are now fixed char buffers that are overwritten in place.
- The same render path was parsing `currentTemp`/`currentApparentTemp`/
  `currentHumidity` with `.toFloat()` every frame - the same race, on the same
  writer. It now reads the float mirrors those Strings are derived from.
- Removed the ~28 KB contiguous heap `String` that every settings write built.
  It was the largest allocation the firmware ever made, it happened on each
  toggle, and it had to succeed alongside the display buffers, WiFi and TLS.
  Settings now hash and stream straight to SPIFFS through a 512-byte buffer.
- Screen 13's two transition canvases are allocated at startup instead of the
  first time the screen is opened, so they no longer carve 5.4 KB out of a
  running, fragmented heap.
- Clamped the forecast weekday before it indexes Screen 13's 7-entry day-name
  table; an out-of-range value there was a wild pointer, not a wrong label.
- Fixed `Screen90()` reading ~267 bytes past the end of all thirteen 6-entry
  `web_*_col[]` palettes (it indexes with `currentScreen - 1`, and runs with
  `currentScreen == 90`).
- Fixed a compile error (`resetReason` undeclared) that left the tree unbuildable.

### WiFi
- STATE_RUNNING no longer tears down the web server, mDNS and OTA on a single
  not-connected sample. `WiFi.status()` dips out of `WL_CONNECTED` during an
  ordinary roam between Orbi satellites; reacting instantly meant every blip
  cost a full service rebuild, so the clock was dropping itself off the LAN and
  that looked like the network dropping out. A loss must now persist 5 seconds.
- Added a WiFi event log (`WiFi.onEvent`) recording the last 12
  `ARDUINO_EVENT_WIFI_*` events with 802.11 disconnect reason codes, served by
  `/debug`. This is the diagnostic `HANDOFF.md` has been asking for: it is
  observational only and makes no network calls.
- Added a crash breadcrumb in `RTC_NOINIT` RAM - screen, state, free heap,
  largest contiguous block, and whether a settings write was in progress -
  captured every loop and reported on the next boot via serial and `/debug`.

## 3.95 - 2026-08-12
- Reclaimed the 16 KB browser-OTA buffer whenever no update is active instead
  of permanently taking that internal RAM away from WiFi and weather TLS.
- Removed Screen 13's per-frame temporary text allocations and reduced its
  unchanged-frame rendering from 30 FPS to 5 FPS. Its value slides and module
  transitions still automatically render at 30 FPS.
- Added reset reason, retained reset sequence, OTA state and both network-task
  stack margins to `/debug` so any further reboot can be distinguished from a
  watchdog, panic, brownout or deliberate software restart without USB.

## 3.94 - 2026-08-12
- Removed the 45-second browser-heartbeat watchdog that could deliberately
  disconnect WiFi while background polling was paused for a firmware upload.
- Rebuilt browser OTA as a preparation step plus fully buffered 16 KB chunks.
  Chunk offsets are now acknowledged explicitly, so losing an `OK` response
  and retrying cannot write the same firmware data twice and corrupt the image.
- OTA now drains existing preview traffic, pauses weather/TLS work, protects a
  validated settings checkpoint before opening the update partition, and
  abandons an interrupted session safely after two minutes.

## 3.93 - 2026-08-12
- Replaced Screen 13's upper-right min/max temperatures with independently
  switched and coloured day/date fields matching the Sunpath presentation.
- Replaced Screen 13's temperature unit letters with its compact two-pixel
  degree mark, including both values during the internal/external animation.
- Shifted the Infographic Wind Dial compass and arrow two pixels right.

## 3.92 - 2026-08-12
- Kept the Infographic clock centred until its measured bounds would overlap
  the widest enabled internal/external reading, then shifts it just far enough
  right to preserve both values. Added a working PM-indicator switch and moved
  the seconds progress line up one pixel.
- Made the five-hour rain graph use exact fractional point positions from its
  available left edge through pixel 63. Disabling its weather icon now expands
  the graph safely to the full panel width while edge hour labels remain visible.
- Moved the Wind Dial weather icon to the far left and down one pixel, with the
  compass and arrow centred in the remaining space.
- Added a deliberate one-pixel gap on both sides of the forecast min/max slash;
  two-digit values such as `88 / 88` still fit in each forecast column.

## 3.91 - 2026-08-12
- Restored the HUB75 shift clock from 20 MHz to the library's 10 MHz default.
  The faster clock left insufficient timing margin on the upper-half B1 lane,
  causing blue from one pixel to appear one position to its left on rows 0-15.
- Limited Screen 13 rendering to 30 FPS. Animated transitions no longer encode
  and flip hundreds of complete HUB75 frames per second while competing with
  the WiFi stack for CPU and internal-memory bandwidth.
- Fixed the web recovery watchdog treating a working live preview as a dead
  connection. Root, status, screenshot and OTA-chunk requests now refresh the
  browser heartbeat instead of only the much larger settings response.
- Removed the repeated 15-second full-settings download. The browser uses its
  lightweight status request for transport recovery and fetches settings only
  when their values may actually have changed.

## 3.69 - 2026-08-11
- Replaced the hard 11% low-brightness lock with median plus exponential LDR
  filtering, a real output deadband and a short dwell, eliminating brightness
  hunting from noisy ESP32 ADC readings and brief shadows.
- Backported the current HUB75 driver's proportional 0-255 OE algorithm instead
  of collapsing the web setting into only 64 row-width values.
- Added hybrid low-light dimming: below about 15%, OE remains at a steady duty
  while post-CIE RGB bitplanes provide the finer brightness range down to zero.
- Enabled atomic DMA double buffering and an approximately 287 Hz panel refresh;
  unchanged canvases are no longer recopied continuously into the scan buffer.
- Added raw/filtered LDR, target/OE/RGB brightness and actual refresh diagnostics
  to `/debug`, `/status` and the once-per-minute serial health line.

## 3.66 - 2026-08-11
- Weather API-key fields are visible text again so pasted keys can be checked
  directly in the settings page, as requested.

## 3.65 - 2026-08-11
- Removed the destructive boot path that automatically formatted SPIFFS after
  any mount failure. The clock now retries a clean mount four times and leaves
  the partition untouched if it is still unavailable; formatting is possible
  only through the explicit Format SSD control.
- The webpage reboot now locks settings writers, refuses to reboot if pending
  settings cannot be saved, cleanly unmounts SPIFFS and only then restarts.
- Settings writes now retain the previous file as a backup. Boot validates and
  recovers the newest complete temporary snapshot, the main file, or the backup
  instead of silently falling back after an interrupted/corrupt write.
- Failed deferred saves remain pending for retry rather than being incorrectly
  marked as successfully persisted.

## 3.64 - 2026-08-11
- Fixed Pirate Weather returning HTTP 200 but appearing as an empty response on
  the ESP32. The clock no longer allocates one large String for the complete
  forecast; it requests only the blocks it uses and filters JSON directly from
  the network stream, avoiding contiguous-heap allocation failure.
- Separated the real HTTP status from the internal retry result so diagnostics
  can accurately report cases such as HTTP 200 with invalid/empty content.
- Masked stored weather API keys in the settings page by default.

## 3.63 - 2026-08-11
- Added a live diagnostics panel to Weather Settings showing the provider and
  each real fetch stage: queued, validation, connection, download, parsing,
  success, error or disabled. It also reports HTTP status, downloaded bytes,
  API-attempt usage, last-success age and an actionable error explanation.
- Added a "Refresh weather now" button and temporary one-second monitoring
  while a fetch is active; normal LCD preview responses carry the same weather
  diagnostics so the feature adds no continuous polling load.
- Replaced the one-bit weather-update trigger with a generation counter so a
  provider/API-key/location change made during another request cannot be lost.
  Switching from None back to a provider now always queues a fresh download.

## 3.61 - 2026-08-11
- Fixed live Wi-Fi strength and current auto-brightness staying blank/stale when
  fast LCD preview polling repeatedly won the ESP32's single HTTP request slot.
  Live values now arrive immediately from `/status`, ride along with every LCD
  preview response, and use a priority fallback only when those updates stop.
- Weather provider, API-key and coordinate changes now take priority over
  scheduled updates and retries, are checked four times per second, and cannot
  be lost or overwritten by a request for the previous provider. A stalled
  weather API now times out after 6 seconds instead of 15.

## 3.60 - 2026-08-11
- Added live Wi-Fi quality to the settings page using the RSSI already supplied
  by `/status`: four signal bars, Excellent/Good/Fair/Weak classification, dBm
  and an estimated percentage. The same status appears in the desktop sidebar.
- Restored the live LCD preview as a large, full-width 3D clock above the
  settings. Once it scrolls out of view it smoothly becomes a compact floating
  preview at the upper-right, leaving the screen visible while editing controls.
- Added separate dock sizing for wide Mac, narrow desktop/tablet and phone
  layouts, with the mobile dock kept clear of the bottom navigation.
- Reused the existing status poll rather than adding network traffic, and gave
  that lightweight request extra time to complete on clocks with weak Wi-Fi.

## 3.59 - 2026-08-11
- Added an active-page heartbeat watchdog for the ESP32/Orbi failure mode where
  `WL_CONNECTED` remains true and outbound traffic works but inbound LAN packets
  are black-holed. After three missed 15-second settings heartbeats, the clock
  recycles Wi-Fi and rebuilds its network services.
- Recovery is single-shot until a browser request is received again, preventing
  a closed or sleeping browser tab from causing repeated reconnects.

## 3.58 - 2026-08-11
- Fixed the HTTP server remaining permanently closed after a Wi-Fi disconnect.
  Web handling now pauses while offline and the listening socket, Arduino OTA,
  mDNS and NetBIOS services are rebuilt when the station reconnects.
- Network reconnection no longer creates duplicate web/weather FreeRTOS tasks;
  persistent task handles ensure their stack allocations happen only once.

## 3.57 - 2026-08-11
- Fixed the page becoming permanently unresponsive when a background settings
  request was slow or interrupted. Network polling no longer holds the global
  control-loading guard while waiting for the ESP32.
- Added one browser-side transport coordinator for settings, status, preview
  and control requests. User changes cancel lower-priority polling, responses
  are fully received before the next request starts, and stale settings can no
  longer overwrite a newer control change.
- Added automatic recovery after a stranded request and after Wi-Fi/browser
  reconnection, while preserving the polling lock used during firmware upload.
- Routed the timezone and all weather API-key controls through the same bounded
  request queue instead of allowing them to collide with live-preview traffic.

## 3.56 - 2026-08-11
- Fixed screen 3's Clock Face and Hour Hand colour controls sharing the same
  `time_col` value. The face now has its own persisted `clock_face_col`, API
  field and endpoint, while `time_col` controls only the hour hand.
- Existing settings migrate safely by copying the old shared colour into the
  new face colour the first time this firmware loads them.

## 3.55 - 2026-08-11
- Fixed live-preview grid drift caused by separately scaling the 64x32 canvas
  and a percentage-based CSS overlay. The renderer now produces one 320x160
  bitmap containing exact 4x4 colour blocks separated by 1-pixel black gaps,
  so the grid and LED pixels remain locked together at every display size.

## 3.54 - 2026-08-11
- Restored the black 64x32 LED pixel grid in the live preview using a lightweight
  overlay, retaining the single-operation `ImageData` renderer.
- The LCD itself now has square corners so edge pixels are never clipped.
- Presented the preview inside a responsive 3D clock enclosure with a recessed
  bezel, dimensional case, feet, branding and speaker detail.

## 3.53 - 2026-08-11
- Web controls now acknowledge immediately and defer/coalesce SPIFFS settings
  persistence until one second after the final change.
- Browser mutations are serialized, time-bounded, and superseded by the newest
  queued value for the same control, preventing request pile-ups.
- `/settings` is cached by configuration revision and screen, with ETag/304
  support; frequently changing values moved to the lightweight `/status` route.
- The main page and live preview now support conditional HTTP caching.
- Live-preview drawing uses one 64x32 `ImageData` update instead of 2,048 canvas
  draw calls, and unchanged frame revisions skip repainting.
- The editable page CSS was consolidated, and build-time HTML/CSS minification
  reduces the delivered gzip payload to about 22 KB.
- GPS changes now queue weather fetching on the background task rather than
  blocking the web request with HTTPS.
- The chip-derived AP/hostname suffix is three digits: for example,
  `Clock-3000` is now `Clock-300` across WiFiManager, DHCP, mDNS, NBNS and OTA.
- Evaluated ESPAsyncWebServer 3.12.0 but retained the existing server: converting
  the captive portal and custom chunked OTA path would add a second networking
  architecture after the blocking work had already been removed.

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
