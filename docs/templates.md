# Render templates

Everything the panel draws comes from a render template: a JSON document that lists the station,
which trains go in which lane, and the elements to draw. Templates are edited and pushed from the
web portal and take effect immediately, without reflashing.

The default template is [`web/default_template.json`](../web/default_template.json).

## Workflow

1. Open the portal at http://bart-panel.local/ (or the panel's IP).
2. The editor loads the template the panel is running. The preview next to it redraws as you type,
   using the panel's live train data. The time-warp slider speeds up time so you can watch trains
   move in.
3. Press **Push to panel** (or Ctrl+Enter). The panel validates the template, saves it to flash and
   switches to it. If validation fails, the panel keeps the old template and the portal shows the
   error.

**Revert to panel's** reloads what the panel is running. **Load default** loads the built-in
default into the editor (it isn't applied until you push it).

At boot, the panel loads the saved template. If there is none, or it no longer parses (for example
after a firmware change), it falls back to the built-in default.

Until the first successful BART fetch, the panel shows `WIFI` (not connected) or `LOAD` (connected,
waiting for data) instead of the template.

## Coordinates and drawing

- The panel is 32x32. `x` runs left to right (0-31), `y` top to bottom (0-31).
- Elements are drawn in the order they're listed, so later elements draw on top of earlier ones.
- Anything outside the panel is clipped.
- Colors are `"#rrggbb"` strings. An invalid or missing color falls back to the element's default.

## Top level

| Field        | Default  | Meaning |
|--------------|----------|---------|
| `station`    | `"12TH"` | BART station abbreviation to fetch departures for |
| `walkMin`    | `4`      | Minutes it takes to walk to the platform. Trains closer than this are dimmed and skipped by counters |
| `brightness` | `60`     | Panel brightness, 0-255 |
| `lanes`      | `[]`     | Up to 4 lanes (below) |
| `elements`   | `[]`     | Up to 32 elements (below) |

## Lanes

A lane is a named group of trains, selected by destination:

```json
{ "id": "SF", "dests": ["SFIA", "MLBR", "DALY"] }
```

- `id`: up to 7 characters, referenced by elements' `lane` field.
- `dests`: up to 8 BART destination abbreviations. A train goes into the first lane whose `dests`
  contains its destination. Trains that match no lane are ignored.
- Each lane keeps its 6 soonest trains.

The destinations at 12th St are listed in the live API response, e.g.
`https://api.bart.gov/api/etd.aspx?cmd=etd&orig=12TH&key=MW9S-E7SL-26DU-VV8V&json=y`
(`abbreviation` field). Northbound Richmond trains are `RICH`; SF-bound trains are `SFIA`, `MLBR`
and `DALY`; `ANTC`, `PITT` and `BERY` also stop here.

## Elements

Every element has a `type`. Fields not listed for a type are ignored.

### `track`

A track that trains travel along toward a station bar at one end.

| Field          | Default     | Meaning |
|----------------|-------------|---------|
| `lane`         | (required)  | Lane id |
| `dir`          | `"down"`    | Direction trains move: `down`, `up`, `left` or `right`. The station is at the end they move toward (bottom, top, left or right edge) |
| `x`            | `0`         | Column of the track, for `down`/`up` |
| `y`            | `0`         | Row of the track, for `left`/`right` |
| `pxPerMin`     | `1`         | Scale: pixels per minute of arrival time |
| `tick`         | `0`         | Offset (across the track) of the 5-minute tick marks. `0` = no ticks. Positive is right of a vertical track or below a horizontal one |
| `width`        | `1`         | Width of the track line, 1-5 px (trains are always 3 px wide) |
| `color`        | `#404040`   | Track line |
| `walkColor`    | `#800000`   | Track line within `walkMin` of the station ("too late" zone) |
| `stationColor` | `#c8c8c8`   | Station bar |
| `tickColor`    | `#606060`   | Tick marks |

Geometry, measured along the track from the station end:

- Position 0 is the station bar, 5 px wide across the track.
- Position 1 is a gap; the track line starts at position 2 and runs to the panel edge.
- A train arriving in `m` minutes starts at position `2 + m * pxPerMin`.
- Trains are 3 px wide across the track. Their length follows car count: 8+ cars is 4 px, 6-7 is
  3 px, fewer is 2 px.
- Trains are drawn in their BART line color, dimmed to 30% inside the walk zone.
- Positions are fractional, so the pixels at a train's ends are drawn at partial brightness. With
  `pxPerMin: 1` a train drifts smoothly one pixel per minute.
- A train under half a minute out (boarding or leaving) blinks.

### `counter`

The minutes until the next train you can still catch: the first one in the lane that is at least
`walkMin` minutes out. Shows `--` when there is none (or it's 100+ minutes away).

| Field        | Default    | Meaning |
|--------------|------------|---------|
| `lane`       | (required) | Lane id |
| `x`, `y`     | `0`, `0`   | Position (see `align`) |
| `align`      | `"left"`   | `left`: `x` is the left edge. `right`: `x` is the right edge. `center`: `x` is the middle |
| `sx`, `sy`   | `1`, `sx`  | Horizontal and vertical scale of the font |
| `hurry`      | `3`        | The counter uses `hurryColor` when the train is less than `walkMin + hurry` minutes out |
| `color`      | `#00c83c`  | Normal color |
| `hurryColor` | `#ff7800`  | Color when you need to leave soon |
| `noneColor`  | `#3c3c3c`  | Color of `--` |

### `text`

| Field      | Default   | Meaning |
|------------|-----------|---------|
| `text`     | `""`      | Up to 15 characters |
| `x`, `y`   | `0`, `0`  | Position (see `align`) |
| `align`    | `"left"`  | `left`, `right` or `center`, as for `counter` |
| `sx`, `sy` | `1`, `sx` | Font scale. `sx: 2, sy: 1` gives bold-looking wide text |
| `color`    | `#787878` | |

The font is 3x5 pixels with 1 px between characters, so a string is `4 * length - 1` pixels wide
at scale 1 (times `sx`). It has `A-Z` (lowercase is drawn as uppercase), `0-9`, `-`, `:` and `.`.
Anything else, including space, is drawn as a blank.

### `sprite`

Pixel art drawn from rows of characters.

```json
{
  "type": "sprite", "x": 8, "y": 11,
  "palette": { "#": "#3c4652", "w": "#c89a50" },
  "rows": [
    "   #   ",
    "  ###  ",
    " #w#w# ",
    " ##### "
  ]
}
```

| Field     | Default    | Meaning |
|-----------|------------|---------|
| `x`, `y`  | `0`, `0`   | Top-left corner |
| `rows`    | (required) | 1-32 strings of up to 32 characters. Shorter rows are padded with transparency |
| `palette` | `{}`       | Up to 8 single-character keys mapped to colors |

Space and `.` are transparent. Any other character must be in the palette.

### `rect`

| Field     | Default   | Meaning |
|-----------|-----------|---------|
| `x`, `y`  | `0`, `0`  | Top-left corner |
| `w`, `h`  | `1`, `1`  | Size |
| `color`   | `#ffffff` | |

### `stale`

A single pixel that lights up when the last successful BART fetch is more than 2 minutes old.

| Field    | Default   |
|----------|-----------|
| `x`, `y` | `0`, `0`  |
| `color`  | `#ff0000` |

## Limits

| What                          | Limit |
|-------------------------------|-------|
| Lanes                         | 4 |
| Destinations per lane         | 8 |
| Trains kept per lane          | 6 |
| Elements                      | 32 |
| Text length                   | 15 characters |
| Sprites                       | 8 |
| Palette entries per sprite    | 8 |
| Sprite pixels, all sprites    | 2048 (width x height, summed) |

Pushing a template over a limit is rejected with a message saying which one.

## Example: a horizontal lane

A single lane of SF trains running right to left along row 20, with the counter centered above:

```json
{
  "station": "12TH",
  "walkMin": 4,
  "lanes": [{ "id": "SF", "dests": ["SFIA", "MLBR", "DALY"] }],
  "elements": [
    { "type": "track", "lane": "SF", "dir": "left", "y": 20, "tick": 3 },
    { "type": "counter", "lane": "SF", "x": 16, "y": 4, "align": "center", "sx": 2, "sy": 2 },
    { "type": "stale", "x": 31, "y": 0 }
  ]
}
```

## HTTP API

The portal uses these endpoints, which can also be called directly (e.g. with curl). If
`PORTAL_PASS` is set in `secrets.h`, the POST endpoints need HTTP basic auth as user `admin`.

| Endpoint                 | Meaning |
|--------------------------|---------|
| `GET /template`          | The template the panel is running |
| `POST /template`         | Validate, save and apply a template (JSON body). 200 on success, 400 with an error message otherwise |
| `GET /template/default`  | The built-in default |
| `GET /data`              | Current trains per lane: `{ok, age, lanes: [{id, trains: [{m, cars, rgb}]}]}`, where `m` is minutes at fetch time and `age` is milliseconds since that fetch |
| `POST /update`           | Firmware upload (multipart form, one `.bin` file). The panel restarts after a successful upload |

```bash
curl -X POST -H "Content-Type: application/json" --data-binary @web/default_template.json http://bart-panel.local/template
```

## Changing the template system

The template format is implemented twice: in the firmware (`bart_panel/template.h` parses,
`bart_panel/render.h` draws) and in JavaScript in `web/portal.html` for the preview. A change to
one needs the same change in the other, or the preview stops matching the panel.

`web/` is embedded into the firmware, so after editing `web/portal.html` or
`web/default_template.json`, run `tools/embed_web.ps1`, rebuild, and upload the firmware.
