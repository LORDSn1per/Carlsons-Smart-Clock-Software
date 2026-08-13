import SwiftUI
import AppKit
import UniformTypeIdentifiers

@main
struct ClockBuilderApp: App {
    var body: some Scene {
        WindowGroup("Clock Builder") {
            ContentView()
                .frame(minWidth: 640, idealWidth: 680, minHeight: 700)
                .preferredColorScheme(.dark)
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
        ZStack {
            Theme.backdrop
            VStack(spacing: 0) {
                header
                ScrollView {
                    VStack(spacing: 14) {
                        deviceCard
                        firmwareCard
                        writeCard
                        if showLog { logCard }
                    }
                    .padding(.horizontal, 20)
                    .padding(.bottom, 18)
                }
                footer
            }
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
        .onAppear {
            flasher.refreshPorts()
            flasher.refreshImages()
        }
    }

    // MARK: - Header / footer

    private var header: some View {
        HStack(spacing: 10) {
            RoundedRectangle(cornerRadius: 7, style: .continuous)
                .fill(
                    LinearGradient(colors: [Theme.accent, Theme.accent.opacity(0.55)],
                                   startPoint: .topLeading, endPoint: .bottomTrailing)
                )
                .frame(width: 26, height: 26)
                .overlay(
                    Image(systemName: "bolt.fill")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(Theme.accentInk)
                )
            Text("Clock Builder")
                .font(.system(size: 19, weight: .semibold))
                .foregroundColor(Theme.text)
            Text(version)
                .font(.system(size: 11, weight: .medium, design: .monospaced))
                .foregroundColor(Theme.accent)
                .padding(.horizontal, 7)
                .padding(.vertical, 3)
                .background(Capsule().fill(Theme.accent.opacity(0.12)))
            Spacer()
        }
        .padding(.horizontal, 22)
        .padding(.top, 18)
        .padding(.bottom, 16)
    }

    private var footer: some View {
        HStack {
            Toggle("Show log", isOn: $showLog)
                .toggleStyle(.checkbox)
                .font(.system(size: 11))
                .foregroundColor(Theme.muted)
            Spacer()
            Text("Writes full images at 0x0")
                .font(.system(size: 10))
                .foregroundColor(Theme.muted.opacity(0.7))
        }
        .padding(.horizontal, 22)
        .padding(.vertical, 11)
        .background(Theme.panel.opacity(0.6))
        .overlay(Rectangle().fill(Theme.line).frame(height: 1), alignment: .top)
    }

    // MARK: - 1. Device

    private var deviceCard: some View {
        Card(step: "1", heading: "ESP32") {
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

                Button { flasher.refreshPorts() } label: {
                    Image(systemName: "arrow.clockwise").font(.system(size: 11, weight: .semibold))
                }
                .buttonStyle(QuietButtonStyle())
                .help("Rescan for connected devices")
                .disabled(flasher.isBusy)

                Button("Identify") { flasher.identify() }
                    .buttonStyle(QuietButtonStyle())
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

    private var firmwareCard: some View {
        Card(step: "2", heading: "Firmware") {
            Picker("", selection: $flasher.sourceMode) {
                ForEach(Flasher.SourceMode.allCases) { mode in
                    Text(mode.rawValue).tag(mode)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .disabled(flasher.isBusy)

            HStack(spacing: 8) {
                if flasher.sourceMode == .folder {
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

                    Button { flasher.refreshImages() } label: {
                        Image(systemName: "arrow.clockwise").font(.system(size: 11, weight: .semibold))
                    }
                    .buttonStyle(QuietButtonStyle())
                    .help("Rescan the folder")
                    .disabled(flasher.isBusy)

                    Button("Choose Folder…") { chooseFolder() }
                        .buttonStyle(QuietButtonStyle())
                        .disabled(flasher.isBusy)
                } else {
                    Text(flasher.selectedImage?.name ?? "No file chosen")
                        .font(.system(size: 12))
                        .foregroundColor(flasher.selectedImage == nil ? Theme.muted : Theme.text)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.vertical, 7)
                        .padding(.horizontal, 10)
                        .background(RoundedRectangle(cornerRadius: 9, style: .continuous)
                            .fill(Theme.panel2))
                        .overlay(RoundedRectangle(cornerRadius: 9, style: .continuous)
                            .stroke(Theme.line, lineWidth: 1))

                    Button("Choose File…") { chooseFile() }
                        .buttonStyle(QuietButtonStyle())
                        .disabled(flasher.isBusy)
                }
            }

            Text(flasher.sourceMode == .folder
                 ? flasher.binFolder
                 : (flasher.selectedImage?.url.deletingLastPathComponent().path ?? ""))
                .font(.system(size: 9, design: .monospaced))
                .foregroundColor(Theme.muted.opacity(0.75))
                .lineLimit(1)
                .truncationMode(.head)
                .frame(maxWidth: .infinity, alignment: .leading)

            if let image = flasher.selectedImage {
                if let problem = image.problem {
                    Hint(icon: "xmark.octagon.fill", tone: .bad, text: problem)
                } else {
                    Hint(icon: "checkmark.seal.fill", tone: .good, text: image.verdict)
                }
            } else if flasher.sourceMode == .folder && flasher.images.isEmpty {
                Hint(icon: "folder", tone: .neutral,
                     text: "No .bin files in that folder. Choose another.")
            } else if flasher.sourceMode == .file {
                Hint(icon: "doc", tone: .neutral, text: "Choose a .bin file to flash.")
            }
        }
    }

    // MARK: - 3. Write

    private var writeCard: some View {
        Card(step: "3", heading: "Write") {
            Picker("", selection: $flasher.eraseEverything) {
                Text("Update — keep settings and WiFi").tag(false)
                Text("Erase everything, then flash").tag(true)
            }
            .pickerStyle(.radioGroup)
            .labelsHidden()
            .disabled(flasher.isBusy)

            HStack(spacing: 10) {
                if flasher.stage == .flashing {
                    Button("Cancel") { flasher.cancel() }
                        .buttonStyle(AccentButtonStyle(destructive: true))
                } else {
                    Button(flasher.eraseEverything ? "Erase and Flash" : "Flash Clock") {
                        if flasher.eraseEverything { showEraseConfirmation = true }
                        else { flasher.flash() }
                    }
                    .buttonStyle(AccentButtonStyle(destructive: flasher.eraseEverything))
                    .keyboardShortcut(.defaultAction)
                    .disabled(!flasher.canFlash)
                }

                Picker("", selection: $flasher.baud) {
                    ForEach(flasher.baudChoices, id: \.self) { rate in
                        Text("\(rate)").tag(rate)
                    }
                }
                .labelsHidden()
                .frame(width: 110)
                .disabled(flasher.isBusy)
            }

            if flasher.stage == .flashing || flasher.progress > 0 {
                VStack(spacing: 7) {
                    AccentProgressBar(value: flasher.progress)
                    HStack {
                        Text("\(Int(flasher.progress * 100))%")
                            .font(.system(size: 12, weight: .semibold, design: .monospaced))
                            .foregroundColor(Theme.accent)
                        Spacer()
                        Text(flasher.transferDescription)
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundColor(Theme.muted)
                    }
                }
                .padding(.top, 2)
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

    private var logCard: some View {
        Card(step: "·", heading: "Log") {
            ScrollView {
                Text(flasher.log.isEmpty ? "Nothing yet." : flasher.log)
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundColor(Theme.muted)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
                    .padding(9)
            }
            .frame(height: 170)
            .background(RoundedRectangle(cornerRadius: 10, style: .continuous).fill(Theme.bg))
            .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous)
                .stroke(Theme.line, lineWidth: 1))
        }
    }

    // MARK: - Pickers

    private func chooseFile() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowsMultipleSelection = false
        // Only .bin - picking anything else here is always a mistake.
        if let binType = UTType(filenameExtension: "bin") {
            panel.allowedContentTypes = [binType]
        }
        panel.directoryURL = URL(fileURLWithPath: flasher.binFolder)
        panel.prompt = "Choose"
        if panel.runModal() == .OK, let url = panel.url {
            flasher.useSingleFile(url)
        }
    }

    private func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: flasher.binFolder)
        panel.prompt = "Choose"
        if panel.runModal() == .OK, let url = panel.url {
            flasher.binFolder = url.path
            flasher.sourceMode = .folder
            flasher.selectedImageURL = nil
            flasher.refreshImages()
        }
    }
}
