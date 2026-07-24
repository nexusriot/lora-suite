package main

import (
	"math"
	"strings"
	"testing"
)

func TestRoleCode(t *testing.T) {
	cases := map[string]uint8{
		"CLIENT":        roleClient,
		"client":        roleClient,
		"CLIENT_BASE":   roleClient,
		"CLIENT_MUTE":   roleClientMute,
		"ROUTER":        roleRouter,
		"ROUTER_CLIENT": roleRouter,
		"REPEATER":      roleRepeater,
		"TRACKER":       roleTracker,
		"SENSOR":        roleSensor,
		"":              roleUnknown,
		"WEIRD_ROLE":    roleOther,
	}
	for in, want := range cases {
		if got := roleCode(in); got != want {
			t.Errorf("roleCode(%q) = %d, want %d", in, got, want)
		}
	}
}

func TestSanitize(t *testing.T) {
	// short: commas stripped, clamped to 4, control/non-ASCII dropped
	if got := sanitize("9,2\t11", 4, false); got != "9211" {
		t.Errorf("short sanitize = %q, want %q", got, "9211")
	}
	if got := sanitize("TOOLONGNAME", 4, false); got != "TOOL" {
		t.Errorf("short clamp = %q, want %q", got, "TOOL")
	}
	// emoji / non-ASCII dropped, surrounding text kept and trimmed
	if got := sanitize("mooncat \U0001F431", 19, true); got != "mooncat" {
		t.Errorf("emoji strip = %q, want %q", got, "mooncat")
	}
	// long: commas preserved (device parser handles them)
	if got := sanitize("Bravo, the second", 19, true); got != "Bravo, the second" {
		t.Errorf("long comma keep = %q, want %q", got, "Bravo, the second")
	}
	// long clamp to 19 then trim
	if got := sanitize("abcdefghijklmnopqrstuvwxyz", 19, true); got != "abcdefghijklmnopqrs" {
		t.Errorf("long clamp = %q (len %d), want 19 chars", got, len(got))
	}
}

func TestHaversineKm(t *testing.T) {
	// ~same point
	if d := haversineKm(40.0, 44.0, 40.0, 44.0); d > 1e-6 {
		t.Errorf("zero distance = %f", d)
	}
	// one degree of latitude ~= 111 km
	if d := haversineKm(40.0, 44.0, 41.0, 44.0); math.Abs(d-111.2) > 1.0 {
		t.Errorf("1deg lat = %f km, want ~111", d)
	}
}

// The device CSV parser reads exactly six commas of fixed fields, then takes the
// rest of the line as the (possibly comma-containing) long name. Verify the tool
// emits a line of that shape.
func TestFormatRowShape(t *testing.T) {
	row := formatRow(outNode{
		id: 1000968721, lat: 36.4249088, lon: 44.515, batt: 90, volt: 384,
		role: roleClientMute, seen: 1784800000, hw: "TBEAM", short: "9211", long: "Meshtastic, 9211",
	})
	// split into the 9 fixed fields + the long remainder
	parts := strings.SplitN(row, ",", 10)
	if len(parts) != 10 {
		t.Fatalf("row %q split into %d fields, want 10", row, len(parts))
	}
	want := []struct {
		i int
		v string
	}{
		{0, "1000968721"}, {3, "90"}, {4, "384"}, {5, "2"}, {6, "1784800000"},
		{7, "TBEAM"}, {8, "9211"}, {9, "Meshtastic, 9211"}, // long keeps its comma
	}
	for _, w := range want {
		if parts[w.i] != w.v {
			t.Errorf("field %d = %q, want %q", w.i, parts[w.i], w.v)
		}
	}
}

func TestClamps(t *testing.T) {
	if clampBatt(-5) != 0 || clampBatt(300) != 255 || clampBatt(90) != 90 {
		t.Error("clampBatt out of range")
	}
	if clampVolt(-1) != 0 || clampVolt(70000) != 65535 || clampVolt(384) != 384 {
		t.Error("clampVolt out of range")
	}
}

func TestSelectNodes(t *testing.T) {
	nodes := map[uint32]meshNode{
		1: {LongName: "Near", ShortName: "NR", Role: "CLIENT", HwModel: "TBEAM", Latitude: 400100000, Longitude: 445000000, BatteryLevel: 80, Voltage: 4.06,
			SeenBy: map[string]int64{"msh/EU_868/2/e/MediumFast/!abc": 1784790000, "msh/EU_868/2/e/MediumFast/!fed": 1784800000}},
		2: {LongName: "Far", ShortName: "FR", Role: "ROUTER", Latitude: 500000000, Longitude: 445000000, BatteryLevel: 50,
			SeenBy: map[string]int64{"msh/US/2/e/LongFast/!def": 1}},
		3: {LongName: "NoPos", ShortName: "NP", Role: "CLIENT", Latitude: 0, Longitude: 0},
	}
	center := selectOpts{hasCenter: true, lat: 40.0, lon: 44.5, radiusKm: 50, max: 96}
	got := selectNodes(nodes, center)
	if len(got) != 1 || got[0].id != 1 {
		t.Fatalf("radius filter: got %d rows %+v, want only node 1", len(got), got)
	}
	if got[0].short != "NR" || got[0].role != roleClient {
		t.Errorf("converted node wrong: %+v", got[0])
	}
	if got[0].hw != "TBEAM" || got[0].volt != 406 || got[0].seen != 1784800000 {
		t.Errorf("new fields wrong: hw=%q volt=%d seen=%d", got[0].hw, got[0].volt, got[0].seen)
	}

	// topic filter keeps only EU_868, drops the no-position node
	topicOnly := selectOpts{topic: "msh/EU_868", max: 96}
	got = selectNodes(nodes, topicOnly)
	if len(got) != 1 || got[0].id != 1 {
		t.Fatalf("topic filter: got %d rows, want only node 1", len(got))
	}

	// no center, no topic: nearest -max by id, no-position node excluded
	all := selectNodes(nodes, selectOpts{max: 96})
	if len(all) != 2 {
		t.Fatalf("no filter: got %d rows, want 2 (positioned) ", len(all))
	}
	if all[0].id != 1 || all[1].id != 2 {
		t.Errorf("id sort order wrong: %+v", all)
	}

	// max cap
	capped := selectNodes(nodes, selectOpts{max: 1})
	if len(capped) != 1 {
		t.Errorf("max cap: got %d, want 1", len(capped))
	}
}
