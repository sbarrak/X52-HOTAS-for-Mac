// Draws the app icon (Dock, Command+Tab, Finder) into an .iconset folder; iconutil turns it into AppIcon.icns.
//   swift tools/make_icon.swift build/AppIcon.iconset && iconutil -c icns build/AppIcon.iconset -o build/AppIcon.icns
import AppKit

let out = CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "AppIcon.iconset"
try? FileManager.default.createDirectory(atPath: out, withIntermediateDirectories: true)

func draw(_ px: Int) -> Data {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8,
                               samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                               bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    let s = CGFloat(px)
    // The usual macOS icon shape: a rounded square inset from the canvas edge.
    let box = NSRect(x: s * 0.1, y: s * 0.1, width: s * 0.8, height: s * 0.8)
    let shape = NSBezierPath(roundedRect: box, xRadius: s * 0.18, yRadius: s * 0.18)
    NSGradient(starting: NSColor(red: 0.16, green: 0.20, blue: 0.27, alpha: 1),
               ending: NSColor(red: 0.04, green: 0.05, blue: 0.08, alpha: 1))!.draw(in: shape, angle: -90)
    // The stick's green screen along the bottom.
    NSColor(red: 0.4, green: 0.95, blue: 0.5, alpha: 0.9).setFill()
    NSBezierPath(roundedRect: NSRect(x: s * 0.3, y: s * 0.2, width: s * 0.4, height: s * 0.05), xRadius: s * 0.02, yRadius: s * 0.02).fill()
    // A flight stick: base, shaft and grip with a red fire button.
    NSColor(white: 0.82, alpha: 1).setFill()
    NSBezierPath(roundedRect: NSRect(x: s * 0.3, y: s * 0.3, width: s * 0.4, height: s * 0.09), xRadius: s * 0.04, yRadius: s * 0.04).fill()
    NSColor(white: 0.6, alpha: 1).setFill()
    NSBezierPath(rect: NSRect(x: s * 0.475, y: s * 0.37, width: s * 0.05, height: s * 0.16)).fill()
    NSColor.white.setFill()
    NSBezierPath(roundedRect: NSRect(x: s * 0.41, y: s * 0.5, width: s * 0.18, height: s * 0.27), xRadius: s * 0.08, yRadius: s * 0.08).fill()
    NSColor(red: 0.95, green: 0.25, blue: 0.2, alpha: 1).setFill()
    NSBezierPath(ovalIn: NSRect(x: s * 0.465, y: s * 0.67, width: s * 0.07, height: s * 0.07)).fill()
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

for base in [16, 32, 128, 256, 512] {
    try! draw(base).write(to: URL(fileURLWithPath: "\(out)/icon_\(base)x\(base).png"))
    try! draw(base * 2).write(to: URL(fileURLWithPath: "\(out)/icon_\(base)x\(base)@2x.png"))
}
