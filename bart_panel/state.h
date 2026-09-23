#pragma once
// Shared state between the fetch task (core 0), the render loop and the web portal (core 1).

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "template.h"

#define MAX_TRAINS 6

struct Train {
  float minutes;  // at fetch time
  uint8_t cars;
  uint32_t rgb;   // 0xRRGGBB line color
};

struct Lane {
  Train trains[MAX_TRAINS];
  uint8_t count;
};

// What the fetch task needs from the template: station and lane destinations.
struct FetchConfig {
  char station[6];
  LaneDef lanes[MAX_LANES];
  uint8_t laneCount;
};

static MatrixPanel_I2S_DMA *display;

// Only touched from core 1 (render loop and web handlers).
static Template tpl;
static String tplJson;

// Guarded by dataLock.
static portMUX_TYPE dataLock = portMUX_INITIALIZER_UNLOCKED;
static FetchConfig fetchCfg;
static Lane lanes[MAX_LANES];  // indexed like tpl.lanes
static uint32_t fetchedAt = 0;  // millis() of the last successful fetch
static bool haveData = false;

static TaskHandle_t fetchTaskHandle = nullptr;
