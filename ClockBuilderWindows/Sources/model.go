package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
)

type imageKind int

const (
	imageUnknown imageKind = iota
	imageMerged
	imageAppOnly
)

type firmwareImage struct {
	Path          string
	Name          string
	Kind          imageKind
	ByteCount     int64
	RequiredFlash uint32
	Partitions    string
	Verdict       string
	Problem       string
}

func (f firmwareImage) flashable() bool { return f.Kind == imageMerged && f.Problem == "" }

func inspectFirmware(path string) firmwareImage {
	result := firmwareImage{Path: path, Name: filepath.Base(path), Kind: imageUnknown}
	file, err := os.Open(path)
	if err != nil {
		result.Problem = "The firmware file could not be opened."
		return result
	}
	defer file.Close()

	if info, err := file.Stat(); err == nil {
		result.ByteCount = info.Size()
	}

	readAt := func(offset int64, count int) []byte {
		buffer := make([]byte, count)
		n, err := file.ReadAt(buffer, offset)
		if err != nil && err != io.EOF {
			return nil
		}
		return buffer[:n]
	}

	atZero := readAt(0, 1)
	bootloader := readAt(0x1000, 1)
	partitionBlock := readAt(0x8000, 0xC00)
	hasBootloader := len(bootloader) == 1 && bootloader[0] == 0xE9
	hasPartitions := len(partitionBlock) >= 2 && partitionBlock[0] == 0xAA && partitionBlock[1] == 0x50

	switch {
	case hasBootloader && hasPartitions:
		result.Kind = imageMerged
		result.RequiredFlash, result.Partitions = parsePartitions(partitionBlock)
		result.Verdict = fmt.Sprintf("Full image  ·  %.2f MB  ·  needs %d MB flash",
			float64(result.ByteCount)/1048576.0, result.RequiredFlash/(1024*1024))
	case len(atZero) == 1 && atZero[0] == 0xE9:
		result.Kind = imageAppOnly
		result.Problem = "App-only firmware cannot be written at 0x0. Choose a full SmartClock_vX.XX.bin image."
	default:
		result.Problem = "This is not a recognisable full ESP32 firmware image."
	}
	return result
}

func parsePartitions(block []byte) (uint32, string) {
	var required uint32
	labels := make([]string, 0, 8)
	for index := 0; index+32 <= len(block); index += 32 {
		entry := block[index : index+32]
		if entry[0] != 0xAA || entry[1] != 0x50 {
			break
		}
		offset := binary.LittleEndian.Uint32(entry[4:8])
		length := binary.LittleEndian.Uint32(entry[8:12])
		label := strings.TrimRight(string(entry[12:28]), "\x00")
		if label != "" {
			labels = append(labels, label)
		}
		if end := offset + length; end > required {
			required = end
		}
	}
	return required, strings.Join(labels, ", ")
}

var versionPattern = regexp.MustCompile(`(?i)[_-]v?(\d+)\.(\d+)`)

func imageVersion(name string) (int, int) {
	match := versionPattern.FindStringSubmatch(name)
	if len(match) != 3 {
		return -1, -1
	}
	major, _ := strconv.Atoi(match[1])
	minor, _ := strconv.Atoi(match[2])
	return major, minor
}

func discoverImages(folder string) []firmwareImage {
	entries, err := os.ReadDir(folder)
	if err != nil {
		return nil
	}
	images := make([]firmwareImage, 0, len(entries))
	for _, entry := range entries {
		if entry.IsDir() || !strings.EqualFold(filepath.Ext(entry.Name()), ".bin") {
			continue
		}
		images = append(images, inspectFirmware(filepath.Join(folder, entry.Name())))
	}
	sort.SliceStable(images, func(i, j int) bool {
		majorI, minorI := imageVersion(images[i].Name)
		majorJ, minorJ := imageVersion(images[j].Name)
		if majorI != majorJ {
			return majorI > majorJ
		}
		if minorI != minorJ {
			return minorI > minorJ
		}
		return strings.ToLower(images[i].Name) > strings.ToLower(images[j].Name)
	})
	return images
}

type deviceInfo struct {
	Chip       string
	MAC        string
	FlashBytes uint32
	Crystal    string
}

func (d deviceInfo) isClassicESP32() bool {
	upper := strings.ToUpper(d.Chip)
	if !strings.Contains(upper, "ESP32") {
		return false
	}
	for _, variant := range []string{"ESP32-S2", "ESP32-S3", "ESP32-C2", "ESP32-C3", "ESP32-C6", "ESP32-H2", "ESP32-P4"} {
		if strings.Contains(upper, variant) {
			return false
		}
	}
	return true
}

func (d deviceInfo) flashDescription() string {
	if d.FlashBytes == 0 {
		return "unknown flash"
	}
	return fmt.Sprintf("%d MB flash", d.FlashBytes/(1024*1024))
}

func (d deviceInfo) summary() string {
	parts := make([]string, 0, 4)
	if d.Chip != "" {
		parts = append(parts, d.Chip)
	}
	if d.FlashBytes > 0 {
		parts = append(parts, d.flashDescription())
	}
	if d.Crystal != "" {
		parts = append(parts, d.Crystal)
	}
	if d.MAC != "" {
		parts = append(parts, "MAC "+d.MAC)
	}
	return strings.Join(parts, "  ·  ")
}

var sizePattern = regexp.MustCompile(`(?i)(\d+)\s*(MB|KB)`)

func parseDevice(output string) deviceInfo {
	var result deviceInfo
	for _, raw := range strings.Split(strings.ReplaceAll(output, "\r", ""), "\n") {
		line := strings.TrimSpace(raw)
		lower := strings.ToLower(line)
		switch {
		case strings.HasPrefix(line, "Chip is "):
			result.Chip = strings.TrimSpace(strings.TrimPrefix(line, "Chip is "))
		case strings.HasPrefix(lower, "chip type:"):
			result.Chip = strings.TrimSpace(line[strings.Index(line, ":")+1:])
		case strings.HasPrefix(line, "MAC: "):
			result.MAC = strings.TrimSpace(strings.TrimPrefix(line, "MAC: "))
		case strings.HasPrefix(line, "Crystal is "):
			result.Crystal = strings.TrimSpace(strings.TrimPrefix(line, "Crystal is "))
		case strings.HasPrefix(lower, "crystal frequency:"):
			result.Crystal = strings.TrimSpace(line[strings.Index(line, ":")+1:])
		case strings.Contains(lower, "flash size:"):
			if match := sizePattern.FindStringSubmatch(line); len(match) == 3 {
				value, _ := strconv.Atoi(match[1])
				if strings.EqualFold(match[2], "MB") {
					result.FlashBytes = uint32(value) * 1024 * 1024
				} else {
					result.FlashBytes = uint32(value) * 1024
				}
			}
		}
	}
	return result
}
