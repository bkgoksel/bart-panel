#pragma once

// ---- Panel ----
#define PANEL_WIDTH   32
#define PANEL_HEIGHT  32
#define PANEL_CHAIN   1
#define BRIGHTNESS    60   // 0-255

// HUB75 pins. Leave commented to use the library defaults
// (R1=25 G1=26 B1=27 R2=14 G2=12 B2=13 A=23 B=19 C=5 D=17 E=-1 LAT=4 OE=15 CLK=16).
// #define PIN_R1 25
// #define PIN_G1 26
// #define PIN_B1 27
// #define PIN_R2 14
// #define PIN_G2 12
// #define PIN_B2 13
// #define PIN_A  23
// #define PIN_B  19
// #define PIN_C  5
// #define PIN_D  17
// #define PIN_E  -1
// #define PIN_LAT 4
// #define PIN_OE  15
// #define PIN_CLK 16

// ---- BART ----
#define BART_STATION  "12TH"
#define BART_API_KEY  "MW9S-E7SL-26DU-VV8V"   // BART's public demo key
#define POLL_MS       30000
#define WIFI_TIMEOUT_MS 15000  // per network, before falling back to the next one

// Which destinations count for each lane (BART station abbreviations).
static const char *const SF_DESTS[] = {"SFIA", "MLBR", "DALY"};
static const char *const EC_DESTS[] = {"RICH"};

// ---- Visualization ----
#define PX_PER_MIN    1.0f // track scale; 30px of track at 1px/min = 30 min horizon
#define WALK_MIN      4    // minutes from the office to the platform; trains closer than this are dimmed
#define STALE_MS      120000  // show the error dot if no successful fetch for this long
