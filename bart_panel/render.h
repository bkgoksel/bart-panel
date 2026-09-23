#pragma once
// Draws the current template. Keep in sync with the JS renderer in web/portal.html.

#include "state.h"

#define TRACK_START 2  // along-track pixels: 0 = station bar, 1 = gap

// 3x5 glyphs, one byte per row, low 3 bits used (bit 2 = leftmost column).
static const uint8_t *glyph(char ch) {
  static const uint8_t digits[10][5] = {
      {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
      {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
  static const uint8_t letters[26][5] = {
      {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {7, 4, 4, 4, 7}, {6, 5, 5, 5, 6}, {7, 4, 6, 4, 7},
      {7, 4, 6, 4, 4}, {7, 4, 5, 5, 7}, {5, 5, 7, 5, 5}, {7, 2, 2, 2, 7}, {1, 1, 1, 5, 7},
      {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7}, {5, 7, 7, 5, 5}, {6, 5, 5, 5, 5}, {7, 5, 5, 5, 7},
      {7, 5, 7, 4, 4}, {7, 5, 5, 7, 1}, {7, 5, 6, 5, 5}, {7, 4, 7, 1, 7}, {7, 2, 2, 2, 2},
      {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5}, {5, 5, 2, 2, 2},
      {7, 1, 2, 4, 7}};
  static const uint8_t dash[5] = {0, 0, 7, 0, 0}, colon[5] = {0, 2, 0, 2, 0},
                       dot[5] = {0, 0, 0, 0, 2};
  if (ch >= '0' && ch <= '9') return digits[ch - '0'];
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
  switch (ch) {
    case '-': return dash;
    case ':': return colon;
    case '.': return dot;
  }
  return nullptr;  // space and anything unknown
}

static uint16_t rgb565(uint32_t rgb, float k = 1.0f) {
  return display->color565(((rgb >> 16) & 0xFF) * k, ((rgb >> 8) & 0xFF) * k, (rgb & 0xFF) * k);
}

static int textWidth(const char *s, int sx) {
  return (4 * (int)strlen(s) - 1) * sx;
}

// x is the left edge, right edge or center depending on align.
static void drawText(int x, int y, const char *s, uint32_t rgb, int sx, int sy, Align align) {
  int w = textWidth(s, sx);
  if (align == AL_RIGHT) x = x - w + 1;
  else if (align == AL_CENTER) x = x - w / 2;
  uint16_t color = rgb565(rgb);
  for (; *s; s++, x += 4 * sx) {
    const uint8_t *g = glyph(*s);
    if (!g) continue;
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 3; c++)
        if (g[r] & (4 >> c)) display->fillRect(x + c * sx, y + r * sy, sx, sy, color);
  }
}

// Track coordinates: a = distance from the station along the track, off = offset across it.
static void trackPixel(const Element &e, int a, int off, uint16_t color) {
  int x, y;
  switch (e.dir) {
    case DIR_DOWN: x = e.x + off; y = PANEL_HEIGHT - 1 - a; break;
    case DIR_UP: x = e.x + off; y = a; break;
    case DIR_LEFT: x = a; y = e.y + off; break;
    default: x = PANEL_WIDTH - 1 - a; y = e.y + off; break;
  }
  if (x >= 0 && y >= 0 && x < PANEL_WIDTH && y < PANEL_HEIGHT) display->drawPixel(x, y, color);
}

static void drawTrack(const Element &e, const Lane &lane, float elapsedMin, uint32_t now) {
  const int len = (e.dir == DIR_DOWN || e.dir == DIR_UP) ? PANEL_HEIGHT : PANEL_WIDTH;
  const int walkPx = (int)(tpl.walkMin * e.f);

  // Station bar, "too late" zone, track, 5-minute ticks
  for (int off = -2; off <= 2; off++) trackPixel(e, 0, off, rgb565(e.c[2]));
  for (int a = TRACK_START; a < len; a++)
    trackPixel(e, a, 0, rgb565(a < TRACK_START + walkPx ? e.c[1] : e.c[0]));
  if (e.tick) {
    for (int m = 5; TRACK_START + m * e.f < len; m += 5)
      trackPixel(e, TRACK_START + (int)(m * e.f), e.tick, rgb565(e.c[3]));
  }

  // Trains, furthest first so nearer ones draw on top. Edge pixels get partial
  // brightness so a slow train drifts instead of jumping.
  for (int i = lane.count - 1; i >= 0; i--) {
    const Train &t = lane.trains[i];
    float m = t.minutes - elapsedMin;
    if (m < 0) m = 0;
    if (m < 0.5f && (now / 300) % 2) continue;  // blink while boarding/leaving
    float a0 = TRACK_START + m * e.f;
    float tl = t.cars >= 8 ? 4 : t.cars >= 6 ? 3 : 2;
    float k = m < tpl.walkMin ? 0.3f : 1.0f;
    for (int a = (int)a0; a <= (int)(a0 + tl) && a < len; a++) {
      float cover = min((float)a + 1, a0 + tl) - max((float)a, a0);
      if (cover <= 0.05f) continue;
      uint16_t c = rgb565(t.rgb, k * cover);
      for (int off = -1; off <= 1; off++) trackPixel(e, a, off, c);
    }
  }
}

// Minutes to the next train you can still make, or "--".
static void drawCounter(const Element &e, const Lane &lane, float elapsedMin) {
  float next = -1;
  for (int i = 0; i < lane.count; i++) {
    float m = lane.trains[i].minutes - elapsedMin;
    if (m >= tpl.walkMin) { next = m; break; }
  }
  char buf[4];
  uint32_t col;
  if (next < 0 || next >= 100) {
    strcpy(buf, "--");
    col = e.c[2];
  } else {
    snprintf(buf, sizeof(buf), "%d", (int)next);
    col = next < tpl.walkMin + e.f ? e.c[1] : e.c[0];
  }
  drawText(e.x, e.y, buf, col, e.sx, e.sy, e.align);
}

static void render() {
  Lane snap[MAX_LANES];
  uint32_t at;
  bool ok;
  portENTER_CRITICAL(&dataLock);
  memcpy(snap, lanes, sizeof(snap));
  at = fetchedAt;
  ok = haveData;
  portEXIT_CRITICAL(&dataLock);

  uint32_t now = millis();
  float elapsed = (now - at) / 60000.0f;
  display->clearScreen();
  if (!ok) {
    drawText(PANEL_WIDTH / 2, 13, WiFi.status() == WL_CONNECTED ? "LOAD" : "WIFI", 0x505050, 1, 1,
             AL_CENTER);
  } else {
    for (int i = 0; i < tpl.elementCount; i++) {
      const Element &e = tpl.elements[i];
      switch (e.type) {
        case EL_TRACK: drawTrack(e, snap[e.lane], elapsed, now); break;
        case EL_COUNTER: drawCounter(e, snap[e.lane], elapsed); break;
        case EL_TEXT: drawText(e.x, e.y, e.text, e.c[0], e.sx, e.sy, e.align); break;
        case EL_RECT: display->fillRect(e.x, e.y, e.w, e.h, rgb565(e.c[0])); break;
        case EL_STALE:
          if (now - at > STALE_MS) display->drawPixel(e.x, e.y, rgb565(e.c[0]));
          break;
      }
    }
  }
  display->flipDMABuffer();
}
