// X52 Pro menu bar app: a control panel for the stick's lights, screen and clock that
// also keeps them applied in the background. All the work is in the C++ Agent (see
// bridge.h); this file is the menu bar icon and the settings window.
import AppKit
import ServiceManagement
import SwiftUI

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
    @Published var mode = 1
    @Published var shift = false        // pinkie switch held
    @Published var lightsPinkie = false // Lights tab: editing the pinkie-held colours
    @Published var revision = -1
    @Published var error = ""
    @Published var held = ""
    @Published var moved = ""
    @Published var screen = ["", "", ""]
    @Published var profiles: [String] = ["default"]
    @Published var lastDeleted = ""             // the profile "Undo delete" brings back
    @Published var key = "default"
    @Published var folder = ""
    @Published var xplane = ""          // the X-Plane 12 folder in use ("" when none was found)
    @Published var tab = 0              // selected tab of the settings window
    @Published var liveOpen: Set<Int> = []     // live light rules shown opened
    @Published var liveChecked: Set<Int> = []  // ticked for "Remove selected"
    @Published var liveStatus: [Int: String] = [:]   // what X-Plane makes of each opened rule's / alarm's datarefs (see statusKey)
    @Published var liveNotice = ""      // why a live light change was refused
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
    @Published var pageSel = ""         // id of the page selected on the Pages tab and in the pages editor
    @Published var wheelMode = 0        // the left wheel's order being edited: 0 every mode, 1..3 that mode's own
    @Published var pageNote = ""        // why a page change was refused
    @Published var pagesConfirm = false // asking before "Restore default pages"
    @Published var pagesOpen = false    // the pages editor window is open
    @Published var pagePreview = ["", "", ""]   // the selected page as the stick shows it
    @Published var pagePreviewLive = false      // with numbers from X-Plane (otherwise made up)
    var openPagesEditor: () -> Void = {}
    @Published var isStandard = false            // the profile being edited is ready-made (never changed)
    @Published var standardKeys: Set<String> = []
    private var askingCopy = false
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

    func tick() {
        x52_tick()
        set(\.connected, x52_connected() != 0)
        set(\.isPro, x52_is_pro() != 0 || x52_connected() == 0)
        set(\.pluginActive, x52_plugin_active() != 0)
        set(\.mode, Int(x52_mode()))
        set(\.shift, x52_shift() != 0)
        set(\.error, str(x52_error()))
        set(\.held, str(x52_held()))
        set(\.moved, str(x52_moved()))
        set(\.screen, (0..<3).map { str(x52_screen(Int32($0))) })
        set(\.revision, Int(x52_revision()))
        set(\.isStandard, x52_standard() != 0)
        if x52_copy_needed() != 0 && !askingCopy {
            if Date() < ignoreCopyUntil || (copyDeclined && NSEvent.pressedMouseButtons != 0) {
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
        if pluginActive { return "X-Plane is running and its plugin is driving the stick (\(layerText))" }
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

// The picker's line for a profile: ready-made ones say so.
func pickerTitle(_ key: String, _ standard: Set<String>) -> String {
    profileTitle(key) + (standard.contains(key) ? "  (ready-made)" : "")
}

// Shown above the tabs while a ready-made profile is selected.
struct StandardBanner: View {
    @ObservedObject var m: Model
    var body: some View {
        if m.isStandard {
            HStack(spacing: 10) {
                Image(systemName: "lock.fill")
                Text("Ready-made profile: it is never changed, so you can always go back to it. A change is saved as a new profile of yours.")
                    .fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 8)
                Button("Save as new profile...") { x52_copy_ask(); m.tick() }
            }
            .padding(8)
            .background(RoundedRectangle(cornerRadius: 6).fill(Color.accentColor.opacity(0.12)))
        }
    }
}

// MARK: - Lights

let swatch: [Color] = [
    Color.gray.opacity(0.25), Color(red: 0.2, green: 0.8, blue: 0.3), Color(red: 1.0, green: 0.7, blue: 0.1),
    Color(red: 0.95, green: 0.25, blue: 0.2),
]
let colorNames = ["Off", "Green", "Amber", "Red"]

struct LightCell: View {
    @ObservedObject var m: Model
    let mode: Int
    let led: Int
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
                .buttonStyle(.plain)
                .help(single && c == 1 ? "On" : colorNames[c])
                .accessibilityLabel("\(str(x52_led_title(Int32(led)))), mode \(mode), \(single && c == 1 ? "on" : colorNames[c])")
            }
        }
        .frame(width: 110, alignment: .leading)
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
    var body: some View {
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
                        ForEach(1...3, id: \.self) { mode in LightCell(m: m, mode: mode, led: led) }
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
    return "\(conditionText(kind, index, 0)) \(joinNames[join]) \(conditionText(kind, index, 1))"
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
                Text(which == 0 ? "Dataref" : "Second dataref").frame(width: 110, alignment: .leading)
                TextField("e.g. sim/flightmodel/controls/parkbrake, or name[3]", text: ref)
                    .textFieldStyle(.roundedBorder)
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

// The first test, and an optional second one joined with AND / OR.
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
                .buttonStyle(.plain)
            }
        }
        .background(Color.secondary.opacity(0.15))
        .clipShape(RoundedRectangle(cornerRadius: 6))
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
        let clash = Int(x52_rule_conflict(r))
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
            if clash >= 0 {
                Text("This light is also used by rule \(clash + 1). That one comes first, so it decides the colour and this rule never shows. Give one of them another light, or remove one.")
                    .font(.callout).foregroundStyle(.orange).fixedSize(horizontal: false, vertical: true)
            }
            TriggerEditor(m: m, kind: 0, index: rule)
            HStack {
                Text("Light is").frame(width: 110, alignment: .leading)
                ColorChoice(selection: then, single: single)
                Text("when true, and")
                ColorChoice(selection: other, single: single)
                Text("when not")
            }
        }
        .padding(.leading, 28).padding(.vertical, 6)
    }
    private func write(led: Int? = nil, then: Int? = nil, other: Int? = nil) {
        let r = Int32(rule)
        let target = led ?? Int(x52_rule_led(r))
        let clash = Int(x52_rule_set(r, Int32(target), Int32(then ?? Int(x52_rule_then(r))), Int32(other ?? Int(x52_rule_else(r)))))
        if clash >= 0 {
            m.liveNotice = "\(str(x52_led_title(Int32(target)))) already follows rule \(clash + 1). A light can follow only one rule, because a rule decides the colour whether its test is true or not, so a second rule on the same light would never be seen. The light was not changed: pick a free light, or remove the other rule first."
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
            Text("Live lights follow the simulator instead of the mode dial. Open a rule to set what it watches and the colours it shows. A rule overrides the plain colours from the Lights tab, and alarms (Alarms tab) override rules. Each light can have one rule.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            HStack {
                Button("Add rule") {
                    let i = Int(x52_rule_add())
                    if i >= 0 {
                        m.liveOpen.insert(i)
                        m.liveNotice = ""
                    } else {
                        m.liveNotice = "Every light already has a rule. A light can follow only one rule, so remove one or change an existing rule."
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
            Text("Alarms have the highest priority: an alarm overrules the live lights (Live lights tab), and those overrule the plain colours (Lights tab). The first alarm in the list that is triggered decides its light. An alarm that is not triggered leaves the light to the next one, so a gear alarm can still show while the stall alarm above it is quiet.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            HStack {
                Menu("Add from template") {
                    ForEach(0..<Int(x52_alarm_preset_count()), id: \.self) { i in
                        Button(str(x52_alarm_preset_name(Int32(i)))) {
                            let n = Int(x52_alarm_add_preset(Int32(i)))
                            if n >= 0 { m.alarmOpen.insert(n) }
                            m.tick()
                        }
                    }
                }
                .frame(width: 150)
                Button("Add blank alarm") {
                    let n = Int(x52_alarm_add())
                    if n >= 0 { m.alarmOpen.insert(n) }
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

struct ScreenTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let date = Binding<Int>(get: { m.get("date_format") }, set: { m.set("date_format", $0) })
        let latched = Binding<Bool>(get: { m.get("clutch_latched") != 0 }, set: { m.set("clutch_latched", $0 ? 1 : 0) })
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
            Spacer()
        }
    }
}

// MARK: - Buttons

// A text field for an X-Plane command (kind 0) or dataref (kind 1) that suggests matches from X-Plane's own
// lists (Commands.txt / DataRefs.txt) while you type.
struct CatalogField: View {
    let kind: Int
    let placeholder: String
    @Binding var text: String
    @FocusState private var focused: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            TextField(placeholder, text: $text).textFieldStyle(.roundedBorder).focused($focused)
            if focused && !text.isEmpty {
                let rows = matches()
                if !rows.isEmpty && !(rows.count == 1 && rows[0][0] == text) {
                    VStack(alignment: .leading, spacing: 0) {
                        ForEach(0..<rows.count, id: \.self) { i in
                            Button { text = rows[i][0]; focused = false } label: {
                                HStack(spacing: 8) {
                                    Text(rows[i][0]).lineLimit(1).truncationMode(.middle).layoutPriority(1)
                                    Text(rows[i][1] + (rows[i][2] == "0" ? "  (read-only)" : ""))
                                        .foregroundStyle(.secondary).lineLimit(1).truncationMode(.tail)
                                    Spacer(minLength: 0)
                                }
                                .padding(.vertical, 3).padding(.horizontal, 6)
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.12)))
                }
            }
        }
    }
    private func matches() -> [[String]] {
        str(x52_catalog_search(Int32(kind), text, 8)).split(separator: "\n").map {
            $0.split(separator: "\t", omittingEmptySubsequences: false).map(String.init)
        }.filter { $0.count >= 3 }
    }
}

let stepTags = ["CMD", "KEY", "WAIT", "DATA"]
let datarefOps = ["Set", "Toggle", "Add"]

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
                    FixedSegments(selection: op, names: datarefOps, width: 70)
                    if op.wrappedValue == 0 {
                        Text("to"); TextField("", value: n(0), format: .number).textFieldStyle(.roundedBorder).frame(width: 80)
                    } else if op.wrappedValue == 1 {
                        Text("between"); TextField("", value: n(0), format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
                        Text("and"); TextField("", value: n(1), format: .number).textFieldStyle(.roundedBorder).frame(width: 70)
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
            HStack {
                Text("Name on screen").frame(width: 110, alignment: .leading)
                TextField("up to 16 characters", text: label).textFieldStyle(.roundedBorder).frame(width: 220)
            }
            ForEach(0..<Int(x52_step_count(b, l, h)), id: \.self) { StepRow(m: m, hold: hold, index: $0) }
            HStack {
                Button("+ Command") { _ = x52_step_add(b, l, h, 0); m.tick() }
                Button("+ Keystroke") { _ = x52_step_add(b, l, h, 1); m.tick() }
                Button("+ Dataref") { _ = x52_step_add(b, l, h, 3); m.tick() }
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
                ScrollView {
                    VStack(spacing: 0) {
                        ForEach(1...39, id: \.self) { b in
                            let sel = m.btn == b
                            let res = x52_button_reserved(Int32(b)) != 0
                            Button { m.btn = b } label: {
                                HStack(spacing: 8) {
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
                        }
                    }
                }
                .frame(width: 250)
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.25)))
                ScrollView {
                    editor.padding(.trailing, 8)
                }
            }
            ClearList(m: m)
            Text("The numbers are the usual X52 button numbers (JOY1 to JOY39 in Windows lists): 9 and 10 are toggle 1 up and down, 11 and 12 toggle 2, 13 and 14 toggle 3, 15 the trigger's second stage, 16 the mouse button, 17 to 19 the scroll wheel, 20 to 23 the stick's hat, 24 to 27 the throttle's hat, 28 to 30 the mode dial, 31 the clutch.")
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
            .frame(maxHeight: 130)
            Spacer()
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
        }
    }
}

struct AxesTab: View {
    @ObservedObject var m: Model
    var body: some View {
        let on = Binding<Bool>(get: { m.get("axis_override") != 0 }, set: { m.set("axis_override", $0 ? 1 : 0) })
        VStack(alignment: .leading, spacing: 10) {
            Toggle("The plugin drives the stick and twist (overrides X-Plane's own axis settings)", isOn: on)
            Text(on.wrappedValue
                 ? "While X-Plane runs, the plugin reads the stick and gives X-Plane the result. The dead zone, curve and inversion below apply."
                 : "Off: X-Plane reads the stick itself, with its own Joystick settings and null zone. The settings below wait until you switch this on.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Toggle("The plugin drives the throttle (engine power and the cockpit levers)", isOn: Binding(get: { m.get("throttle_plugin") != 0 }, set: { m.set("throttle_plugin", $0 ? 1 : 0) }))
                .help("Engine power and the cockpit levers follow the X52 lever whatever X-Plane's own axis assignment is. The stick and twist stay X-Plane's.")
            Text("The throttle is only taken over once it has moved. This works inside X-Plane only: the app itself never moves the controls.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            AxisCurveRow(m: m, title: "Stick left/right", key: "roll", usage: 0x30)
            AxisCurveRow(m: m, title: "Stick fwd/back", key: "pitch", usage: 0x31)
            AxisCurveRow(m: m, title: "Stick twist", key: "yaw", usage: 0x35)
            AxisCurveRow(m: m, title: "Throttle", key: "throttle", usage: 0x32)
            Text("Dials and slider: a change smaller than Steady is ignored, so the value does not flicker. Saved in the aircraft's profile.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            DialSteadyRow(m: m, title: "Lower rotary (i)", key: "lower", usage: 0x33)
            DialSteadyRow(m: m, title: "Upper rotary (E)", key: "upper", usage: 0x34)
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
            ScrollView {
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
            Spacer()
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

struct WheelList: View {
    @ObservedObject var m: Model
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
                        .buttonStyle(.plain).foregroundStyle(e.hidden ? Color.secondary : Color.primary)
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
                        .buttonStyle(.plain)
                    }
                    if !e.builtin { SubPageRows(m: m, outline: outline, topId: String(e.token.dropFirst())) }
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
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
            .buttonStyle(.plain)
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
                PageList(m: m).frame(width: 300, height: 360)
                    .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color.secondary.opacity(0.25)))
                PagePreview(m: m)
            }
            Spacer()
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
                Picker("", selection: Binding(get: { m.key }, set: { m.select($0); m.pageSel = ""; m.pageNote = "" })) {
                    ForEach(m.profiles, id: \.self) { Text(pickerTitle($0, m.standardKeys)).tag($0) }
                }
                .labelsHidden().frame(width: 280)
                Spacer()
                RestorePagesButton(m: m)
            }
            if !m.error.isEmpty {
                Text(m.error).foregroundStyle(.red).fixedSize(horizontal: false, vertical: true)
            }
            StandardBanner(m: m)
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
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Circle().fill(m.pluginActive ? Color.blue : m.connected ? Color.green : Color.gray).frame(width: 9, height: 9)
                Text(m.statusText)
                Spacer()
                if !m.lastDeleted.isEmpty {
                    Button("Undo delete") { m.undoDelete() }
                        .help("Brings back “\(profileTitle(m.lastDeleted))”")
                }
                Picker("Profile", selection: Binding(get: { m.key }, set: { m.select($0) })) {
                    ForEach(m.profiles, id: \.self) { Text(pickerTitle($0, m.standardKeys)).tag($0) }
                }
                .frame(width: 280)
                Menu {
                    Button("New blank profile...") { m.newBlank() }
                    Button("Save as new profile...") { x52_copy_ask(); m.tick() }
                    Divider()
                    Button("Delete this profile...") { m.deleteCurrent() }.disabled(!m.canDelete)
                } label: {
                    Image(systemName: "ellipsis.circle")
                }
                .menuStyle(.borderlessButton).fixedSize()
                .help(m.canDelete ? "New, copy or delete a profile" : "New or copy a profile (Default and ready-made profiles are never deleted)")
            }
            if m.pluginActive {
                Text("Changes you make here are saved, and X-Plane applies them within a second.")
                    .font(.callout).foregroundStyle(.secondary)
            }
            if !m.error.isEmpty {
                Text(m.error).foregroundStyle(.red).fixedSize(horizontal: false, vertical: true)
            }
            StandardBanner(m: m)
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
                        .buttonStyle(.plain)
                        .keyboardShortcut(KeyEquivalent(Character(String(n + 1))), modifiers: .command)
                        .help("\(entry.1) (Command-\(n + 1))")
                    }
                }
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

// MARK: - App

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {
    let model = Model()
    var item: NSStatusItem!
    var window: NSWindow?
    let statusLine = NSMenuItem(title: "", action: nil, keyEquivalent: "")
    let loginLine = NSMenuItem(title: "Start at login", action: #selector(toggleLogin), keyEquivalent: "")

    func applicationDidFinishLaunching(_ note: Notification) {
        model.start()
        model.openPagesEditor = { [weak self] in self?.openPagesEditor() }
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
        menu.addItem(withTitle: "Open settings...", action: #selector(openSettings), keyEquivalent: ",").target = self
        menu.addItem(withTitle: "Open pages editor...", action: #selector(openPagesEditor), keyEquivalent: "").target = self
        loginLine.target = self
        menu.addItem(loginLine)
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit X52 Pro", action: #selector(quit), keyEquivalent: "q").target = self
        item.menu = menu
        installEditMenu()

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
        edit.addItem(withTitle: "Undo", action: Selector(("undo:")), keyEquivalent: "z")
        let redo = edit.addItem(withTitle: "Redo", action: Selector(("redo:")), keyEquivalent: "z")
        redo.keyEquivalentModifierMask = [.command, .shift]
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

    func menuWillOpen(_ menu: NSMenu) {
        statusLine.title = model.statusText
        model.loginItem = SMAppService.mainApp.status == .enabled
        loginLine.state = model.loginItem ? .on : .off
    }

    @objc func toggleLogin() { model.setLoginItem(!model.loginItem) }

    @objc func openSettings() {
        if window == nil {
            let w = NSWindow(contentRect: .zero, styleMask: [.titled, .closable, .miniaturizable, .resizable],
                             backing: .buffered, defer: false)
            w.title = "X52 Pro"
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
            w.title = "X52 Pro: Pages"
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

    var reallyQuit = false
    @objc func quit() {
        reallyQuit = true
        NSApp.terminate(nil)
    }

    func applicationWillTerminate(_ note: Notification) { model.stop() }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
