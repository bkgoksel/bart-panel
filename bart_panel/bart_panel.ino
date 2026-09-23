// BART departures visualizer for 12th St Oakland.
//
// Two vertical lanes: SF-bound on the left moving down, El Cerrito-bound (Richmond trains)
// on the right moving up, each ending at a station bar. Trains are
// blocks in their line color, placed by minutes-to-departure and sized by car count,
// and they slide toward the station between API polls.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"
#include "secrets.h"

#define MAX_TRAINS 6
#define LANE_SF 0
#define LANE_EC 1
#define TRACK_START 2  // along-track pixels: 0 = station bar, 1 = gap

struct Train {
  float minutes;   // at fetch time
  uint8_t cars;
  uint32_t rgb;  // 0xRRGGBB line color
};

struct Lane {
  Train trains[MAX_TRAINS];
  uint8_t count;
};

struct LaneGeom {
  int trackX;          // center column of the track
  int tickX;           // column for 5-minute ticks
  bool stationBottom;  // true: station at the bottom, trains move down
  bool textRight;      // right-align counter and label
  int textEdge;        // left edge (or right edge if textRight) of the text column
};

static MatrixPanel_I2S_DMA *display;
static Lane lanes[2];
static uint32_t fetchedAt = 0;   // millis() of the last successful fetch
static bool haveData = false;
static portMUX_TYPE dataLock = portMUX_INITIALIZER_UNLOCKED;

static int laneFor(const char *abbr) {
  for (auto d : SF_DESTS) if (!strcmp(d, abbr)) return LANE_SF;
  for (auto d : EC_DESTS) if (!strcmp(d, abbr)) return LANE_EC;
  return -1;
}

static uint32_t parseHex(const char *hex) {
  if (*hex == '#') hex++;
  return strtoul(hex, nullptr, 16);
}

static bool fetchDepartures() {
  WiFiClientSecure client;
  client.setInsecure();  // public, read-only data; skip cert pinning
  HTTPClient http;
  String url = String("https://api.bart.gov/api/etd.aspx?cmd=etd&json=y&orig=") +
               BART_STATION + "&key=" + BART_API_KEY;
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("BART HTTP %d\n", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument filter;
  JsonObject f = filter["root"]["station"][0]["etd"][0].to<JsonObject>();
  f["abbreviation"] = true;
  JsonObject fe = f["estimate"][0].to<JsonObject>();
  fe["minutes"] = true;
  fe["length"] = true;
  fe["hexcolor"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }

  Lane fresh[2] = {};
  for (JsonObject etd : doc["root"]["station"][0]["etd"].as<JsonArray>()) {
    int lane = laneFor(etd["abbreviation"] | "");
    if (lane < 0) continue;
    for (JsonObject est : etd["estimate"].as<JsonArray>()) {
      Lane &l = fresh[lane];
      if (l.count >= MAX_TRAINS) break;
      const char *m = est["minutes"] | "0";  // number or "Leaving"
      Train &t = l.trains[l.count++];
      t.minutes = isdigit((unsigned char)m[0]) ? atoi(m) : 0;
      t.cars = atoi(est["length"] | "0");
      t.rgb = parseHex(est["hexcolor"] | "#ffffff");
    }
  }
  for (Lane &l : fresh) {
    std::sort(l.trains, l.trains + l.count,
              [](const Train &a, const Train &b) { return a.minutes < b.minutes; });
  }

  portENTER_CRITICAL(&dataLock);
  memcpy(lanes, fresh, sizeof(lanes));
  fetchedAt = millis();
  haveData = true;
  portEXIT_CRITICAL(&dataLock);

  Serial.printf("SF: %d trains, EC: %d trains\n", fresh[LANE_SF].count, fresh[LANE_EC].count);
  return true;
}

// Tries each network in secrets.h in order, giving each WIFI_TIMEOUT_MS.
static bool connectWifi() {
  for (auto &net : WIFI_NETWORKS) {
    Serial.printf("Wi-Fi: trying %s\n", net[0]);
    WiFi.disconnect();
    WiFi.begin(net[0], net[1]);
    uint32_t start = millis();
    while (millis() - start < WIFI_TIMEOUT_MS) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Wi-Fi: connected to %s, %s\n", net[0], WiFi.localIP().toString().c_str());
        return true;
      }
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
  Serial.println("Wi-Fi: no network available");
  return false;
}

// Runs on core 0 so Wi-Fi and HTTP/TLS never stall the animation.
static void fetchTask(void *) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    fetchDepartures();
    vTaskDelay(pdMS_TO_TICKS(POLL_MS));
  }
}

// 3x5 glyphs, one byte per row, low 3 bits used (bit 2 = leftmost column).
static const uint8_t *glyph(char ch) {
  static const uint8_t digits[10][5] = {
      {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
      {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
  static const uint8_t S[5] = {7, 4, 7, 1, 7}, F[5] = {7, 4, 6, 4, 4}, E[5] = {7, 4, 6, 4, 7},
                       C[5] = {7, 4, 4, 4, 7}, dash[5] = {0, 0, 7, 0, 0};
  if (ch >= '0' && ch <= '9') return digits[ch - '0'];
  switch (ch) {
    case 'S': return S;
    case 'F': return F;
    case 'E': return E;
    case 'C': return C;
    case '-': return dash;
  }
  return nullptr;
}

// Draws text in the 3x5 font, each font pixel `scale` LEDs square.
static void drawTiny(int x, int y, const char *s, uint16_t color, int scale = 1) {
  for (; *s; s++, x += 4 * scale) {
    const uint8_t *g = glyph(*s);
    if (!g) continue;
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 3; c++)
        if (g[r] & (4 >> c)) display->fillRect(x + c * scale, y + r * scale, scale, scale, color);
  }
}

static int tinyWidth(const char *s, int scale) {
  return (4 * (int)strlen(s) - 1) * scale;
}

static uint16_t scaled(uint32_t rgb, float k) {
  return display->color565(((rgb >> 16) & 0xFF) * k, ((rgb >> 8) & 0xFF) * k, (rgb & 0xFF) * k);
}

// Vertical lanes. "along" is the distance from the station in pixels (0 = station bar).
// SF runs down the left edge into a station at the bottom; EC runs up the right edge
// into a station at the top. The 2x counters sit in the middle next to their station,
// with the lane label just inside them.
static const LaneGeom SF_GEOM = {2, 5, true, false, 7};
static const LaneGeom EC_GEOM = {29, 26, false, true, 24};

static int alongToY(int a, const LaneGeom &g) {
  return g.stationBottom ? PANEL_HEIGHT - 1 - a : a;
}

static void drawLane(const Lane &lane, float elapsedMin, const LaneGeom &g, const char *label,
                     uint32_t now) {
  const int walkPx = (int)(WALK_MIN * PX_PER_MIN);

  // Station bar, "too late" zone, track, 5-minute ticks
  display->drawFastHLine(g.trackX - 2, alongToY(0, g), 5, display->color565(200, 200, 200));
  for (int a = TRACK_START; a < PANEL_HEIGHT; a++) {
    display->drawPixel(g.trackX, alongToY(a, g),
                       a < TRACK_START + walkPx ? display->color565(40, 0, 0)
                                                : display->color565(20, 20, 20));
  }
  for (int m = 5; TRACK_START + m * PX_PER_MIN < PANEL_HEIGHT; m += 5) {
    display->drawPixel(g.tickX, alongToY(TRACK_START + (int)(m * PX_PER_MIN), g),
                       display->color565(35, 35, 35));
  }

  // Trains, furthest first so nearer ones draw on top. Edge rows get partial
  // brightness so a train moving 1px/min drifts instead of jumping.
  for (int i = lane.count - 1; i >= 0; i--) {
    const Train &t = lane.trains[i];
    float m = t.minutes - elapsedMin;
    if (m < 0) m = 0;
    if (m < 0.5f && (now / 300) % 2) continue;  // blink while boarding/leaving
    float a0 = TRACK_START + m * PX_PER_MIN;
    float len = t.cars >= 8 ? 4 : t.cars >= 6 ? 3 : 2;
    float k = m < WALK_MIN ? 0.3f : 1.0f;
    for (int a = (int)a0; a <= (int)(a0 + len) && a < PANEL_HEIGHT; a++) {
      float cover = min((float)a + 1, a0 + len) - max((float)a, a0);
      if (cover <= 0.05f) continue;
      display->drawFastHLine(g.trackX - 1, alongToY(a, g), 3, scaled(t.rgb, k * cover));
    }
  }

  // Minutes to the next train you can still make
  float next = -1;
  for (int i = 0; i < lane.count; i++) {
    float m = lane.trains[i].minutes - elapsedMin;
    if (m >= WALK_MIN) { next = m; break; }
  }
  char buf[4];
  uint16_t col;
  if (next < 0 || next >= 100) {
    strcpy(buf, "--");
    col = display->color565(60, 60, 60);
  } else {
    snprintf(buf, sizeof(buf), "%d", (int)next);
    col = next < WALK_MIN + 3 ? display->color565(255, 120, 0) : display->color565(0, 200, 60);
  }
  const int numY = g.stationBottom ? PANEL_HEIGHT - 11 : 1;  // 10 rows tall at 2x
  const int labelY = g.stationBottom ? numY - 7 : numY + 12;
  auto textX = [&](const char *s, int sc) {
    return g.textRight ? g.textEdge - tinyWidth(s, sc) + 1 : g.textEdge;
  };
  drawTiny(textX(buf, 2), numY, buf, col, 2);
  drawTiny(textX(label, 1), labelY, label, display->color565(120, 120, 120));
}

static void render() {
  Lane snap[2];
  uint32_t at;
  bool ok;
  portENTER_CRITICAL(&dataLock);
  memcpy(snap, lanes, sizeof(snap));
  at = fetchedAt;
  ok = haveData;
  portEXIT_CRITICAL(&dataLock);

  uint32_t now = millis();
  display->clearScreen();
  if (!ok) {
    display->setTextColor(display->color565(80, 80, 80));
    display->setCursor(0, 12);
    display->print(WiFi.status() == WL_CONNECTED ? "load" : "wifi");
  } else {
    float elapsed = (now - at) / 60000.0f;
    drawLane(snap[LANE_SF], elapsed, SF_GEOM, "SF", now);
    drawLane(snap[LANE_EC], elapsed, EC_GEOM, "EC", now);
    if (now - at > STALE_MS) {
      display->drawPixel(PANEL_WIDTH / 2, PANEL_HEIGHT / 2, display->color565(255, 0, 0));
    }
  }
  display->flipDMABuffer();
}

void setup() {
  Serial.begin(115200);

  HUB75_I2S_CFG cfg(PANEL_WIDTH, PANEL_HEIGHT, PANEL_CHAIN);
#ifdef PIN_R1
  cfg.gpio = {PIN_R1, PIN_G1, PIN_B1, PIN_R2, PIN_G2, PIN_B2, PIN_A, PIN_B, PIN_C, PIN_D, PIN_E,
              PIN_LAT, PIN_OE, PIN_CLK};
#endif
  cfg.double_buff = true;
  display = new MatrixPanel_I2S_DMA(cfg);
  display->begin();
  display->setBrightness8(BRIGHTNESS);
  display->setTextWrap(false);
  display->clearScreen();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // connectWifi() handles fallback between networks

  xTaskCreatePinnedToCore(fetchTask, "fetch", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
  render();
  delay(33);
}
