// Makes an A4 PDF from a plain text file: "# " is the title, "## " a heading, "- " a list item.
//   swift tools/make_text_pdf.swift input.txt output.pdf
import AppKit
import CoreText

let args = CommandLine.arguments
guard args.count == 3, let source = try? String(contentsOfFile: args[1], encoding: .utf8) else {
    FileHandle.standardError.write("usage: make_text_pdf.swift input.txt output.pdf\n".data(using: .utf8)!)
    exit(1)
}

let text = NSMutableAttributedString()
for raw in source.split(separator: "\n", omittingEmptySubsequences: true) {
    var line = String(raw)
    var font = NSFont.systemFont(ofSize: 10)
    let style = NSMutableParagraphStyle()
    style.paragraphSpacing = 4
    style.lineSpacing = 1
    if line.hasPrefix("## ") {
        line.removeFirst(3)
        font = NSFont.boldSystemFont(ofSize: 13)
        style.paragraphSpacingBefore = 4
    } else if line.hasPrefix("# ") {
        line.removeFirst(2)
        font = NSFont.boldSystemFont(ofSize: 20)
        style.paragraphSpacing = 14
    } else if line.hasPrefix("- ") {
        line = "\u{2022}\t" + line.dropFirst(2)
        style.tabStops = [NSTextTab(textAlignment: .left, location: 20)]
        style.firstLineHeadIndent = 8
        style.headIndent = 20
        style.paragraphSpacing = 1
    }
    text.append(NSAttributedString(string: line + "\n", attributes: [.font: font, .paragraphStyle: style, .foregroundColor: NSColor.black]))
}

var page = CGRect(x: 0, y: 0, width: 595, height: 842)
guard let pdf = CGContext(URL(fileURLWithPath: args[2]) as CFURL, mediaBox: &page, nil) else { exit(1) }
let setter = CTFramesetterCreateWithAttributedString(text)
var start = 0
while start < text.length {
    pdf.beginPDFPage(nil)
    let frame = CTFramesetterCreateFrame(setter, CFRange(location: start, length: 0),
                                         CGPath(rect: page.insetBy(dx: 56, dy: 56), transform: nil), nil)
    CTFrameDraw(frame, pdf)
    let shown = CTFrameGetVisibleStringRange(frame).length
    pdf.endPDFPage()
    if shown == 0 { exit(1) }
    start += shown
}
pdf.closePDF()
