package main

import (
	"encoding/binary"
	"os"
	"path/filepath"
	"testing"
)

func writeImage(t *testing.T, name string, contents []byte) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), name)
	if err := os.WriteFile(path, contents, 0o600); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestInspectMergedImage(t *testing.T) {
	contents := make([]byte, 0x9000)
	contents[0x1000] = 0xE9
	entry := contents[0x8000:0x8020]
	entry[0], entry[1] = 0xAA, 0x50
	binary.LittleEndian.PutUint32(entry[4:8], 0x290000)
	binary.LittleEndian.PutUint32(entry[8:12], 0x170000)
	copy(entry[12:28], []byte("spiffs"))

	image := inspectFirmware(writeImage(t, "SmartClock_v4.33.bin", contents))
	if !image.flashable() {
		t.Fatalf("merged image was rejected: %s", image.Problem)
	}
	if image.RequiredFlash != 0x400000 {
		t.Fatalf("required flash = %#x, want 0x400000", image.RequiredFlash)
	}
}

func TestRejectsAppOnlyImage(t *testing.T) {
	contents := make([]byte, 4096)
	contents[0] = 0xE9
	image := inspectFirmware(writeImage(t, "firmware.bin", contents))
	if image.Kind != imageAppOnly || image.flashable() {
		t.Fatalf("app-only firmware was not rejected: %#v", image)
	}
}

func TestParsesCurrentAndLegacyEsptoolOutput(t *testing.T) {
	current := parseDevice("Chip type: ESP32-D0WD-V3\nMAC: 01:23:45:67:89:ab\nCrystal frequency: 40MHz\nFlash size: 4MB\n")
	if !current.isClassicESP32() || current.FlashBytes != 4*1024*1024 {
		t.Fatalf("current output parsed incorrectly: %#v", current)
	}

	legacy := parseDevice("Chip is ESP32-D0WDQ6\nCrystal is 40MHz\nDetected flash size: 4MB\n")
	if !legacy.isClassicESP32() || legacy.FlashBytes != 4*1024*1024 {
		t.Fatalf("legacy output parsed incorrectly: %#v", legacy)
	}
}

func TestRejectsOtherESP32Families(t *testing.T) {
	for _, chip := range []string{"ESP32-S3", "ESP32-C3", "ESP32-C6"} {
		if (deviceInfo{Chip: chip}).isClassicESP32() {
			t.Fatalf("%s incorrectly accepted", chip)
		}
	}
}
