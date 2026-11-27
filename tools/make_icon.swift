// Builds the app icon (Dock, Command+Tab, Finder) from the artwork in assets/AppIcon.png into an .iconset folder;
// iconutil turns it into AppIcon.icns.
//   swift tools/make_icon.swift assets/AppIcon.png build/AppIcon.iconset && iconutil -c icns build/AppIcon.iconset -o build/AppIcon.icns
// The artwork is a square picture; each size puts it in the usual macOS icon shape (a rounded square inset from the
// canvas edge, transparent around it), so the picture's own corners and background are cut away.
import AppKit

let args = CommandLine.arguments
let src = args.count > 1 ? args[1] : "assets/AppIcon.png"
let out = args.count > 2 ? args[2] : "AppIcon.iconset"
guard let art = NSImage(contentsOfFile: src) else { fatalError("cannot read \(src)") }
try? FileManager.default.createDirectory(atPath: out, withIntermediateDirectories: true)

func draw(_ px: Int) -> Data {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8,
                               samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB,
                               bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    let ctx = NSGraphicsContext(bitmapImageRep: rep)!
    ctx.imageInterpolation = .high
    NSGraphicsContext.current = ctx
    let s = CGFloat(px)
    let box = NSRect(x: s * 0.1, y: s * 0.1, width: s * 0.8, height: s * 0.8)
    NSBezierPath(roundedRect: box, xRadius: s * 0.18, yRadius: s * 0.18).addClip()
    art.draw(in: box, from: .zero, operation: .copy, fraction: 1)
    NSGraphicsContext.restoreGraphicsState()
    return rep.representation(using: .png, properties: [:])!
}

for base in [16, 32, 128, 256, 512] {
    try! draw(base).write(to: URL(fileURLWithPath: "\(out)/icon_\(base)x\(base).png"))
    try! draw(base * 2).write(to: URL(fileURLWithPath: "\(out)/icon_\(base)x\(base)@2x.png"))
}
