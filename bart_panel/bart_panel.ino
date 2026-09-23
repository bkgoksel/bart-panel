// BART departures visualizer for 12th St Oakland.
//
// What gets drawn is a JSON render template (template.h, render.h) that can be edited
// and pushed from the web portal (portal.h) without reflashing. The default is two
// vertical lanes: SF-bound on the left moving down, El Cerrito-bound on the right moving
// up. Trains are blocks in their line color, placed by minutes-to-departure and sized by
// car count, and they slide toward the station between API polls.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"
#include "secrets.h"
#include "state.h"
#include "render.h"
#include "portal.h"

static int laneFor(const FetchConfig &cfg, const char *abbr) {
  for (int i = 0; i < cfg.laneCount; i++)
    for (int j = 0; j < cfg.lanes[i].destCount; j++)
      if (!strcmp(cfg.lanes[i].dests[j], abbr)) return i;
  return -1;
}

static bool fetchDepartures() {
  FetchConfig cfg;
  portENTER_CRITICAL(&dataLock);
  cfg = fetchCfg;
  portEXIT_CRITICAL(&dataLock);

  WiFiClientSecure client;
  client.setInsecure();  // public, read-only data; skip cert pinning
  HTTPClient http;
  String url = String("https://api.bart.gov/api/etd.aspx?cmd=etd&json=y&orig=") + cfg.station +
               "&key=" + BART_API_KEY;
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

  Lane fresh[MAX_LANES] = {};
  for (JsonObject etd : doc["root"]["station"][0]["etd"].as<JsonArray>()) {
    int lane = laneFor(cfg, etd["abbreviation"] | "");
    if (lane < 0) continue;
    for (JsonObject est : etd["estimate"].as<JsonArray>()) {
      Lane &l = fresh[lane];
      if (l.count >= MAX_TRAINS) break;
      const char *m = est["minutes"] | "0";  // number or "Leaving"
      Train &t = l.trains[l.count++];
      t.minutes = isdigit((unsigned char)m[0]) ? atoi(m) : 0;
      t.cars = atoi(est["length"] | "0");
      t.rgb = parseColor(est["hexcolor"], 0xFFFFFF);
    }
  }
  for (Lane &l : fresh) {
    std::sort(l.trains, l.trains + l.count,
              [](const Train &a, const Train &b) { return a.minutes < b.minutes; });
  }

  portENTER_CRITICAL(&dataLock);
  bool stale = strcmp(cfg.station, fetchCfg.station) || cfg.laneCount != fetchCfg.laneCount ||
               memcmp(cfg.lanes, fetchCfg.lanes, sizeof(cfg.lanes));
  if (!stale) {  // template didn't change mid-fetch
    memcpy(lanes, fresh, sizeof(lanes));
    fetchedAt = millis();
    haveData = true;
  }
  portEXIT_CRITICAL(&dataLock);

  for (int i = 0; i < cfg.laneCount; i++) Serial.printf("%s: %d  ", cfg.lanes[i].id, fresh[i].count);
  Serial.println();
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

// Runs on core 0 so Wi-Fi and HTTP/TLS never stall the animation. A task
// notification (sent when the template changes) triggers an immediate refetch.
static void fetchTask(void *) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !connectWifi()) {
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    fetchDepartures();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_MS));
  }
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
  display->clearScreen();

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  loadTemplate();

  WiFi.mode(WIFI_STA);  // also brings up the network stack, which the web server needs
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(false);  // connectWifi() handles fallback between networks
  setupPortal();

  xTaskCreatePinnedToCore(fetchTask, "fetch", 12288, nullptr, 1, &fetchTaskHandle, 0);
}

void loop() {
  static bool netStarted = false;
  if (!netStarted && WiFi.status() == WL_CONNECTED) {
    startNetworkServices();
    netStarted = true;
  }
  if (netStarted) ArduinoOTA.handle();
  server.handleClient();

  static uint32_t lastFrame = 0;
  if (millis() - lastFrame >= 33) {
    lastFrame = millis();
    render();
  }
  delay(2);
}
