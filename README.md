# bart-panel

A 32x32 LED panel on an ESP32 that shows upcoming BART departures from 12th St Oakland.

```
 SF   |   .. 13      SF (left): trains move down into the station at the bottom.
      .   |          EC (right): trains move up into the station at the top.
      |   | .        Counters sit next to each lane's station.
      |   |
 7   ===  |   EC
```

- Left lane: SF-bound trains (destinations SFIA, MLBR, DALY). Right lane: El Cerrito-bound (RICH).
- Each train is a block in its line color (Red, Orange, Yellow). Length follows car count.
- 1 px = 1 minute, so each track shows the next 30 minutes. Ticks every 5 minutes.
- The first `WALK_MIN` minutes of track are dim red. Trains in that zone are dimmed and don't count
  toward the number, since you can't walk there in time.
- The number turns orange when the next catchable train is close to the walk-time cutoff.
- A train blinks at the station when it's boarding or leaving.
- Open `tools/preview.html` in a browser for a live-data simulation of the panel.
- A red pixel in the top-right corner means the last successful API fetch was over 2 minutes ago.

## Hardware

- ESP32-D0WD-V3 (4 MB flash) with a CP2102 USB-serial bridge, on COM6 on this machine.
- 32x32 HUB75 RGB panel on Dave Elfving's LED_Art_Panel PCB (https://github.com/DCElfving/LED_Art_Panel).
  That board uses a non-standard pin map, set in `bart_panel/config.h`.

## Setup

Toolchain: Arduino CLI with the `esp32:esp32` core (3.3.12) and these libraries:
ArduinoJson, Adafruit GFX Library, ESP32 HUB75 LED MATRIX PANEL DMA Display.

1. Copy `bart_panel/secrets.h.example` to `bart_panel/secrets.h` and list the Wi-Fi networks to try, in order of preference. If none connect within 15 s each, it keeps cycling through them.
2. Adjust `bart_panel/config.h` (walk time, brightness, pins).
3. Build and flash:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build bart_panel
arduino-cli upload --fqbn esp32:esp32:esp32 -p COM6 --input-dir build bart_panel
arduino-cli monitor -p COM6 -c baudrate=115200
```

## Original firmware

`tools/backup_flash.ps1` dumped the factory firmware to `firmware-backup/original_flash_4MB.bin`
(gitignored). To restore it:

```bash
esptool --port COM6 --baud 115200 write-flash 0 firmware-backup/original_flash_4MB.bin
```

esptool is bundled with the core at `%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\5.3.1\`.

## Data

BART legacy API, `etd.aspx?cmd=etd&orig=12TH`, polled every 30 s with BART's public key. Minutes are
interpolated between polls so trains move smoothly.
