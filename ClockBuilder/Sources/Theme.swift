import SwiftUI

/// Shared with the clock web UI: deep graphite surfaces, cool cyan data and a
/// lime action colour. The extra depth and restrained glow make this feel like
/// a native control desk rather than a form in a dark window.
enum Theme {
    static let bg          = Color(hex: 0x080A0F)
    static let panel       = Color(hex: 0x10141B)
    static let panelRaised = Color(hex: 0x151A23)
    static let well        = Color(hex: 0x0B0E14)
    static let line        = Color.white.opacity(0.085)
    static let strongLine  = Color.white.opacity(0.15)
    static let muted       = Color(hex: 0x8D96A8)
    static let text        = Color(hex: 0xF4F6F8)
    static let accent      = Color(hex: 0xD9FF62)
    static let accentInk   = Color(hex: 0x141A08)
    static let cyan        = Color(hex: 0x2FD9F3)
    static let danger      = Color(hex: 0xFF6B72)
    static let good        = Color(hex: 0x8EE7A8)
    static let warning     = Color(hex: 0xFFB547)
    static let radius: CGFloat = 18

    static var backdrop: some View {
        ZStack {
            bg
            MatrixGrid()
                .opacity(0.38)
            RadialGradient(
                gradient: Gradient(colors: [accent.opacity(0.12), .clear]),
                center: UnitPoint(x: 0.77, y: -0.10),
                startRadius: 0,
                endRadius: 560
            )
            RadialGradient(
                gradient: Gradient(colors: [cyan.opacity(0.075), .clear]),
                center: UnitPoint(x: -0.08, y: 0.68),
                startRadius: 0,
                endRadius: 440
            )
        }
        .ignoresSafeArea()
    }
}

extension Color {
    init(hex: UInt32) {
        self.init(
            .sRGB,
            red: Double((hex >> 16) & 0xFF) / 255,
            green: Double((hex >> 8) & 0xFF) / 255,
            blue: Double(hex & 0xFF) / 255,
            opacity: 1
        )
    }
}

/// A very faint 32-point grid, echoing the LED preview on the settings page.
private struct MatrixGrid: View {
    var body: some View {
        GeometryReader { geometry in
            Path { path in
                for x in stride(from: 0.0, through: geometry.size.width, by: 32.0) {
                    path.move(to: CGPoint(x: x, y: 0))
                    path.addLine(to: CGPoint(x: x, y: geometry.size.height))
                }
                for y in stride(from: 0.0, through: geometry.size.height, by: 32.0) {
                    path.move(to: CGPoint(x: 0, y: y))
                    path.addLine(to: CGPoint(x: geometry.size.width, y: y))
                }
            }
            .stroke(Color.white.opacity(0.035), lineWidth: 1)
        }
    }
}

/// A raised workflow panel. The illuminated step marker keeps the three-stage
/// process obvious without making the interface feel like a wizard.
struct Card<Content: View>: View {
    let step: String
    let heading: String
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                Text(step)
                    .font(.system(size: 10, weight: .heavy, design: .rounded))
                    .foregroundColor(Theme.accentInk)
                    .frame(width: 22, height: 22)
                    .background(
                        RoundedRectangle(cornerRadius: 7, style: .continuous)
                            .fill(Theme.accent)
                            .shadow(color: Theme.accent.opacity(0.38), radius: 8)
                    )
                Text(heading.uppercased())
                    .font(.system(size: 12, weight: .bold, design: .rounded))
                    .tracking(1.25)
                    .foregroundColor(Theme.text)
                Rectangle()
                    .fill(Theme.line)
                    .frame(height: 1)
            }
            content
        }
        .padding(17)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: Theme.radius, style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [Theme.panelRaised, Theme.panel],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    )
                )
                .shadow(color: Color.black.opacity(0.34), radius: 18, y: 10)
        )
        .overlay(
            RoundedRectangle(cornerRadius: Theme.radius, style: .continuous)
                .stroke(
                    LinearGradient(
                        colors: [Color.white.opacity(0.15), Color.white.opacity(0.035)],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing
                    ),
                    lineWidth: 1
                )
        )
    }
}

struct AccentButtonStyle: ButtonStyle {
    var destructive = false
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        let base = destructive ? Theme.danger : Theme.accent
        return configuration.label
            .font(.system(size: 13, weight: .bold))
            .foregroundColor(destructive ? .white : Theme.accentInk)
            .padding(.vertical, 10)
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 11, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [base, base.opacity(0.78)],
                            startPoint: .top,
                            endPoint: .bottom
                        )
                    )
                    .shadow(
                        color: isEnabled ? base.opacity(configuration.isPressed ? 0.18 : 0.38) : .clear,
                        radius: configuration.isPressed ? 5 : 12,
                        y: 3
                    )
            )
            .overlay(
                RoundedRectangle(cornerRadius: 11, style: .continuous)
                    .stroke(Color.white.opacity(0.23), lineWidth: 1)
            )
            .scaleEffect(configuration.isPressed ? 0.985 : 1)
            .opacity(isEnabled ? 1 : 0.30)
            .animation(.easeOut(duration: 0.12), value: configuration.isPressed)
            .contentShape(Rectangle())
    }
}

struct QuietButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 12, weight: .semibold))
            .foregroundColor(Theme.text)
            .padding(.vertical, 8)
            .padding(.horizontal, 12)
            .background(
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .fill(configuration.isPressed ? Theme.panelRaised : Theme.well)
                    .shadow(color: Theme.cyan.opacity(isEnabled ? 0.10 : 0), radius: 7)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .stroke(configuration.isPressed ? Theme.cyan.opacity(0.38) : Theme.line,
                            lineWidth: 1)
            )
            .opacity(isEnabled ? 1 : 0.34)
            .contentShape(Rectangle())
    }
}

enum Tone { case good, bad, neutral }

struct Hint: View {
    let icon: String
    let tone: Tone
    let text: String

    private var color: Color {
        switch tone {
        case .good: return Theme.good
        case .bad: return Theme.danger
        case .neutral: return Theme.cyan
        }
    }

    var body: some View {
        HStack(alignment: .top, spacing: 9) {
            Image(systemName: icon)
                .foregroundColor(color)
                .font(.system(size: 11, weight: .bold))
                .frame(width: 15)
            Text(text)
                .font(.system(size: 11))
                .foregroundColor(tone == .neutral ? Theme.muted : Theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(11)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(color.opacity(tone == .neutral ? 0.055 : 0.10))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .stroke(color.opacity(0.16), lineWidth: 1)
        )
    }
}

struct AccentProgressBar: View {
    let value: Double

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .leading) {
                Capsule().fill(Theme.well)
                Capsule()
                    .fill(
                        LinearGradient(
                            colors: [Theme.cyan, Theme.accent],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                    .frame(width: max(0, min(1, value)) * geometry.size.width)
                    .shadow(color: Theme.accent.opacity(0.45), radius: 8)
            }
        }
        .frame(height: 9)
    }
}

struct StatusChip: View {
    let icon: String
    let label: String
    let value: String
    let active: Bool

    var body: some View {
        HStack(spacing: 9) {
            Image(systemName: icon)
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(active ? Theme.accent : Theme.muted)
                .frame(width: 25, height: 25)
                .background(Circle().fill((active ? Theme.accent : Theme.muted).opacity(0.10)))
            VStack(alignment: .leading, spacing: 1) {
                Text(label.uppercased())
                    .font(.system(size: 8, weight: .bold))
                    .tracking(1.0)
                    .foregroundColor(Theme.muted)
                Text(value)
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundColor(Theme.text)
                    .lineLimit(1)
            }
        }
        .padding(.vertical, 8)
        .padding(.horizontal, 10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 11, style: .continuous).fill(Theme.well))
        .overlay(RoundedRectangle(cornerRadius: 11, style: .continuous).stroke(Theme.line))
    }
}
