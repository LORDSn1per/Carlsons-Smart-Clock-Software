import Foundation

// MARK: - Serial ports

struct SerialPort: Identifiable, Hashable {
    let path: String
    var id: String { path }

    /// Bluetooth shows up as a /dev/cu.* device too and is never the clock.
    var isPlausibleUSBSerial: Bool {
        let name = (path as NSString).lastPathComponent.lowercased()
        if name.contains("bluetooth") { return false }
        return name.contains("usbserial")     // CH340 - what this clock uses
            || name.contains("wchusbserial")  // CH34x under some drivers
            || name.contains("slab_usbtouart") // CP210x
            || name.contains("usbmodem")      // native USB / some ESP32 boards
    }

    var displayName: String { (path as NSString).lastPathComponent }

    static func discover() -> [SerialPort] {
        let entries = (try? FileManager.default.contentsOfDirectory(atPath: "/dev")) ?? []
        return entries
            .filter { $0.hasPrefix("cu.") }
            .map { SerialPort(path: "/dev/\($0)") }
            .filter { $0.isPlausibleUSBSerial }
            .sorted { $0.path < $1.path }
    }
}

// MARK: - Firmware image

/// What a .bin actually is, decided by reading it rather than trusting its name.
///
/// The merged images this project builds are written at flash offset 0x0 and
/// contain the bootloader (0x1000), the partition table (0x8000) and the app
/// (0x10000). The app-only firmware.bin is a different thing entirely: it has
/// the app at offset 0 and belongs at 0x10000. Writing an app-only image at 0x0
/// produces a clock that will not boot, so Clock Builder refuses rather than
/// letting the mistake through - it is the single easiest way to brick a flash.
struct FirmwareImage {
    enum Kind {
        case merged          // safe to write at 0x0
        case appOnly         // belongs at 0x10000; refuse
        case unrecognised
    }

    let url: URL
    let kind: Kind
    let byteCount: Int
    let requiredFlashBytes: UInt32   // largest partition end, 0 if unknown
    let partitionSummary: String

    var name: String { url.lastPathComponent }

    var sizeDescription: String {
        String(format: "%.2f MB", Double(byteCount) / 1_048_576.0)
    }

    var isFlashable: Bool { kind == .merged }

    var verdict: String {
        switch kind {
        case .merged:
            let needed = requiredFlashBytes / (1024 * 1024)
            return "Full image · \(sizeDescription) · needs \(needed) MB flash"
        case .appOnly:
            return "App-only image — cannot be written at 0x0"
        case .unrecognised:
            return "Not a recognisable ESP32 image"
        }
    }

    var problem: String? {
        switch kind {
        case .merged:
            return nil
        case .appOnly:
            return """
            This is an app-only image (the kind produced at \
            .pio/build/esp32dev/firmware.bin). It belongs at offset 0x10000 and \
            is what the clock's own browser update page expects.

            Clock Builder writes at 0x0, which needs a full merged image — one \
            of the SmartClock_vX.XX.bin files.
            """
        case .unrecognised:
            return "No ESP32 bootloader or app header was found in this file."
        }
    }

    static func inspect(url: URL) -> FirmwareImage {
        guard let handle = try? FileHandle(forReadingFrom: url) else {
            return FirmwareImage(url: url, kind: .unrecognised, byteCount: 0,
                                 requiredFlashBytes: 0, partitionSummary: "")
        }
        defer { try? handle.close() }

        let attributes = try? FileManager.default.attributesOfItem(atPath: url.path)
        let byteCount = (attributes?[.size] as? Int) ?? 0

        func bytes(at offset: UInt64, count: Int) -> [UInt8] {
            guard (try? handle.seek(toOffset: offset)) != nil,
                  let data = try? handle.read(upToCount: count) else { return [] }
            return [UInt8](data)
        }

        let atZero = bytes(at: 0, count: 1)
        let atBootloader = bytes(at: 0x1000, count: 1)
        let partitionBlock = bytes(at: 0x8000, count: 0xC00)

        let hasBootloader = atBootloader.first == 0xE9
        let hasPartitionTable = partitionBlock.count >= 2
            && partitionBlock[0] == 0xAA && partitionBlock[1] == 0x50

        if hasBootloader && hasPartitionTable {
            let (needed, summary) = parsePartitions(partitionBlock)
            return FirmwareImage(url: url, kind: .merged, byteCount: byteCount,
                                 requiredFlashBytes: needed, partitionSummary: summary)
        }
        if atZero.first == 0xE9 {
            return FirmwareImage(url: url, kind: .appOnly, byteCount: byteCount,
                                 requiredFlashBytes: 0, partitionSummary: "")
        }
        return FirmwareImage(url: url, kind: .unrecognised, byteCount: byteCount,
                             requiredFlashBytes: 0, partitionSummary: "")
    }

    /// Partition entries are 32 bytes: magic(2) type(1) subtype(1) offset(4)
    /// size(4) label(16) flags(4). The end of the last one is how much flash
    /// the image actually requires, which is what we check the chip against.
    private static func parsePartitions(_ block: [UInt8]) -> (UInt32, String) {
        var required: UInt32 = 0
        var labels: [String] = []
        var index = 0
        while index + 32 <= block.count {
            let entry = Array(block[index ..< index + 32])
            guard entry[0] == 0xAA && entry[1] == 0x50 else { break }
            func word(_ at: Int) -> UInt32 {
                UInt32(entry[at]) | UInt32(entry[at + 1]) << 8
                    | UInt32(entry[at + 2]) << 16 | UInt32(entry[at + 3]) << 24
            }
            let offset = word(4)
            let length = word(8)
            let labelBytes = Array(entry[12 ..< 28]).prefix { $0 != 0 }
            let label = String(bytes: labelBytes, encoding: .utf8) ?? "?"
            labels.append(label)
            required = max(required, offset + length)
            index += 32
        }
        return (required, labels.joined(separator: ", "))
    }
}

// MARK: - Device identity

/// What esptool told us about the chip on the other end of the cable.
struct DeviceInfo {
    var chip = ""
    var mac = ""
    var flashBytes: UInt32 = 0
    var crystal = ""

    var flashDescription: String {
        flashBytes == 0 ? "unknown" : "\(flashBytes / (1024 * 1024))MB"
    }

    /// This project is pinned to Arduino-ESP32 2.x and builds for `esp32dev`.
    /// An S2/S3/C3/C6 is a different architecture and the image will not run on
    /// it, so identifying the family is the check that actually matters.
    var isClassicESP32: Bool {
        let value = chip.uppercased()
        guard value.contains("ESP32") else { return false }
        for other in ["ESP32-S2", "ESP32-S3", "ESP32-C2", "ESP32-C3",
                      "ESP32-C6", "ESP32-H2", "ESP32-P4"] where value.contains(other) {
            return false
        }
        return true
    }

    var summary: String {
        var parts: [String] = []
        if !chip.isEmpty { parts.append(chip) }
        if flashBytes > 0 { parts.append("\(flashDescription) flash") }
        if !crystal.isEmpty { parts.append(crystal) }
        if !mac.isEmpty { parts.append("MAC \(mac)") }
        return parts.joined(separator: " · ")
    }

    /// Parses the human-readable output of `esptool.py flash_id`.
    static func parse(_ output: String) -> DeviceInfo {
        var info = DeviceInfo()
        for line in output.split(separator: "\n") {
            let text = line.trimmingCharacters(in: .whitespaces)
            if text.hasPrefix("Chip is ") {
                info.chip = String(text.dropFirst("Chip is ".count))
            } else if text.hasPrefix("MAC: ") {
                info.mac = String(text.dropFirst("MAC: ".count))
            } else if text.hasPrefix("Crystal is ") {
                info.crystal = String(text.dropFirst("Crystal is ".count))
            } else if text.hasPrefix("Detected flash size: ") {
                let value = String(text.dropFirst("Detected flash size: ".count))
                if value.hasSuffix("MB"), let mb = UInt32(value.dropLast(2)) {
                    info.flashBytes = mb * 1024 * 1024
                } else if value.hasSuffix("KB"), let kb = UInt32(value.dropLast(2)) {
                    info.flashBytes = kb * 1024
                }
            }
        }
        return info
    }
}
