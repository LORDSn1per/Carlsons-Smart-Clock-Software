import Foundation
import SwiftUI

@MainActor
final class Flasher: ObservableObject {

    enum Stage: Equatable {
        case idle
        case identifying
        case flashing
        case done(String)
        case failed(String)
    }

    // Where this project keeps its built images. Editable in the UI if the NAS
    // is not mounted or the user keeps them elsewhere.
    static let defaultBinFolder =
        "/Volumes/home/Documents/Arduino/SmartClock/Software/Firmware/BIN"

    @Published var ports: [SerialPort] = []
    @Published var selectedPort: SerialPort?
    @Published var device: DeviceInfo?
    @Published var deviceProblem: String?

    @Published var binFolder = Flasher.defaultBinFolder
    @Published var images: [FirmwareImage] = []
    /// Selection is held as a URL rather than the struct so it survives the list
    /// being rebuilt by a rescan.
    @Published var selectedImageURL: URL?

    var selectedImage: FirmwareImage? {
        images.first(where: { $0.url == selectedImageURL })
    }

    @Published var eraseEverything = false
    @Published var baud = 460800

    @Published var stage: Stage = .idle
    @Published var progress: Double = 0
    @Published var log = ""

    /// 921600 is deliberately absent. The CH340 on this clock cannot sustain it
    /// on every Mac - it fails with "Invalid head of packet" partway through -
    /// which is why platformio.ini was lowered to 460800.
    let baudChoices = [460800, 230400, 115200]

    private let runner = CommandRunner()

    var isBusy: Bool { stage == .identifying || stage == .flashing }

    var canFlash: Bool {
        guard !isBusy, selectedPort != nil, let image = selectedImage else { return false }
        return image.isFlashable && device != nil && deviceProblem == nil
    }

    // MARK: - Discovery

    func refreshPorts() {
        ports = SerialPort.discover()
        if let current = selectedPort, !ports.contains(current) {
            selectedPort = nil
            device = nil
            deviceProblem = nil
        }
        if selectedPort == nil { selectedPort = ports.first }
    }

    func refreshImages() {
        let folder = URL(fileURLWithPath: binFolder)
        let entries = (try? FileManager.default.contentsOfDirectory(
            at: folder, includingPropertiesForKeys: nil)) ?? []

        images = entries
            .filter { $0.pathExtension.lowercased() == "bin" }
            // Newest version first: these are named SmartClock_v4.21.bin, so a
            // plain descending sort on the name puts the latest at the top.
            .sorted { $0.lastPathComponent.localizedStandardCompare($1.lastPathComponent) == .orderedDescending }
            .map { FirmwareImage.inspect(url: $0) }

        if let current = selectedImageURL,
           !images.contains(where: { $0.url == current }) {
            selectedImageURL = nil
        }
        if selectedImageURL == nil {
            selectedImageURL = images.first(where: { $0.isFlashable })?.url
        }
    }

    // MARK: - Identify

    /// Reads the chip's identity before anything is written. This is the check
    /// that the right module is plugged in: the wrong ESP32 family, or a chip
    /// with too little flash for this partition table, is caught here rather
    /// than after a half-written image.
    func identify() {
        guard let port = selectedPort, let tools = Toolchain.locate() else {
            if Toolchain.locate() == nil { fail(Toolchain.lastFailureReason) }
            return
        }

        device = nil
        deviceProblem = nil
        stage = .identifying
        progress = 0
        appendLog("\n$ esptool flash_id on \(port.displayName)\n")

        var captured = ""
        runner.run(executable: tools.python,
                   arguments: [tools.esptool, "--port", port.path,
                               "--baud", "115200", "flash_id"],
                   onOutput: { [weak self] text in
                       captured += text
                       self?.appendLog(text)
                   },
                   onFinish: { [weak self] status in
                       guard let self else { return }
                       guard status == 0 else {
                           self.fail("""
                           Could not talk to a chip on \(port.displayName).

                           Check the cable carries data (many are charge-only), \
                           and that nothing else holds the port — the ESP Decoder \
                           VS Code extension and an open Serial Monitor both do.
                           """)
                           return
                       }
                       self.evaluate(DeviceInfo.parse(captured))
                   })
    }

    private func evaluate(_ info: DeviceInfo) {
        device = info
        stage = .idle

        guard info.isClassicESP32 else {
            deviceProblem = """
            This is a \(info.chip.isEmpty ? "different ESP32 variant" : info.chip).

            SmartClock is built for the classic ESP32 (esp32dev) and is pinned to \
            Arduino-ESP32 2.x. The image will not run on an S2, S3, C3 or C6.
            """
            return
        }

        if let image = selectedImage, image.isFlashable,
           info.flashBytes > 0, info.flashBytes < image.requiredFlashBytes {
            let needMB = image.requiredFlashBytes / (1024 * 1024)
            deviceProblem = """
            This chip reports \(info.flashDescription) of flash, but the image's \
            partition table runs to \(needMB) MB. Flashing it would truncate the \
            partitions and the clock would not boot.
            """
            return
        }

        appendLog("\nIdentified: \(info.summary)\n")
    }

    // MARK: - Flash

    func flash() {
        guard let port = selectedPort, let image = selectedImage,
              let tools = Toolchain.locate() else { return }

        stage = .flashing
        progress = 0

        // Erase first when the user asked for a clean device. write_flash alone
        // leaves the SPIFFS partition untouched, which is what preserves saved
        // settings and WiFi credentials on an update.
        var arguments = [tools.esptool,
                         "--chip", "esp32",
                         "--port", port.path,
                         "--baud", String(baud),
                         "--before", "default_reset",
                         "--after", "hard_reset"]
        if eraseEverything {
            arguments += ["erase_flash"]
            appendLog("\n$ esptool erase_flash (wipes settings and WiFi)\n")
            runner.run(executable: tools.python, arguments: arguments,
                       onOutput: { [weak self] in self?.consume($0) },
                       onFinish: { [weak self] status in
                           guard let self else { return }
                           guard status == 0 else {
                               self.fail("Erase failed before anything was written.")
                               return
                           }
                           self.write(image: image, port: port, tools: tools)
                       })
        } else {
            write(image: image, port: port, tools: tools)
        }
    }

    private func write(image: FirmwareImage, port: SerialPort, tools: Toolchain.Location) {
        // 0x0 because these are merged images containing the bootloader and the
        // partition table. FirmwareImage.inspect() has already refused anything
        // that is not one.
        let arguments = [tools.esptool,
                         "--chip", "esp32",
                         "--port", port.path,
                         "--baud", String(baud),
                         "--before", "default_reset",
                         "--after", "hard_reset",
                         "write_flash", "-z", "0x0", image.url.path]

        appendLog("\n$ esptool write_flash 0x0 \(image.name)\n")
        stage = .flashing
        runner.run(executable: tools.python, arguments: arguments,
                   onOutput: { [weak self] in self?.consume($0) },
                   onFinish: { [weak self] status in
                       guard let self else { return }
                       if status == 0 {
                           self.progress = 1
                           self.stage = .done("\(image.name) written. The clock is restarting.")
                       } else {
                           self.fail("""
                           Flashing stopped partway through.

                           A dropout here is almost always the USB cable or port \
                           rather than the image. Try another cable first, then a \
                           lower speed from the Speed menu.
                           """)
                       }
                   })
    }

    func cancel() {
        runner.cancel()
        appendLog("\nCancelled.\n")
        stage = .idle
        progress = 0
    }

    // MARK: - Output

    private func consume(_ text: String) {
        appendLog(text)
        // esptool reports "Writing at 0x00010000... (43 %)".
        if let range = text.range(of: #"\((\s*\d+)\s*%\)"#, options: .regularExpression) {
            let digits = text[range].filter { $0.isNumber }
            if let value = Double(digits) { progress = min(value / 100.0, 1.0) }
        }
    }

    private func appendLog(_ text: String) {
        log += text
        // Keep the buffer bounded; a full erase plus write is chatty.
        if log.count > 60_000 { log = String(log.suffix(40_000)) }
    }

    private func fail(_ message: String) {
        stage = .failed(message)
        progress = 0
    }
}
