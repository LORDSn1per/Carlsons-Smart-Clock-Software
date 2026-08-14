import SwiftUI
import AppKit
import UniformTypeIdentifiers

@main
struct ClockBuilderApp: App {
    var body: some Scene {
        WindowGroup("Clock Builder") {
            ContentView()
                .frame(minWidth: 760, idealWidth: 880, minHeight: 720)
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
                    VStack(spacing: 16) {
                        hero
                        deviceCard
                        firmwareCard
                        writeCard
                        if showLog { logCard }
                    }
                    .padding(.horizontal, 24)
                    .padding(.bottom, 22)
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
        .onChange(of: flasher.selectedPort) { _ in
            flasher.device = nil
            flasher.deviceProblem = nil
        }
        .onChange(of: flasher.selectedImageURL) { _ in
            flasher.revalidateAgainstDevice()
        }
        .onChange(of: flasher.sourceMode) { mode in
            if mode == .folder { flasher.refreshImages() }
        }
    }

    // MARK: - Header / footer

    private var header: some View {
        HStack(spacing: 12) {
            brandMark
            VStack(alignment: .leading, spacing: 1) {
                Text("CARLSON SMART CLOCK")
                    .font(.system(size: 9, weight: .bold, design: .rounded))
                    .tracking(2.1)
                    .foregroundColor(Theme.muted)
                Text("Clock Builder")
                    .font(.system(size: 19, weight: .semibold))
                    .foregroundColor(Theme.text)
            }
            Text("v\(version)")
                .font(.system(size: 10, weight: .bold, design: .monospaced))
                .foregroundColor(Theme.accent)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(Capsule().fill(Theme.accent.opacity(0.10)))
                .overlay(Capsule().stroke(Theme.accent.opacity(0.18)))
            Spacer()
            HStack(spacing: 7) {
                Circle()
                    .fill(flasher.ports.isEmpty ? Theme.muted : Theme.good)
                    .frame(width: 6, height: 6)
                    .shadow(color: flasher.ports.isEmpty ? .clear : Theme.good.opacity(0.7), radius: 5)
                Text(flasher.ports.isEmpty ? "USB WAITING" : "USB ONLINE")
                    .font(.system(size: 9, weight: .bold, design: .monospaced))
                    .tracking(0.8)
                    .foregroundColor(flasher.ports.isEmpty ? Theme.muted : Theme.good)
            }
        }
        .padding(.horizontal, 24)
        .padding(.top, 17)
        .padding(.bottom, 15)
        .background(Theme.bg.opacity(0.64))
        .overlay(Rectangle().fill(Theme.line).frame(height: 1), alignment: .bottom)
    }

    private var brandMark: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(Theme.panelRaised)
                .frame(width: 38, height: 38)
                .overlay(RoundedRectangle(cornerRadius: 10, style: .continuous)
                    .stroke(Theme.accent.opacity(0.32)))
                .shadow(color: Theme.accent.opacity(0.18), radius: 10)
            HStack(spacing: 2) {
                ForEach(0..<4) { index in
                    Capsule()
                        .fill(index == 3 ? Theme.accent : Theme.cyan)
                        .frame(width: 3, height: CGFloat(8 + index * 4))
                }
            }
        }
    }

    private var footer: some View {
        HStack(spacing: 12) {
            Toggle("Show diagnostic log", isOn: $showLog)
                .toggleStyle(.checkbox)
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(Theme.muted)
            Spacer()
            Image(systemName: "lock.shield.fill")
                .font(.system(size: 10))
                .foregroundColor(Theme.accent)
            Text("Full images only · protected write at 0x0")
                .font(.system(size: 10, weight: .medium))
                .foregroundColor(Theme.muted)
        }
        .padding(.horizontal, 24)
        .padding(.vertical, 12)
        .background(Theme.panel.opacity(0.82))
        .overlay(Rectangle().fill(Theme.line).frame(height: 1), alignment: .top)
    }

    // MARK: - Hero

    private var hero: some View {
        VStack(spacing: 15) {
            HStack(spacing: 22) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("USB FIRMWARE STUDIO")
                        .font(.system(size: 10, weight: .bold, design: .rounded))
                        .tracking(2.0)
                        .foregroundColor(Theme.accent)
                    Text("A safer route from firmware to clock.")
                        .font(.system(size: 25, weight: .semibold, design: .rounded))
                        .foregroundColor(Theme.text)
                    Text("Identify the hardware, validate a complete image, then flash with live progress — without touching saved settings unless you choose to erase them.")
                        .font(.system(size: 12))
                        .foregroundColor(Theme.muted)
                        .fixedSize(horizontal: false, vertical: true)
                        .lineSpacing(2)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                controlDisplay
                    .frame(width: 260)
            }

            HStack(spacing: 10) {
                StatusChip(
                    icon: "cable.connector",
                    label: "Device",
                    value: flasher.selectedPort?.displayName ?? "Not connected",
                    active: flasher.selectedPort != nil
                )
                StatusChip(
                    icon: "shippingbox.fill",
                    label: "Firmware",
                    value: flasher.selectedImage?.name ?? "No image",
                    active: flasher.selectedImage?.isFlashable == true
                )
                StatusChip(
                    icon: flasher.eraseEverything ? "exclamationmark.triangle.fill" : "shield.checkered",
                    label: "Write mode",
                    value: flasher.eraseEverything ? "Erase + flash" : "Keep settings",
                    active: !flasher.eraseEverything
                )
            }
        }
        .padding(19)
        .background(
            RoundedRectangle(cornerRadius: 22, style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [Theme.panelRaised.opacity(0.96), Theme.panel.opacity(0.92)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .shadow(color: Color.black.opacity(0.42), radius: 24, y: 12)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 22, style: .continuous)
                .stroke(
                    LinearGradient(
                        colors: [Theme.accent.opacity(0.28), Theme.cyan.opacity(0.08), Theme.line],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
        )
    }

    private var controlDisplay: some View {
        VStack(alignment: .leading, spacing: 11) {
            HStack {
                Text("LIVE STATUS")
                    .font(.system(size: 8, weight: .bold, design: .monospaced))
                    .tracking(1.4)
                    .foregroundColor(Theme.muted)
                Spacer()
                Circle()
                    .fill(displayColor)
                    .frame(width: 6, height: 6)
                    .shadow(color: displayColor.opacity(0.8), radius: 6)
            }
            Text(displayTitle)
                .font(.system(size: 17, weight: .bold, design: .monospaced))
                .tracking(1.2)
                .foregroundColor(displayColor)
                .shadow(color: displayColor.opacity(0.36), radius: 7)
                .lineLimit(1)
            Text(displayDetail)
                .font(.system(size: 9, weight: .medium, design: .monospaced))
                .foregroundColor(Theme.muted)
                .lineLimit(2)
            if flasher.stage == .flashing {
                AccentProgressBar(value: flasher.progress)
            } else {
                HStack(spacing: 3) {
                    ForEach(0..<16) { index in
                        RoundedRectangle(cornerRadius: 1)
                            .fill(index < displayBars ? displayColor : Theme.panelRaised)
                            .frame(height: 4)
                    }
                }
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 13, style: .continuous)
                .fill(Color.black.opacity(0.64))
                .shadow(color: displayColor.opacity(0.10), radius: 14)
        )
        .overlay(
            RoundedRectangle(cornerRadius: 13, style: .continuous)
                .stroke(displayColor.opacity(0.22), lineWidth: 1)
        )
    }

    private var displayTitle: String {
        switch flasher.stage {
        case .identifying: return "READING CHIP"
        case .flashing: return "WRITING  \(Int(flasher.progress * 100))%"
        case .done: return "FLASH COMPLETE"
        case .failed: return "ATTENTION"
        case .idle:
            if flasher.device != nil { return "READY TO FLASH" }
            return flasher.ports.isEmpty ? "AWAITING USB" : "DEVICE FOUND"
        }
    }

    private var displayDetail: String {
        switch flasher.stage {
        case .identifying: return "Checking ESP32 family and flash capacity"
        case .flashing: return flasher.transferDescription
        case .done: return "The clock is restarting"
        case .failed: return "Review the message and diagnostic log"
        case .idle:
            if let device = flasher.device { return device.summary }
            return flasher.ports.isEmpty ? "Connect the clock with a data-capable USB cable" : "Identify the connected hardware to continue"
        }
    }

    private var displayColor: Color {
        switch flasher.stage {
        case .failed: return Theme.danger
        case .identifying: return Theme.cyan
        case .flashing, .done: return Theme.accent
        case .idle: return flasher.device == nil ? Theme.cyan : Theme.good
        }
    }

    private var displayBars: Int {
        if flasher.device != nil { return 16 }
        if flasher.selectedPort != nil { return 10 }
        return 4
    }

    // MARK: - 1. Device

    private var deviceCard: some View {
        Card(step: "1", heading: "Connect & identify") {
            HStack(spacing: 9) {
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
                    Image(systemName: "arrow.clockwise")
                        .font(.system(size: 11, weight: .bold))
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
                Hint(icon: "scope", tone: .neutral,
                     text: "Identify confirms the ESP32 family and available flash before any write is allowed.")
            }
        }
    }

    // MARK: - 2. Firmware

    private var firmwareCard: some View {
        Card(step: "2", heading: "Choose firmware") {
            Picker("", selection: $flasher.sourceMode) {
                ForEach(Flasher.SourceMode.allCases) { mode in
                    Text(mode.rawValue).tag(mode)
                }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            .disabled(flasher.isBusy)

            HStack(spacing: 9) {
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
                        Image(systemName: "arrow.clockwise")
                            .font(.system(size: 11, weight: .bold))
                    }
                    .buttonStyle(QuietButtonStyle())
                    .help("Rescan the folder")
                    .disabled(flasher.isBusy)

                    Button("Choose Folder…") { chooseFolder() }
                        .buttonStyle(QuietButtonStyle())
                        .disabled(flasher.isBusy)
                } else {
                    Text(flasher.selectedImage?.name ?? "No file chosen")
                        .font(.system(size: 12, weight: .medium))
                        .foregroundColor(flasher.selectedImage == nil ? Theme.muted : Theme.text)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.vertical, 8)
                        .padding(.horizontal, 11)
                        .background(RoundedRectangle(cornerRadius: 9, style: .continuous).fill(Theme.well))
                        .overlay(RoundedRectangle(cornerRadius: 9, style: .continuous).stroke(Theme.line))

                    Button("Choose File…") { chooseFile() }
                        .buttonStyle(QuietButtonStyle())
                        .disabled(flasher.isBusy)
                }
            }

            Text(flasher.sourceMode == .folder
                 ? flasher.binFolder
                 : (flasher.selectedImage?.url.deletingLastPathComponent().path ?? ""))
                .font(.system(size: 9, design: .monospaced))
                .foregroundColor(Theme.muted.opacity(0.72))
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
        Card(step: "3", heading: "Flash clock") {
            Picker("", selection: $flasher.eraseEverything) {
                Text("Update — keep settings and WiFi").tag(false)
                Text("Erase everything, then flash").tag(true)
            }
            .pickerStyle(.radioGroup)
            .labelsHidden()
            .disabled(flasher.isBusy)

            HStack(spacing: 11) {
                if flasher.stage == .flashing {
                    Button("Cancel write") { flasher.cancel() }
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

                VStack(alignment: .leading, spacing: 3) {
                    Text("SPEED")
                        .font(.system(size: 8, weight: .bold))
                        .tracking(1)
                        .foregroundColor(Theme.muted)
                    Picker("", selection: $flasher.baud) {
                        ForEach(flasher.baudChoices, id: \.self) { rate in
                            Text("\(rate)").tag(rate)
                        }
                    }
                    .labelsHidden()
                    .frame(width: 125)
                    .disabled(flasher.isBusy)
                }
            }

            if flasher.stage == .flashing || flasher.progress > 0 {
                VStack(spacing: 8) {
                    AccentProgressBar(value: flasher.progress)
                    HStack {
                        Text("\(Int(flasher.progress * 100))%")
                            .font(.system(size: 12, weight: .bold, design: .monospaced))
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
                    Hint(icon: "lock.shield", tone: .neutral,
                         text: "Identify the ESP32 and choose a validated full image to unlock flashing.")
                }
            }
        }
    }

    private var logCard: some View {
        Card(step: "·", heading: "Diagnostic log") {
            ScrollView {
                Text(flasher.log.isEmpty ? "Nothing yet." : flasher.log)
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundColor(Theme.muted)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .textSelection(.enabled)
                    .padding(11)
            }
            .frame(height: 180)
            .background(RoundedRectangle(cornerRadius: 10, style: .continuous).fill(Color.black.opacity(0.48)))
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
