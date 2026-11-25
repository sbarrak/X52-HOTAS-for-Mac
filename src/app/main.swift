// X52 Pro menu bar app: a control panel for the stick's lights, screen and clock that
// also keeps them applied in the background. All the work is in the C++ Agent (see
// bridge.h); this file is the menu bar icon and the settings window.
import AppKit
import ServiceManagement
import SwiftUI
import UniformTypeIdentifiers

func str(_ p: UnsafePointer<CChar>?) -> String { p.map { String(cString: $0) } ?? "" }

// MARK: - Model

struct AxisRow: Equatable, Identifiable {
    let name: String
    let percent: Int
    let text: String
    let raw: String
    let moving: Bool
    let usage: Int
    var id: String { name }
}

func parseAxes(_ text: String) -> [AxisRow] {
    text.split(separator: "\n").compactMap { line in
        let f = line.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        guard f.count >= 6 else { return nil }
        return AxisRow(name: f[0], percent: Int(f[1]) ?? 0, text: f[2], raw: f[3], moving: f[4] == "1", usage: Int(f[5]) ?? 0)
    }
}

final class Model: ObservableObject {
    @Published var connected = false
    @Published var isPro = true
    @Published var pluginActive = false
    @Published var pluginEnabled = true   // the app's switch for the X-Plane plugin
    @Published var mode = 1
    @Published var shift = false        // pinkie switch held
    @Published var lightsPinkie = false // Lights tab: editing the pinkie-held colours
    @Published var revision = -1
    @Published var error = ""
    @Published var held = ""
    @Published var moved = ""
    @Published var screen = ["", "", ""]
    @Published var profiles: [String] = ["default"]
    @Published var customTemplate = UserDefaults.standard.string(forKey: "manualTemplate") ?? ""   // a manual template the user chose ("": the built-in one)
    @Published var lastDeleted = ""             // the profile "Undo delete" brings back
    @Published var key = "default"
    @Published var folder = ""
    @Published var xplane = ""          // the X-Plane 12 folder in use ("" when none was found)
    @Published var tab = 0              // selected tab of the settings window
    @Published var liveOpen: Set<Int> = []     // live light rules shown opened
    @Published var liveChecked: Set<Int> = []  // ticked for "Remove selected"
    @Published var liveStatus: [Int: String] = [:]   // what X-Plane makes of each opened rule's / alarm's datarefs (see statusKey)
    @Published var liveNotice = ""      // why a live light change was refused
    @Published var alarmNotice = ""     // the same for the alarms
    @Published var alarmOpen: Set<Int> = []
    @Published var alarmChecked: Set<Int> = []
    @Published var tried = -1           // the alarm being shown on the stick for a look
    @Published var brightTried = -1     // the screen (0) or lights (1) shown at an end of their brightness link
    @Published var importNote = ""      // result of "Import what X-Plane has"
    @Published var importOpen = false   // the import panel is showing
    @Published var importFrom = ""      // the X-Plane joystick profile chosen to import from
    @Published var importName = ""      // name for the new profile
    @Published var btn = 1              // button selected on the Buttons tab
    @Published var btnLayer = 0         // 0 any mode, 1..3 mode, 4..6 mode + pinkie
    @Published var masterTried = -1     // the master switch being shown off on the stick for a look
    @Published var pluginState = 0      // 0 not installed, 1 out of date, 2 up to date
    @Published var pluginMessage = ""
    @Published var loginItem = false
    @Published var testLines = ["", "", ""]   // text typed on the Test tab
    @Published var axes: [AxisRow] = []       // every analog control, while the Axes tab is showing
    @Published var calibration: [String] = [] // the stick calibration: what to say, then one line per measured axis
    @Published var calibrating = false
    @Published var calLive: [Int: CalAxis] = [:]   // the stick axes while the calibration window is open
    @Published var calibrationOpen = false
    @Published var calStep = 0           // calibration window: 0 start, 1 stick, 2 twist, 3 let go, 4 save, 5 saved
    @Published var calTwistSkipped = false
    @Published var calProblem = ""       // why Save was refused
    var openCalibration: () -> Void = {}
    @Published var pageSel = ""         // id of the page selected on the Pages tab and in the pages editor
    @Published var wheelMode = 0        // the left wheel's order being edited: 0 every mode, 1..3 that mode's own
    @Published var pageNote = ""        // why a page change was refused
    @Published var pagesConfirm = false // asking before "Restore default pages"
    @Published var pagesOpen = false    // the pages editor window is open
    @Published var pagePreview = ["", "", ""]   // the selected page as the stick shows it
    @Published var pagePreviewLive = false      // with numbers from X-Plane (otherwise made up)
    var openPagesEditor: () -> Void = {}
    @Published var isStandard = false            // the profile being edited is ready-made (never changed)
    @Published var canUndo = false               // there is an edit to take back (never for a ready-made profile)
    @Published var canRedo = false
    @Published var standardKeys: Set<String> = []
    private var askingCopy = false
    var snapshotMode = false            // drawing pictures for the project: never ask for a name
    private var copyDeclined = false        // the name was just refused: drop the rest of that gesture
    private var ignoreCopyUntil = Date.distantPast

    private var timer: Timer?
    private var slow = 0

    func start() {
        let d = UserDefaults.standard
        // The folder chosen by hand if it is still an X-Plane, else the first one found.
        let chosen = d.string(forKey: "xplaneFolder") ?? ""
        xplane = x52_valid_xplane(chosen) != 0 ? chosen : str(x52_find_xplane()).split(separator: "\n").map(String.init).first ?? ""
        refreshPlugin()
        let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
            .appendingPathComponent("X52Pro").path
        try? FileManager.default.createDirectory(atPath: support, withIntermediateDirectories: true)
        x52_start(xplane, support, d.string(forKey: "profileKey") ?? "default")
        folder = str(x52_dir())
        loginItem = SMAppService.mainApp.status == .enabled
        revision = -1
        if timer == nil {
            let t = Timer(timeInterval: 0.05, repeats: true) { [weak self] _ in self?.tick() }
            RunLoop.main.add(t, forMode: .common)   // keeps running while a menu or slider is tracked
            timer = t
        }
        tick()
    }

    func stop() {
        timer?.invalidate()
        timer = nil
        x52_stop()
    }

    // Only publish what changed, so the window is not redrawn ten times a second.
    private func set<T: Equatable>(_ path: ReferenceWritableKeyPath<Model, T>, _ value: T) {
        if self[keyPath: path] != value { self[keyPath: path] = value }
    }

    private var pressSeen = Int(x52_press_seq())
    // A press on the stick picks that button on the Buttons tab (the mode dial and the pinkie cannot be programmed).
    private func followPress() {
        let seq = Int(x52_press_seq())
        if seq == pressSeen { return }
        let restarted = seq < pressSeen   // the counter starts again when the stick is reconnected
        pressSeen = seq
        if restarted { return }
        let b = Int(x52_press_button())
        if tab == 6 && b >= 1 && b <= 51 && x52_button_reserved(Int32(b)) == 0 {
            if btn != b { btn = b }
            let layer = Int(x52_press_layer())   // the layer the press happened in, Any mode included
            if btnLayer != layer { btnLayer = layer }
        }
    }

    func tick() {
        x52_tick()
        followPress()
        set(\.connected, x52_connected() != 0)
        set(\.isPro, x52_is_pro() != 0 || x52_connected() == 0)
        set(\.pluginActive, x52_plugin_active() != 0)
        set(\.pluginEnabled, x52_plugin_enabled() != 0)
        set(\.mode, Int(x52_mode()))
        set(\.shift, x52_shift() != 0)
        set(\.error, str(x52_error()))
        set(\.held, str(x52_held()))
        set(\.moved, str(x52_moved()))
        set(\.screen, (0..<3).map { str(x52_screen(Int32($0))) })
        set(\.revision, Int(x52_revision()))
        set(\.isStandard, x52_standard() != 0)
        set(\.canUndo, x52_can_undo() != 0)
        set(\.canRedo, x52_can_redo() != 0)
        if x52_copy_needed() != 0 && !askingCopy {
            if snapshotMode || Date() < ignoreCopyUntil || (copyDeclined && NSEvent.pressedMouseButtons != 0) {
                // Still the gesture that was just cancelled (a stepper or slider keeps repeating while the
                // mouse is held): drop it rather than ask again, which used to loop.
                x52_copy_cancel()
            } else if NSEvent.pressedMouseButtons != 0 {
                // Wait for the mouse to come up, so the control that made the change stops tracking first.
            } else {
                copyDeclined = false
                askingCopy = true
                DispatchQueue.main.async { [weak self] in self?.askCopyName() }
            }
        }
        if copyDeclined && NSEvent.pressedMouseButtons == 0 && Date() >= ignoreCopyUntil { copyDeclined = false }
        if tab == 7 { set(\.axes, parseAxes(str(x52_axes()))) }   // changes constantly: only while it is shown
        if tab == 7 || calibrationOpen {
            set(\.calibration, str(x52_calibration()).split(separator: "\n", omittingEmptySubsequences: true).map(String.init))
            set(\.calibrating, x52_calibrating() != 0)
        }
        if calibrationOpen { set(\.calLive, parseCalLive(str(x52_calibration_live()))) }
        set(\.liveStatus, statusMap())
        set(\.tried, Int(x52_alarm_tried()))
        set(\.masterTried, Int(x52_master_tried()))
        set(\.brightTried, Int(x52_bright_tried()))
        if pagesOpen || tab == 8 {
            if pageSel.hasPrefix("#") {   // a built-in page
                let key = String(pageSel.dropFirst())
                set(\.pagePreview, (0..<3).map { str(x52_builtin_preview(key, Int32($0))) })
                set(\.pagePreviewLive, false)
            } else {
                let i = Int32(pageIndex(pageSel))
                set(\.pagePreview, (0..<3).map { str(x52_page_preview(i, Int32($0))) })
                set(\.pagePreviewLive, x52_page_preview_live(i) != 0)
            }
        }
        slow += 1
        if slow % 40 == 1 {
            set(\.profiles, str(x52_profiles()).split(separator: "\n").map(String.init))
            set(\.standardKeys, Set(profiles.filter { x52_profile_standard($0) != 0 }))
            set(\.key, str(x52_key()))
            refreshPlugin()
        }
    }

    // What X-Plane makes of the datarefs of the rows opened on the Live lights or Alarms tab.
    private func statusMap() -> [Int: String] {
        var d: [Int: String] = [:]
        if tab == 5 {
            for i in 0..<2 { for w in 0..<2 { d[statusKey(2, i, w)] = str(x52_trg_status(2, Int32(i), Int32(w))) } }
            for i in 0..<2 { d[statusKey(3, i, 0)] = str(x52_bright_status(Int32(i))) }
            return d
        }
        guard tab == 1 || tab == 2 else { return [:] }
        let kind = tab == 1 ? 0 : 1
        for i in (kind == 0 ? liveOpen : alarmOpen) {
            for w in 0..<2 { d[statusKey(kind, i, w)] = str(x52_trg_status(Int32(kind), Int32(i), Int32(w))) }
        }
        return d
    }

    func led(_ mode: Int, _ led: Int) -> Int { Int(x52_get_led(Int32(mode), Int32(led), lightsPinkie ? 1 : 0)) }
    func setLed(_ mode: Int, _ led: Int, _ color: Int) {
        x52_set_led(Int32(mode), Int32(led), lightsPinkie ? 1 : 0, Int32(color))
        tick()
    }
    // A change to a ready-made profile: ask for the name of the copy it goes into (or drop it).
    func askCopyName() {
        guard x52_copy_needed() != 0 else { askingCopy = false; return }
        var name = str(x52_copy_suggest())
        var problem = ""
        while true {
            let alert = NSAlert()
            alert.messageText = "Save as a new profile"
            alert.informativeText = "“\(profileTitle(key))” is a ready-made profile: it is never changed, so you can always go back to it. "
                + "Your change goes into a new profile of yours, which is used from now on instead (pick the ready-made one again to go back)."
                + "\n\nCancel (or Escape) drops the change."
                + (problem.isEmpty ? "" : "\n\n" + problem)
            let field = NSTextField(frame: NSRect(x: 0, y: 0, width: 300, height: 24))
            field.stringValue = name
            alert.accessoryView = field
            alert.addButton(withTitle: "Save")
            let cancel = alert.addButton(withTitle: "Cancel")
            cancel.keyEquivalent = "\u{1b}"   // Escape
            alert.window.initialFirstResponder = field
            NSApp.activate(ignoringOtherApps: true)
            if alert.runModal() != .alertFirstButtonReturn {
                x52_copy_cancel()
                // Whatever the same click or key repeat still sends is dropped too, not asked about again.
                copyDeclined = true
                ignoreCopyUntil = Date().addingTimeInterval(0.6)
                break
            }
            name = field.stringValue
            problem = str(x52_copy_save(name))
            if problem.isEmpty { break }
        }
        askingCopy = false
        tick()
    }

    func get(_ name: String) -> Int { Int(x52_get(name)) }
    func set(_ name: String, _ value: Int) {
        guard get(name) != value else { return }
        x52_set(name, Int32(value))
        tick()
    }
    func select(_ k: String) {
        x52_select(k)
        keyChanged()
    }
    // After the profile in use changed: remember it, and list the profiles again now rather than in a few seconds.
    private func keyChanged() {
        UserDefaults.standard.set(str(x52_key()), forKey: "profileKey")
        key = str(x52_key())
        profiles = str(x52_profiles()).split(separator: "\n").map(String.init)
        standardKeys = Set(profiles.filter { x52_profile_standard($0) != 0 })
        tick()
    }
    // "New blank profile...": asks for a name, then makes and uses it.
    func newBlank() {
        var name = str(x52_blank_suggest())
        var problem = ""
        while true {
            let alert = NSAlert()
            alert.messageText = "New blank profile"
            alert.informativeText = "Starts from scratch: no buttons, live lights or alarms, plain green lights and the built-in screen pages. "
                + "The axis settings of “\(profileTitle(key))” are kept so the aircraft still flies. The profile in use now is not changed."
                + (problem.isEmpty ? "" : "\n\n" + problem)
            let field = NSTextField(frame: NSRect(x: 0, y: 0, width: 300, height: 24))
            field.stringValue = name
            alert.accessoryView = field
            alert.addButton(withTitle: "Create")
            alert.addButton(withTitle: "Cancel").keyEquivalent = "\u{1b}"
            alert.window.initialFirstResponder = field
            NSApp.activate(ignoringOtherApps: true)
            guard alert.runModal() == .alertFirstButtonReturn else { return }
            name = field.stringValue
            problem = str(x52_blank_create(name))
            if problem.isEmpty { break }
        }
        lastDeleted = ""
        keyChanged()
    }
    // Export manual: fills the manual template (the built-in one, or the one the user chose) with the profile in use and
    // saves it as a Word file. Turning it into a PDF is done in Word.
    var builtInTemplate: String { (Bundle.main.resourcePath ?? "") + "/X52-Manual-Template.docx" }
    var manualTemplate: String {
        !customTemplate.isEmpty && FileManager.default.fileExists(atPath: customTemplate) ? customTemplate : builtInTemplate
    }
    func chooseManualTemplate() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = false
        panel.allowedContentTypes = [UTType(filenameExtension: "docx")!]
        panel.message = "Choose a manual template (a Word file with tagged content controls)"
        panel.prompt = "Use this template"
        NSApp.activate(ignoringOtherApps: true)
        guard panel.runModal() == .OK, let url = panel.url else { return }
        customTemplate = url.path
        UserDefaults.standard.set(url.path, forKey: "manualTemplate")
    }
    // A copy of the built-in template to start a custom one from (it lives inside the app, where nobody looks).
    func saveBuiltInTemplateCopy() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "docx")!]
        panel.nameFieldStringValue = "My X52 Pro manual template.docx"
        panel.message = "Save a blank copy of the built-in template. Open it in Word, change it, then choose it with “Choose Custom Template...”."
        panel.prompt = "Save copy"
        NSApp.activate(ignoringOtherApps: true)
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let alert = NSAlert()
        do {
            if FileManager.default.fileExists(atPath: url.path) { try FileManager.default.removeItem(at: url) }
            try FileManager.default.copyItem(atPath: builtInTemplate, toPath: url.path)
            alert.messageText = "Blank template copy saved"
            alert.informativeText = "Open it in Word and change what you like. The guide “Your own manual template” explains how the fields work."
            alert.addButton(withTitle: "Show in Finder")
            alert.addButton(withTitle: "OK")
            if alert.runModal() == .alertFirstButtonReturn { NSWorkspace.shared.activateFileViewerSelecting([url]) }
        } catch {
            alert.messageText = "The copy could not be saved"
            alert.informativeText = error.localizedDescription
            alert.alertStyle = .warning
            alert.runModal()
        }
    }
    func useBuiltInTemplate() {
        customTemplate = ""
        UserDefaults.standard.removeObject(forKey: "manualTemplate")
    }
    func exportManual() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [UTType(filenameExtension: "docx")!]
        panel.nameFieldStringValue = "\(profileTitle(key)) - X52 Pro map.docx"
        panel.message = "Save the manual for “\(profileTitle(key))”"
        panel.prompt = "Export"
        NSApp.activate(ignoringOtherApps: true)
        guard panel.runModal() == .OK, let url = panel.url else { return }
        let ok = x52_export_manual(key, manualTemplate, (Bundle.main.resourcePath ?? "") + "/light-icons", url.path) != 0
        let note = str(x52_export_message())
        let alert = NSAlert()
        if ok {
            alert.messageText = "Manual saved"
            alert.informativeText = "Open it in Word to check it. For a PDF, use File > Save as PDF in Word."
                + (note.isEmpty ? "" : "\n\n" + note)
            alert.addButton(withTitle: "Show in Finder")
            alert.addButton(withTitle: "OK")
            if alert.runModal() == .alertFirstButtonReturn { NSWorkspace.shared.activateFileViewerSelecting([url]) }
        } else {
            alert.messageText = "The manual was not exported"
            alert.informativeText = note
            alert.alertStyle = .warning
            alert.runModal()
        }
    }
    var canDelete: Bool { key != "default" && !standardKeys.contains(key) }
    func deleteCurrent() {
        let k = key
        guard confirm("Delete “\(profileTitle(k))”?",
                      "The profile goes away from the list. Undo delete brings it back until you delete another one.", "Delete")
        else { return }
        let problem = str(x52_delete_profile(k))
        if problem.isEmpty {
            lastDeleted = k
        } else {
            let alert = NSAlert()
            alert.messageText = "Not deleted"
            alert.informativeText = problem
            alert.runModal()
        }
        keyChanged()
    }
    func undo() { if x52_undo() != 0 { tick() } }
    func redo() { if x52_redo() != 0 { tick() } }
    func undoDelete() {
        if x52_restore_profile(lastDeleted) != 0 { lastDeleted = "" }
        keyChanged()
    }
    var bundledPlugin: String { (Bundle.main.resourcePath ?? "") + "/X52Pro.xpl" }
    func refreshPlugin() {
        pluginState = xplane.isEmpty ? 0 : Int(x52_plugin_state(xplane, bundledPlugin))
    }
    func installPlugin() {
        pluginMessage = str(x52_install_plugin(xplane, bundledPlugin))
        if pluginMessage.isEmpty { pluginMessage = "Installed. Restart X-Plane to load it." }
        refreshPlugin()
    }
    func chooseXPlaneFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.message = "Choose your X-Plane 12 folder"
        panel.prompt = "Choose"
        if panel.runModal() == .OK, let url = panel.url {
            guard x52_valid_xplane(url.path) != 0 else {
                pluginMessage = "That folder is not an X-Plane 12 install (it has no Resources/plugins)."
                return
            }
            pluginMessage = ""
            UserDefaults.standard.set(url.path, forKey: "xplaneFolder")
            start()
        }
    }
    func setLoginItem(_ on: Bool) {
        do {
            if on { try SMAppService.mainApp.register() } else { try SMAppService.mainApp.unregister() }
        } catch {
            self.error = "Start at login could not be changed: \(error.localizedDescription)"
        }
        loginItem = SMAppService.mainApp.status == .enabled
    }

    var layerText: String { shift ? "Pinkie + \(mode)" : "Mode \(mode)" }
    var statusText: String {
        if !pluginEnabled { return connected ? "Stick connected, plugin off" : "Stick not connected, plugin off" }
        if pluginActive { return "X-Plane is driving the stick (\(layerText))" }
        if !connected { return "Stick not connected" }
        return "Stick connected, \(layerText)"
    }
}

// Asks before a change that cannot be undone. Escape cancels.
func confirm(_ title: String, _ message: String, _ action: String) -> Bool {
    let alert = NSAlert()
    alert.messageText = title
    alert.informativeText = message
    alert.alertStyle = .warning
    alert.addButton(withTitle: action).hasDestructiveAction = true
    alert.addButton(withTitle: "Cancel").keyEquivalent = "\u{1b}"
    return alert.runModal() == .alertFirstButtonReturn
}

func plural(_ n: Int, _ word: String) -> String { n == 1 ? "1 \(word)" : "\(n) \(word)s" }

func profileTitle(_ key: String) -> String {
    key == "default" ? "Default (all aircraft)" : key.hasPrefix("aircraft-") ? String(key.dropFirst(9)) : key
}

// A tip that shows after the pointer has rested on the view for two seconds (the system's own help tag is faster
// and cannot be set), under the view and above what follows it.
final class TipState: ObservableObject {
    @Published var show = false
    var task: Task<Void, Never>?
}

struct DelayedTip: ViewModifier {
    let text: String
    @StateObject private var st = TipState()
    func body(content: Content) -> some View {
        content
            .onHover { inside in
                st.task?.cancel()
                st.show = false
                guard inside else { return }
                st.task = Task { @MainActor in
                    try? await Task.sleep(nanoseconds: 2_000_000_000)
                    if !Task.isCancelled { st.show = true }
                }
            }
            .overlay(alignment: .topTrailing) {
                if st.show {
                    Text(text)
                        .font(.caption)
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(RoundedRectangle(cornerRadius: 5).fill(Color(nsColor: .windowBackgroundColor)))
                        .overlay(RoundedRectangle(cornerRadius: 5).stroke(Color.secondary.opacity(0.5), lineWidth: 0.5))
                        .shadow(radius: 2)
                        .fixedSize()
                        .offset(y: 32)
                        .allowsHitTesting(false)
                }
            }
    }
}

extension View {
    func delayedTip(_ text: String) -> some View { modifier(DelayedTip(text: text)) }
}

// Undo and redo for the edits to the profile in use.
struct UndoRedoButtons: View {
    @ObservedObject var m: Model
    var body: some View {
        HStack(spacing: 2) {
            Button { m.undo() } label: { Image(systemName: "arrow.uturn.backward") }
                .disabled(!m.canUndo)
                .delayedTip(m.canUndo ? "Undo the last change  (⌘Z)" : m.isStandard ? "Ready-made profiles are never changed" : "Nothing to undo")
            Button { m.redo() } label: { Image(systemName: "arrow.uturn.forward") }
                .disabled(!m.canRedo)
                .delayedTip(m.canRedo ? "Redo  (⇧⌘Z)" : "Nothing to redo")
        }
    }
}

// A profile dropdown whose two groups (the user's own, and the ready-made ones) fold open with a + and shut with a -.
// Opening it shows the group of the current profile unfolded; picking a profile closes it.
final class PickerState: ObservableObject {
    @Published var shown = false
    @Published var openMine = true
    @Published var openReady = false
    @Published var contentH: CGFloat = 60
    @Published var maxH: CGFloat = 420
}

private struct PickerHeight: PreferenceKey {
    static var defaultValue: CGFloat = 0
    static func reduce(value: inout CGFloat, nextValue: () -> CGFloat) { value = max(value, nextValue()) }
}

struct ProfilePicker: View {
    @ObservedObject var m: Model
    var onSelect: (String) -> Void
    @StateObject private var ui = PickerState()

    private var mine: [String] { m.profiles.filter { !m.standardKeys.contains($0) } }
    private var ready: [String] { m.profiles.filter { m.standardKeys.contains($0) } }

    var body: some View {
        Button {
            let inReady = m.standardKeys.contains(m.key)
            ui.openMine = !inReady || ready.isEmpty
            ui.openReady = inReady || mine.isEmpty
            // never taller than the window; the list scrolls beyond that
            let winH = NSApp.keyWindow?.contentView?.bounds.height ?? 600
            ui.maxH = max(120, winH - 60)
            ui.shown = true
        } label: {
            HStack(spacing: 6) {
                Text(profileTitle(m.key)).lineLimit(1).truncationMode(.tail)
                Spacer(minLength: 4)
                Image(systemName: "chevron.up.chevron.down").font(.caption2).foregroundStyle(.secondary)
            }
            .frame(maxWidth: .infinity)
        }
        .popover(isPresented: $ui.shown, arrowEdge: .bottom) {
            ScrollView {
                VStack(alignment: .leading, spacing: 2) {
                    group("Your profiles", mine, $ui.openMine)
                    group("Ready-made profiles", ready, $ui.openReady)
                }
                .padding(8)
                .background(GeometryReader { g in Color.clear.preference(key: PickerHeight.self, value: g.size.height) })
            }
            .onPreferenceChange(PickerHeight.self) { ui.contentH = $0 }
            .frame(width: 280, height: min(ui.contentH, ui.maxH))
        }
    }

    @ViewBuilder private func group(_ title: String, _ keys: [String], _ open: Binding<Bool>) -> some View {
        if !keys.isEmpty {
            Button { open.wrappedValue.toggle() } label: {
                HStack(spacing: 6) {
                    Image(systemName: open.wrappedValue ? "minus.square" : "plus.square")
                    Text(title).fontWeight(.semibold)
                    Text("(\(keys.count))").foregroundStyle(.secondary)
                    Spacer()
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .padding(.vertical, 3)
            if open.wrappedValue {
                ForEach(keys, id: \.self) { k in
                    Button { ui.shown = false; onSelect(k) } label: {
                        HStack(spacing: 6) {
                            Image(systemName: "checkmark").opacity(k == m.key ? 1 : 0).frame(width: 14)
                            Text(profileTitle(k)).lineLimit(1)
                            Spacer()
                        }
                        .padding(.vertical, 2).padding(.horizontal, 4)
                        .background(RoundedRectangle(cornerRadius: 4).fill(k == m.key ? Color.accentColor.opacity(0.18) : Color.clear))
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    .padding(.leading, 16)
                }
            }
        }
    }
}

// Shown above the tabs while a ready-made profile is selected.
struct StandardBanner: View {
    @ObservedObject var m: Model
    var body: some View {
        HStack(spacing: 10) {
            Image(systemName: "lock.fill")
            Text("Ready-made profile: never changed. A change is saved as a new profile of yours.").lineLimit(1)
            Spacer(minLength: 8)
            Button("Save as new profile...") { x52_copy_ask(); m.tick() }
        }
        .padding(.horizontal, 8)
        .frame(maxHeight: .infinity)
        .background(RoundedRectangle(cornerRadius: 6).fill(Color.accentColor.opacity(0.12)))
    }
}

// One line of status under the header, always the same height so nothing below it moves. It shows the most
// important message: an error, else the ready-made notice, else (main window) the note about the plugin.
struct StatusStrip: View {
    @ObservedObject var m: Model
    var pluginNote = false
    static let height: CGFloat = 34
    var body: some View {
        Group {
            if !m.error.isEmpty {
                Text(m.error).foregroundStyle(.red).lineLimit(2).font(.callout)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
            } else if m.isStandard {
                StandardBanner(m: m)
            } else if pluginNote && m.pluginActive {
                Text("Changes you make here are saved, and X-Plane applies them within a second.")
                    .font(.callout).foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
            } else {
                Color.clear
            }
        }
        .frame(height: StatusStrip.height)
    }
}

// MARK: - Lights

let swatch: [Color] = [
    Color.gray.opacity(0.25), Color(red: 0.2, green: 0.8, blue: 0.3), Color(red: 1.0, green: 0.7, blue: 0.1),
    Color(red: 0.95, green: 0.25, blue: 0.2),
]
let colorNames = ["Off", "Green", "Amber", "Red"]

// MARK: - Keyboard navigation

// macOS 14 and newer can give a custom control one Tab stop and read the arrow keys. On macOS 13 the
// plain buttons inside stay separate Tab stops, so nothing becomes unreachable.
let hasKeyPress: Bool = { if #available(macOS 14, *) { return true } else { return false } }()

// One Tab stop for a group of controls: the arrows (and Space / Return) act on the group.
struct ArrowKeys: ViewModifier {
    var left: (() -> Void)? = nil
    var right: (() -> Void)? = nil
    var up: (() -> Void)? = nil
    var down: (() -> Void)? = nil
    var activate: (() -> Void)? = nil
    func body(content: Content) -> some View {
        if #available(macOS 14, *) {
            content.focusable().onKeyPress(keys: [.leftArrow, .rightArrow, .upArrow, .downArrow, .space, .return]) { press in
                let fn: (() -> Void)?
                switch press.key {
                case .leftArrow: fn = left
                case .rightArrow: fn = right
                case .upArrow: fn = up
                case .downArrow: fn = down
                default: fn = activate
                }
                guard let fn else { return .ignored }
                fn()
                return .handled
            }
        } else {
            content
        }
    }
}

extension View {
    func arrowKeys(left: (() -> Void)? = nil, right: (() -> Void)? = nil, up: (() -> Void)? = nil,
                   down: (() -> Void)? = nil, activate: (() -> Void)? = nil) -> some View {
        modifier(ArrowKeys(left: left, right: right, up: up, down: down, activate: activate))
    }
    // The buttons inside a group that handles the arrows are not Tab stops of their own (macOS 14 and newer).
    func insideStop() -> some View { focusable(!hasKeyPress) }
}

struct LightCell: View {
    @ObservedObject var m: Model
    let mode: Int
    let led: Int
    var focus: FocusState<Int?>.Binding   // which cell has the keyboard: mode * 100 + light
    var body: some View {
        let single = x52_led_single(Int32(led)) != 0
        let current = m.led(mode, led)
        HStack(spacing: 6) {
            ForEach(single ? [0, 1] : [0, 1, 2, 3], id: \.self) { c in
                Button {
                    m.setLed(mode, led, c)
                } label: {
                    Circle()
                        .fill(swatch[c])
                        .frame(width: 16, height: 16)
                        .overlay(Circle().stroke(Color.primary, lineWidth: current == c ? 2 : 0).padding(-3))
                        .overlay(Circle().stroke(Color.secondary.opacity(0.5), lineWidth: 0.5))
                }
                .buttonStyle(.plain).insideStop()
                .help(single && c == 1 ? "On" : colorNames[c])
                .accessibilityLabel("\(str(x52_led_title(Int32(led)))), mode \(mode), \(single && c == 1 ? "on" : colorNames[c])")
            }
        }
        .frame(width: 110, alignment: .leading)
        .arrowKeys(left: { m.setLed(mode, led, max(0, current - 1)) },
                   right: { m.setLed(mode, led, min(single ? 1 : 3, current + 1)) },
                   up: { if led > 0 { focus.wrappedValue = mode * 100 + led - 1 } },
                   down: { if led < Int(x52_led_count()) - 1 { focus.wrappedValue = mode * 100 + led + 1 } })
        .focused(focus, equals: mode * 100 + led)
    }
}

struct BrightnessRow: View {
    @ObservedObject var m: Model
    let title: String
    let name: String
    var body: some View {
        let value = Binding<Double>(get: { Double(m.get(name)) }, set: { m.set(name, Int($0.rounded())) })
        HStack {
            Text(title).frame(width: 130, alignment: .leading)
            Slider(value: value, in: 0...127)
            Text("\(m.get(name) * 100 / 127) %").monospacedDigit().frame(width: 48, alignment: .trailing)
        }
    }
}

struct LightsTab: View {
    @ObservedObject var m: Model
    @FocusState private var cell: Int?
    var body: some View {
        ScrollView {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Colours for").lineLimit(1).fixedSize()
                Picker("", selection: $m.lightsPinkie) {
                    Text("Pinkie released").tag(false)
                    Text("Pinkie held").tag(true)
                }
                .labelsHidden().pickerStyle(.segmented).frame(width: 300)
                Spacer()
                Text("On the stick now: \(m.layerText)").fontWeight(.bold)
            }
            Grid(alignment: .leading, horizontalSpacing: 18, verticalSpacing: 9) {
                GridRow {
                    Text("Light").foregroundStyle(.secondary)
                    ForEach(1...3, id: \.self) { mode in
                        HStack(spacing: 4) {
                            let here = m.mode == mode && m.shift == m.lightsPinkie
                            Text(m.lightsPinkie ? "Pinkie + \(mode)" : "Mode \(mode)").fontWeight(here ? .bold : .regular)
                            if here && m.connected { Text("(now)").foregroundStyle(.secondary) }
                        }
                    }
                }
                Divider()
                ForEach(0..<Int(x52_led_count()), id: \.self) { led in
                    GridRow {
                        Text(str(x52_led_title(Int32(led))))
                        ForEach(1...3, id: \.self) { mode in LightCell(m: m, mode: mode, led: led, focus: $cell) }
                    }
                }
            }
            HStack {
                Text("Copy to all modes from").foregroundStyle(.secondary)
                ForEach(1...3, id: \.self) { mode in
                    Button(m.lightsPinkie ? "Pinkie + \(mode)" : "Mode \(mode)") { x52_copy_mode(Int32(mode), m.lightsPinkie ? 1 : 0); m.tick() }
                }
            }
            .padding(.top, 4)
            Divider()
            BrightnessRow(m: m, title: "Light brightness", name: "led_brightness")
            Text("The mode dial on the stick picks the column, and holding the pinkie switch picks the Pinkie set. Changing the lights of another mode does not change what the stick shows now; it shows them when you turn the dial to that mode.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            if !m.isPro {
                Text("This stick is a standard X52: button colours need the X52 Pro.").foregroundStyle(.orange)
            }
        }
        .padding(.trailing, 8)
        }
    }
}

// MARK: - Conditions shared by live lights and alarms

let ruleOps = [">", ">=", "<", "<=", "==", "!="]
let joinNames = ["Only this test", "AND", "OR"]

// kind 0 is a live light rule, kind 1 an alarm, kind 2 a master switch (index 0 screen, 1 lights)
func statusKey(_ kind: Int, _ index: Int, _ which: Int) -> Int { kind * 100000 + index * 10 + which }

func numberText(_ v: Double) -> String { String(format: "%g", v) }

func conditionText(_ kind: Int, _ index: Int, _ which: Int) -> String {
    let k = Int32(kind), i = Int32(index), w = Int32(which)
    let ref = str(x52_trg_ref(k, i, w))
    if ref.isEmpty { return "no dataref yet" }
    let core = "\(ref) \(ruleOps[Int(x52_trg_op(k, i, w))]) \(numberText(x52_trg_value(k, i, w)))"
    return x52_trg_negate(k, i, w) != 0 ? "NOT (\(core))" : core
}

func triggerSummary(_ kind: Int, _ index: Int) -> String {
    let join = Int(x52_trg_join(Int32(kind), Int32(index)))
    if join == 0 { return conditionText(kind, index, 0) }
    var text = "\(conditionText(kind, index, 0)) \(joinNames[join]) \(conditionText(kind, index, 1))"
    if !str(x52_trg_ref(Int32(kind), Int32(index), 2)).isEmpty { text += " \(joinNames[join]) \(conditionText(kind, index, 2))" }
    return text
}

struct ConditionRow: View {
    @ObservedObject var m: Model
    let kind: Int
    let index: Int
    let which: Int
    var body: some View {
        let k = Int32(kind), i = Int32(index), w = Int32(which)
        let ref = Binding<String>(get: { str(x52_trg_ref(k, i, w)) }, set: { write(ref: $0) })
        let op = Binding<Int>(get: { Int(x52_trg_op(k, i, w)) }, set: { write(op: $0) })
        let value = Binding<Double>(get: { x52_trg_value(k, i, w) }, set: { write(value: $0) })
        let negate = Binding<Bool>(get: { x52_trg_negate(k, i, w) != 0 }, set: { write(negate: $0) })
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(which == 0 ? "Dataref" : which == 1 ? "Second dataref" : "Third dataref").frame(width: 110, alignment: .leading)
                // Searches X-Plane's own list of datarefs (and the add-on aircraft's), also without X-Plane running.
                CatalogField(kind: 1, placeholder: "e.g. sim/flightmodel/controls/parkbrake, or name[3]", text: ref)
            }
            if let status = m.liveStatus[statusKey(kind, index, which)], !status.isEmpty {
                Text(status).font(.callout).foregroundStyle(.secondary).padding(.leading, 118)
                    .fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Text(which == 0 ? "When value is" : "and its value is").frame(width: 110, alignment: .leading)
                Picker("", selection: op) { ForEach(0..<ruleOps.count, id: \.self) { Text(ruleOps[$0]).tag($0) } }
                    .labelsHidden().frame(width: 70)
                TextField("", value: value, format: .number).textFieldStyle(.roundedBorder).frame(width: 90)
                Toggle("NOT", isOn: negate).toggleStyle(.checkbox)
                    .help("Turns the test around: true when the test is not met")
            }
        }
    }
    private func write(ref: String? = nil, op: Int? = nil, value: Double? = nil, negate: Bool? = nil) {
        let k = Int32(kind), i = Int32(index), w = Int32(which)
        x52_trg_set(k, i, w, ref ?? str(x52_trg_ref(k, i, w)), Int32(op ?? Int(x52_trg_op(k, i, w))),
                    value ?? x52_trg_value(k, i, w), (negate ?? (x52_trg_negate(k, i, w) != 0)) ? 1 : 0)
        m.tick()
    }
}

// The first test, and optionally a second and a third, all joined with AND or all with OR.
struct TriggerEditor: View {
    @ObservedObject var m: Model
    let kind: Int
    let index: Int
    var body: some View {
        let join = Binding<Int>(get: { Int(x52_trg_join(Int32(kind), Int32(index))) },
                                set: { x52_trg_set_join(Int32(kind), Int32(index), Int32($0)); m.tick() })
        VStack(alignment: .leading, spacing: 10) {
            ConditionRow(m: m, kind: kind, index: index, which: 0)
            HStack {
                Text("Combine with").frame(width: 110, alignment: .leading)
                FixedSegments(selection: join, names: joinNames, width: 110)
            }
            if join.wrappedValue != 0 {
                ConditionRow(m: m, kind: kind, index: index, which: 1)
                // A third test (same AND / OR) once the second one has a dataref.
                if !str(x52_trg_ref(Int32(kind), Int32(index), 1)).isEmpty {
                    ConditionRow(m: m, kind: kind, index: index, which: 2)
                }
            }
        }
    }
}

// Like a segmented picker, but every segment has the same fixed width, so the control never
// changes size when the selection changes.
struct FixedSegments: View {
    @Binding var selection: Int
    let names: [String]
    let width: CGFloat
    var body: some View {
        HStack(spacing: 0) {
            ForEach(0..<names.count, id: \.self) { n in
                let on = selection == n
                Button { selection = n } label: {
                    Text(names[n]).lineLimit(1).frame(width: width, height: 24)
                        .background(on ? Color.accentColor : Color.clear)
                        .foregroundStyle(on ? Color.white : Color.primary)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain).insideStop()
            }
        }
        .background(Color.secondary.opacity(0.15))
        .clipShape(RoundedRectangle(cornerRadius: 6))
        .arrowKeys(left: { selection = max(0, selection - 1) }, right: { selection = min(names.count - 1, selection + 1) })
    }
}

struct ColorChoice: View {
    @Binding var selection: Int
    let single: Bool
    var allowMode = true
    var body: some View {
        Picker("", selection: $selection) {
            if allowMode { Text("Mode colour").tag(-1) }
            Text("Off").tag(0)
            if single {
                Text("On").tag(1)
            } else {
                Text("Green").tag(1)
                Text("Amber").tag(2)
                Text("Red").tag(3)
            }
        }
        .labelsHidden().frame(width: 120)
    }
}

// MARK: - Live lights (conditional colours)

struct LiveRow: View {
    @ObservedObject var m: Model
    let rule: Int
    var body: some View {
        let r = Int32(rule)
        let led = Binding<Int>(get: { Int(x52_rule_led(r)) }, set: { write(led: $0) })
        let single = x52_led_single(x52_rule_led(r)) != 0
        let then = Binding<Int>(get: { Int(x52_rule_then(r)) }, set: { write(then: $0) })
        let other = Binding<Int>(get: { Int(x52_rule_else(r)) }, set: { write(other: $0) })
        let level = Int(x52_rule_level(r)), levels = Int(x52_rule_levels(r))
        let last = level == levels
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Light").frame(width: 110, alignment: .leading)
                Picker("", selection: led) {
                    ForEach(0..<Int(x52_led_count()), id: \.self) { Text(str(x52_led_title(Int32($0)))).tag($0) }
                }
                .labelsHidden().frame(width: 180)
                Spacer()
                Menu("Start from") {
                    ForEach(0..<Int(x52_preset_count()), id: \.self) { i in
                        Button(str(x52_preset_name(Int32(i)))) { x52_rule_preset(r, Int32(i)); m.tick() }
                    }
                }
                .frame(width: 100)
            }
            HStack {
                Text("Level \(level) of \(levels)").foregroundStyle(.secondary)
                Button("Add level") {
                    let n = Int(x52_rule_add_level(r))
                    if n >= 0 {
                        m.liveOpen = [n]
                        m.liveNotice = ""
                    } else {
                        m.liveNotice = "A light can have 3 levels at most."
                    }
                    m.tick()
                }
                .disabled(levels >= 3)
                .help("Adds another test for this light. The first level whose test is true decides the colour.")
            }
            TriggerEditor(m: m, kind: 0, index: rule)
            HStack {
                Text("Light is").frame(width: 110, alignment: .leading)
                ColorChoice(selection: then, single: single)
                if last {
                    Text(levels > 1 ? "when true, and otherwise" : "when true, and")
                    ColorChoice(selection: other, single: single)
                    if levels == 1 { Text("when not") }
                } else {
                    Text("when true; if not, the next level decides")
                }
            }
        }
        .padding(.leading, 28).padding(.vertical, 6)
    }
    private func write(led: Int? = nil, then: Int? = nil, other: Int? = nil) {
        let r = Int32(rule)
        let target = led ?? Int(x52_rule_led(r))
        let full = Int(x52_rule_set(r, Int32(target), Int32(then ?? Int(x52_rule_then(r))), Int32(other ?? Int(x52_rule_else(r))))) == -2
        if full {
            m.liveNotice = "\(str(x52_led_title(Int32(target)))) already has 3 levels, the most a light can have. The light was not changed: pick another light, or remove a level first."
        } else {
            m.liveNotice = ""
        }
        m.tick()
    }
}

struct LiveTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let count = Int(x52_rule_count())
        VStack(alignment: .leading, spacing: 10) {
            Text("Live lights follow the simulator instead of the mode dial. Open a rule to set what it watches and the colours it shows. A rule overrides the plain colours from the Lights tab, and alarms (Alarms tab) override rules. A light can have up to 3 levels: open a rule and press Add level, and the first level whose test is true decides the colour. The last level also says what to show when none is true.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            HStack {
                Button("Add rule") {
                    let i = Int(x52_rule_add())
                    if i >= 0 {
                        m.liveOpen.insert(i)
                        m.liveNotice = ""
                    } else {
                        m.liveNotice = "Every light already has a rule. Open a rule and press Add level to give a light more levels, or remove a rule."
                    }
                    m.tick()
                }
                Button("Remove selected") {
                    guard confirm("Remove \(plural(m.liveChecked.count, "live light rule"))?",
                                  "The lights go back to their colours on the Lights tab.", "Remove") else { return }
                    for i in m.liveChecked.sorted(by: >) { x52_rule_remove(Int32(i)) }
                    m.liveChecked = []
                    m.liveOpen = []
                    m.tick()
                }
                .disabled(m.liveChecked.isEmpty)
                Button(m.liveChecked.count == count && count > 0 ? "Select none" : "Select all") {
                    m.liveChecked = m.liveChecked.count == count ? [] : Set(0..<count)
                }
                .disabled(count == 0)
                Spacer()
                Text(count == 1 ? "1 rule" : "\(count) rules").foregroundStyle(.secondary)
            }
            if !m.liveNotice.isEmpty {
                Text(m.liveNotice).font(.callout).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
            }
            Divider()
            if count == 0 {
                Text("No live lights yet. Press Add rule.").foregroundStyle(.secondary)
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    ForEach(0..<count, id: \.self) { i in
                        let open = m.liveOpen.contains(i)
                        HStack(spacing: 8) {
                            Toggle("", isOn: Binding(get: { m.liveChecked.contains(i) },
                                                     set: { if $0 { m.liveChecked.insert(i) } else { m.liveChecked.remove(i) } }))
                                .labelsHidden().toggleStyle(.checkbox)
                            Button {
                                if open { m.liveOpen.remove(i) } else { m.liveOpen.insert(i) }
                            } label: {
                                HStack(spacing: 8) {
                                    Image(systemName: open ? "chevron.down" : "chevron.right").frame(width: 12)
                                    Text(str(x52_led_title(x52_rule_led(Int32(i))))).frame(width: 110, alignment: .leading)
                                    Text(triggerSummary(0, i)).foregroundStyle(.secondary).lineLimit(1).truncationMode(.middle)
                                    Spacer()
                                }
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                        }
                        .padding(.vertical, 6)
                        if open { LiveRow(m: m, rule: i) }
                        Divider()
                    }
                }
            }
            Text("The plugin reads the dataref inside X-Plane. Without the plugin this app asks X-Plane over the network (X-Plane Settings > Network > accept incoming connections, port 49000).")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

// MARK: - Alarms

let effectNames = ["Steady", "Blink", "Alternate"]

func alarmEffectText(_ a: Int32) -> String {
    let effect = Int(x52_alarm_effect(a))
    if effect == 0 { return "steady" }
    return (effect == 1 ? "blinks" : "alternates") + (x52_alarm_fast(a) != 0 ? " fast" : " slowly")
}

struct AlarmRow: View {
    @ObservedObject var m: Model
    let alarm: Int
    var body: some View {
        let a = Int32(alarm)
        let name = Binding<String>(get: { str(x52_alarm_name(a)) }, set: { write(name: $0) })
        let led = Binding<Int>(get: { Int(x52_alarm_led(a)) }, set: { write(led: $0) })
        let effect = Binding<Int>(get: { Int(x52_alarm_effect(a)) }, set: { write(effect: $0) })
        let fast = Binding<Int>(get: { Int(x52_alarm_fast(a)) }, set: { write(fast: $0) })
        let c1 = Binding<Int>(get: { Int(x52_alarm_color(a)) }, set: { write(color: $0) })
        let c2 = Binding<Int>(get: { Int(x52_alarm_color2(a)) }, set: { write(color2: $0) })
        let single = x52_led_single(x52_alarm_led(a)) != 0
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Name").frame(width: 110, alignment: .leading)
                TextField("e.g. Stall", text: name).textFieldStyle(.roundedBorder).frame(width: 240)
                Spacer()
                Button("Move up") { x52_alarm_move(a, -1); m.alarmOpen = [alarm - 1]; m.tick() }.disabled(alarm == 0)
                Button("Move down") { x52_alarm_move(a, 1); m.alarmOpen = [alarm + 1]; m.tick() }
                    .disabled(alarm >= Int(x52_alarm_count()) - 1)
            }
            HStack {
                Text("Light").frame(width: 110, alignment: .leading)
                Picker("", selection: led) {
                    ForEach(0..<Int(x52_led_count()), id: \.self) { Text(str(x52_led_title(Int32($0)))).tag($0) }
                }
                .labelsHidden().frame(width: 180)
            }
            HStack {
                Text("Effect").frame(width: 110, alignment: .leading)
                FixedSegments(selection: effect, names: effectNames, width: 90)
                if effect.wrappedValue != 0 {
                    Picker("", selection: fast) {
                        Text("Slow (1 s a cycle)").tag(0)
                        Text("Fast (0.5 s a cycle)").tag(1)
                    }
                    .labelsHidden().frame(width: 170)
                }
            }
            HStack {
                Text(effect.wrappedValue == 2 ? "Colours" : "Colour").frame(width: 110, alignment: .leading)
                ColorChoice(selection: c1, single: single, allowMode: false)
                if effect.wrappedValue == 2 {
                    Text("and")
                    ColorChoice(selection: c2, single: single, allowMode: false)
                }
            }
            Text("Triggered when").foregroundStyle(.secondary)
            TriggerEditor(m: m, kind: 1, index: alarm)
            HStack {
                Button("Try on the stick") { x52_alarm_try(a); m.tick() }
                    .disabled(m.pluginActive || !m.connected)
                    .help("Shows this alarm on its light for a few seconds, whatever the simulator says")
                if m.tried == alarm { Text("Showing on the stick now").foregroundStyle(.secondary) }
            }
        }
        .padding(.leading, 28).padding(.vertical, 6)
    }
    private func write(name: String? = nil, led: Int? = nil, effect: Int? = nil, fast: Int? = nil, color: Int? = nil, color2: Int? = nil) {
        let a = Int32(alarm)
        x52_alarm_set(a, name ?? str(x52_alarm_name(a)), Int32(led ?? Int(x52_alarm_led(a))),
                      Int32(effect ?? Int(x52_alarm_effect(a))), Int32(fast ?? Int(x52_alarm_fast(a))),
                      Int32(color ?? Int(x52_alarm_color(a))), Int32(color2 ?? Int(x52_alarm_color2(a))))
        m.tick()
    }
}

struct AlarmsTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let count = Int(x52_alarm_count())
        VStack(alignment: .leading, spacing: 10) {
            Text("Alarms have the highest priority: an alarm overrules the live lights (Live lights tab), and those overrule the plain colours (Lights tab). The first alarm in the list that is triggered decides its light. An alarm that is not triggered leaves the light to the next one, so a gear alarm can still show while the stall alarm above it is quiet. A light can have up to 3 alarms, which makes levels: for example amber, then red, then red flashing, as the test gets worse. When the situation ends the alarm stops and the light goes back to its live light or colour.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            if !m.alarmNotice.isEmpty {
                Text(m.alarmNotice).font(.callout).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Menu("Add from template") {
                    ForEach(0..<Int(x52_alarm_preset_count()), id: \.self) { i in
                        Button(str(x52_alarm_preset_name(Int32(i)))) {
                            let n = Int(x52_alarm_add_preset(Int32(i)))
                            if n >= 0 { m.alarmOpen.insert(n); m.alarmNotice = "" }
                            else { m.alarmNotice = "That light already has 3 alarms, the most a light can have." }
                            m.tick()
                        }
                    }
                }
                .frame(width: 150)
                Button("Add blank alarm") {
                    let n = Int(x52_alarm_add())
                    if n >= 0 { m.alarmOpen.insert(n); m.alarmNotice = "" }
                    else { m.alarmNotice = "Every light already has 3 alarms, the most a light can have." }
                    m.tick()
                }
                Button("Remove selected") {
                    guard confirm("Remove \(plural(m.alarmChecked.count, "alarm"))?",
                                  "Their settings and tests are lost.", "Remove") else { return }
                    for i in m.alarmChecked.sorted(by: >) { x52_alarm_remove(Int32(i)) }
                    m.alarmChecked = []
                    m.alarmOpen = []
                    m.tick()
                }
                .disabled(m.alarmChecked.isEmpty)
                Button(m.alarmChecked.count == count && count > 0 ? "Select none" : "Select all") {
                    m.alarmChecked = m.alarmChecked.count == count ? [] : Set(0..<count)
                }
                .disabled(count == 0)
                Spacer()
                Text(count == 1 ? "1 alarm" : "\(count) alarms").foregroundStyle(.secondary)
            }
            Divider()
            if count == 0 {
                Text("No alarms yet. Pick a template, for example the stall alarm, or add a blank one.").foregroundStyle(.secondary)
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    ForEach(0..<count, id: \.self) { i in
                        let open = m.alarmOpen.contains(i)
                        HStack(spacing: 8) {
                            Toggle("", isOn: Binding(get: { m.alarmChecked.contains(i) },
                                                     set: { if $0 { m.alarmChecked.insert(i) } else { m.alarmChecked.remove(i) } }))
                                .labelsHidden().toggleStyle(.checkbox)
                            Button {
                                if open { m.alarmOpen.remove(i) } else { m.alarmOpen.insert(i) }
                            } label: {
                                HStack(spacing: 8) {
                                    Image(systemName: open ? "chevron.down" : "chevron.right").frame(width: 12)
                                    Text("\(i + 1).").foregroundStyle(.secondary).frame(width: 22, alignment: .trailing)
                                    Text(str(x52_alarm_name(Int32(i)))).fontWeight(.medium).frame(width: 130, alignment: .leading).lineLimit(1)
                                    Text(str(x52_led_title(x52_alarm_led(Int32(i))))).frame(width: 100, alignment: .leading).lineLimit(1)
                                    Text("\(alarmEffectText(Int32(i))): \(triggerSummary(1, i))").foregroundStyle(.secondary)
                                        .lineLimit(1).truncationMode(.middle)
                                    Spacer()
                                }
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                        }
                        .padding(.vertical, 6)
                        if open { AlarmRow(m: m, alarm: i) }
                        Divider()
                    }
                }
            }
            Text("Blinking is done by the app or the plugin, 1 s a cycle (slow) or 0.5 s (fast). The same dataref can feed several alarms. Brightness stays as set on the Lights tab.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

// MARK: - Screen and clock

func offsetLabel(_ minutes: Int) -> String {
    let a = abs(minutes)
    return minutes == 0 ? "GMT" : String(format: "GMT %@%d:%02d", minutes < 0 ? "-" : "+", a / 60, a % 60)
}

// Minutes from GMT, in half hours (an odd value from the file is kept and shown too).
struct OffsetPicker: View {
    @ObservedObject var m: Model
    let name: String
    var body: some View {
        let offset = Binding<Int>(get: { m.get(name) }, set: { m.set(name, $0) })
        Picker("", selection: offset) {
            ForEach(Array(stride(from: -720, through: 840, by: 30)), id: \.self) { Text(offsetLabel($0)).tag($0) }
            if m.get(name) % 30 != 0 { Text(offsetLabel(m.get(name))).tag(m.get(name)) }
        }
        .labelsHidden().frame(width: 150)
    }
}

struct ClockRow: View {
    @ObservedObject var m: Model
    let number: Int
    var body: some View {
        let h12 = Binding<Bool>(get: { m.get("clock\(number)_12h") != 0 }, set: { m.set("clock\(number)_12h", $0 ? 1 : 0) })
        HStack {
            Text("Clock \(number)").frame(width: 130, alignment: .leading)
            if number == 1 {
                let source = Binding<Int>(get: { m.get("clock1_source") }, set: { m.set("clock1_source", $0) })
                Picker("", selection: source) {
                    Text("GMT").tag(0)
                    Text("Simulator local time").tag(1)
                    Text("Mac local time").tag(2)
                    Text("Fixed time zone").tag(3)
                }
                .labelsHidden().frame(width: 190)
                if source.wrappedValue == 3 { OffsetPicker(m: m, name: "clock1_offset") }
            } else {
                OffsetPicker(m: m, name: "clock\(number)_offset")
            }
            Toggle("12-hour", isOn: h12)
        }
    }
}

struct ModeNamesRow: View {
    @ObservedObject var m: Model
    var body: some View {
        let ms = Binding<Int>(get: { m.get("mode_banner_ms") }, set: { m.set("mode_banner_ms", $0) })
        VStack(alignment: .leading, spacing: 8) {
            ForEach(1...3, id: \.self) { mode in
                let name = Binding<String>(get: { str(x52_mode_name(Int32(mode))) }, set: { x52_set_mode_name(Int32(mode), $0) })
                HStack {
                    Text("Mode \(mode) name").frame(width: 130, alignment: .leading)
                    TextField("e.g. Takeoff", text: name).textFieldStyle(.roundedBorder).frame(width: 200)
                }
            }
            HStack {
                Text("Show the name for").frame(width: 130, alignment: .leading)
                Stepper(ms.wrappedValue == 0 ? "never" : String(format: "%.1f s", Double(ms.wrappedValue) / 1000), value: ms, in: 0...10000, step: 500)
                    .frame(width: 200, alignment: .leading)
            }
            Text("When the mode dial moves, the stick's screen says the mode's name for this long, then goes back to the page you were on. Saved in the aircraft's profile; 0 turns it off.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

struct ScreenTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let date = Binding<Int>(get: { m.get("date_format") }, set: { m.set("date_format", $0) })
        let latched = Binding<Bool>(get: { m.get("clutch_latched") != 0 }, set: { m.set("clutch_latched", $0 ? 1 : 0) })
        ScrollView {
        VStack(alignment: .leading, spacing: 12) {
            BrightnessRow(m: m, title: "Screen brightness", name: "mfd_brightness")
            Divider()
            ForEach(1...3, id: \.self) { ClockRow(m: m, number: $0) }
            HStack {
                Text("Date order").frame(width: 130, alignment: .leading)
                Picker("", selection: date) {
                    Text("Day - Month - Year").tag(0)
                    Text("Month - Day - Year").tag(1)
                    Text("Year - Month - Day").tag(2)
                }
                .labelsHidden().frame(width: 190)
            }
            Text("On the stick, the Start/Stop and Reset buttons under the screen switch between the three clocks. Function switches to the stopwatch. Clock 2 and 3 are always set from GMT, whatever Clock 1 shows. Simulator local time follows X-Plane's clock; while X-Plane is not running, Clock 1 shows the Mac's local time instead, and GMT the Mac's GMT.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Divider()
            ModeNamesRow(m: m)
            Divider()
            Toggle("Latched clutch: one press of the clutch (i) turns safe mode on, the next turns it off", isOn: latched)
            Text("Safe mode applies in X-Plane: buttons only show their name on the screen. The clutch and POV lights flash while it is on.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Divider()
            let dialOn = Binding<Bool>(get: { m.get("dial_accel") != 0 }, set: { m.set("dial_accel", $0 ? 1 : 0) })
            let factor = Binding<Int>(get: { m.get("dial_factor") }, set: { m.set("dial_factor", $0) })
            Toggle("Fast turns of the right wheel jump: heading, altitude, speed and fine tuning change in bigger steps", isOn: dialOn)
            if dialOn.wrappedValue {
                Stepper("Fast jump: x \(factor.wrappedValue)", value: factor, in: 2...50).frame(width: 200, alignment: .leading)
            }
            Text("A slow turn still steps by one; spinning the wheel repeats each click (HDG 10 degrees, altitude 1000 ft at x 10).")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(.trailing, 8)
        }
    }
}

// MARK: - Buttons

// A text field for an X-Plane command (kind 0) or dataref (kind 1) that suggests matches from X-Plane's own
// lists (Commands.txt / DataRefs.txt) while you type.
final class DraftText: ObservableObject {
    @Published var value = ""
    @Published var selected: Int? = nil   // the suggestion the arrow keys are on
    private var monitor: Any?
    // While the field has the keyboard: the handler gets each key code first and says whether it used it.
    func listen(_ handler: @escaping (UInt16) -> Bool) {
        stop()
        monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { e in handler(e.keyCode) ? nil : e }
    }
    func stop() {
        if let m = monitor { NSEvent.removeMonitor(m) }
        monitor = nil
    }
    deinit { stop() }
}

struct CatalogField: View {
    let kind: Int
    let placeholder: String
    @Binding var text: String
    // What is typed here is a search until it is picked or committed: the step keeps no blanks in a name, so the
    // field must not write every keystroke into it ("f14 weapon" would become "f14weapon").
    @StateObject private var draftText = DraftText()
    @FocusState private var focused: Bool
    private let rowHeight: CGFloat = 24
    private var draft: String {
        get { draftText.value }
        nonmutating set { draftText.value = newValue }
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            TextField(placeholder, text: $draftText.value).textFieldStyle(.roundedBorder).focused($focused)
                .onAppear { draft = text }
                .onSubmit { commit() }
                .onChange(of: focused) { now in
                    if now {
                        draft = text
                        draftText.selected = nil
                        draftText.listen { key in handleKey(key) }
                    } else {
                        draftText.stop()
                        commit()
                    }
                }
                .onChange(of: text) { now in if !focused { draft = now } }
                .onChange(of: draftText.value) { _ in draftText.selected = nil }   // typing starts the list over
                .onDisappear { draftText.stop() }
            if focused && !draft.isEmpty {
                let rows = matches()
                if !rows.isEmpty && !(rows.count == 1 && rows[0][0] == draft) {
                    ScrollViewReader { proxy in
                        ScrollView(.vertical) {
                            VStack(alignment: .leading, spacing: 0) {
                                ForEach(0..<rows.count, id: \.self) { i in
                                    Button { pick(rows[i][0]) } label: {
                                        HStack(spacing: 8) {
                                            Text(rows[i][0]).lineLimit(1).truncationMode(.middle).layoutPriority(1)
                                            Text(rows[i][1] + (rows[i][2] == "0" ? "  (read-only)" : ""))
                                                .foregroundStyle(.secondary).lineLimit(1).truncationMode(.tail)
                                            Spacer(minLength: 0)
                                        }
                                        .padding(.horizontal, 6)
                                        .frame(height: rowHeight)
                                        .background(draftText.selected == i ? Color.accentColor.opacity(0.3) : Color.clear)
                                        .contentShape(Rectangle())
                                    }
                                    .buttonStyle(.plain)
                                    .id(i)
                                }
                            }
                        }
                        .scrollIndicators(.visible)
                        .frame(height: min(240, CGFloat(rows.count) * rowHeight))
                        .background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.12)))
                        .onChange(of: draftText.selected) { sel in
                            if let sel = sel { withAnimation(.easeOut(duration: 0.1)) { proxy.scrollTo(sel) } }
                        }
                    }
                }
            }
        }
    }
    private func pick(_ name: String) {
        draft = name
        text = name
        focused = false
    }
    // Down from the field goes to the first suggestion and on down the list, up goes back and out to the field,
    // Return takes the one the arrows are on. Anything else (typing, Tab) is left to the field.
    private func handleKey(_ key: UInt16) -> Bool {
        let rows = draft.isEmpty ? [] : matches()
        switch key {
        case 125:   // down
            if rows.isEmpty { return false }
            draftText.selected = min((draftText.selected ?? -1) + 1, rows.count - 1)
            return true
        case 126:   // up
            guard let s = draftText.selected else { return false }
            draftText.selected = s > 0 ? s - 1 : nil
            return true
        case 36, 76:   // return, enter
            guard let s = draftText.selected, s < rows.count else { return false }
            pick(rows[s][0])
            return true
        default:
            return false
        }
    }
    // A name with no blanks is kept as typed (add-on commands may be missing from the lists); a text with blanks
    // was a search, so the field goes back to what the step has.
    private func commit() {
        let t = draft.trimmingCharacters(in: .whitespaces)
        if t.contains(where: { $0.isWhitespace }) { draft = text; return }
        draft = t
        if t != text { text = t }
    }
    private func matches() -> [[String]] {
        str(x52_catalog_search(Int32(kind), draft, 50)).split(separator: "\n").map {
            $0.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        }.filter { $0.count >= 3 }
    }
}

let stepTags = ["CMD", "KEY", "WAIT", "DATA"]
let datarefOps = ["Set", "Toggle", "Add", "Save", "Restore"]

struct StepRow: View {
    @ObservedObject var m: Model
    let hold: Int
    let index: Int
    var body: some View {
        let b = Int32(m.btn), l = Int32(m.btnLayer), h = Int32(hold), i = Int32(index)
        let kind = Int(x52_step_kind(b, l, h, i))
        let text = Binding<String>(get: { str(x52_step_text(b, l, h, i)) }, set: { write(text: $0) })
        VStack(alignment: .leading, spacing: 4) {
            HStack(alignment: .top, spacing: 8) {
                Text(stepTags[kind]).font(.caption).frame(width: 44, height: 22)
                    .background(RoundedRectangle(cornerRadius: 4).fill(Color.secondary.opacity(0.2)))
                Group {
                    if kind == 0 {
                        CatalogField(kind: 0, placeholder: "command, e.g. sim/flight_controls/gear_toggle", text: text)
                    } else if kind == 1 {
                        TextField("key, e.g. shift+f1", text: text).textFieldStyle(.roundedBorder)
                    } else if kind == 2 {
                        HStack {
                            TextField("", value: Binding<Int>(get: { Int(x52_step_ms(b, l, h, i)) }, set: { write(ms: $0) }),
                                      format: .number).textFieldStyle(.roundedBorder).frame(width: 80)
                            Text("ms")
                            Spacer()
                        }
                    } else {
                        CatalogField(kind: 1, placeholder: "dataref, e.g. sim/cockpit2/switches/..., or name[3]", text: text)
                    }
                }
                Button { x52_step_move(b, l, h, i, -1); m.tick() } label: { Image(systemName: "chevron.up") }
                    .disabled(index == 0)
                Button { x52_step_move(b, l, h, i, 1); m.tick() } label: { Image(systemName: "chevron.down") }
                    .disabled(index >= Int(x52_step_count(b, l, h)) - 1)
                Button { x52_step_remove(b, l, h, i); m.tick() } label: { Image(systemName: "xmark") }
            }
            if kind == 3 {
                let op = Binding<Int>(get: { Int(x52_step_op(b, l, h, i)) }, set: { write(op: $0) })
                let n = { (which: Int) in Binding<Double>(get: { x52_step_num(b, l, h, i, Int32(which)) }, set: { write(num: which, $0) }) }
                HStack(spacing: 8) {
                    FixedSegments(selection: op, names: datarefOps, width: 64)
                    if op.wrappedValue == 0 {
                        Text("to"); TextField("", value: n(0), format: .number).textFieldStyle(.roundedBorder).frame(width: 80)
                    } else if op.wrappedValue == 1 {
                        Text("between"); TextField("", value: n(0), format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
                        Text("and"); TextField("", value: n(1), format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
                    } else if op.wrappedValue >= 3 {
                        Text(op.wrappedValue == 3 ? "remember its value" : op.wrappedValue == 4 ? "write the remembered value back" : "run a command when the remembered value was in a range (edit in the profile file)").foregroundStyle(.secondary)
                    } else {
                        Text("by"); TextField("", value: n(0), format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
                        Text("keep between"); TextField("", value: n(2), format: .number).textFieldStyle(.roundedBorder).frame(width: 60)
                        Text("and"); TextField("", value: n(3), format: .number).textFieldStyle(.roundedBorder).frame(width: 60)
                    }
                    Spacer()
                }
                .padding(.leading, 52)
            }
        }
    }
    private func write(text: String? = nil, ms: Int? = nil, op: Int? = nil, num: Int? = nil, _ v: Double = 0) {
        let b = Int32(m.btn), l = Int32(m.btnLayer), h = Int32(hold), i = Int32(index)
        var nums = (0..<4).map { x52_step_num(b, l, h, i, Int32($0)) }
        if let n = num { nums[n] = v }
        x52_step_set(b, l, h, i, text ?? str(x52_step_text(b, l, h, i)), Int32(ms ?? Int(x52_step_ms(b, l, h, i))),
                     Int32(op ?? Int(x52_step_op(b, l, h, i))), nums[0], nums[1], nums[2], nums[3])
        m.tick()
    }
}

struct StepList: View {
    @ObservedObject var m: Model
    let hold: Int
    let title: String
    var body: some View {
        let b = Int32(m.btn), l = Int32(m.btnLayer), h = Int32(hold)
        let label = Binding<String>(get: { str(x52_bind_label(b, l, h)) },
                                    set: { x52_bind_set_label(b, l, h, String($0.prefix(16))); m.tick() })
        VStack(alignment: .leading, spacing: 8) {
            Text(title).foregroundStyle(.orange)
            if hold != 2 {   // a release has no name of its own
                HStack {
                    Text("Name on screen").frame(width: 110, alignment: .leading)
                    TextField("up to 16 characters", text: label).textFieldStyle(.roundedBorder).frame(width: 220)
                }
            }
            ForEach(0..<Int(x52_step_count(b, l, h)), id: \.self) { StepRow(m: m, hold: hold, index: $0) }
            HStack {
                Button("+ Command") { _ = x52_step_add(b, l, h, 0); m.tick() }
                Button("+ Keystroke") { _ = x52_step_add(b, l, h, 1); m.tick() }
                Button("+ Dataref") { _ = x52_step_add(b, l, h, 3); m.tick() }
                if hold == 2 {
                    Text("remember a dataref when pressed, put it back here").font(.caption).foregroundStyle(.secondary)
                }
                Button("+ Wait") { _ = x52_step_add(b, l, h, 2); m.tick() }
            }
        }
    }
}

// The buttons this profile programs: each needs to be set to Do nothing in X-Plane's Settings > Joystick.
struct ClearList: View {
    @ObservedObject var m: Model
    var body: some View {
        let rows = str(x52_bound_buttons()).split(separator: "\n").map {
            $0.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        }.filter { $0.count >= 5 }
        let still = rows.filter { !$0[4].isEmpty && $0[4] != "sim/none/none" }.count
        DisclosureGroup("Set to Do nothing in X-Plane: \(rows.count) button\(rows.count == 1 ? "" : "s")"
                        + (still > 0 ? ", \(still) still set in X-Plane" : "")) {
            VStack(alignment: .leading, spacing: 3) {
                if rows.isEmpty {
                    Text("No button is programmed here yet.").foregroundStyle(.secondary)
                }
                ForEach(0..<rows.count, id: \.self) { i in
                    HStack(alignment: .top, spacing: 8) {
                        Text(rows[i][0]).frame(width: 24, alignment: .trailing).foregroundStyle(.secondary)
                        Text(rows[i][1]).frame(width: 90, alignment: .leading)
                        Text(rows[i][2]).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                            .frame(width: 200, alignment: .leading)
                        if rows[i][3] == "-1" {
                            Text("X-Plane Btn: not known yet").foregroundStyle(.secondary)
                                .help("With X-Plane running and the plugin loaded, press this button on the stick once; the plugin finds X-Plane's number for it.")
                        } else if rows[i][4].isEmpty {
                            Text("X-Plane Btn \(rows[i][3])").foregroundStyle(.secondary)
                        } else if rows[i][4] == "sim/none/none" {
                            Text("X-Plane Btn \(rows[i][3]): Do nothing, as it should be").foregroundStyle(.green)
                        } else {
                            Text("X-Plane Btn \(rows[i][3]): still \(rows[i][4])").foregroundStyle(.orange)
                        }
                    }
                }
                Text("In X-Plane: Settings > Joystick > Buttons, press each button on the stick and choose Do nothing. Otherwise X-Plane and the plugin both act on it. The number is the Btn number X-Plane shows for that button; the plugin learns it when you press the button with X-Plane running. Assignments are read from X-Plane's file \(str(x52_assignment_source())) (the one for the aircraft last loaded), which X-Plane saves when you leave its settings.")
                    .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).padding(.top, 4)
            }
            .padding(.top, 4)
        }
    }
}

// The colour of the light that sits on a button, as set for the layer being edited (for Any mode: the stick's mode
// now). Only a picture; the colours are set on the Lights tab. A button without a light leaves the space empty.
struct ButtonLightDot: View {
    @ObservedObject var m: Model
    let button: Int
    var body: some View {
        let led = Int(x52_button_led(Int32(button)))
        let layer = m.btnLayer
        let mode = layer == 0 ? min(3, max(1, m.mode)) : (layer - 1) % 3 + 1
        let pinkie = layer == 0 ? m.shift : layer >= 4
        let color = led >= 0 ? Int(x52_get_led(Int32(mode), Int32(led), pinkie ? 1 : 0)) : 0
        Group {
            if led >= 0 {
                Circle().fill(swatch[color])
                    .overlay(Circle().stroke(Color.secondary.opacity(0.5), lineWidth: 0.5))
                    .help("\(str(x52_led_title(Int32(led)))): \(led >= 0 && x52_led_single(Int32(led)) != 0 && color == 1 ? "on" : colorNames[color]), mode \(mode)\(pinkie ? " with the pinkie" : "")")
            } else {
                Color.clear
            }
        }
        .frame(width: 11, height: 11)
    }
}

struct ButtonsTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let layer = Binding<Int>(get: { m.btnLayer }, set: { m.btnLayer = $0 })
        VStack(alignment: .leading, spacing: 10) {
            VStack(alignment: .leading, spacing: 6) {
                Text("To use the buttons here, start from a blank profile (New blank profile in the menu next to the profile picker), or import what X-Plane has on the stick's buttons into a new profile. Each button you program here must then be set to Do nothing in X-Plane's Settings > Joystick, or X-Plane and the plugin both act on it.")
                    .font(.callout).fixedSize(horizontal: false, vertical: true)
                if m.importOpen {
                    let names = str(x52_xplane_profiles()).split(separator: "\n").map(String.init)
                    HStack {
                        Text("Import from").frame(width: 80, alignment: .leading)
                        Picker("", selection: Binding(get: { m.importFrom }, set: {
                            m.importFrom = $0
                            m.importName = str(x52_import_suggest($0))
                        })) {
                            ForEach(names, id: \.self) { Text($0).tag($0) }
                        }
                        .labelsHidden().frame(width: 240)
                    }
                    HStack {
                        Text("New profile").frame(width: 80, alignment: .leading)
                        TextField("name", text: Binding(get: { m.importName }, set: { m.importName = $0 }))
                            .textFieldStyle(.roundedBorder).frame(width: 240)
                        Button("Create") {
                            let n = Int(x52_import_profile(m.importFrom, m.importName))
                            m.importNote = str(x52_import_message())
                            if n >= 0 { m.importOpen = false }
                            m.tick()
                        }
                        Button("Cancel") { m.importOpen = false; m.importNote = "" }
                    }
                    Text("A new profile is made; existing profiles are never changed.").font(.callout).foregroundStyle(.secondary)
                } else {
                    Button("Import what X-Plane has...") {
                        let names = str(x52_xplane_profiles()).split(separator: "\n").map(String.init)
                        m.importFrom = names.first ?? ""
                        m.importName = str(x52_import_suggest(m.importFrom))
                        m.importNote = ""
                        m.importOpen = true
                    }
                }
                if !m.importNote.isEmpty {
                    Text(m.importNote).font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                }
            }
            .padding(8).background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.1)))
            HStack(spacing: 8) {
                Text("Programming for").foregroundStyle(.secondary)
                Picker("", selection: layer) {
                    ForEach(0..<7, id: \.self) { Text(str(x52_layer_name(Int32($0)))).tag($0) }
                }
                .labelsHidden().frame(width: 190)
                Spacer()
            }
            HStack(alignment: .top, spacing: 14) {
                ScrollViewReader { proxy in
                ScrollView {
                    VStack(spacing: 0) {
                        ForEach(1...51, id: \.self) { b in
                            let sel = m.btn == b
                            let res = x52_button_reserved(Int32(b)) != 0
                            Button { m.btn = b } label: {
                                HStack(spacing: 8) {
                                    ButtonLightDot(m: m, button: b)
                                    Text("\(b)").frame(width: 24, alignment: .trailing).foregroundStyle(.secondary)
                                    Text(str(x52_button_name(Int32(b)))).lineLimit(1)
                                    Spacer(minLength: 4)
                                    Text(res ? "reserved" : str(x52_bind_summary(Int32(b), Int32(m.btnLayer))))
                                        .foregroundStyle(res ? Color.secondary : Color.green).lineLimit(1).truncationMode(.tail)
                                }
                                .padding(.vertical, 4).padding(.horizontal, 6)
                                .background(sel ? Color.accentColor.opacity(0.25) : Color.clear)
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                            .id(b)
                        }
                    }
                }
                .onChange(of: m.btn) { now in proxy.scrollTo(now) }   // a press on the stick may pick a row out of view
                }
                .frame(width: 275)
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.25)))
                ScrollView {
                    editor.padding(.trailing, 8)
                }
            }
            ClearList(m: m)
            Text("The numbers are the usual X52 button numbers (JOY1 to JOY39 in Windows lists): 9 and 10 are toggle 1 up and down, 11 and 12 toggle 2, 13 and 14 toggle 3, 15 the trigger's second stage, 16 the mouse button, 17 to 19 the scroll wheel, 20 to 23 the stick's hat, 24 to 27 the throttle's hat, 40 to 43 the stick's main hat (POV 1, which the stick reports as a hat switch, not as buttons: a diagonal holds two), 44 to 47 the throttle's mouse nub (arrows), 48 to 51 the nub pushed all the way (Shift + arrows, a faster pan), 28 to 30 the mode dial, 31 the clutch.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Text("Searches use X-Plane's own lists of commands and datarefs, so this works without X-Plane running. A button you program here must be set to Do nothing in X-Plane's Settings > Joystick.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
    @ViewBuilder private var editor: some View {
        let b = Int32(m.btn), l = Int32(m.btnLayer)
        VStack(alignment: .leading, spacing: 14) {
            Text("\(m.btn)  \(str(x52_button_name(b)))  -  \(str(x52_layer_name(l)))").font(.headline)
            if x52_button_reserved(b) != 0 {
                Text("The mode dial, the pinkie switch and the clutch work as switches and cannot carry assignments.")
                    .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            } else {
                if x52_catalog_size(0) == 0 {
                    Text("X-Plane's command list was not found (the X-Plane folder is shown at the bottom of the window). You can still type names by hand.")
                        .foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
                }
                StepList(m: m, hold: 0, title: "When pressed")
                Divider()
                StepList(m: m, hold: 1, title: "When held longer than 0.4 s (optional)")
                Divider()
                StepList(m: m, hold: 2, title: "When released (optional)")
                Divider()
                Button("Clear this button in \(str(x52_layer_name(l)))") {
                    guard confirm("Clear button \(m.btn) in \(str(x52_layer_name(l)))?",
                                  "Its commands, keys and name on the screen are removed.", "Clear") else { return }
                    x52_bind_clear(b, l); m.tick()
                }
                    .disabled(x52_bind_exists(b, l) == 0)
            }
        }
    }
}

// MARK: - Master switches

struct MasterBlock: View {
    @ObservedObject var m: Model
    let which: Int          // 0 screen, 1 lights
    let title: String
    var body: some View {
        let hasRef = !str(x52_trg_ref(2, Int32(which), 0)).isEmpty
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.headline)
            Text(hasRef ? "On while: \(triggerSummary(2, which))" : "Always on (no dataref chosen).")
                .foregroundStyle(.secondary)
            TriggerEditor(m: m, kind: 2, index: which)
            HStack {
                Button("Use avionics power") {
                    x52_trg_set(2, Int32(which), 0, "sim/cockpit2/switches/avionics_power_on", 4, 1, 0)
                    x52_trg_set_join(2, Int32(which), 0)
                    m.tick()
                }
                Button("Use battery") {
                    x52_trg_set(2, Int32(which), 0, "sim/cockpit/electrical/battery_on", 4, 1, 0)
                    x52_trg_set_join(2, Int32(which), 0)
                    m.tick()
                }
                Button("Always on") {
                    x52_trg_set(2, Int32(which), 0, "", 4, 1, 0)
                    x52_trg_set_join(2, Int32(which), 0)
                    m.tick()
                }
                Spacer()
                Button("Try it off on the stick") { x52_master_try(Int32(which)); m.tick() }
                    .disabled(m.pluginActive || !m.connected)
                    .help("Switches it off on the stick for a few seconds, whatever the simulator says")
                if m.masterTried == which { Text("Off for a few seconds").foregroundStyle(.secondary) }
            }
        }
    }
}

struct BrightBlock: View {
    @ObservedObject var m: Model
    let which: Int
    let title: String
    var body: some View {
        let w = Int32(which)
        let ref = Binding<String>(get: { str(x52_bright_ref(w)) }, set: { write(ref: $0) })
        let inLow = Binding<Double>(get: { x52_bright_in_low(w) }, set: { write(inLow: $0) })
        let inHigh = Binding<Double>(get: { x52_bright_in_high(w) }, set: { write(inHigh: $0) })
        let outLow = Binding<Double>(get: { Double(x52_bright_out_low(w)) }, set: { write(outLow: Int($0.rounded())) })
        let outHigh = Binding<Double>(get: { Double(x52_bright_out_high(w)) }, set: { write(outHigh: Int($0.rounded())) })
        let on = !ref.wrappedValue.isEmpty
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.headline)
            HStack {
                Text("Dataref").frame(width: 110, alignment: .leading)
                TextField("empty: the fixed brightness slider applies", text: ref).textFieldStyle(.roundedBorder)
            }
            if let status = m.liveStatus[statusKey(3, which, 0)], !status.isEmpty {
                Text(status).font(.callout).foregroundStyle(.secondary).padding(.leading, 118)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if on {
                HStack {
                    Text("Value range").frame(width: 110, alignment: .leading)
                    TextField("", value: inLow, format: .number).textFieldStyle(.roundedBorder).frame(width: 80)
                    Text("(dim)  to")
                    TextField("", value: inHigh, format: .number).textFieldStyle(.roundedBorder).frame(width: 80)
                    Text("(bright)")
                }
                HStack {
                    Text("Dimmest").frame(width: 110, alignment: .leading)
                    Slider(value: outLow, in: 0...127)
                    Text("\(Int(outLow.wrappedValue) * 100 / 127) %").monospacedDigit().frame(width: 48, alignment: .trailing)
                }
                HStack {
                    Text("Brightest").frame(width: 110, alignment: .leading)
                    Slider(value: outHigh, in: 0...127)
                    Text("\(Int(outHigh.wrappedValue) * 100 / 127) %").monospacedDigit().frame(width: 48, alignment: .trailing)
                }
            }
            HStack {
                Button("Use panel light dial") {
                    x52_bright_set(w, "sim/cockpit2/electrical/instrument_brightness_ratio[0]", 0, 1, 10, 127)
                    m.tick()
                }
                Button("Fixed brightness") { x52_bright_set(w, "", 0, 1, 10, 127); m.tick() }
                Spacer()
                Button("Try dim") { x52_bright_try(w, 0); m.tick() }.disabled(!on || m.pluginActive || !m.connected)
                Button("Try bright") { x52_bright_try(w, 1); m.tick() }.disabled(!on || m.pluginActive || !m.connected)
                if m.brightTried == which { Text("Showing for a few seconds").foregroundStyle(.secondary) }
            }
        }
    }
    private func write(ref: String? = nil, inLow: Double? = nil, inHigh: Double? = nil, outLow: Int? = nil, outHigh: Int? = nil) {
        let w = Int32(which)
        x52_bright_set(w, ref ?? str(x52_bright_ref(w)), inLow ?? x52_bright_in_low(w), inHigh ?? x52_bright_in_high(w),
                       Int32(outLow ?? Int(x52_bright_out_low(w))), Int32(outHigh ?? Int(x52_bright_out_high(w))))
        m.tick()
    }
}

struct MasterTab: View {
    @ObservedObject var m: Model
    var body: some View {
        ScrollView {
        VStack(alignment: .leading, spacing: 14) {
            Text("A master switch keeps the screen, or every light, on only while a dataref has the value you choose, for example while the avionics are powered. When it is false the screen goes blank or the lights go dark. With no dataref chosen, or if X-Plane does not know it, it stays on.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            MasterBlock(m: m, which: 0, title: "Screen")
            Divider()
            MasterBlock(m: m, which: 1, title: "Lights")
            Divider()
            Text("Brightness can follow a dataref instead of the fixed sliders, for example the panel light dial: the value runs between the two points and the brightness between dimmest and brightest. With no dataref, or one X-Plane does not know, the slider on the Lights and Screen tabs applies.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            BrightBlock(m: m, which: 0, title: "Screen brightness")
            Divider()
            BrightBlock(m: m, which: 1, title: "Light brightness")
        }
        .padding(.trailing, 8)
        }
    }
}

// MARK: - Test

struct TestTab: View {
    @ObservedObject var m: Model
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(alignment: .top, spacing: 24) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("On the stick's screen").foregroundStyle(.secondary)
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(0..<3, id: \.self) { i in
                            Text(m.screen[i].isEmpty ? " " : m.screen[i]).font(.system(.body, design: .monospaced))
                        }
                    }
                    .padding(8).frame(width: 190, alignment: .leading)
                    .background(RoundedRectangle(cornerRadius: 6).fill(Color.black.opacity(0.8)))
                    .foregroundStyle(Color(red: 0.4, green: 0.95, blue: 0.5))
                    if m.pluginActive {
                        Text("Shown by X-Plane right now.").font(.callout).foregroundStyle(.secondary)
                    }
                }
                VStack(alignment: .leading, spacing: 8) {
                    Text("Held now").foregroundStyle(.secondary)
                    Text(m.held.isEmpty ? "Nothing" : m.held).fixedSize(horizontal: false, vertical: true)
                    Text("Last moved").foregroundStyle(.secondary)
                    Text(m.moved.isEmpty ? "Move the stick, throttle, a rotary or the slider" : m.moved)
                }
            }
            Divider()
            Text("Write on the stick's screen (16 characters a line)").foregroundStyle(.secondary)
            ForEach(0..<3, id: \.self) { i in
                TextField("Line \(i + 1)", text: Binding(get: { m.testLines[i] }, set: { m.testLines[i] = String($0.prefix(16)) }))
                    .font(.system(.body, design: .monospaced)).frame(width: 220)
            }
            HStack {
                Button("Show on stick") { x52_text(m.testLines[0], m.testLines[1], m.testLines[2]); m.tick() }
                Button("Back to normal") { x52_clear_text(); m.tick() }
            }
            .disabled(m.pluginActive || !m.connected)
            Divider()
            Text("Alarms: see how each one looks on the stick").foregroundStyle(.secondary)
            let alarms = Int(x52_alarm_count())
            if alarms == 0 {
                Text("No alarms yet. Add some on the Alarms tab.").foregroundStyle(.secondary)
            }
            ScrollView {
                VStack(alignment: .leading, spacing: 6) {
                    ForEach(0..<alarms, id: \.self) { i in
                        HStack(spacing: 10) {
                            Button("Try") { x52_alarm_try(Int32(i)); m.tick() }
                                .disabled(m.pluginActive || !m.connected)
                            Text(str(x52_alarm_name(Int32(i)))).fontWeight(.medium).frame(width: 140, alignment: .leading).lineLimit(1)
                            Text("\(str(x52_led_title(x52_alarm_led(Int32(i))))), \(alarmEffectText(Int32(i)))")
                                .foregroundStyle(.secondary)
                            if m.tried == i { Text("showing for a few seconds").foregroundStyle(.secondary) }
                        }
                    }
                }
            }
            // Takes whatever height the window has left, so a taller window shows more alarms.
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        }
        .onAppear { x52_test(1) }
        .onDisappear { x52_test(0) }
    }
}

// MARK: - Axes

struct AxisCurveRow: View {
    @ObservedObject var m: Model
    let title: String
    let key: String
    let usage: Int
    var body: some View {
        let dz = Binding<Int>(get: { m.get("axis_\(key)_deadzone") }, set: { m.set("axis_\(key)_deadzone", $0) })
        let ex = Binding<Int>(get: { m.get("axis_\(key)_expo") }, set: { m.set("axis_\(key)_expo", $0) })
        let inv = Binding<Bool>(get: { m.get("axis_\(key)_invert") != 0 }, set: { m.set("axis_\(key)_invert", $0 ? 1 : 0) })
        let row = m.axes.first { $0.usage == usage }
        HStack(spacing: 12) {
            Text(title).frame(width: 120, alignment: .leading).foregroundStyle(row?.moving == true ? Color.green : Color.primary)
            ProgressView(value: Double(row?.percent ?? 0), total: 100).frame(width: 110)
            Text(row.map { "\($0.percent) %" } ?? "-").frame(width: 48, alignment: .trailing).monospacedDigit()
            Stepper("Dead zone \(dz.wrappedValue) %", value: dz, in: 0...40).frame(width: 170, alignment: .leading)
            Stepper("Expo \(ex.wrappedValue) %", value: ex, in: 0...100, step: 5).frame(width: 150, alignment: .leading)
            Toggle("Invert", isOn: inv)
        }
    }
}

// The stick's own centre and ends, measured here or in X-Plane's plugin window (the same file).
struct CalibrationRow: View {
    @ObservedObject var m: Model
    var body: some View {
        let status = m.calibration.first ?? ""
        let measured = Array(m.calibration.dropFirst())
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 12) {
                Text("Calibration").frame(width: 120, alignment: .leading)
                Button("Calibrate stick...") { m.openCalibration() }
                    .help("Opens a window that guides you step by step: the stick to its four edges, the twist both ways, let go, save.")
                Button("Reset") { x52_calibrate(3); m.tick() }
                    .disabled(measured.isEmpty || m.calibrating)
                    .help("Forget the calibration: the stick's full range is used again.")
                Text(m.calibrating ? "Calibrating..." : status).foregroundStyle(m.calibrating ? Color.orange : Color.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            ForEach(measured, id: \.self) { line in
                Text(line).font(.callout.monospacedDigit()).foregroundStyle(.secondary).padding(.leading, 132)
            }
            Text("Measures where your stick really rests and how far it really goes (min .. centre .. max), so the centre is exactly zero and the ends give full travel. It belongs to the stick, not to a profile, and is used by the plugin while it drives the stick. Do it with the aircraft on the ground or paused: the stick still flies the aircraft meanwhile.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
        }
    }
}

// MARK: - Stick calibration window

// One stick axis as the calibration window sees it: where it is now and how far it has gone each way.
struct CalAxis: Equatable {
    var usage = 0, value = 0, lo = 0, hi = 0, min = 0, max = 0
    var mid: Double { Double(min + max) / 2 }
    // -1 .. 1 of the HID range (the middle of the range is 0)
    var pos: Double { max > min ? Swift.max(-1, Swift.min(1, (Double(value) - mid) / ((Double(max) - Double(min)) / 2))) : 0 }
    // How far each side has been reached, 0 .. 1 of that half of the range.
    var lowReach: Double { max > min ? Swift.max(0, Swift.min(1, (mid - Double(lo)) / (mid - Double(min)))) : 0 }
    var highReach: Double { max > min ? Swift.max(0, Swift.min(1, (Double(hi) - mid) / (Double(max) - mid))) : 0 }
}

func parseCalLive(_ text: String) -> [Int: CalAxis] {
    var out: [Int: CalAxis] = [:]
    for line in text.split(separator: "\n") {
        let f = line.split(separator: "\t").compactMap { Int($0) }
        guard f.count >= 6 else { continue }
        out[f[0]] = CalAxis(usage: f[0], value: f[1], lo: f[2], hi: f[3], min: f[4], max: f[5])
    }
    return out
}

let kCalDone = 0.9    // a side counts as reached (green) at 90 % of its half of the range
let kCalEnough = 0.5  // the least the calibration accepts (a quarter of the whole range)

// The stick seen from above: a circle with a target dot at each edge, a bar growing from the centre towards
// each edge as far as the stick has gone, and the stick's position now.
struct StickTarget: View {
    let x: CalAxis?
    let y: CalAxis?
    var centreZone = false   // show the middle zone the stick must rest in
    var body: some View {
        Canvas { ctx, size in
            let r = Swift.min(size.width, size.height) / 2 - 16
            let c = CGPoint(x: size.width / 2, y: size.height / 2)
            ctx.stroke(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)), with: .color(.secondary.opacity(0.6)), lineWidth: 2)
            var cross = Path()
            cross.move(to: CGPoint(x: c.x - r, y: c.y)); cross.addLine(to: CGPoint(x: c.x + r, y: c.y))
            cross.move(to: CGPoint(x: c.x, y: c.y - r)); cross.addLine(to: CGPoint(x: c.x, y: c.y + r))
            ctx.stroke(cross, with: .color(.secondary.opacity(0.25)), lineWidth: 1)
            if centreZone {
                let z = r * 0.3
                let inside = abs(x?.pos ?? 1) <= 0.3 && abs(y?.pos ?? 1) <= 0.3
                ctx.fill(Path(ellipseIn: CGRect(x: c.x - z, y: c.y - z, width: 2 * z, height: 2 * z)),
                         with: .color((inside ? Color.green : Color.orange).opacity(0.25)))
            } else {
                // up: stick forward (the low end of fwd/back), down: back, left / right: the ends of left/right
                let sides: [(CGFloat, CGFloat, Double)] = [(0, -1, y?.lowReach ?? 0), (0, 1, y?.highReach ?? 0),
                                                           (-1, 0, x?.lowReach ?? 0), (1, 0, x?.highReach ?? 0)]
                for (dx, dy, reach) in sides {
                    var p = Path()
                    p.move(to: c)
                    p.addLine(to: CGPoint(x: c.x + dx * r * reach, y: c.y + dy * r * reach))
                    ctx.stroke(p, with: .color(reach >= kCalDone ? .green : .accentColor), style: StrokeStyle(lineWidth: 10, lineCap: .round))
                    let d = CGPoint(x: c.x + dx * r, y: c.y + dy * r)
                    ctx.fill(Path(ellipseIn: CGRect(x: d.x - 9, y: d.y - 9, width: 18, height: 18)),
                             with: .color(reach >= kCalDone ? .green : .secondary.opacity(0.5)))
                }
            }
            let p = CGPoint(x: c.x + r * (x?.pos ?? 0), y: c.y + r * (y?.pos ?? 0))
            ctx.fill(Path(ellipseIn: CGRect(x: p.x - 7, y: p.y - 7, width: 14, height: 14)), with: .color(.primary))
        }
        .frame(width: 260, height: 260)
    }
}

// The twist seen from above: an arc grows left and right of the top as far as the stick has been turned, with a
// target dot at each end and a pointer showing the twist now.
struct TwistRing: View {
    let z: CalAxis?
    var body: some View {
        Canvas { ctx, size in
            let r = Swift.min(size.width, size.height) / 2 - 16
            let c = CGPoint(x: size.width / 2, y: size.height / 2)
            let sweep = 135.0   // degrees each way for a full twist
            ctx.stroke(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)), with: .color(.secondary.opacity(0.6)), lineWidth: 2)
            func point(_ deg: Double, _ radius: CGFloat) -> CGPoint {
                let a = (deg - 90) * .pi / 180
                return CGPoint(x: c.x + radius * CGFloat(cos(a)), y: c.y + radius * CGFloat(sin(a)))
            }
            for (sign, reach) in [(-1.0, z?.lowReach ?? 0), (1.0, z?.highReach ?? 0)] {
                var arc = Path()
                arc.addArc(center: c, radius: r, startAngle: .degrees(-90), endAngle: .degrees(-90 + sign * sweep * reach), clockwise: sign < 0)
                ctx.stroke(arc, with: .color(reach >= kCalDone ? .green : .accentColor), style: StrokeStyle(lineWidth: 10, lineCap: .round))
                let d = point(sign * sweep, r)
                ctx.fill(Path(ellipseIn: CGRect(x: d.x - 9, y: d.y - 9, width: 18, height: 18)),
                         with: .color(reach >= kCalDone ? .green : .secondary.opacity(0.5)))
            }
            var needle = Path()
            needle.move(to: c)
            needle.addLine(to: point(sweep * (z?.pos ?? 0), r - 14))
            ctx.stroke(needle, with: .color(.primary), style: StrokeStyle(lineWidth: 4, lineCap: .round))
            ctx.fill(Path(ellipseIn: CGRect(x: c.x - 6, y: c.y - 6, width: 12, height: 12)), with: .color(.primary))
        }
        .frame(width: 260, height: 260)
    }
}

struct CalValues: View {
    let title: String
    let a: CalAxis?
    var body: some View {
        HStack(spacing: 14) {
            Text(title).frame(width: 110, alignment: .leading)
            if let a = a {
                Text("now \(a.value)").frame(width: 80, alignment: .leading)
                Text("reached \(a.lo) .. \(a.hi)").frame(width: 160, alignment: .leading)
                Text("of \(a.min) .. \(a.max)").foregroundStyle(.secondary)
            } else {
                Text("not reported yet: move it").foregroundStyle(.secondary)
            }
        }
        .font(.body.monospacedDigit())
    }
}

// A guided calibration, one step at a time: the stick to its four edges, the twist both ways, let go, save.
// Nothing changes until Save; the plugin picks the saved calibration up within a second.
struct CalibrationWizard: View {
    @ObservedObject var m: Model
    let close: () -> Void
    private let steps = ["Start", "Stick", "Twist", "Let go", "Save"]

    var body: some View {
        let x = m.calLive[0x30], y = m.calLive[0x31], z = m.calLive[0x35]
        VStack(alignment: .leading, spacing: 16) {
            HStack(spacing: 8) {
                ForEach(0..<steps.count, id: \.self) { i in
                    Text("\(i + 1)  \(steps[i])")
                        .padding(.horizontal, 10).padding(.vertical, 4)
                        .background(Capsule().fill(i == Swift.min(m.calStep, 4) ? Color.accentColor : Color.secondary.opacity(0.15)))
                        .foregroundStyle(i == Swift.min(m.calStep, 4) ? Color.white : (i < m.calStep ? Color.primary : Color.secondary))
                }
            }
            Divider()
            switch m.calStep {
            case 0: start
            case 1:
                Text("Step 1: the stick").font(.title3)
                Text("Push the stick all the way forward, back, left and right, one edge at a time. Each bar grows as you go; push until all four dots turn green.")
                    .fixedSize(horizontal: false, vertical: true)
                HStack { Spacer(); StickTarget(x: x, y: y); Spacer() }
                CalValues(title: "Left/right", a: x)
                CalValues(title: "Fwd/back", a: y)
                let reach = [x?.lowReach ?? 0, x?.highReach ?? 0, y?.lowReach ?? 0, y?.highReach ?? 0]
                footer(next: "Next", enabled: reach.allSatisfy { $0 >= kCalEnough },
                       hint: reach.allSatisfy { $0 >= kCalDone } ? "All four edges reached." : "Keep going until every dot is green.") { m.calStep = 2 }
            case 2:
                Text("Step 2: the twist").font(.title3)
                Text("Hold the stick in the middle and turn it all the way left, then all the way right, until both dots turn green. If you do not use the twist, skip this m.calStep: it keeps its full range.")
                    .fixedSize(horizontal: false, vertical: true)
                HStack { Spacer(); TwistRing(z: z); Spacer() }
                CalValues(title: "Twist", a: z)
                let reach = [z?.lowReach ?? 0, z?.highReach ?? 0]
                HStack {
                    Button("Skip the twist") { m.calTwistSkipped = true; m.calStep = 3 }
                    Spacer()
                }
                footer(next: "Next", enabled: reach.allSatisfy { $0 >= kCalEnough },
                       hint: reach.allSatisfy { $0 >= kCalDone } ? "Both ends reached." : "Keep turning until both dots are green.") { m.calTwistSkipped = false; m.calStep = 3 }
            case 3:
                Text("Step 3: let go").font(.title3)
                Text("Let go of the stick and the twist so they spring back to where they rest. That resting point becomes the exact centre. The circle turns green when the stick is resting in the middle.")
                    .fixedSize(horizontal: false, vertical: true)
                let resting = abs(x?.pos ?? 1) <= 0.3 && abs(y?.pos ?? 1) <= 0.3 && (m.calTwistSkipped || abs(z?.pos ?? 0) <= 0.3)
                HStack { Spacer(); StickTarget(x: x, y: y, centreZone: true); Spacer() }
                CalValues(title: "Left/right", a: x)
                CalValues(title: "Fwd/back", a: y)
                if !m.calTwistSkipped { CalValues(title: "Twist", a: z) }
                footer(next: "Next", enabled: resting, hint: resting ? "Resting in the middle." : "Let go of the stick.") { m.calStep = 4 }
            case 4:
                Text("Step 4: save").font(.title3)
                Text("This is what was measured (raw values from the stick). Save it and the plugin uses it from now on, within a second, also while X-Plane is running.")
                    .fixedSize(horizontal: false, vertical: true)
                Grid(alignment: .leading, horizontalSpacing: 24, verticalSpacing: 8) {
                    GridRow { Text("Axis").bold(); Text("Min").bold(); Text("Centre").bold(); Text("Max").bold() }
                    summaryRow("Left/right", x)
                    summaryRow("Fwd/back", y)
                    if m.calTwistSkipped { GridRow { Text("Twist"); Text("full range").foregroundStyle(.secondary) } } else { summaryRow("Twist", z) }
                }
                .font(.body.monospacedDigit())
                if !m.calProblem.isEmpty { Text(m.calProblem).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true) }
                Spacer()
                HStack {
                    Button("Back") { m.calProblem = ""; m.calStep = 3 }
                    Spacer()
                    Button("Cancel") { cancel() }
                    Button("Save calibration") { save() }.keyboardShortcut(.defaultAction)
                }
            default:
                Text("Calibration saved").font(.title3)
                Text("The plugin uses it from now on. Your profiles (curves, smoothing, damping) are unchanged.")
                    .fixedSize(horizontal: false, vertical: true)
                ForEach(m.calibration.dropFirst(), id: \.self) { Text($0).font(.body.monospacedDigit()) }
                Spacer()
                HStack { Spacer(); Button("Close") { close() }.keyboardShortcut(.defaultAction) }
            }
        }
        .padding(20)
        .frame(minWidth: 560, minHeight: 640, alignment: .topLeading)
    }

    private var start: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Calibrate the stick").font(.title3)
            Text("Every X52 rests a little differently and reaches its edges at slightly different values. Calibrating measures your stick, so its resting point is exactly the centre and its edges give full travel. It takes about half a minute.")
                .fixedSize(horizontal: false, vertical: true)
            Text("Do it with the aircraft on the ground or paused: the stick still flies the aircraft meanwhile. Nothing changes until you press Save at the end.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Text("Now: \(m.calibration.first ?? "")").foregroundStyle(.secondary)
            ForEach(m.calibration.dropFirst(), id: \.self) { Text($0).font(.body.monospacedDigit()).foregroundStyle(.secondary) }
            if !m.connected { Text("The stick is not connected.").foregroundStyle(.orange) }
            Spacer()
            HStack {
                Spacer()
                Button("Cancel") { cancel() }
                Button("Start") { x52_calibrate(1); m.tick(); m.calProblem = ""; m.calStep = 1 }
                    .keyboardShortcut(.defaultAction).disabled(!m.connected)
            }
        }
    }

    @ViewBuilder private func footer(next: String, enabled: Bool, hint: String, action: @escaping () -> Void) -> some View {
        Spacer()
        HStack {
            Text(hint).foregroundStyle(enabled ? Color.secondary : Color.orange)
            Spacer()
            Button("Cancel") { cancel() }
            Button(next, action: action).keyboardShortcut(.defaultAction).disabled(!enabled)
        }
    }

    @ViewBuilder private func summaryRow(_ title: String, _ a: CalAxis?) -> some View {
        GridRow {
            Text(title)
            Text(a.map { String(Swift.min($0.lo, $0.value)) } ?? "-")
            Text(a.map { String($0.value) } ?? "-")
            Text(a.map { String(Swift.max($0.hi, $0.value)) } ?? "-")
        }
    }

    private func save() {
        x52_calibrate(2)
        m.tick()
        if x52_calibrating() == 0 {
            m.calProblem = ""
            m.calStep = 5
        } else {
            m.calProblem = m.calibration.first ?? "The calibration could not be saved."
        }
    }

    private func cancel() {
        if x52_calibrating() != 0 { x52_calibrate(0) }
        m.tick()
        close()
    }
}

struct DialSteadyRow: View {
    @ObservedObject var m: Model
    let title: String
    let key: String
    let usage: Int
    var body: some View {
        let st = Binding<Int>(get: { m.get("steady_\(key)") }, set: { m.set("steady_\(key)", $0) })
        let row = m.axes.first { $0.usage == usage }
        HStack(spacing: 12) {
            Text(title).frame(width: 120, alignment: .leading).foregroundStyle(row?.moving == true ? Color.green : Color.primary)
            ProgressView(value: Double(row?.percent ?? 0), total: 100).frame(width: 110)
            Text(row.map { "\($0.percent) %" } ?? "-").frame(width: 48, alignment: .trailing).monospacedDigit()
            Stepper("Steady \(st.wrappedValue) %", value: st, in: 0...20).frame(width: 170, alignment: .leading)
            if key != "slider" {   // the dials: their job in X-Plane, and which way they turn
                Picker("", selection: Binding<Int>(get: { m.get("rotary_\(key)") }, set: { m.set("rotary_\(key)", $0) })) {
                    ForEach(0..<Int(x52_dial_use_count()), id: \.self) { j in
                        Text(String(cString: x52_dial_use_name(Int32(j)))).tag(j)
                    }
                }
                .labelsHidden().frame(width: 130)
                Toggle("Invert", isOn: Binding(get: { m.get("rotary_\(key)_invert") != 0 }, set: { m.set("rotary_\(key)_invert", $0 ? 1 : 0) }))
                    .disabled(m.get("rotary_\(key)") == 0)
            }
        }
        if key != "slider" && m.get("rotary_\(key)") >= 3 {   // a lever job: the part of its range a full turn covers
            let from = Binding<Int>(get: { m.get("rotary_\(key)_from") }, set: { m.set("rotary_\(key)_from", $0) })
            let to = Binding<Int>(get: { m.get("rotary_\(key)_to") }, set: { m.set("rotary_\(key)_to", $0) })
            HStack(spacing: 12) {
                Text("").frame(width: 120)
                Stepper("From \(from.wrappedValue) %", value: from, in: 0...100, step: 5).frame(width: 150, alignment: .leading)
                Stepper("To \(to.wrappedValue) %", value: to, in: 0...100, step: 5).frame(width: 150, alignment: .leading)
            }
            .help("A full turn of the dial covers From..To percent of the job's range for this aircraft. From above To turns it the other way.")
        }
    }
}

struct AxesTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let on = Binding<Bool>(get: { m.get("axis_override") != 0 }, set: { m.set("axis_override", $0 ? 1 : 0) })
        // Scrolls: the tab is taller than the window at its smallest size.
        ScrollView {
        VStack(alignment: .leading, spacing: 10) {
            Toggle("The plugin drives the stick and twist (overrides X-Plane's own axis settings)", isOn: on)
            Text(on.wrappedValue
                 ? "While X-Plane runs, the plugin reads the stick and gives X-Plane the result. The dead zone, curve and inversion below apply."
                 : "Off: X-Plane reads the stick itself, with its own Joystick settings and null zone. The settings below wait until you switch this on.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Toggle("The plugin drives the throttle (engine power and the cockpit levers)", isOn: Binding(get: { m.get("throttle_plugin") != 0 }, set: { m.set("throttle_plugin", $0 ? 1 : 0) }))
                .help("Engine power and the cockpit levers follow the X52 lever whatever X-Plane's own axis assignment is. The stick and twist stay X-Plane's.")
            Text("The throttle is only taken over once it has moved. While X-Plane's autothrottle is on it drives the power; afterwards the lever takes over when it reaches that power. This works inside X-Plane only: the app itself never moves the controls.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            AxisCurveRow(m: m, title: "Stick left/right", key: "roll", usage: 0x30)
            AxisCurveRow(m: m, title: "Stick fwd/back", key: "pitch", usage: 0x31)
            AxisCurveRow(m: m, title: "Stick twist", key: "yaw", usage: 0x35)
            AxisCurveRow(m: m, title: "Throttle", key: "throttle", usage: 0x32)
            HStack(spacing: 16) {
                let sm = Binding<Int>(get: { m.get("stick_smoothing") }, set: { m.set("stick_smoothing", $0) })
                let dm = Binding<Int>(get: { m.get("stick_damping") }, set: { m.set("stick_damping", $0) })
                Text("Stick handling").frame(width: 120, alignment: .leading)
                Stepper("Smoothing \(sm.wrappedValue) ms", value: sm, in: 0...200, step: 10).frame(width: 190, alignment: .leading)
                    .help("Small, slow stick changes (jitter, hand tremor) are smoothed over about this long. Big or quick moves, such as letting go of the stick, reach X-Plane at once. 0 = off.")
                Stepper("Damping \(dm.wrappedValue) %", value: dm, in: 0...100, step: 10).frame(width: 170, alignment: .leading)
                    .help("A light pitch and roll damper in place of X-Plane's stability augmentation, which is skipped while the plugin drives the stick. Only near the centre of the stick, never more than a few percent of the travel, off on the ground and with the autopilot on. 0 = off.")
            }
            Text("With the plugin driving the stick, X-Plane's own curve and stability augmentation are skipped: the curves, smoothing and damping here take their place.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            CalibrationRow(m: m)
            Text("Dials and slider: a change smaller than Steady is ignored, so the value does not flicker. Saved in the aircraft's profile.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            DialSteadyRow(m: m, title: "Lower rotary (i)", key: "lower", usage: 0x33)
            DialSteadyRow(m: m, title: "Upper rotary (E)", key: "upper", usage: 0x34)
            if m.get("rotary_lower") == 2 || m.get("rotary_upper") == 2 {
                let zs = Binding<Int>(get: { m.get("rotary_zoom_steps") }, set: { m.set("rotary_zoom_steps", $0) })
                HStack(spacing: 12) {
                    Text("Zoom strength").frame(width: 120, alignment: .leading)
                    Stepper("\(zs.wrappedValue) zoom steps per turn", value: zs, in: 12...480, step: 12).frame(width: 260, alignment: .leading)
                        .help("How far one full turn of a dial set to Zoom zooms the view, in steps of X-Plane's own zoom. More is a stronger zoom. You can change it while flying.")
                }
            }
            Text("The lower dial (i) and the E dial can set the speed brake in four steps (Armed, Retracted, Half, Full) or zoom the view. The plugin only acts while you turn the dial, so buttons, the mouse and the auto-spoilers still work. Set the dial to None in X-Plane's Joystick settings if X-Plane uses it too.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Text("Smooth jobs (prop, mixture, trims, cowl flaps, carb heat) cover the game's range for this aircraft (From/To narrow it). The plugin waits until you turn the dial to X-Plane's own value, so nothing jumps.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            DialSteadyRow(m: m, title: "Slider", key: "slider", usage: 0x36)
            HStack(spacing: 16) {
                Toggle("Slider sets the flaps", isOn: Binding(get: { m.get("slider_flaps") != 0 }, set: { m.set("slider_flaps", $0 ? 1 : 0) }))
                Toggle("Invert flaps", isOn: Binding(get: { m.get("slider_invert") != 0 }, set: { m.set("slider_invert", $0 ? 1 : 0) }))
                    .disabled(m.get("slider_flaps") == 0)
            }
            Text("In X-Plane the slider moves the flap lever to the aircraft's own stops. With Invert flaps off (as in the ready-made profiles), pushed away from you deploys the flaps and towards you retracts them; Invert flaps reverses that. This works whether the axes above are on or off. Flap buttons still work in between. Set the slider to None in X-Plane's Joystick settings if X-Plane uses it for something else.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Divider()
            let moving = m.axes.first { $0.moving }
            Text(moving.map { "Moving now: \($0.name)   \($0.text)" } ?? "Moving now: move a control to see which one it is")
                .foregroundStyle(.orange)
            Text("Every control the stick reports").foregroundStyle(.secondary)
            VStack(alignment: .leading, spacing: 4) {
                ForEach(m.axes) { a in
                    HStack(spacing: 12) {
                        Text(a.name).frame(width: 150, alignment: .leading).foregroundStyle(a.moving ? Color.green : Color.primary)
                        ProgressView(value: Double(a.percent), total: 100).frame(width: 200)
                        Text(a.text).frame(width: 90, alignment: .leading)
                        Text("raw \(a.raw)").foregroundStyle(.secondary)
                    }
                }
                if m.axes.isEmpty { Text("The stick is not reporting anything yet.").foregroundStyle(.secondary) }
            }
        }
        .padding(.trailing, 8)
        }
    }
}

// MARK: - Pages

func pageIndex(_ id: String) -> Int {
    if id.isEmpty { return -1 }
    return (0..<Int(x52_page_count())).first { str(x52_page_id(Int32($0))) == id } ?? -1
}

// The pages in the order the editor lists them: (page number, depth).
func pageOutline() -> [(Int, Int)] {
    str(x52_pages_outline()).split(separator: "\n").compactMap { line in
        let f = line.split(separator: "\t").compactMap { Int($0) }
        return f.count == 2 ? (f[0], f[1]) : nil
    }
}

func pageName(_ i: Int) -> String {
    let t = str(x52_page_title(Int32(i)))
    return t.isEmpty ? "(untitled: \(str(x52_page_id(Int32(i)))))" : t
}

// The stick's screen: three lines of 16 characters, green on black.
struct StickScreen: View {
    let lines: [String]
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            ForEach(0..<3, id: \.self) { i in
                Text(i < lines.count && !lines[i].isEmpty ? lines[i] : " ").font(.system(.body, design: .monospaced))
            }
        }
        .padding(8).frame(width: 190, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 6).fill(Color.black.opacity(0.85)))
        .foregroundStyle(Color(red: 0.4, green: 0.95, blue: 0.5))
    }
}

struct PagePreview: View {
    @ObservedObject var m: Model
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text("On the stick's screen").foregroundStyle(.secondary)
            StickScreen(lines: m.pagePreview)
            Text(m.pagePreviewLive ? "Numbers from X-Plane, live." : m.pagesOpen
     ? "Numbers are made up (1234.5): X-Plane is not answering. In X-Plane, Settings > Network: accept incoming connections."
     : "Numbers are made up (1234.5). Open the pages editor to see X-Plane's.")
                .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).frame(width: 190, alignment: .leading)
        }
    }
}

// The list of pages: every page the left wheel reaches, built-in ones too, in its order (the eye shows or skips it),
// the user's sub-pages indented under their page.
struct WheelEntryRow {
    let token: String, title: String, hidden: Bool, builtin: Bool
}
func wheelEntryRows() -> [WheelEntryRow] {
    str(x52_wheel_entries()).split(separator: "\n").compactMap { line in
        let f = line.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        return f.count == 4 ? WheelEntryRow(token: f[0], title: f[1], hidden: f[2] == "1", builtin: f[3] == "1") : nil
    }
}

// The ids the list shows, top to bottom (the same values as `pageSel`), for the arrow keys.
func pageSelectionOrder() -> [String] {
    let outline = pageOutline()
    var out: [String] = []
    for e in wheelEntryRows() {
        out.append(e.builtin ? "#" + e.token : String(e.token.dropFirst()))
        if e.builtin { continue }
        let top = String(e.token.dropFirst())
        guard let s = outline.firstIndex(where: { str(x52_page_id(Int32($0.0))) == top && $0.1 == 0 }) else { continue }
        for row in outline[(s + 1)...].prefix(while: { $0.1 > 0 }) { out.append(str(x52_page_id(Int32(row.0)))) }
    }
    return out
}

struct WheelList: View {
    @ObservedObject var m: Model
    // Up / Down move the selection; Space or Return shows or skips the selected page on the left wheel.
    private func step(_ d: Int) {
        let order = pageSelectionOrder()
        guard !order.isEmpty else { return }
        let at = order.firstIndex(of: m.pageSel) ?? (d > 0 ? -1 : order.count)
        m.pageSel = order[max(0, min(order.count - 1, at + d))]
        m.pageNote = ""
        m.tick()
    }
    private func toggleSkip() {
        guard let e = wheelEntryRows().first(where: { ($0.builtin ? "#" + $0.token : String($0.token.dropFirst())) == m.pageSel })
        else { return }
        x52_wheel_hide(e.token, e.hidden ? 0 : 1)
        m.tick()
    }
    var body: some View {
        let outline = pageOutline()
        // The mode choice sits above the scrolling list at a fixed width: inside the ScrollView the segmented
        // picker and the list kept re-measuring each other and the list jumped sideways.
        VStack(alignment: .leading, spacing: 4) {
            // A menu, not segments: four segments are wider than the list at larger text sizes.
            Picker("Order for:", selection: Binding(get: { m.wheelMode }, set: {
                m.wheelMode = $0; x52_wheel_edit_mode(Int32($0)); m.tick()
            })) {
                Text("Every mode").tag(0)
                Text("Mode 1").tag(1)
                Text("Mode 2").tag(2)
                Text("Mode 3").tag(3)
            }
            .pickerStyle(.menu).fixedSize().padding(.horizontal, 6).padding(.top, 6)
            if m.wheelMode != 0 {
                Toggle("Same as every mode", isOn: Binding(get: { x52_wheel_mode_own(Int32(m.wheelMode)) == 0 }, set: {
                    x52_wheel_set_mode_own(Int32(m.wheelMode), $0 ? 0 : 1); m.tick()
                }))
                .toggleStyle(.checkbox).padding(.leading, 6)
                .help("Off: this mode has its own order, and turning the mode dial to it opens its first page")
            }
        ScrollViewReader { proxy in
        ScrollView {
            VStack(alignment: .leading, spacing: 2) {
                Text("The left wheel, in order. The eye: shown or skipped. A lock: a built-in page.")
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).padding(.leading, 6)
                ForEach(wheelEntryRows(), id: \.token) { e in
                    let sel = e.builtin ? "#" + e.token : String(e.token.dropFirst())
                    let on = m.pageSel == sel
                    HStack(spacing: 4) {
                        Button { x52_wheel_hide(e.token, e.hidden ? 0 : 1); m.tick() } label: {
                            Image(systemName: e.hidden ? "eye.slash" : "eye").frame(width: 18)
                        }
                        .buttonStyle(.plain).insideStop().foregroundStyle(e.hidden ? Color.secondary : Color.primary)
                        .help(e.hidden ? "Skipped by the left wheel: click to show it" : "Shown by the left wheel: click to skip it")
                        Button { m.pageSel = sel; m.pageNote = ""; m.tick() } label: {
                            HStack(spacing: 6) {
                                Image(systemName: e.builtin ? "lock.rectangle" : "rectangle.on.rectangle").frame(width: 16)
                                    .foregroundStyle(on ? Color.white : Color.secondary)
                                Text(e.title).lineLimit(1).strikethrough(e.hidden)
                                Spacer(minLength: 4)
                                if !e.builtin {
                                    Text("\(x52_row_count(Int32(pageIndex(String(e.token.dropFirst())))))").font(.caption)
                                        .foregroundStyle(on ? Color.white.opacity(0.8) : Color.secondary)
                                }
                            }
                            .padding(.horizontal, 6).frame(height: 24)
                            .background(on ? Color.accentColor : Color.clear)
                            .foregroundStyle(on ? Color.white : e.hidden || e.builtin ? Color.secondary : Color.primary)
                            .clipShape(RoundedRectangle(cornerRadius: 5)).contentShape(Rectangle())
                        }
                        .buttonStyle(.plain).insideStop()
                    }
                    .id(sel)
                    if !e.builtin { SubPageRows(m: m, outline: outline, topId: String(e.token.dropFirst())) }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .arrowKeys(up: { step(-1) }, down: { step(1) }, activate: toggleSkip)
        .onChange(of: m.pageSel) { id in withAnimation { proxy.scrollTo(id) } }
        }
        }
    }
}

// The pages inside a top-level page, indented.
struct SubPageRows: View {
    @ObservedObject var m: Model
    let outline: [(Int, Int)]
    let topId: String
    var body: some View {
        let start = outline.firstIndex { str(x52_page_id(Int32($0.0))) == topId && $0.1 == 0 }
        let subs: [(Int, Int)] = start.map { s in Array(outline[(s + 1)...].prefix { $0.1 > 0 }) } ?? []
        ForEach(subs, id: \.0) { row in
            let id = str(x52_page_id(Int32(row.0)))
            let on = id == m.pageSel
            Button { m.pageSel = id; m.pageNote = ""; m.tick() } label: {
                HStack(spacing: 6) {
                    Image(systemName: "arrow.turn.down.right").frame(width: 16).foregroundStyle(on ? Color.white : Color.secondary)
                    Text(pageName(row.0)).lineLimit(1)
                    Spacer(minLength: 4)
                }
                .padding(.leading, CGFloat(row.1) * 16 + 28).padding(.trailing, 6).frame(height: 24)
                .background(on ? Color.accentColor : Color.clear)
                .foregroundStyle(on ? Color.white : Color.primary)
                .clipShape(RoundedRectangle(cornerRadius: 5)).contentShape(Rectangle())
            }
            .buttonStyle(.plain).insideStop().id(id)
        }
    }
}

// A built-in page chosen in the list. Its layout and tuning are fixed; the left wheel can skip it or meet it elsewhere.
struct BuiltinDetail: View {
    @ObservedObject var m: Model
    let token: String
    var body: some View {
        let e = wheelEntryRows().first { $0.token == token }
        VStack(alignment: .leading, spacing: 10) {
            Text(e?.title ?? token).font(.title3)
            Text("A built-in page: what it shows and how the right wheel tunes it are fixed. Here you choose whether the left wheel stops on it, and where (the arrows under the list).")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Toggle("The left wheel shows this page", isOn: Binding(get: { !(e?.hidden ?? false) }, set: {
                x52_wheel_hide(token, $0 ? 0 : 1); m.tick()
            }))
        }
    }
}

struct PageList: View {
    @ObservedObject var m: Model
    var body: some View { WheelList(m: m) }
}

let rowKinds = ["Text", "Value", "Link"]

struct PageRowEditor: View {
    @ObservedObject var m: Model
    let page: Int
    let row: Int
    var body: some View {
        let p = Int32(page), r = Int32(row)
        let kind = Int(x52_row_kind(p, r))
        let label = Binding<String>(get: { str(x52_row_text(p, r)) }, set: { write(text: $0) })
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                FixedSegments(selection: Binding(get: { kind }, set: { write(kind: $0) }), names: rowKinds, width: 54)
                if kind == 1 {
                    // A value row: label and format here, the dataref gets a whole line below.
                    let format = str(x52_row_format(p, r))
                    Text("Label").padding(.leading, 6)
                    TextField("e.g. Fuel", text: label).textFieldStyle(.roundedBorder).frame(width: 130)
                    Text("Format").padding(.leading, 6)
                    TextField("format", text: Binding(get: { format }, set: { write(format: $0) }))
                        .textFieldStyle(.roundedBorder).font(.system(.body, design: .monospaced)).frame(width: 100)
                        .help("One number: %.0f (no decimals), %.1f (one), %d (whole number), %g (as needed), with words around it, e.g. %.0f kg")
                    Text("→ \(str(x52_format_example(format, 1234.5)))").font(.system(.callout, design: .monospaced))
                        .foregroundStyle(.secondary).lineLimit(1)
                        .help("What the format makes of 1234.5")
                    Spacer(minLength: 0)
                } else {
                    TextField(kind == 0 ? "words, up to 15 characters show" : "label", text: label)
                        .textFieldStyle(.roundedBorder)
                }
                Button { x52_row_move(p, r, -1); m.tick() } label: { Image(systemName: "chevron.up") }
                    .disabled(row == 0).help("Move up")
                Button { x52_row_move(p, r, 1); m.tick() } label: { Image(systemName: "chevron.down") }
                    .disabled(row >= Int(x52_row_count(p)) - 1).help("Move down")
                Button { x52_row_remove(p, r); m.tick() } label: { Image(systemName: "xmark") }.help("Remove this row")
            }
            if kind == 1 {
                HStack(alignment: .top, spacing: 8) {
                    Text("Dataref").padding(.top, 4)
                    CatalogField(kind: 1, placeholder: "e.g. sim/flightmodel/weight/m_fuel_total, or name[3]",
                                 text: Binding(get: { str(x52_row_dataref(p, r)) }, set: { write(dataref: $0) }))
                        .frame(maxWidth: .infinity)
                }
            } else if kind == 2 {
                let target = str(x52_row_target(p, r))
                HStack(spacing: 8) {
                    Text("Opens")
                    Picker("", selection: Binding(get: { target }, set: { write(target: $0) })) {
                        Text("Choose a page").tag("")
                        ForEach(pageOutline().map { $0.0 }.filter { $0 != page }, id: \.self) { i in
                            Text(pageName(i)).tag(str(x52_page_id(Int32(i))))
                        }
                    }
                    .labelsHidden().frame(width: 240)
                    Button("New page inside this one") {
                        let n = x52_page_add("New page", str(x52_page_id(p)))
                        if n >= 0 { write(target: str(x52_page_id(n))) }
                    }
                    if target.isEmpty { Text("The wheel does nothing on this row yet.").font(.caption).foregroundStyle(.orange) }
                    Spacer()
                }
                .padding(.leading, 170)
            }
        }
        .padding(.vertical, 4)
    }
    private func write(kind: Int? = nil, text: String? = nil, format: String? = nil, dataref: String? = nil, target: String? = nil) {
        let p = Int32(page), r = Int32(row)
        x52_row_set(p, r, Int32(kind ?? Int(x52_row_kind(p, r))), text ?? str(x52_row_text(p, r)),
                    format ?? str(x52_row_format(p, r)), dataref ?? str(x52_row_dataref(p, r)), target ?? str(x52_row_target(p, r)))
        m.tick()
    }
}

// The selected page: its title, where it opens from, and its rows.
struct PageDetail: View {
    @ObservedObject var m: Model
    let page: Int
    var body: some View {
        let p = Int32(page)
        let id = str(x52_page_id(p))
        let parent = str(x52_page_parent(p))
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Title").frame(width: 90, alignment: .leading)
                TextField("shown when the page has no rows", text: Binding(get: { str(x52_page_title(p)) }, set: {
                    _ = x52_page_set(p, $0, str(x52_page_parent(p))); m.tick()
                }))
                .textFieldStyle(.roundedBorder).frame(width: 260)
                Text("id \(id)").font(.caption).foregroundStyle(.secondary)
                    .help("How the profile file and link rows name this page. It stays the same when you rename the page.")
            }
            HStack {
                Text("Opens from").frame(width: 90, alignment: .leading)
                Picker("", selection: Binding(get: { parent }, set: {
                    m.pageNote = x52_page_set(p, str(x52_page_title(p)), $0) == 0 ? "That page is inside this one: a page cannot open itself." : ""
                    m.tick()
                })) {
                    Text("The left wheel (top level)").tag("")
                    ForEach(pageOutline().map { $0.0 }.filter { $0 != page }, id: \.self) { i in
                        Text("Inside “\(pageName(i))”").tag(str(x52_page_id(Int32(i))))
                    }
                }
                .labelsHidden().frame(width: 260)
                Text(parent.isEmpty ? "The left wheel reaches it; its place is set in the list on the left." : "Only a link row on that page opens it.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            if !m.pageNote.isEmpty { Text(m.pageNote).foregroundStyle(.orange) }
            Divider()
            Text("Rows (the right wheel moves the cursor; pressing it on a link opens the page)").foregroundStyle(.secondary)
            ScrollView {
                VStack(alignment: .leading, spacing: 2) {
                    if x52_row_count(p) == 0 {
                        Text("No rows: the stick shows the title alone.").foregroundStyle(.secondary)
                    }
                    ForEach(0..<Int(x52_row_count(p)), id: \.self) { r in
                        PageRowEditor(m: m, page: page, row: r)
                        Divider()
                    }
                }
                .padding(.trailing, 8)
            }
            HStack {
                Button("+ Text") { _ = x52_row_add(p, 0); m.tick() }
                Button("+ Value") { _ = x52_row_add(p, 1); m.tick() }
                Button("+ Link") { _ = x52_row_add(p, 2); m.tick() }
                Spacer()
            }
        }
    }
}

struct PageListButtons: View {
    @ObservedObject var m: Model
    var body: some View {
        let sel = pageIndex(m.pageSel)
        let builtin = m.pageSel.hasPrefix("#")
        // A built-in or top-level page moves in the left wheel's order; a sub-page among its siblings.
        let wheelToken: String? = builtin ? String(m.pageSel.dropFirst())
            : (sel >= 0 && str(x52_page_parent(Int32(sel))).isEmpty ? "@" + m.pageSel : nil)
        let canMove = sel >= 0 || wheelToken != nil
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 6) {
                Button("Add page") { select(x52_page_add("New page", "")) }
                Button("Add inside") { select(x52_page_add("New page", m.pageSel)) }.disabled(sel < 0)
                    .help("A page that only a link row on the selected page opens")
            }
            HStack(spacing: 6) {
                Button { move(-1, wheelToken, sel) } label: { Image(systemName: "chevron.up") }
                    .disabled(!canMove).help("Move up")
                Button { move(1, wheelToken, sel) } label: { Image(systemName: "chevron.down") }
                    .disabled(!canMove).help("Move down")
                Button("Remove") {
                    guard confirm("Remove the page “\(pageName(sel))”?",
                                  "The pages inside it go too, and links to them are cleared.", "Remove") else { return }
                    x52_page_remove(Int32(sel))
                    m.pageSel = pageOutline().first.map { str(x52_page_id(Int32($0.0))) } ?? ""
                    m.tick()
                }
                .disabled(sel < 0).help("Removes the page and the pages inside it; links to them are cleared")
            }
        }
    }
    private func move(_ dir: Int32, _ token: String?, _ sel: Int) {
        if let t = token { x52_wheel_move(t, dir) } else { x52_page_move(Int32(sel), dir) }
        m.tick()
    }
    private func select(_ i: Int32) {
        if i >= 0 { m.pageSel = str(x52_page_id(i)) }
        m.pageNote = ""
        m.tick()
    }
}

struct RestorePagesButton: View {
    @ObservedObject var m: Model
    var body: some View {
        Button("Restore default pages...") { m.pagesConfirm = true }
            .alert("Remove all your own pages?", isPresented: $m.pagesConfirm) {
                Button("Remove all", role: .destructive) { x52_pages_restore_default(); m.pageSel = ""; m.tick() }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("The stick goes back to the built-in pages, all shown, in their usual order: radios, Buttons and Brightness. This profile's buttons, lights and other settings stay as they are.")
            }
    }
}

// The Pages tab of the settings window: an overview, and the button to the editor window.
struct PagesTab: View {
    @ObservedObject var m: Model
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Screen pages of your own: rows of text, live dataref values, and links to more pages. The list is the left wheel's order: move any page, built-in ones too, or skip it with the eye.")
                .foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Picker("Radio screens:", selection: Binding(get: { m.get("radio_side") }, set: { m.set("radio_side", $0) })) {
                Text("Classic: one radio per screen (COM1 / NAV1, COM2 / NAV2, ADF)").tag(0)
                Text("Side by side: COM 1+2, NAV 1+2, ADF 1+2").tag(1)
            }
            .pickerStyle(.radioGroup)
            HStack(spacing: 10) {
                Button("Open pages editor...") { m.openPagesEditor() }.keyboardShortcut(.defaultAction)
                RestorePagesButton(m: m)
            }
            HStack(alignment: .top, spacing: 24) {
                PageList(m: m).frame(width: 300).frame(minHeight: 240, maxHeight: .infinity)
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.25)))
                PagePreview(m: m)
            }
            .frame(maxHeight: .infinity, alignment: .top)
        }
    }
}

// The editor's own window: pages on the left, the selected page in the middle, the stick and the file on the right.
struct PagesEditor: View {
    @ObservedObject var m: Model
    var body: some View {
        let sel = pageIndex(m.pageSel)
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("Pages of the profile").font(.headline)
                ProfilePicker(m: m) { m.select($0); m.pageSel = ""; m.pageNote = "" }
                    .frame(width: 280)
                Spacer()
                RestorePagesButton(m: m)
            }
            StatusStrip(m: m)
            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 8) {
                    PageList(m: m)
                    PageListButtons(m: m)
                }
                .frame(width: 210)
                Divider()
                Group {
                    if m.pageSel.hasPrefix("#") {
                        BuiltinDetail(m: m, token: String(m.pageSel.dropFirst()))
                    } else if sel >= 0 {
                        PageDetail(m: m, page: sel)
                    } else {
                        Text(x52_page_count() == 0 ? "Add a page to begin." : "Choose a page on the left.")
                            .foregroundStyle(.secondary)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
                Divider()
                VStack(alignment: .leading, spacing: 12) {
                    PagePreview(m: m)
                    PageSource(m: m)
                }
                .frame(width: 250)
            }
        }
        .padding(16)
        .frame(minWidth: 980, minHeight: 560)
        .onAppear { if pageIndex(m.pageSel) < 0 { m.pageSel = pageOutline().first.map { str(x52_page_id(Int32($0.0))) } ?? "" } }
    }
}

// What the editor writes in the profile file, read back after every change.
struct PageSource: View {
    @ObservedObject var m: Model
    var body: some View {
        let path = m.folder + "/" + m.key + ".x52"
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("In the profile file").foregroundStyle(.secondary)
                Spacer()
                Button("Show file") { NSWorkspace.shared.activateFileViewerSelecting([URL(fileURLWithPath: path)]) }
                    .controlSize(.small)
            }
            Text(path).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head).help(path)
            ScrollView([.vertical, .horizontal]) {
                let src = str(x52_pages_source())
                Text(src.isEmpty ? "(no page lines: the built-in pages only)" : src)
                    .font(.system(.caption, design: .monospaced)).textSelection(.enabled)
                    .frame(maxWidth: .infinity, alignment: .topLeading).padding(6)
            }
            .background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.08)))
            .id(m.revision)
        }
    }
}


// MARK: - Window

let tabList: [(Int, String)] = [(6, "Buttons"), (0, "Lights"), (1, "Live lights"), (2, "Alarms"), (3, "Screen & clock"),
                                (8, "Pages"), (5, "Master & brightness"), (7, "Axes"), (4, "Test")]

struct SettingsView: View {
    @ObservedObject var m: Model
    private func stepTab(_ d: Int) {
        guard let i = tabList.firstIndex(where: { $0.0 == m.tab }) else { return }
        m.tab = tabList[max(0, min(tabList.count - 1, i + d))].0
    }
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Button {   // the switch for the X-Plane plugin: green on, red off
                    x52_set_plugin_enabled(m.pluginEnabled ? 0 : 1)
                    m.tick()
                } label: {
                    HStack(spacing: 6) {
                        Circle().fill(m.pluginEnabled ? Color.green : Color.red).frame(width: 9, height: 9)
                        Text(m.pluginEnabled ? "Plugin on" : "Plugin off")
                    }
                }
                .help("Switches the X-Plane plugin on or off. Off: the plugin does nothing in X-Plane, and X-Plane's own Joystick settings are in charge. Takes effect within a second and is kept until you switch it back on.")
                Divider().frame(height: 14)
                Circle().fill(m.pluginActive ? Color.blue : m.connected ? Color.green : Color.gray).frame(width: 9, height: 9)
                Text(m.statusText).lineLimit(1).truncationMode(.middle).help(m.statusText)
                Spacer()
                if !m.lastDeleted.isEmpty {
                    Button("Undo delete") { m.undoDelete() }
                        .help("Brings back “\(profileTitle(m.lastDeleted))”")
                }
                UndoRedoButtons(m: m)
                ProfilePicker(m: m) { m.select($0) }
                    .frame(width: 280)
                Menu {
                    Button("New blank profile...") { m.newBlank() }
                    Button("Save as new profile...") { x52_copy_ask(); m.tick() }
                    Divider()
                    Button("Export Current Template...") { m.exportManual() }
                        .help("Fills the manual template with the profile in use and saves it as a Word file (for a PDF, use Save as PDF in Word)")
                    Button("Save a Built-In Template Blank Copy...") { m.saveBuiltInTemplateCopy() }
                        .help("Saves a blank copy of the built-in template, to open in Word and make a template of your own from")
                    Button("Choose Custom Template...") { m.chooseManualTemplate() }
                        .help("Uses a Word template of your own when you export the manual")
                    if !m.customTemplate.isEmpty {
                        Button("Use the Built-In Template") { m.useBuiltInTemplate() }
                            .help("Goes back to the template that comes with the app")
                    }
                    Divider()
                    Button("Delete this profile...") { m.deleteCurrent() }.disabled(!m.canDelete)
                } label: {
                    Image(systemName: "ellipsis.circle")
                }
                .menuStyle(.borderlessButton).fixedSize()
                .help(m.canDelete ? "New, copy or delete a profile, or export its manual (Export Current Template)" : "New or copy a profile, or export its manual with Export Current Template (Default and ready-made profiles are never deleted)")
            }
            .zIndex(1)
            StatusStrip(m: m, pluginNote: true)
            // A scrolling strip of tabs, so a narrow window never hides some behind a ">>" menu.
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 6) {
                    ForEach(Array(tabList.enumerated()), id: \.element.0) { n, entry in
                        let on = m.tab == entry.0
                        Button { m.tab = entry.0 } label: {
                            Text(entry.1).lineLimit(1).padding(.horizontal, 12).frame(height: 26)
                                .background(on ? Color.accentColor : Color.secondary.opacity(0.15))
                                .foregroundStyle(on ? Color.white : Color.primary)
                                .clipShape(RoundedRectangle(cornerRadius: 6))
                                .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain).insideStop()
                        .keyboardShortcut(KeyEquivalent(Character(String(n + 1))), modifiers: .command)
                        .help("\(entry.1) (Command-\(n + 1))")
                    }
                }
                .arrowKeys(left: { stepTab(-1) }, right: { stepTab(1) })
            }
            Group {
                switch m.tab {
                case 6: ButtonsTab(m: m)
                case 0: LightsTab(m: m)
                case 1: LiveTab(m: m)
                case 2: AlarmsTab(m: m)
                case 3: ScreenTab(m: m)
                case 5: MasterTab(m: m)
                case 7: AxesTab(m: m)
                case 8: PagesTab(m: m)
                default: TestTab(m: m)
                }
            }
            .padding(14).frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color.secondary.opacity(0.25)))
            HStack {
                if m.xplane.isEmpty {
                    Text("X-Plane 12 was not found. Choose its folder to install the plugin.")
                        .font(.caption).foregroundStyle(.orange)
                } else {
                    Text("X-Plane: \(m.xplane)").font(.caption).foregroundStyle(.secondary).lineLimit(1)
                        .truncationMode(.head).help("The X-Plane 12 folder in use")
                    Text(m.pluginState == 2 ? "Plugin installed" : m.pluginState == 1 ? "Plugin out of date" : "Plugin not installed")
                        .font(.caption).foregroundStyle(m.pluginState == 2 ? Color.green : Color.orange)
                    if m.pluginState != 2 {
                        Button(m.pluginState == 1 ? "Update plugin" : "Install plugin") { m.installPlugin() }
                            .controlSize(.small)
                    }
                }
                Spacer()
            }
            if !m.pluginMessage.isEmpty {
                Text(m.pluginMessage).font(.caption).fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Text(m.folder).font(.caption).foregroundStyle(.secondary).lineLimit(1).truncationMode(.head)
                    .help("Where the profiles are saved")
                Spacer()
                Button("X-Plane folder...") { m.chooseXPlaneFolder() }.controlSize(.small)
            }
        }
        .padding(16)
        .frame(minWidth: 900, maxWidth: .infinity, minHeight: 600, maxHeight: .infinity)
    }
}

// MARK: - Update check

/// Asks GitHub for the latest release of the project and says whether it is newer than this app. It only
/// reads (one GET, nothing about the user or the Mac is sent) and never downloads or installs anything:
/// the person opens the release page and takes the new version from there.
enum UpdateCheck {
    static let api = "https://api.github.com/repos/sbarrak/X52-HOTAS-for-Mac/releases/latest"
    static let enabledKey = "checkForUpdates"      // default on
    static let lastKey = "lastUpdateCheck"

    struct Release { let version: String; let page: URL }
    enum Outcome { case newer(Release), current, failed(String) }

    static var automatic: Bool {
        get { UserDefaults.standard.object(forKey: enabledKey) as? Bool ?? true }
        set { UserDefaults.standard.set(newValue, forKey: enabledKey) }
    }
    /// The automatic check runs at most once a day.
    static var dueToday: Bool {
        guard let last = UserDefaults.standard.object(forKey: lastKey) as? Date else { return true }
        return Date().timeIntervalSince(last) > 24 * 3600
    }

    static func run(_ done: @escaping (Outcome) -> Void) {
        guard let url = URL(string: api) else { return done(.failed("Bad address.")) }
        var req = URLRequest(url: url, timeoutInterval: 10)
        req.setValue("application/vnd.github+json", forHTTPHeaderField: "Accept")
        req.setValue("X52Pro/\(appVersion)", forHTTPHeaderField: "User-Agent")
        URLSession.shared.dataTask(with: req) { data, response, error in
            let outcome: Outcome
            if let error = error {
                outcome = .failed(error.localizedDescription)
            } else if let code = (response as? HTTPURLResponse)?.statusCode, code != 200 {
                outcome = .failed("GitHub answered \(code).")
            } else if let data = data,
                      let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                      let tag = json["tag_name"] as? String,
                      let page = (json["html_url"] as? String).flatMap(URL.init(string:)),
                      page.scheme == "https", page.host == "github.com" {
                UserDefaults.standard.set(Date(), forKey: lastKey)
                outcome = x52_version_newer(appVersion, tag) != 0 ? .newer(Release(version: tag, page: page)) : .current
            } else {
                outcome = .failed("Could not read GitHub's answer.")
            }
            DispatchQueue.main.async { done(outcome) }
        }.resume()
    }
}

// MARK: - App

/// The version in the app's Info.plist (set from the VERSION file when the app is built).
let appVersion: String = (Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String) ?? "dev"

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate, NSMenuItemValidation {
    let model = Model()
    var item: NSStatusItem!
    var window: NSWindow?
    let statusLine = NSMenuItem(title: "", action: nil, keyEquivalent: "")
    let loginLine = NSMenuItem(title: "Start at login", action: #selector(toggleLogin), keyEquivalent: "")
    let updateLine = NSMenuItem(title: "Check for updates...", action: #selector(checkForUpdatesByHand), keyEquivalent: "")
    let autoUpdateLine = NSMenuItem(title: "Check for updates automatically", action: #selector(toggleAutoUpdate), keyEquivalent: "")
    var newRelease: UpdateCheck.Release?

    func applicationDidFinishLaunching(_ note: Notification) {
        model.start()
        model.openPagesEditor = { [weak self] in self?.openPagesEditor() }
        model.openCalibration = { [weak self] in self?.openCalibration() }
        item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        let image = NSImage(systemSymbolName: "gamecontroller.fill", accessibilityDescription: "X52 Pro")
        image?.isTemplate = true
        item.button?.image = image
        if image == nil { item.button?.title = "X52" }

        let menu = NSMenu()
        menu.delegate = self
        statusLine.isEnabled = false
        menu.addItem(statusLine)
        menu.addItem(.separator())
        let versionLine = NSMenuItem(title: "X52 Pro \(appVersion)", action: nil, keyEquivalent: "")
        versionLine.isEnabled = false
        menu.addItem(versionLine)
        menu.addItem(withTitle: "About X52 Pro", action: #selector(showAbout), keyEquivalent: "").target = self
        updateLine.target = self
        menu.addItem(updateLine)
        autoUpdateLine.target = self
        menu.addItem(autoUpdateLine)
        menu.addItem(.separator())
        menu.addItem(withTitle: "Open settings...", action: #selector(openSettings), keyEquivalent: ",").target = self
        menu.addItem(withTitle: "Open pages editor...", action: #selector(openPagesEditor), keyEquivalent: "").target = self
        menu.addItem(withTitle: "Calibrate stick...", action: #selector(openCalibration), keyEquivalent: "").target = self
        loginLine.target = self
        menu.addItem(loginLine)
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit X52 Pro", action: #selector(quit), keyEquivalent: "q").target = self
        item.menu = menu
        installEditMenu()

        // Hidden option for the project's own pictures: `X52Pro --snapshot <folder>` draws the windows into PNG files.
        if let i = CommandLine.arguments.firstIndex(of: "--snapshot"), i + 1 < CommandLine.arguments.count {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
                self?.snapshots(into: CommandLine.arguments[i + 1],
                                profile: CommandLine.arguments.firstIndex(of: "--profile").flatMap { j in
                                    j + 1 < CommandLine.arguments.count ? CommandLine.arguments[j + 1] : nil })
            }
            return
        }

        if UpdateCheck.automatic && UpdateCheck.dueToday {
            UpdateCheck.run { [weak self] outcome in
                if case .newer(let r) = outcome { self?.setNewRelease(r) }   // quiet: no answer, no error shown
            }
        }

        // Opened by hand (not at login): show the window so there is something to see.
        if !CommandLine.arguments.contains("--background") && !launchedAsLoginItem() { openSettings() }
    }

    // The menu bar while a window is open (the app is then in the Dock and in Command+Tab). It also gives
    // the text fields Command+C, V, X, A and Z. Command+Q only puts the app back in the menu bar: the stick
    // keeps being driven. "Quit Completely" here, or Quit in the menu bar icon's menu, stops it.
    func installEditMenu() {
        let main = NSMenu()
        let appItem = NSMenuItem()
        main.addItem(appItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "About X52 Pro", action: #selector(showAbout), keyEquivalent: "").target = self
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Settings...", action: #selector(openSettings), keyEquivalent: ",").target = self
        appMenu.addItem(withTitle: "Pages Editor...", action: #selector(openPagesEditor), keyEquivalent: "e").target = self
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Hide X52 Pro", action: #selector(NSApplication.hide(_:)), keyEquivalent: "h")
        let others = appMenu.addItem(withTitle: "Hide Others", action: #selector(NSApplication.hideOtherApplications(_:)), keyEquivalent: "h")
        others.keyEquivalentModifierMask = [.command, .option]
        appMenu.addItem(.separator())
        appMenu.addItem(withTitle: "Quit X52 Pro (keeps running in the menu bar)", action: #selector(backToMenuBar), keyEquivalent: "q").target = self
        appMenu.addItem(withTitle: "Quit Completely", action: #selector(quit), keyEquivalent: "").target = self
        appItem.submenu = appMenu
        let editItem = NSMenuItem()
        main.addItem(editItem)
        let edit = NSMenu(title: "Edit")
        // Aimed at this delegate, not left to the responder chain: the SwiftUI window answers undo: first with
        // its own (empty) undo manager, which greyed the items out. The delegate hands the command to a text
        // field being edited, else to the profile history.
        edit.addItem(withTitle: "Undo", action: #selector(undo(_:)), keyEquivalent: "z").target = self
        let redo = edit.addItem(withTitle: "Redo", action: #selector(redo(_:)), keyEquivalent: "z")
        redo.keyEquivalentModifierMask = [.command, .shift]
        redo.target = self
        edit.addItem(.separator())
        edit.addItem(withTitle: "Cut", action: #selector(NSText.cut(_:)), keyEquivalent: "x")
        edit.addItem(withTitle: "Copy", action: #selector(NSText.copy(_:)), keyEquivalent: "c")
        edit.addItem(withTitle: "Paste", action: #selector(NSText.paste(_:)), keyEquivalent: "v")
        edit.addItem(withTitle: "Select All", action: #selector(NSText.selectAll(_:)), keyEquivalent: "a")
        editItem.submenu = edit
        let windowItem = NSMenuItem()
        main.addItem(windowItem)
        let windows = NSMenu(title: "Window")
        windows.addItem(withTitle: "Minimize", action: #selector(NSWindow.performMiniaturize(_:)), keyEquivalent: "m")
        windows.addItem(withTitle: "Close Window", action: #selector(NSWindow.performClose(_:)), keyEquivalent: "w")
        windowItem.submenu = windows
        NSApp.windowsMenu = windows
        NSApp.mainMenu = main
    }

    // In the Dock and Command+Tab while there is a window to switch to; menu bar only otherwise.
    func showInDock() {
        if NSApp.activationPolicy() != .regular { NSApp.setActivationPolicy(.regular) }
    }

    // Command+Q (and Quit in the Dock): close the windows and leave only the menu bar icon.
    @objc func backToMenuBar() {
        for w in [window, pagesWindow] { w?.performClose(nil) }
        NSApp.setActivationPolicy(.accessory)
    }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if reallyQuit { return .terminateNow }
        // Log out, restart and shut down must still be able to end the app.
        if let event = NSAppleEventManager.shared().currentAppleEvent, event.eventID == kAEQuitApplication,
           let reason = event.attributeDescriptor(forKeyword: kAEQuitReason)?.enumCodeValue,
           [kAELogOut, kAEReallyLogOut, kAEShowRestartDialog, kAERestart, kAEShowShutdownDialog, kAEShutDown].map({ OSType($0) }).contains(reason) {
            return .terminateNow
        }
        backToMenuBar()
        return .terminateCancel
    }

    // Clicking the Dock icon with no window open brings the settings back.
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        if !flag { openSettings() }
        return true
    }

    func launchedAsLoginItem() -> Bool {
        guard let event = NSAppleEventManager.shared().currentAppleEvent else { return false }
        return event.eventID == kAEOpenApplication
            && event.paramDescriptor(forKeyword: keyAEPropData)?.enumCodeValue == keyAELaunchedAsLogInItem
    }

    // Edit > Undo / Redo (Command+Z, Shift+Command+Z) when no text field is being edited: a field keeps its own.
    // The undo manager of a text field that is being edited (it keeps its own undo), if any.
    private var fieldUndo: UndoManager? {
        (NSApp.keyWindow?.firstResponder as? NSTextView)?.undoManager
    }
    @objc func undo(_ sender: Any?) {
        if let u = fieldUndo, u.canUndo { u.undo() } else { model.undo() }
    }
    @objc func redo(_ sender: Any?) {
        if let u = fieldUndo, u.canRedo { u.redo() } else { model.redo() }
    }
    func validateMenuItem(_ item: NSMenuItem) -> Bool {
        if item.action == #selector(undo(_:)) { return (fieldUndo?.canUndo ?? false) || model.canUndo }
        if item.action == #selector(redo(_:)) { return (fieldUndo?.canRedo ?? false) || model.canRedo }
        return true
    }

    func menuWillOpen(_ menu: NSMenu) {
        statusLine.title = model.statusText
        model.loginItem = SMAppService.mainApp.status == .enabled
        loginLine.state = model.loginItem ? .on : .off
        autoUpdateLine.state = UpdateCheck.automatic ? .on : .off
    }

    // Draws a window, title bar included, into a PNG file.
    private func draw(_ w: NSWindow?, to path: String) {
        guard let w = w, let frame = w.contentView?.superview else { return }
        w.displayIfNeeded()
        guard let rep = frame.bitmapImageRepForCachingDisplay(in: frame.bounds) else { return }
        frame.cacheDisplay(in: frame.bounds, to: rep)
        if let png = rep.representation(using: .png, properties: [:]) { try? png.write(to: URL(fileURLWithPath: path)) }
    }

    private func snapshots(into dir: String, profile: String?) {
        try? FileManager.default.createDirectory(atPath: dir, withIntermediateDirectories: true)
        let saved = UserDefaults.standard.string(forKey: "profileKey")
        model.snapshotMode = true
        if let p = profile { model.select(p) }
        openSettings()
        window?.setContentSize(NSSize(width: 960, height: 800))
        let shots: [(String, Int)] = [("buttons", 6), ("lights", 0), ("live-lights", 1), ("alarms", 2), ("screen", 3),
                                      ("pages", 8), ("master", 5), ("axes", 7), ("test", 4)]
        var n = 0
        func next() {
            if n < shots.count {
                let (name, tab) = shots[n]; n += 1
                model.tab = tab
                window?.setContentSize(NSSize(width: 960, height: tab == 7 ? 1000 : 800))   // Axes is taller than the rest
                if tab == 6 { model.btn = 4; model.btnLayer = 1 }
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { [self] in
                    draw(window, to: dir + "/" + name + ".png"); next()
                }
            } else if n == shots.count {
                n += 1
                openPagesEditor()
                DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { [self] in
                    draw(pagesWindow, to: dir + "/pages-editor.png"); next()
                }
            } else {
                if let s = saved { UserDefaults.standard.set(s, forKey: "profileKey") }
                else { UserDefaults.standard.removeObject(forKey: "profileKey") }
                reallyQuit = true
                NSApp.terminate(nil)
            }
        }
        next()
    }

    @objc func showAbout() {
        NSApp.activate(ignoringOtherApps: true)
        let para = NSMutableParagraphStyle()
        para.alignment = .center
        let base: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 11), .foregroundColor: NSColor.labelColor, .paragraphStyle: para]
        let credits = NSMutableAttributedString(string: "Modes, macros, screen and lights for the Saitek / Logitech X52 and X52 Pro on Apple Silicon Macs, for X-Plane 12.\n\nDeveloped by Sergio Barrak\n", attributes: base)
        let url = "https://github.com/sbarrak/X52-HOTAS-for-Mac"
        var link = base
        link[.link] = URL(string: url)!
        credits.append(NSAttributedString(string: url, attributes: link))
        credits.append(NSAttributedString(string: "\n\nMIT licence. Not affiliated with Laminar Research, Logitech or Saitek.", attributes: base))
        NSApp.orderFrontStandardAboutPanel(options: [
            .applicationName: "X52 Pro",
            .applicationVersion: appVersion,
            .version: "",
            .credits: credits
        ])
    }

    /// An update was found: the menu item turns into "Update available" and opens the release page.
    func setNewRelease(_ r: UpdateCheck.Release) {
        newRelease = r
        updateLine.title = "Update available: \(r.version)..."
    }

    @objc func checkForUpdatesByHand() {
        if let r = newRelease { return offer(r) }
        updateLine.isEnabled = false
        UpdateCheck.run { [weak self] outcome in
            guard let self = self else { return }
            self.updateLine.isEnabled = true
            NSApp.activate(ignoringOtherApps: true)
            switch outcome {
            case .newer(let r): self.setNewRelease(r); self.offer(r)
            case .current:
                let a = NSAlert()
                a.messageText = "X52 Pro is up to date"
                a.informativeText = "You have the latest version (\(appVersion))."
                a.runModal()
            case .failed(let why):
                let a = NSAlert()
                a.messageText = "Could not check for updates"
                a.informativeText = why + " Check your internet connection and try again."
                a.runModal()
            }
        }
    }

    private func offer(_ r: UpdateCheck.Release) {
        NSApp.activate(ignoringOtherApps: true)
        let a = NSAlert()
        a.messageText = "A new version is available: \(r.version)"
        a.informativeText = "You have \(appVersion). The release page has the download and what changed."
        a.addButton(withTitle: "Open release page")
        a.addButton(withTitle: "Later")
        if a.runModal() == .alertFirstButtonReturn { NSWorkspace.shared.open(r.page) }
    }

    @objc func toggleAutoUpdate() { UpdateCheck.automatic.toggle() }

    @objc func toggleLogin() { model.setLoginItem(!model.loginItem) }

    @objc func openSettings() {
        if window == nil {
            let w = NSWindow(contentRect: .zero, styleMask: [.titled, .closable, .miniaturizable, .resizable],
                             backing: .buffered, defer: false)
            w.title = "X52 Pro \(appVersion)"
            w.contentView = NSHostingView(rootView: SettingsView(m: model))
            w.isReleasedWhenClosed = false
            w.setContentSize(NSSize(width: 900, height: 720))
            w.contentMinSize = NSSize(width: 900, height: 600)
            w.setFrameAutosaveName("X52ProSettings")   // comes back where it was left
            if !w.setFrameUsingName("X52ProSettings") { w.center() }
            window = w
        }
        showInDock()
        NSApp.activate(ignoringOtherApps: true)
        window?.makeKeyAndOrderFront(nil)
    }

    // The pages editor gets a window of its own: there is a lot to see side by side.
    var pagesWindow: NSWindow?
    @objc func openPagesEditor() {
        if pagesWindow == nil {
            let w = NSWindow(contentRect: .zero, styleMask: [.titled, .closable, .miniaturizable, .resizable],
                             backing: .buffered, defer: false)
            w.title = "X52 Pro \(appVersion): Pages"
            w.contentView = NSHostingView(rootView: PagesEditor(m: model))
            w.isReleasedWhenClosed = false
            w.setContentSize(NSSize(width: 1320, height: 760))
            w.contentMinSize = NSSize(width: 980, height: 560)
            w.setFrameAutosaveName("X52ProPages")
            if !w.setFrameUsingName("X52ProPages") { w.center() }
            NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: w, queue: .main) { [weak self] _ in
                self?.model.pagesOpen = false
                x52_pages_watch(0)
            }
            pagesWindow = w
        }
        model.pagesOpen = true
        x52_pages_watch(1)
        showInDock()
        NSApp.activate(ignoringOtherApps: true)
        pagesWindow?.makeKeyAndOrderFront(nil)
    }

    // The stick calibration gets a window of its own, a fresh one each time (it starts at step 1).
    var calibrationWindow: NSWindow?
    @objc func openCalibration() {
        if calibrationWindow == nil {
            let w = NSWindow(contentRect: .zero, styleMask: [.titled, .closable, .miniaturizable],
                             backing: .buffered, defer: false)
            w.title = "X52 Pro \(appVersion): Calibrate stick"
            w.isReleasedWhenClosed = false
            w.contentView = NSHostingView(rootView: CalibrationWizard(m: model, close: { [weak self] in self?.calibrationWindow?.close() }))
            w.setContentSize(NSSize(width: 600, height: 700))
            w.center()
            NotificationCenter.default.addObserver(forName: NSWindow.willCloseNotification, object: w, queue: .main) { [weak self] _ in
                if x52_calibrating() != 0 { x52_calibrate(0) }   // closed half way: nothing changes
                self?.model.calibrationOpen = false
                self?.calibrationWindow = nil
            }
            calibrationWindow = w
        }
        model.calibrationOpen = true
        model.calStep = 0
        model.calTwistSkipped = false
        model.calProblem = ""
        model.tick()
        showInDock()
        NSApp.activate(ignoringOtherApps: true)
        calibrationWindow?.makeKeyAndOrderFront(nil)
    }

    var reallyQuit = false
    @objc func quit() {
        reallyQuit = true
        NSApp.terminate(nil)
    }

    func applicationWillTerminate(_ note: Notification) { model.stop() }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
}

// Tab reaches every control (buttons, check boxes, radio buttons, menus), as with "Keyboard navigation" on in
// System Settings, but for this app only. Space or Return opens a menu or presses a button; the arrows choose.
UserDefaults.standard.set(2, forKey: "AppleKeyboardUIMode")

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
