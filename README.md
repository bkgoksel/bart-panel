# bart-panel

A 32x32 LED panel on an ESP32 that shows upcoming BART departures from 12th St Oakland.

```n |EC      13   |    EC (right edge): trains move up into the station at the top.
 |  .    #     |    SF (left edge): trains move down into the station at the bottom.
 |  .   ###    ||   The office building sits in the middle; each counter is next to
 ||    #####   ||   its own station, with its lane label beside it.
 ||  17      SF|
```

- Left lane: SF-bound trains (destinations SFIA, MLBR, DALY). Right lane: El Cerrito-bound (RICH).
- Each train is a block in its line color (Red, Orange, Yellow). Length follows car count.
- 1 px = 1 minute, so each track shows the next 30 minutes. Ticks every 5 minutes.
- The first `walkMin` minutes of track are dim red. Trains in that zone are dimmed and don't count
  toward the number, since you can't walk there in time.
- The number turns orange when the next catchable train is close to the walk-time cutoff.
- A train blinks at the station when it's boarding or leaving.
- A red pixel in the top-left corner means the last successful API fetch was over 2 minutes ago.

## Web portal

Open http://bart-panel.local/ (or the panel's IP, printed on the serial console at boot) from the
same network.

- **Render template.** Everything drawn on the panel, plus the station, lane destinations, walk time
  and brightness, comes from a JSON template. Edit it in the portal, check the live preview (it uses
  the panel's current train data), and press **Push to panel** (or Ctrl+Enter). The panel validates
  it, saves it to flash and applies it immediately. The format is documented under "Template
  reference" on the page; the default is [web/default_template.json](web/default_template.json).
- **Firmware.** Upload a compiled `.bin` from the portal, or from the command line (below).

`PORTAL_PASS` in `secrets.h` protects template pushes, firmware uploads and network uploads
(user `admin`). It's empty by default, which means anyone on the network can change the panel.

## Hardware

- ESP32-D0WD-V3 (4 MB flash) with a CP2102 USB-serial bridge, on COM6 on this machine.
- 32x32 HUB75 RGB panel on Dave Elfving's LED_Art_Panel PCB (https://github.com/DCElfving/LED_Art_Panel).
  That board uses a non-standard pin map, set in `bart_panel/config.h`.

## Building

Toolchain: Arduino CLI with the `esp32:esp32` core (3.3.12) and these libraries:
ArduinoJson, Adafruit GFX Library, ESP32 HUB75 LED MATRIX PANEL DMA Display.

1. Copy `bart_panel/secrets.h.example` to `bart_panel/secrets.h` and list the Wi-Fi networks to try,
   in order of preference. If none connect within 15 s each, it keeps cycling through them.
2. After editing anything in `web/`, regenerate the embedded copy: `tools/embed_web.ps1`.
3. Build, then upload over Wi-Fi (or USB):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --output-dir build bart_panel
```

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs -p bart-panel.local -l network --upload-field password= --input-dir build bart_panel
```

```bash
arduino-cli upload --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs -p COM6 --upload-property upload.speed=115200 --input-dir build bart_panel
```

The `min_spiffs` partition scheme gives two 1.9 MB app slots (needed for OTA) and a small LittleFS
partition for the saved template. The USB link on this board drops out at high baud rates, hence
`upload.speed=115200`.

## Original firmware

`tools/backup_flash.ps1` dumped the factory firmware to `firmware-backup/original_flash_4MB.bin`
(gitignored). To restore it over USB:

```bash
esptool --port COM6 --baud 115200 write-flash 0 firmware-backup/original_flash_4MB.bin
```

esptool is bundled with the core at `%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\`.

## Code layout

- `bart_panel/bart_panel.ino`: Wi-Fi, BART fetching (core 0), main loop
- `bart_panel/template.h`: template format and parser
- `bart_panel/render.h`: draws a template; mirrored in JS in `web/portal.html` for the preview
- `bart_panel/portal.h`: web server, template storage, firmware upload, Arduino OTA
- `bart_panel/portal_html.h`: generated from `web/` by `tools/embed_web.ps1`

## Data

BART legacy API, `etd.aspx?cmd=etd&orig=12TH`, polled every 30 s with BART's public key. Minutes are
interpolated between polls so trains move smoothly.
