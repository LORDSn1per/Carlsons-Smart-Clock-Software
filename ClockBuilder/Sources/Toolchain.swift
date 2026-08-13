import Foundation

/// Locates esptool and runs it, streaming output back line by line.
///
/// esptool is a Python program. macOS ships a python3 that has no pyserial, and
/// pyserial is exactly what esptool needs to touch a serial port, so calling the
/// system interpreter fails with "Pyserial is not installed". PlatformIO keeps
/// its own virtualenv that does have it, so that is the first thing we look for.
enum Toolchain {

    struct Location {
        let python: String
        let esptool: String
    }

    /// Human-readable reason discovery failed, for the UI to show.
    static var lastFailureReason = ""

    static func locate() -> Location? {
        let home = FileManager.default.homeDirectoryForCurrentUser.path
        let fm = FileManager.default

        // PlatformIO's virtualenv interpreter - the one that has pyserial.
        let pioPython = "\(home)/.platformio/penv/bin/python"

        // The esptool package directory is versioned in some installs, so take
        // whichever copy is present rather than hard-coding a version.
        var candidates: [String] = ["\(home)/.platformio/packages/tool-esptoolpy/esptool.py"]
        let packages = "\(home)/.platformio/packages"
        if let entries = try? fm.contentsOfDirectory(atPath: packages) {
            for entry in entries where entry.hasPrefix("tool-esptoolpy") {
                candidates.append("\(packages)/\(entry)/esptool.py")
            }
        }

        if fm.isExecutableFile(atPath: pioPython) {
            for candidate in candidates where fm.fileExists(atPath: candidate) {
                return Location(python: pioPython, esptool: candidate)
            }
        }

        // Fallbacks: a user-installed esptool on PATH, or one importable by a
        // python3 that does have pyserial.
        for python in ["/usr/local/bin/python3", "/opt/homebrew/bin/python3", "/usr/bin/python3"] {
            guard fm.isExecutableFile(atPath: python) else { continue }
            for candidate in candidates where fm.fileExists(atPath: candidate) {
                if probeWorks(python: python, esptool: candidate) {
                    return Location(python: python, esptool: candidate)
                }
            }
        }

        lastFailureReason = """
        Could not find a working esptool.

        Clock Builder uses the copy that ships with PlatformIO:
          ~/.platformio/penv/bin/python
          ~/.platformio/packages/tool-esptoolpy/esptool.py

        Install PlatformIO, or install esptool into a Python that has pyserial.
        """
        return nil
    }

    /// esptool exits non-zero and prints a pyserial complaint when it cannot
    /// import its serial backend, so "version" is enough to tell a usable
    /// interpreter from an unusable one.
    private static func probeWorks(python: String, esptool: String) -> Bool {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: python)
        process.arguments = [esptool, "version"]
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        do { try process.run() } catch { return false }
        process.waitUntilExit()
        return process.terminationStatus == 0
    }
}

/// Runs a command, delivering stdout+stderr to `onOutput` as it arrives.
final class CommandRunner {
    private var process: Process?

    var isRunning: Bool { process?.isRunning ?? false }

    func run(executable: String,
             arguments: [String],
             onOutput: @escaping (String) -> Void,
             onFinish: @escaping (Int32) -> Void) {

        let process = Process()
        process.executableURL = URL(fileURLWithPath: executable)
        process.arguments = arguments

        // esptool buffers its progress output when it is not talking to a
        // terminal; without this the percentages arrive in one lump at the end
        // and the progress bar sits at zero for the whole write.
        var environment = ProcessInfo.processInfo.environment
        environment["PYTHONUNBUFFERED"] = "1"
        process.environment = environment

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe

        pipe.fileHandleForReading.readabilityHandler = { handle in
            let data = handle.availableData
            guard !data.isEmpty, let text = String(data: data, encoding: .utf8) else { return }
            DispatchQueue.main.async { onOutput(text) }
        }

        process.terminationHandler = { finished in
            pipe.fileHandleForReading.readabilityHandler = nil
            DispatchQueue.main.async {
                self.process = nil
                onFinish(finished.terminationStatus)
            }
        }

        self.process = process
        do {
            try process.run()
        } catch {
            self.process = nil
            DispatchQueue.main.async {
                onOutput("Failed to start \(executable): \(error.localizedDescription)\n")
                onFinish(-1)
            }
        }
    }

    func cancel() {
        process?.terminate()
    }
}
