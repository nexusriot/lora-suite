package lorakit

import (
	"encoding/hex"
	"testing"
)

// Golden frame produced by the C encoder (src/proto/frame.cpp):
//
//	type=TEXT flags=ACK_REQ chan=0 hop=3 src=0x1234 dst=0xABCD msgid=7 payload="hi"
const goldenHex = "4c02010100033412cdab0700026869f4cb"

func TestCrc16CheckValue(t *testing.T) {
	// CRC-16/CCITT-FALSE check value for "123456789" is 0x29B1.
	if got := Crc16([]byte("123456789")); got != 0x29B1 {
		t.Fatalf("Crc16 check = %04x, want 29b1", got)
	}
}

func TestGoldenDecode(t *testing.T) {
	buf, _ := hex.DecodeString(goldenHex)
	f, err := Decode(buf)
	if err != nil {
		t.Fatalf("decode golden: %v", err)
	}
	if f.Type != MsgText || f.Flags != FlagAckReq || f.Chan != 0 || f.Hop != 3 {
		t.Errorf("header wrong: %+v", f)
	}
	if f.Src != 0x1234 || f.Dst != 0xABCD || f.MsgID != 7 {
		t.Errorf("addrs wrong: src=%04x dst=%04x id=%d", f.Src, f.Dst, f.MsgID)
	}
	if string(f.Payload) != "hi" {
		t.Errorf("payload = %q, want %q", f.Payload, "hi")
	}
	// Re-encode must reproduce the exact C bytes.
	out, err := Encode(f)
	if err != nil {
		t.Fatalf("encode: %v", err)
	}
	if hex.EncodeToString(out) != goldenHex {
		t.Errorf("re-encode = %s, want %s", hex.EncodeToString(out), goldenHex)
	}
}

func TestRoundTrip(t *testing.T) {
	f := Frame{Type: MsgBeacon, Flags: FlagMesh | FlagHealth, Chan: 2, Hop: 3,
		Src: 0xBEEF, Dst: AddrBroadcast, MsgID: 42, Payload: []byte{1, 2, 3, 4, 5}}
	out, err := Encode(f)
	if err != nil {
		t.Fatal(err)
	}
	g, err := Decode(out)
	if err != nil {
		t.Fatal(err)
	}
	if g.Type != f.Type || g.Flags != f.Flags || g.Src != f.Src || g.Dst != f.Dst ||
		g.MsgID != f.MsgID || string(g.Payload) != string(f.Payload) {
		t.Errorf("round-trip mismatch: %+v vs %+v", f, g)
	}
}

func TestDecodeRejects(t *testing.T) {
	good, _ := hex.DecodeString(goldenHex)

	if _, err := Decode(good[:5]); err == nil {
		t.Error("short frame should fail")
	}

	bad := append([]byte(nil), good...)
	bad[0] = 0x00 // wrong magic
	if _, err := Decode(bad); err == nil {
		t.Error("bad magic should fail")
	}

	corrupt := append([]byte(nil), good...)
	corrupt[14] ^= 0xFF // flip a payload byte -> CRC mismatch
	if _, err := Decode(corrupt); err == nil {
		t.Error("corrupt payload should fail CRC")
	}
}

func TestFlagString(t *testing.T) {
	if FlagString(0) != "-" {
		t.Error("no flags should render -")
	}
	if FlagString(FlagMesh|FlagHealth) != "MESH|HEALTH" {
		t.Errorf("flags = %q", FlagString(FlagMesh|FlagHealth))
	}
}
