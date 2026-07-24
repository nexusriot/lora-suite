// meshpull fetches the public meshmap.net node snapshot, trims it to a local
// area, and writes the compact CSV that the lora-suite "Mesh" app reads from
// SD (/mesh/import.csv). The ~3.7 MB feed can't be parsed on the ESP32, so the
// filtering happens here.
//
//	meshpull -lat 40.18 -lon 44.51 -radius 25 -out import.csv
//	meshpull -topic msh/EU_868 -max 96 -out import.csv
//
// Then copy import.csv to the SD card's /mesh/ directory.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"net/http"
	"os"
	"sort"
	"strconv"
	"strings"
	"time"
)

const defaultURL = "https://meshmap.net/nodes.json"

// Compact role codes shared with src/proto/meshoverlay.h (keep in sync).
const (
	roleUnknown    = 0
	roleClient     = 1
	roleClientMute = 2
	roleRouter     = 3
	roleRepeater   = 4
	roleTracker    = 5
	roleSensor     = 6
	roleBase       = 7
	roleOther      = 255
)

// meshNode is the subset of a meshmap.net node record we use.
type meshNode struct {
	LongName     string           `json:"longName"`
	ShortName    string           `json:"shortName"`
	Role         string           `json:"role"`
	HwModel      string           `json:"hwModel"`
	Latitude     int64            `json:"latitude"`  // degrees * 1e7
	Longitude    int64            `json:"longitude"` // degrees * 1e7
	BatteryLevel int              `json:"batteryLevel"`
	Voltage      float64          `json:"voltage"`
	SeenBy       map[string]int64 `json:"seenBy"`
}

// outNode is a filtered, converted node ready to serialize.
type outNode struct {
	id     uint32
	lat    float64
	lon    float64
	batt   int
	volt   int // centivolts
	role   uint8
	seen   int64 // freshest seenBy unix time
	hw     string
	short  string
	long   string
	distKm float64
}

func roleCode(s string) uint8 {
	switch strings.ToUpper(strings.TrimSpace(s)) {
	case "CLIENT", "CLIENT_BASE":
		return roleClient
	case "CLIENT_MUTE", "CLIENT_HIDDEN":
		return roleClientMute
	case "ROUTER", "ROUTER_CLIENT", "ROUTER_LATE":
		return roleRouter
	case "REPEATER":
		return roleRepeater
	case "TRACKER", "TAK_TRACKER":
		return roleTracker
	case "SENSOR":
		return roleSensor
	case "":
		return roleUnknown
	default:
		return roleOther
	}
}

// sanitize keeps printable ASCII, trims, clamps to maxRunes, and (unless the
// field may contain commas, i.e. the long name) strips commas.
func sanitize(s string, maxRunes int, allowComma bool) string {
	var b strings.Builder
	for _, r := range s {
		if r < 0x20 || r > 0x7E {
			continue // drop control chars, emoji, non-ASCII the device can't render
		}
		if r == ',' && !allowComma {
			continue
		}
		b.WriteRune(r)
	}
	out := strings.TrimSpace(b.String())
	if len(out) > maxRunes {
		out = strings.TrimSpace(out[:maxRunes])
	}
	return out
}

func haversineKm(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371.0
	rad := math.Pi / 180.0
	dLat := (lat2 - lat1) * rad
	dLon := (lon2 - lon1) * rad
	a := math.Sin(dLat/2)*math.Sin(dLat/2) +
		math.Cos(lat1*rad)*math.Cos(lat2*rad)*math.Sin(dLon/2)*math.Sin(dLon/2)
	return R * 2 * math.Atan2(math.Sqrt(a), math.Sqrt(1-a))
}

func clampBatt(b int) int {
	if b < 0 {
		return 0
	}
	if b > 255 {
		return 255
	}
	return b
}

func clampVolt(v int) int {
	if v < 0 {
		return 0
	}
	if v > 65535 {
		return 65535
	}
	return v
}

// freshestSeen returns the newest seenBy timestamp, the best "last heard via MQTT".
func freshestSeen(n meshNode) int64 {
	var m int64
	for _, t := range n.SeenBy {
		if t > m {
			m = t
		}
	}
	return m
}

func formatRow(n outNode) string {
	return fmt.Sprintf("%d,%.7f,%.7f,%d,%d,%d,%d,%s,%s,%s",
		n.id, n.lat, n.lon, clampBatt(n.batt), clampVolt(n.volt), n.role, n.seen, n.hw, n.short, n.long)
}

type selectOpts struct {
	hasCenter bool
	lat, lon  float64
	radiusKm  float64 // 0 = no distance cutoff
	topic     string  // "" = no topic filter
	max       int
}

func seenByTopic(n meshNode, prefix string) bool {
	for topic := range n.SeenBy {
		if strings.HasPrefix(topic, prefix) {
			return true
		}
	}
	return false
}

// selectNodes filters, converts, sorts (nearest-first when a center is given,
// else by id) and caps the raw feed to at most opts.max output rows.
func selectNodes(nodes map[uint32]meshNode, opts selectOpts) []outNode {
	var out []outNode
	for id, n := range nodes {
		if n.Latitude == 0 && n.Longitude == 0 {
			continue // no usable position
		}
		if opts.topic != "" && !seenByTopic(n, opts.topic) {
			continue
		}
		lat := float64(n.Latitude) / 1e7
		lon := float64(n.Longitude) / 1e7
		dist := 0.0
		if opts.hasCenter {
			dist = haversineKm(opts.lat, opts.lon, lat, lon)
			if opts.radiusKm > 0 && dist > opts.radiusKm {
				continue
			}
		}
		out = append(out, outNode{
			id:     id,
			lat:    lat,
			lon:    lon,
			batt:   n.BatteryLevel,
			volt:   int(math.Round(n.Voltage * 100)),
			role:   roleCode(n.Role),
			seen:   freshestSeen(n),
			hw:     sanitize(n.HwModel, 9, false),
			short:  sanitize(n.ShortName, 4, false),
			long:   sanitize(n.LongName, 19, true),
			distKm: dist,
		})
	}
	if opts.hasCenter {
		sort.Slice(out, func(i, j int) bool { return out[i].distKm < out[j].distKm })
	} else {
		sort.Slice(out, func(i, j int) bool { return out[i].id < out[j].id })
	}
	if opts.max > 0 && len(out) > opts.max {
		out = out[:opts.max]
	}
	return out
}

func fetchNodes(url string, timeout time.Duration) (map[uint32]meshNode, error) {
	client := &http.Client{Timeout: timeout}
	req, err := http.NewRequest(http.MethodGet, url, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("User-Agent", "meshpull/1.0 (+lora-suite)")
	req.Header.Set("Accept", "application/json")
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("GET %s: HTTP %d", url, resp.StatusCode)
	}
	// keys are decimal node numbers as strings
	raw := map[string]meshNode{}
	if err := json.NewDecoder(resp.Body).Decode(&raw); err != nil {
		return nil, err
	}
	out := make(map[uint32]meshNode, len(raw))
	for k, v := range raw {
		id, err := strconv.ParseUint(k, 10, 32)
		if err != nil {
			continue // skip ids that don't fit a Meshtastic uint32 node num
		}
		out[uint32(id)] = v
	}
	return out, nil
}

func writeCSV(path string, rows []outNode, now int64) error {
	var b strings.Builder
	fmt.Fprintf(&b, "# generated %d\n", now)
	b.WriteString("# id,lat,lon,batt,volt,role,seen,hw,short,long\n")
	for _, r := range rows {
		b.WriteString(formatRow(r))
		b.WriteByte('\n')
	}
	return os.WriteFile(path, []byte(b.String()), 0o644)
}

func main() {
	url := flag.String("url", defaultURL, "meshmap.net nodes.json URL")
	lat := flag.Float64("lat", 200, "center latitude for the distance filter/sort")
	lon := flag.Float64("lon", 200, "center longitude for the distance filter/sort")
	radius := flag.Float64("radius", 0, "keep nodes within this many km of the center (0 = no cutoff)")
	topic := flag.String("topic", "", "keep only nodes seen under this MQTT topic prefix, e.g. msh/EU_868")
	max := flag.Int("max", 96, "maximum nodes to write (must be <= device MeshOverlay::CAP)")
	out := flag.String("out", "import.csv", "output CSV path (copy to SD:/mesh/import.csv)")
	timeout := flag.Duration("timeout", 20*time.Second, "HTTP timeout")
	flag.Parse()

	opts := selectOpts{
		lat:      *lat,
		lon:      *lon,
		radiusKm: *radius,
		topic:    *topic,
		max:      *max,
	}
	opts.hasCenter = *lat >= -90 && *lat <= 90 && *lon >= -180 && *lon <= 180
	if *radius > 0 && !opts.hasCenter {
		fmt.Fprintln(os.Stderr, "meshpull: -radius needs a valid -lat/-lon center")
		os.Exit(2)
	}
	if !opts.hasCenter && opts.topic == "" {
		fmt.Fprintln(os.Stderr, "meshpull: warning: no -lat/-lon/-radius or -topic filter; writing the nearest -max nodes by id")
	}

	nodes, err := fetchNodes(*url, *timeout)
	if err != nil {
		fmt.Fprintln(os.Stderr, "meshpull:", err)
		os.Exit(1)
	}
	rows := selectNodes(nodes, opts)
	if err := writeCSV(*out, rows, time.Now().Unix()); err != nil {
		fmt.Fprintln(os.Stderr, "meshpull:", err)
		os.Exit(1)
	}
	fmt.Fprintf(os.Stderr, "meshpull: %d nodes in feed, wrote %d to %s\n", len(nodes), len(rows), *out)
	fmt.Fprintln(os.Stderr, "meshpull: please poll no more than once a minute (the feed caches for 60s)")
}
