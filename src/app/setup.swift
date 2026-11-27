// The setup window: finds X-Plane 12, asks what to install and installs it. The file work is in
// C++ (src/mac/setup.cpp), reached through bridge.h.
import AppKit
import ServiceManagement
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

// MARK: - Uninstall

/// The app itself goes to the Trash only from an Applications folder (never a copy someone built or keeps elsewhere).
var appIsInApplications: Bool {
    let parent = Bundle.main.bundleURL.deletingLastPathComponent().path
    return parent == "/Applications" || parent == NSHomeDirectory() + "/Applications"
}

/// To the Trash; in a sandbox, into its own "trash" folder.
func moveToTrash(_ url: URL) throws {
    guard let box = sandboxDir else {
        try FileManager.default.trashItem(at: url, resultingItemURL: nil)
        return
    }
    guard url.path.hasPrefix(box + "/") || url.path.hasPrefix(NSTemporaryDirectory()) else {
        throw CocoaError(.fileWriteNoPermission)
    }
    let bin = URL(fileURLWithPath: box + "/trash")
    try FileManager.default.createDirectory(at: bin, withIntermediateDirectories: true)
    try FileManager.default.moveItem(at: url, to: bin.appendingPathComponent(UUID().uuidString + " " + url.lastPathComponent))
}

final class UninstallState: ObservableObject {
    @Published var deleteCustom = false
    @Published var remove: [String] = []
    @Published var custom: [String] = []
    @Published var unknown: [String] = []
    @Published var missing: [String] = []
    @Published var failed: [String] = []
    @Published var blocked = false     // X-Plane is running

    func look(chosen: String) {
        x52_uninstall_plan(chosen)
        remove = lines(x52_uninstall_list(0))
        custom = lines(x52_uninstall_list(1))
        unknown = lines(x52_uninstall_list(2))
        missing = lines(x52_uninstall_list(3))
        blocked = xplaneIsRunning && sandboxDir == nil
    }

    func begin(chosen: String) {
        deleteCustom = false
        failed = []
        look(chosen: chosen)
    }

    /// Takes everything out, except the app itself. Fills `failed`; a failed step does not stop the others.
    func perform(m: Model) {
        let chosen = m.xplane
        m.stop()                       // nothing writes into the folders while they are emptied
        look(chosen: chosen)           // what is there now
        guard !blocked else {
            m.start()
            return
        }
        var problems = lines(x52_uninstall_remove())
        if deleteCustom {
            for path in custom {
                do { try moveToTrash(URL(fileURLWithPath: path)) }
                catch { problems.append("\(path): \(error.localizedDescription)") }
            }
        }
        x52_uninstall_prune()
        if sandboxDir == nil, let id = Bundle.main.bundleIdentifier {
            if SMAppService.mainApp.status == .enabled {
                do { try SMAppService.mainApp.unregister() }
                catch { problems.append("Start at login: \(error.localizedDescription)") }
            }
            // The app's own entries in Privacy & Security (X-Plane's Accessibility entry is X-Plane's and stays).
            let reset = Process()
            reset.executableURL = URL(fileURLWithPath: "/usr/bin/tccutil")
            reset.arguments = ["reset", "All", id]
            try? reset.run()
            reset.waitUntilExit()
            if deleteCustom { deletePreferences(id, &problems) }
        }
        failed = problems
        if !problems.isEmpty {
            m.start()
            look(chosen: chosen)
        }
    }

    /// The preferences are removed; a copy goes to the Trash like the rest of the custom setup.
    private func deletePreferences(_ id: String, _ problems: inout [String]) {
        if let values = UserDefaults.standard.persistentDomain(forName: id) {
            let copy = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("X52 Pro preferences.plist")
            do {
                try? FileManager.default.removeItem(at: copy)
                let data = try PropertyListSerialization.data(fromPropertyList: values, format: .xml, options: 0)
                try data.write(to: copy)
                try moveToTrash(copy)
            } catch { problems.append("Preferences: \(error.localizedDescription)") }
        }
        UserDefaults.standard.removePersistentDomain(forName: id)
    }

    /// Asks once more, uninstalls, and quits when all went well.
    func run(m: Model, closeOthers: () -> Void) {
        let ask = NSAlert()
        ask.alertStyle = .critical
        ask.messageText = "Uninstall X52 Pro?"
        ask.informativeText = deleteCustom
            ? "The plugin, ready-made profiles and guides are deleted. Your own profiles and settings go to the Trash."
            : "The plugin, ready-made profiles and guides are deleted. Your own profiles and settings are kept."
        ask.addButton(withTitle: "Cancel")
        ask.addButton(withTitle: "Uninstall").hasDestructiveAction = true
        guard ask.runModal() == .alertSecondButtonReturn else { return }
        closeOthers()
        perform(m: m)
        guard failed.isEmpty && !blocked else { return }
        var text = deleteCustom ? "Your own profiles and settings are in the Trash." : "Your own profiles and settings were kept."
        if !unknown.isEmpty { text += "\n\nLeft in place (not made by X52 Pro):\n" + unknown.joined(separator: "\n") }
        if !missing.isEmpty { text += "\n\nNot reachable, so not cleaned:\n" + missing.joined(separator: "\n") }
        var appGone = false
        if sandboxDir == nil && appIsInApplications { appGone = (try? moveToTrash(Bundle.main.bundleURL)) != nil }
        text += appGone ? "\n\nThe app is in the Trash." : "\n\nNow drag the X52 Pro app to the Trash."
        let done = NSAlert()
        done.messageText = "X52 Pro is uninstalled"
        done.informativeText = text
        done.addButton(withTitle: "Quit")
        done.runModal()
        NSApp.terminate(nil)
    }
}

struct UninstallView: View {
    @ObservedObject var m: Model
    @ObservedObject var s: UninstallState
    let closeOthers: () -> Void
    let close: () -> Void
    private let byHand = Bundle.main.url(forResource: "Uninstall by hand", withExtension: "pdf", subdirectory: "guides")

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Uninstall X52 Pro").font(.title2.bold())
            if s.blocked {
                Text("X-Plane is running. Quit it first.").foregroundStyle(.orange)
                Button("Check again") { s.look(chosen: m.xplane) }
            }
            Text("Your custom setup: your profiles, calibration and settings.")
            Picker("", selection: $s.deleteCustom) {
                Text("Keep my custom setup").tag(false)
                Text("Delete it too (moved to the Trash)").tag(true)
            }
            .pickerStyle(.radioGroup).labelsHidden()
            Text("Also removed: start at login, the app's caches and privacy entries, and the app itself. X-Plane's own settings are not touched.")
                .font(.callout).fixedSize(horizontal: false, vertical: true)
            ScrollView {
                VStack(alignment: .leading, spacing: 10) {
                    PathList(title: "Could not remove", paths: s.failed, color: .orange)
                    PathList(title: s.deleteCustom ? "Your own files, moved to the Trash" : "Your own files, kept", paths: s.custom)
                    PathList(title: "Not made by X52 Pro, left in place", paths: s.unknown)
                    PathList(title: "X-Plane folders that cannot be reached", paths: s.missing)
                    PathList(title: "Will be removed", paths: s.remove)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            HStack {
                Button("How to remove it by hand") { if let u = byHand { NSWorkspace.shared.open(u) } }.disabled(byHand == nil)
                Spacer()
                Button("Cancel", action: close).keyboardShortcut(.cancelAction)
                Button("Uninstall...", role: .destructive) { s.run(m: m, closeOthers: closeOthers) }.disabled(s.blocked)
            }
        }
        .padding(20)
        .frame(width: 620, height: 600, alignment: .topLeading)
    }
}
