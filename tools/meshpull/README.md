# meshpull

Fetches the public [meshmap.net](https://meshmap.net/) node snapshot, trims it to
your area, and writes the compact CSV that the lora-suite **Mesh** app reads from
SD (`/mesh/import.csv`). The live `/nodes.json` feed is ~3.7 MB — far too big to
parse on the ESP32 — so the filtering happens here on the host.

These are **foreign Meshtastic** nodes: lora-suite can't talk to them (different
framing/addressing/crypto). The import is read-only situational awareness.

## Usage

```bash
# nodes within 30 km of a point, on the EU_868 mesh, at most 96:
go run . -lat 40.18 -lon 44.51 -radius 30 -topic msh/EU_868 -max 96 -out import.csv

# or just by MQTT region topic, no distance filter:
go run . -topic msh/EU_868 -max 96 -out import.csv
```

Then copy `import.csv` to the SD card as `/mesh/import.csv`. In the Mesh app press
`r` to reload.

| Flag | Default | Meaning |
|---|---|---|
| `-lat` / `-lon` | (unset) | Center for the distance filter/sort (decimal degrees) |
| `-radius` | `0` | Keep nodes within this many km of the center (0 = no cutoff) |
| `-topic` | `""` | Keep only nodes seen under this MQTT topic prefix, e.g. `msh/EU_868` |
| `-max` | `96` | Max nodes to write — must be ≤ device `MeshOverlay::CAP` (96) |
| `-out` | `import.csv` | Output path |
| `-url` | meshmap.net/nodes.json | Source feed |
| `-timeout` | `20s` | HTTP timeout |

With a center, output is sorted nearest-first (so `-max` keeps the closest). Nodes
without a position are always dropped.

**Please poll no more than once a minute** — the feed caches for 60 s. The tool is
one-shot; schedule it with cron/systemd if you want the SD file kept fresh.

## Output format

Shared with the device parser (`src/proto/meshoverlay.h`); both are unit-tested.

```
# generated <unix>
id,lat,lon,batt,role,short,long
```

Six comma-separated fixed fields, then the long name as the rest of the line (may
contain commas): id (uint32), lat/lon (decimal degrees), batt (0–255; >100 =
externally powered), role (a `MeshRole` code), short (≤4 chars), long. `#` lines
are comments.

## Test

```bash
go test ./...
```
