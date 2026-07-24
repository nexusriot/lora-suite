// Package lorakit is a Go port of the lora-suite wire codec (src/proto/frame.h),
// for host-side tooling: dissectors, gateways, and a future meshobserv fork that
// ingests our native 13-byte frames. Kept byte-compatible with the firmware
// (cross-checked against a C-encoded golden frame in the tests).
package lorakit

import "fmt"

const (
	MagicL        = 0x4C // 'L'
	Version       = 2
	HeaderLen     = 13
	CRCLen        = 2
	MaxPayload    = 200
	AddrBroadcast = 0xFFFF
	DefaultHop    = 3
)

// Message types (proto/frame.h MsgType).
const (
	MsgText      = 1
	MsgAck       = 2
	MsgBeacon    = 3
	MsgPing      = 4
	MsgPong      = 5
	MsgTelemetry = 6
	MsgAlert     = 7
	MsgNodeInfo  = 8
	MsgFileChunk = 9
	MsgTimeSync  = 10
	MsgWaypoint  = 11
	MsgCountdown = 12
)

// Flag bits (proto/frame.h Flags).
const (
	FlagAckReq    = 0x01
	FlagEncrypted = 0x02
	FlagMesh      = 0x04
	FlagFragment  = 0x08
	FlagHealth    = 0x10
	FlagLowPwr    = 0x20
)

type Frame struct {
	Type    uint8
	Flags   uint8
	Chan    uint8
	Hop     uint8
	Src     uint16
	Dst     uint16
	MsgID   uint16
	Payload []byte
}

// Crc16 is CRC-16/CCITT-FALSE (init 0xFFFF, poly 0x1021), matching frame.cpp.
func Crc16(data []byte) uint16 {
	crc := uint16(0xFFFF)
	for _, b := range data {
		crc ^= uint16(b) << 8
		for i := 0; i < 8; i++ {
			if crc&0x8000 != 0 {
				crc = (crc << 1) ^ 0x1021
			} else {
				crc <<= 1
			}
		}
	}
	return crc
}

func put16(p []byte, v uint16) { p[0] = byte(v); p[1] = byte(v >> 8) }
func get16(p []byte) uint16    { return uint16(p[0]) | uint16(p[1])<<8 }

func Encode(f Frame) ([]byte, error) {
	if len(f.Payload) > MaxPayload {
		return nil, fmt.Errorf("payload too long: %d", len(f.Payload))
	}
	ln := len(f.Payload)
	out := make([]byte, HeaderLen+ln+CRCLen)
	out[0] = MagicL
	out[1] = Version
	out[2] = f.Type
	out[3] = f.Flags
	out[4] = f.Chan
	out[5] = f.Hop
	put16(out[6:], f.Src)
	put16(out[8:], f.Dst)
	put16(out[10:], f.MsgID)
	out[12] = byte(ln)
	copy(out[HeaderLen:], f.Payload)
	put16(out[HeaderLen+ln:], Crc16(out[:HeaderLen+ln]))
	return out, nil
}

func Decode(buf []byte) (Frame, error) {
	var f Frame
	if len(buf) < HeaderLen+CRCLen {
		return f, fmt.Errorf("short frame (%d bytes)", len(buf))
	}
	if buf[0] != MagicL || buf[1] != Version {
		return f, fmt.Errorf("bad magic/version %02x %02x", buf[0], buf[1])
	}
	ln := int(buf[12])
	if ln > MaxPayload {
		return f, fmt.Errorf("bad length %d", ln)
	}
	total := HeaderLen + ln + CRCLen
	if len(buf) < total {
		return f, fmt.Errorf("truncated: need %d, have %d", total, len(buf))
	}
	want := get16(buf[HeaderLen+ln:])
	if got := Crc16(buf[:HeaderLen+ln]); got != want {
		return f, fmt.Errorf("crc mismatch: got %04x want %04x", got, want)
	}
	f.Type = buf[2]
	f.Flags = buf[3]
	f.Chan = buf[4]
	f.Hop = buf[5]
	f.Src = get16(buf[6:])
	f.Dst = get16(buf[8:])
	f.MsgID = get16(buf[10:])
	f.Payload = append([]byte(nil), buf[HeaderLen:HeaderLen+ln]...)
	return f, nil
}

func TypeName(t uint8) string {
	switch t {
	case MsgText:
		return "TEXT"
	case MsgAck:
		return "ACK"
	case MsgBeacon:
		return "BEACON"
	case MsgPing:
		return "PING"
	case MsgPong:
		return "PONG"
	case MsgTelemetry:
		return "TELEMETRY"
	case MsgAlert:
		return "ALERT"
	case MsgNodeInfo:
		return "NODEINFO"
	case MsgFileChunk:
		return "FILECHUNK"
	case MsgTimeSync:
		return "TIMESYNC"
	case MsgWaypoint:
		return "WAYPOINT"
	case MsgCountdown:
		return "COUNTDOWN"
	default:
		return fmt.Sprintf("TYPE%d", t)
	}
}

// FlagString renders set flag bits as a compact "|"-joined string.
func FlagString(fl uint8) string {
	names := []struct {
		bit  uint8
		name string
	}{
		{FlagAckReq, "ACK"}, {FlagEncrypted, "ENC"}, {FlagMesh, "MESH"},
		{FlagFragment, "FRAG"}, {FlagHealth, "HEALTH"}, {FlagLowPwr, "LOWPWR"},
	}
	out := ""
	for _, n := range names {
		if fl&n.bit != 0 {
			if out != "" {
				out += "|"
			}
			out += n.name
		}
	}
	if out == "" {
		return "-"
	}
	return out
}
