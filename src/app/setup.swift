// The setup window: finds X-Plane 12, asks what to install and installs it. The file work is in
// C++ (src/mac/setup.cpp), reached through bridge.h.
import AppKit
import SwiftUI

/// X52_SANDBOX=<folder> tries setup and uninstall out inside that folder: nothing outside it is looked
/// for or changed (X-Plane, home folder, preferences, login item, the app itself).
let sandboxDir: String? = ProcessInfo.processInfo.environment["X52_SANDBOX"]
var userHome: String { sandboxDir.map { $0 + "/home" } ?? NSHomeDirectory() }

func lines(_ p: UnsafePointer<CChar>?) -> [String] { str(p).split(separator: "\n").map(String.init) }

/// Started from the download itself: the disk image is read-only, or macOS runs a copy from a hidden folder.
var runningFromDownload: Bool {
    let url = Bundle.main.bundleURL
    if url.path.contains("/AppTranslocation/") { return true }
    return (try? url.resourceValues(forKeys: [.volumeIsReadOnlyKey]))?.volumeIsReadOnly ?? false
}

var xplaneIsRunning: Bool {
    NSWorkspace.shared.runningApplications.contains { ($0.localizedName ?? "").hasPrefix("X-Plane") }
}

func openAccessibilitySettings() {
    if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility") {
        NSWorkspace.shared.open(url)
    }
}

/// A folder or file list with a heading; nothing when the list is empty.
struct PathList: View {
    let title: String
    let paths: [String]
    var color: Color = .secondary
    var body: some View {
        if !paths.isEmpty {
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.callout.bold())
                ForEach(paths, id: \.self) { p in
                    Text(p).font(.caption).foregroundStyle(color).textSelection(.enabled)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }
        }
    }
}

final class SetupState: ObservableObject {
    @Published var step = 0            // 0 X-Plane folder, 1 what to install, 2 result
    @Published var found: [String] = []
    @Published var folder = ""         // the X-Plane folder shown, or confirmed after step 0
    @Published var note = ""
    @Published var plugin = true
    @Published var profiles = true
    @Published var guides = true
    @Published var done: [String] = []
    @Published var kept: [String] = []
    @Published var failed: [String] = []

    func begin(current: String) {
        found = lines(x52_find_xplane())
        if !current.isEmpty && !found.contains(current) { found.insert(current, at: 0) }
        folder = current.isEmpty ? found.first ?? "" : current
        step = 0
        note = ""
        (plugin, profiles, guides) = (true, true, true)
        (done, kept, failed) = ([], [], [])
    }

    func chooseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.message = "Choose your X-Plane 12 folder"
        panel.prompt = "Choose"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        guard x52_valid_xplane(url.path) != 0 else {
            note = "That folder is not an X-Plane 12 install (it has no Resources/plugins)."
            return
        }
        note = ""
        if !found.contains(url.path) { found.append(url.path) }
        folder = url.path
    }

    /// Without an X-Plane folder only the guides can be installed.
    func guidesOnly() {
        folder = ""
        (plugin, profiles, guides) = (false, false, true)
        step = 1
    }

    func install() {
        x52_setup_install(folder, plugin ? 1 : 0, profiles ? 1 : 0, guides ? 1 : 0)
        done = lines(x52_setup_report(0))
        kept = lines(x52_setup_report(1))
        failed = lines(x52_setup_report(2))
        step = 2
    }
}

struct SetupView: View {
    @ObservedObject var m: Model
    @ObservedObject var s: SetupState
    let close: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            if runningFromDownload {
                fromDownload
            } else {
                switch s.step {
                case 0: folderStep
                case 1: choiceStep
                default: resultStep
                }
            }
        }
        .padding(20)
        .frame(width: 580, height: 480, alignment: .topLeading)
    }

    @ViewBuilder private var fromDownload: some View {
        Text("Drag X52 Pro to Applications first").font(.title2.bold())
        Text("You opened X52 Pro from the download. Quit it, drag X52 Pro onto the Applications folder in the download window, then open it from Applications. Setup then starts.")
            .fixedSize(horizontal: false, vertical: true)
        Spacer()
        HStack {
            Spacer()
            Button("Quit X52 Pro") { NSApp.terminate(nil) }.keyboardShortcut(.defaultAction)
        }
    }

    @ViewBuilder private var folderStep: some View {
        Text("Set up X52 Pro: your X-Plane 12 folder").font(.title2.bold())
        if s.found.isEmpty && s.folder.isEmpty {
            Text("X-Plane 12 was not found. Choose its folder (the one that holds Resources).")
                .fixedSize(horizontal: false, vertical: true)
        } else {
            Text(s.found.count > 1 ? "X-Plane 12 was found in these places. Pick the one to use." : "X-Plane 12 was found here:")
            Picker("", selection: $s.folder) {
                ForEach(s.found, id: \.self) { Text($0).tag($0) }
            }
            .pickerStyle(.radioGroup).labelsHidden()
        }
        if !s.note.isEmpty { Text(s.note).foregroundStyle(.orange) }
        Spacer()
        HStack {
            Button("Not now", action: close)
            Spacer()
            if s.folder.isEmpty {
                Button("Install the guides only") { s.guidesOnly() }
                Button("Choose folder...") { s.chooseFolder() }.keyboardShortcut(.defaultAction)
            } else {
                Button("Choose another...") { s.chooseFolder() }
                Button("Use this folder") {
                    m.useXPlane(s.folder)
                    s.step = 1
                }
                .keyboardShortcut(.defaultAction)
            }
        }
    }

    @ViewBuilder private var choiceStep: some View {
        Text("What to install").font(.title2.bold())
        Text(s.folder.isEmpty ? "No X-Plane folder: only the guides can be installed." : "Into \(s.folder)")
            .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        Toggle("The X-Plane plugin", isOn: $s.plugin).disabled(s.folder.isEmpty)
        Toggle("\(x52_setup_profile_count()) ready-made aircraft profiles", isOn: $s.profiles).disabled(s.folder.isEmpty)
        Toggle("Guides: the manual and the button maps, in Documents/X52 Pro", isOn: $s.guides)
        Text("Profiles of your own are never overwritten.").font(.callout)
        if xplaneIsRunning {
            Text("X-Plane is running: restart it after installing.").foregroundStyle(.orange)
        }
        Spacer()
        HStack {
            Button("Not now", action: close)
            Spacer()
            Button("Back") { s.step = 0 }
            Button("Install") {
                s.install()
                m.refreshPlugin()
            }
            .keyboardShortcut(.defaultAction)
            .disabled(!s.plugin && !s.profiles && !s.guides)
        }
    }

    @ViewBuilder private var resultStep: some View {
        Text(s.failed.isEmpty ? "Installed" : "Installed, with problems").font(.title2.bold())
        ScrollView {
            VStack(alignment: .leading, spacing: 10) {
                Text("\(s.done.count) file\(s.done.count == 1 ? "" : "s") installed.")
                PathList(title: "Could not install", paths: s.failed, color: .orange)
                PathList(title: "Your own files, left as they are", paths: s.kept)
                if s.done.contains(where: { $0.hasSuffix(".xpl") || $0.hasSuffix(".x52") }) {
                    Text("Restart X-Plane to load what was installed.").bold()
                    Text("In X-Plane, Settings > Joystick: set each button your profile programs to Do nothing.")
                        .fixedSize(horizontal: false, vertical: true)
                    Text("Profiles that send keystrokes also need X-Plane allowed in Privacy & Security > Accessibility.")
                        .fixedSize(horizontal: false, vertical: true)
                    Button("Open Accessibility settings") { openAccessibilitySettings() }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        HStack {
            if s.guides { Button("Open guides") { m.openGuides() } }
            Spacer()
            if !s.failed.isEmpty { Button("Try again") { s.step = 1 } }
            Button("Done", action: close).keyboardShortcut(.defaultAction)
        }
    }
}
