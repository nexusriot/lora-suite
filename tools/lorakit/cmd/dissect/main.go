// dissect decodes lora-suite frames from stdin, one per line. Each line is either
// raw hex (spaces/colons ignored) or a JSON object from the Gateway app
// ({"t":..,"rssi":..,"snr":..,"hex":".."}). Undecodable lines are reported, so
// foreign/corrupt traffic is visible too.
//
//	cat capture.hex | go run ./cmd/dissect
//	pio device monitor | go run ./cmd/dissect      # live, via the Gateway app
package main

import (
	"bufio"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"strings"

	"lorakit"
)

type gwLine struct {
	T    uint32  `json:"t"`
	Rssi int     `json:"rssi"`
	Snr  float64 `json:"snr"`
	Hex  string  `json:"hex"`
}

func printable(b []byte) string {
	var sb strings.Builder
	for _, c := range b {
		if c >= 0x20 && c < 0x7f {
			sb.WriteByte(c)
		} else {
			sb.WriteByte('.')
		}
	}
	return sb.String()
}

func main() {
	sc := bufio.NewScanner(os.Stdin)
	sc.Buffer(make([]byte, 0, 64*1024), 1<<20)
	decoded := 0
	for sc.Scan() {
		line := strings.TrimSpace(sc.Text())
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		meta, hexStr := "", line
		if strings.HasPrefix(line, "{") {
			var g gwLine
			if err := json.Unmarshal([]byte(line), &g); err != nil {
				fmt.Printf("! bad json: %v\n", err)
				continue
			}
			hexStr = g.Hex
			meta = fmt.Sprintf("  rssi=%d snr=%.1f", g.Rssi, g.Snr)
		}
		hexStr = strings.NewReplacer(" ", "", ":", "", "\t", "").Replace(hexStr)

		buf, err := hex.DecodeString(hexStr)
		if err != nil {
			fmt.Printf("! bad hex: %v\n", err)
			continue
		}
		f, err := lorakit.Decode(buf)
		if err != nil {
			fmt.Printf("! %s  (%d bytes)\n", err, len(buf))
			continue
		}
		decoded++

		body := fmt.Sprintf("%q", printable(f.Payload))
		if f.Flags&lorakit.FlagEncrypted != 0 {
			body = fmt.Sprintf("<%d bytes encrypted>", len(f.Payload))
		}
		fmt.Printf("%-9s %04x->%04x id=%-5d hop=%d ch=%d [%s] len=%-3d%s  %s\n",
			lorakit.TypeName(f.Type), f.Src, f.Dst, f.MsgID, f.Hop, f.Chan,
			lorakit.FlagString(f.Flags), len(f.Payload), meta, body)
	}
	if err := sc.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "dissect:", err)
		os.Exit(1)
	}
	fmt.Fprintf(os.Stderr, "dissect: %d frames decoded\n", decoded)
}
