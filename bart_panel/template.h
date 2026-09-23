#pragma once
// Render templates: a JSON description of what to draw, editable from the web portal.
// See web/default_template.json and the reference in the portal page.

#include <ArduinoJson.h>

#define MAX_LANES    4
#define MAX_DESTS    8
#define MAX_ELEMENTS 32
#define MAX_TEXT     16
#define MAX_SPRITES  8
#define MAX_PALETTE  8
#define SPRITE_POOL  2048  // pixels across all sprites

enum ElemType : uint8_t { EL_TRACK, EL_COUNTER, EL_TEXT, EL_RECT, EL_STALE, EL_SPRITE };
enum Dir : uint8_t { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };
enum Align : uint8_t { AL_LEFT, AL_RIGHT, AL_CENTER };

// Pixel art: rows of characters mapped to colors by a palette. Space and '.' are transparent.
struct SpriteDef {
  uint16_t offset;    // into Template::spritePixels
  uint8_t w, h;
  uint8_t palCount;
  char keys[MAX_PALETTE];
  uint32_t colors[MAX_PALETTE];
};

struct Element {
  ElemType type;
  int8_t lane;        // index into Template::lanes, -1 if unused
  int8_t sprite;      // index into Template::sprites
  int16_t x, y, w, h;
  Dir dir;            // track: direction trains travel (toward the station)
  int8_t tick;        // track: offset of the 5-minute ticks from the track line, 0 = none
  float f;            // track: px per minute; counter: "hurry" margin in minutes
  Align align;
  uint8_t sx, sy;     // text/counter scale
  uint32_t c[4];      // colors, 0xRRGGBB; meaning depends on type
  char text[MAX_TEXT];
};

struct LaneDef {
  char id[8];
  char dests[MAX_DESTS][6];
  uint8_t destCount;
};

struct Template {
  char station[6];
  float walkMin;
  uint8_t brightness;
  LaneDef lanes[MAX_LANES];
  uint8_t laneCount;
  Element elements[MAX_ELEMENTS];
  uint8_t elementCount;
  SpriteDef sprites[MAX_SPRITES];
  uint8_t spriteCount;
  uint8_t spritePixels[SPRITE_POOL];  // 0 = transparent, else palette index + 1
  uint16_t spritePixelsUsed;
};

static uint32_t parseColor(JsonVariantConst v, uint32_t def) {
  const char *s = v.as<const char *>();
  if (!s) return def;
  if (*s == '#') s++;
  if (strlen(s) != 6) return def;
  return strtoul(s, nullptr, 16);
}

static int laneIndex(const Template &t, const char *id) {
  for (int i = 0; i < t.laneCount; i++)
    if (!strcmp(t.lanes[i].id, id)) return i;
  return -1;
}

// Parses json into t. Returns "" on success, otherwise an error message.
static String parseTemplate(const char *json, Template &t) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return String("invalid JSON: ") + err.c_str();

  memset(&t, 0, sizeof(t));
  strlcpy(t.station, doc["station"] | "12TH", sizeof(t.station));
  t.walkMin = doc["walkMin"] | 4.0f;
  t.brightness = doc["brightness"] | 60;

  for (JsonObjectConst l : doc["lanes"].as<JsonArrayConst>()) {
    if (t.laneCount >= MAX_LANES) return "too many lanes (max " + String(MAX_LANES) + ")";
    LaneDef &d = t.lanes[t.laneCount++];
    strlcpy(d.id, l["id"] | "", sizeof(d.id));
    if (!d.id[0]) return "lane without an id";
    for (const char *dest : l["dests"].as<JsonArrayConst>()) {
      if (dest && d.destCount < MAX_DESTS) strlcpy(d.dests[d.destCount++], dest, sizeof(d.dests[0]));
    }
  }

  for (JsonObjectConst e : doc["elements"].as<JsonArrayConst>()) {
    if (t.elementCount >= MAX_ELEMENTS) return "too many elements (max " + String(MAX_ELEMENTS) + ")";
    Element &el = t.elements[t.elementCount];
    const char *type = e["type"] | "";
    el.x = e["x"] | 0;
    el.y = e["y"] | 0;
    el.w = e["w"] | 1;
    el.h = e["h"] | 1;
    el.sx = e["sx"] | 1;
    el.sy = e["sy"] | el.sx;
    const char *al = e["align"] | "left";
    el.align = !strcmp(al, "right") ? AL_RIGHT : !strcmp(al, "center") ? AL_CENTER : AL_LEFT;
    el.lane = -1;
    if (e["lane"].is<const char *>()) {
      el.lane = laneIndex(t, e["lane"]);
      if (el.lane < 0) return String("unknown lane \"") + (const char *)e["lane"] + "\"";
    }
    String where = String(" (element ") + t.elementCount + ")";

    if (!strcmp(type, "track")) {
      el.type = EL_TRACK;
      if (el.lane < 0) return "track needs a lane" + where;
      const char *d = e["dir"] | "down";
      if (!strcmp(d, "down")) el.dir = DIR_DOWN;
      else if (!strcmp(d, "up")) el.dir = DIR_UP;
      else if (!strcmp(d, "left")) el.dir = DIR_LEFT;
      else if (!strcmp(d, "right")) el.dir = DIR_RIGHT;
      else return "dir must be down, up, left or right" + where;
      el.tick = e["tick"] | 0;
      el.f = e["pxPerMin"] | 1.0f;
      if (el.f <= 0) return "pxPerMin must be positive" + where;
      el.w = e["width"] | 1;
      if (el.w < 1 || el.w > 5) return "width must be 1-5" + where;
      el.c[0] = parseColor(e["color"], 0x404040);
      el.c[1] = parseColor(e["walkColor"], 0x800000);
      el.c[2] = parseColor(e["stationColor"], 0xC8C8C8);
      el.c[3] = parseColor(e["tickColor"], 0x606060);
    } else if (!strcmp(type, "counter")) {
      el.type = EL_COUNTER;
      if (el.lane < 0) return "counter needs a lane" + where;
      el.f = e["hurry"] | 3.0f;
      el.c[0] = parseColor(e["color"], 0x00C83C);
      el.c[1] = parseColor(e["hurryColor"], 0xFF7800);
      el.c[2] = parseColor(e["noneColor"], 0x3C3C3C);
    } else if (!strcmp(type, "text")) {
      el.type = EL_TEXT;
      strlcpy(el.text, e["text"] | "", sizeof(el.text));
      el.c[0] = parseColor(e["color"], 0x787878);
    } else if (!strcmp(type, "rect")) {
      el.type = EL_RECT;
      el.c[0] = parseColor(e["color"], 0xFFFFFF);
    } else if (!strcmp(type, "stale")) {
      el.type = EL_STALE;
      el.c[0] = parseColor(e["color"], 0xFF0000);
    } else if (!strcmp(type, "sprite")) {
      el.type = EL_SPRITE;
      if (t.spriteCount >= MAX_SPRITES) return "too many sprites (max " + String(MAX_SPRITES) + ")";
      SpriteDef &s = t.sprites[t.spriteCount];
      for (JsonPairConst kv : e["palette"].as<JsonObjectConst>()) {
        const char *k = kv.key().c_str();
        if (strlen(k) != 1) return "palette keys must be single characters" + where;
        if (s.palCount >= MAX_PALETTE) return "palette too big (max " + String(MAX_PALETTE) + ")" + where;
        s.keys[s.palCount] = k[0];
        s.colors[s.palCount++] = parseColor(kv.value(), 0xFFFFFF);
      }
      JsonArrayConst rows = e["rows"];
      s.h = rows.size();
      s.w = 0;
      for (const char *row : rows) s.w = max<size_t>(s.w, row ? strlen(row) : 0);
      if (s.h == 0 || s.h > 32 || s.w > 32) return "sprite needs 1-32 rows of up to 32 characters" + where;
      if (t.spritePixelsUsed + s.w * s.h > SPRITE_POOL) return "sprites too large in total" + where;
      s.offset = t.spritePixelsUsed;
      for (int r = 0; r < s.h; r++) {
        const char *row = rows[r] | "";
        for (int c = 0; c < s.w; c++) {
          char ch = c < (int)strlen(row) ? row[c] : ' ';
          uint8_t v = 0;
          if (ch != ' ' && ch != '.') {
            for (int i = 0; i < s.palCount; i++)
              if (s.keys[i] == ch) v = i + 1;
            if (!v) return String("character '") + ch + "' not in palette" + where;
          }
          t.spritePixels[s.offset + r * s.w + c] = v;
        }
      }
      t.spritePixelsUsed += s.w * s.h;
      el.sprite = t.spriteCount++;
    } else {
      return String("unknown element type \"") + type + "\"" + where;
    }
    t.elementCount++;
  }
  return "";
}
