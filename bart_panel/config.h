#pragma once
// Build-time settings. Layout, station, lanes, walk time and brightness live in the
// render template instead (web/default_template.json, editable from the web portal).

// ---- Panel ----
#define PANEL_WIDTH   32
#define PANEL_HEIGHT  32
#define PANEL_CHAIN   1

// HUB75 pins for Dave Elfving's LED_Art_Panel PCB
// (github.com/DCElfving/LED_Art_Panel). B1/B2 are non-standard on this board.
// Comment these out to fall back to the library's default pin map.
#define PIN_R1 18
#define PIN_G1 21
#define PIN_B1 4
#define PIN_R2 32
#define PIN_G2 19
#define PIN_B2 33
#define PIN_A  25
#define PIN_B  5
#define PIN_C  26
#define PIN_D  17
#define PIN_E  -1
#define PIN_LAT 16
#define PIN_OE  14
#define PIN_CLK 27

// ---- Network ----
#define HOSTNAME        "bart-panel"  // portal at http://bart-panel.local/
#define WIFI_TIMEOUT_MS 15000         // per network, before falling back to the next one

// ---- BART ----
#define BART_API_KEY  "MW9S-E7SL-26DU-VV8V"   // BART's public demo key
#define POLL_MS       30000
#define STALE_MS      120000  // "stale" elements light up after this long without a fetch
