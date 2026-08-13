import SwiftUI

@main
struct ClockBuilderApp: App {
    var body: some Scene {
        WindowGroup("Clock Builder") {
            ContentView()
                .frame(minWidth: 620, idealWidth: 660, minHeight: 640)
        }
    }
}

struct ContentView: View {
    @StateObject private var flasher = Flasher()
    @State private var showEraseConfirmation = false
    @State private var showLog = false

    private var version: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "0.1"
    }

    var body: some View {
        VStack(spacing: 0) {
            header
            Divider()
            ScrollView {
                VStack(alignment: .leading, spacing: 22) {
                    deviceSection
                    firmwareSection
                    flashSection
                    if showLog { logSection }
                }
                .padding(22)
            }
            Divider()
            footer
        }
        .onAppear {
            flasher.refreshPorts()
            flasher.refreshImages()
        }
        .alert("Erase everything on this ESP32?", isPresented: $showEraseConfirmation) {
            Button("Cancel", role: .cancel) { }
            Button("Erase and Flash", role: .destructive) { flasher.flash() }
        } message: {
            Text("""
            This wipes the whole chip before writing, including saved settings, \
            WiFi credentials and cached weather. The clock will come up asking to \
            be set up again.

            Choose Update instead to keep them.
            """)
        }
    }

    // MARK: - Header

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            Text("Clock Builder")
                .font(.system(size: 20, weight: .semibold))
            Text("v\(version)")
                .font(.system(size: 12, design: .monospaced))
                .foregroundColor(.secondary)
            Spacer()
        }
        .padding(.horizontal, 22)
        .padding(.vertical, 14)
    }

    // MARK: - 1. Device

    private var deviceSection: some View {
        Section(title: "1", heading: "ESP32") {
            HStack(spacing: 8) {
                Picker("", selection: $flasher.selectedPort) {
                    if flasher.ports.isEmpty {
                        Text("No ESP32 detected").tag(SerialPort?.none)
                    }
                    ForEach(flasher.ports) { port in
                        Text(port.displayName).tag(SerialPort?.some(port))
                    }
                }
                .labelsHidden()
                .disabled(flasher.isBusy)

                Button {
                    flasher.refreshPorts()
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .help("Rescan for connected devices")
                .disabled(flasher.isBusy)

                Button("Identify") { flasher.identify() }
                    .disabled(flasher.selectedPort == nil || flasher.isBusy)
            }

            if flasher.ports.isEmpty {
                Hint(icon: "cable.connector", tone: .neutral, text: """
                Plug the clock in over USB. If it still does not appear, the cable \
                may be charge-only — a surprising number are.
                """)
            } else if flasher.stage == .identifying {
                Hint(icon: "hourglass", tone: .neutral, text: "Reading chip identity…")
            } else if let problem = flasher.deviceProblem {
                Hint(icon: "xmark.octagon.fill", tone: .bad, text: problem)
            } else if let device = flasher.device {
                Hint(icon: "checkmark.seal.fill", tone: .good, text: device.summary)
            } else {
                Hint(icon: "questionmark.circle", tone: .neutral,
                     text: "Press Identify to confirm the right module is connected.")
            }
        }
    }

    // MARK: - 2. Firmware

    private var firmwareSection: some View {
        Section(title: "2", heading: "Firmware") {
            HStack(spacing: 8) {
                Picker("", selection: $flasher.selectedImageURL) {
                    if flasher.images.isEmpty {
                        Text("No .bin files found").tag(URL?.none)
                    }
                    ForEach(flasher.images, id: \.url) { image in
                        Text(image.name).tag(URL?.some(image.url))
                    }
                }
                .labelsHidden()
                .disabled(flasher.isBusy)

                Button {
                    flasher.refreshImages()
                } label: {
                    Image(systemName: "arrow.clockwise")
                }
                .help("Rescan the firmware folder")
                .disabled(flasher.isBusy)

                Button("Browse…") { chooseFolder() }
                    .disabled(flasher.isBusy)
            }

            Text(flasher.binFolder)
                .font(.system(size: 10, design: .monospaced))
                .foregroundColor(.secondary)
                .lineLimit(1)
                .truncationMode(.head)

            if let image = flasher.selectedImage {
                if let problem = image.problem {
                    Hint(icon: "xmark.octagon.fill", tone: .bad, text: problem)
                } else {
                    Hint(icon: "checkmark.seal.fill", tone: .good, text: image.verdict)
                }
            } else if flasher.images.isEmpty {
                Hint(icon: "folder", tone: .neutral,
                     text: "No .bin files in that folder. Use Browse to pick another.")
            }
        }
    }

    // MARK: - 3. Flash

    private var flashSection: some View {
        Section(title: "3", heading: "Write") {
            Picker("", selection: $flasher.eraseEverything) {
                Text("Update — keep settings and WiFi").tag(false)
                Text("Erase everything, then flash").tag(true)
            }
            .pickerStyle(.radioGroup)
            .labelsHidden()
            .disabled(flasher.isBusy)

            HStack(spacing: 12) {
                if flasher.stage == .flashing {
                    Button("Cancel") { flasher.cancel() }
                } else {
                    Button {
                        if flasher.eraseEverything { showEraseConfirmation = true }
                        else { flasher.flash() }
                    } label: {
                        Text(flasher.eraseEverything ? "Erase and Flash" : "Flash Clock")
                            .frame(maxWidth: .infinity)
                    }
                    .keyboardShortcut(.defaultAction)
                    .disabled(!flasher.canFlash)
                }

                Picker("Speed", selection: $flasher.baud) {
                    ForEach(flasher.baudChoices, id: \.self) { rate in
                        Text("\(rate)").tag(rate)
                    }
                }
                .frame(width: 150)
                .disabled(flasher.isBusy)
            }

            if flasher.stage == .flashing || flasher.progress > 0 {
                ProgressView(value: flasher.progress)
            }

            switch flasher.stage {
            case .done(let message):
                Hint(icon: "checkmark.circle.fill", tone: .good, text: message)
            case .failed(let message):
                Hint(icon: "exclamationmark.triangle.fill", tone: .bad, text: message)
            default:
                if !flasher.canFlash && !flasher.isBusy {
                    Hint(icon: "info.circle", tone: .neutral,
                         text: "Identify the ESP32 and choose a full image to enable flashing.")
                }
            }
        }
    }

    private var logSection: some View {
        Section(title: "·", heading: "Log") {
            ScrollView {
                Text(flasher.log.isEmpty ? "Nothing yet." : flasher.log)
                    .font(.system(size: 10, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
                    .padding(8)
            }
            .frame(height: 180)
            .background(Color(nsColor: .textBackgroundColor))
            .cornerRadius(6)
        }
    }

    private var footer: some View {
        HStack {
            Toggle("Show log", isOn: $showLog)
                .toggleStyle(.checkbox)
            Spacer()
            Text("Writes merged images at 0x0")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding(.horizontal, 22)
        .padding(.vertical, 10)
    }

    private func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: flasher.binFolder)
        if panel.runModal() == .OK, let url = panel.url {
            flasher.binFolder = url.path
            flasher.refreshImages()
        }
    }
}

// MARK: - Small building blocks

private struct Section<Content: View>: View {
    let title: String
    let heading: String
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Text(title)
                    .font(.system(size: 11, weight: .bold, design: .rounded))
                    .foregroundColor(.secondary)
                    .frame(width: 16, height: 16)
                    .background(Circle().fill(Color.secondary.opacity(0.15)))
                Text(heading)
                    .font(.system(size: 13, weight: .semibold))
            }
            content
        }
    }
}

private enum Tone { case good, bad, neutral }

private struct Hint: View {
    let icon: String
    let tone: Tone
    let text: String

    private var color: Color {
        switch tone {
        case .good: return .green
        case .bad: return .red
        case .neutral: return .secondary
        }
    }

    var body: some View {
        HStack(alignment: .top, spacing: 7) {
            Image(systemName: icon)
                .foregroundColor(color)
                .font(.system(size: 11))
                .frame(width: 14)
            Text(text)
                .font(.system(size: 11))
                .foregroundColor(tone == .neutral ? .secondary : .primary)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(9)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(color.opacity(tone == .neutral ? 0.06 : 0.10))
        .cornerRadius(6)
    }
}
