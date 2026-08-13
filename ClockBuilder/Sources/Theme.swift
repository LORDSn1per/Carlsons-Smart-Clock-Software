import SwiftUI

/// Lifted from web/index.html's :root block so the app and the clock's own
/// settings page read as the same product rather than two different tools.
enum Theme {
    static let bg       = Color(hex: 0x090B10)
    static let panel    = Color(hex: 0x11141B)
    static let panel2   = Color(hex: 0x171B24)
    static let line     = Color.white.opacity(0.09)
    static let muted    = Color(hex: 0x9097A8)
    static let text     = Color(hex: 0xF5F6F8)
    static let accent   = Color(hex: 0xD9FF62)
    static let accentInk = Color(hex: 0x171B0D)
    static let danger   = Color(hex: 0xFF6B6B)
    static let good     = Color(hex: 0x8EE7A8)
    static let radius: CGFloat = 18

    /// The page paints a lime glow off the top-right corner; same idea here.
    static var backdrop: some View {
        ZStack {
            bg
            RadialGradient(
                gradient: Gradient(colors: [accent.opacity(0.09), .clear]),
                center: UnitPoint(x: 0.74, y: -0.08),
                startRadius: 0,
                endRadius: 520
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

/// A rounded panel with a hairline border, matching the page's cards.
struct Card<Content: View>: View {
    let step: String
    let heading: String
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 9) {
                Text(step)
                    .font(.system(size: 10, weight: .bold, design: .rounded))
                    .foregroundColor(Theme.accentInk)
                    .frame(width: 18, height: 18)
                    .background(Circle().fill(Theme.accent))
                Text(heading)
                    .font(.system(size: 13, weight: .semibold))
                    .foregroundColor(Theme.text)
                Spacer()
            }
            content
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: Theme.radius, style: .continuous)
                .fill(Theme.panel)
        )
        .overlay(
            RoundedRectangle(cornerRadius: Theme.radius, style: .continuous)
                .stroke(Theme.line, lineWidth: 1)
        )
    }
}

/// Primary action: accent fill with a subtle vertical gradient, dark ink.
struct AccentButtonStyle: ButtonStyle {
    var destructive = false
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        let base = destructive ? Theme.danger : Theme.accent
        return configuration.label
            .font(.system(size: 13, weight: .semibold))
            .foregroundColor(destructive ? .white : Theme.accentInk)
            .padding(.vertical, 9)
            .frame(maxWidth: .infinity)
            .background(
                RoundedRectangle(cornerRadius: 11, style: .continuous)
                    .fill(
                        LinearGradient(
                            colors: [base, base.opacity(0.82)],
                            startPoint: .top,
                            endPoint: .bottom
                        )
                    )
            )
            .opacity(isEnabled ? (configuration.isPressed ? 0.78 : 1) : 0.32)
            .contentShape(Rectangle())
    }
}

/// Secondary action: quiet panel fill, used for Browse / Identify / Refresh.
struct QuietButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 12, weight: .medium))
            .foregroundColor(Theme.text)
            .padding(.vertical, 7)
            .padding(.horizontal, 12)
            .background(
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .fill(Theme.panel2)
            )
            .overlay(
                RoundedRectangle(cornerRadius: 9, style: .continuous)
                    .stroke(Theme.line, lineWidth: 1)
            )
            .opacity(isEnabled ? (configuration.isPressed ? 0.7 : 1) : 0.35)
            .contentShape(Rectangle())
    }
}

enum Tone { case good, bad, neutral }

/// Inline status line under each control.
struct Hint: View {
    let icon: String
    let tone: Tone
    let text: String

    private var color: Color {
        switch tone {
        case .good: return Theme.good
        case .bad: return Theme.danger
        case .neutral: return Theme.muted
        }
    }

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            Image(systemName: icon)
                .foregroundColor(color)
                .font(.system(size: 11, weight: .semibold))
                .frame(width: 14)
            Text(text)
                .font(.system(size: 11))
                .foregroundColor(tone == .neutral ? Theme.muted : Theme.text)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 10, style: .continuous)
                .fill(color.opacity(tone == .neutral ? 0.05 : 0.11))
        )
    }
}

/// Progress bar drawn by hand so it can carry the accent gradient.
struct AccentProgressBar: View {
    let value: Double

    var body: some View {
        GeometryReader { geometry in
            ZStack(alignment: .leading) {
                Capsule().fill(Theme.panel2)
                Capsule()
                    .fill(
                        LinearGradient(
                            colors: [Theme.accent.opacity(0.75), Theme.accent],
                            startPoint: .leading,
                            endPoint: .trailing
                        )
                    )
                    .frame(width: max(0, min(1, value)) * geometry.size.width)
            }
        }
        .frame(height: 8)
    }
}
