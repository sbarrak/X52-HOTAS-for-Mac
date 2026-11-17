// X52 Pro menu bar app: a control panel for the stick's lights, screen and clock that
// also keeps them applied in the background. All the work is in the C++ Agent (see
// bridge.h); this file is the menu bar icon and the settings window.
import AppKit
import ServiceManagement
import SwiftUI

func str(_ p: UnsafePointer<CChar>?) -> String { p.map { String(cString: $0) } ?? "" }

// MARK: - Model

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
    @Published var pluginState = 0      // 0 not installed, 1 out of date, 2 up to date
    @Published var pluginMessage = ""
    @Published var loginItem = false
    @Published var testLines = ["", "", ""]   // text typed on the Test tab

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
        set(\.liveStatus, statusMap())
        set(\.tried, Int(x52_alarm_tried()))
        slow += 1
        if slow % 40 == 1 {
            set(\.profiles, str(x52_profiles()).split(separator: "\n").map(String.init))
            set(\.key, str(x52_key()))
            refreshPlugin()
        }
    }

    // What X-Plane makes of the datarefs of the rows opened on the Live lights or Alarms tab.
    private func statusMap() -> [Int: String] {
        guard tab == 1 || tab == 2 else { return [:] }
        let kind = tab == 1 ? 0 : 1
        var d: [Int: String] = [:]
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
    func get(_ name: String) -> Int { Int(x52_get(name)) }
    func set(_ name: String, _ value: Int) {
        guard get(name) != value else { return }
        x52_set(name, Int32(value))
        tick()
    }
    func select(_ k: String) {
        x52_select(k)
        UserDefaults.standard.set(str(x52_key()), forKey: "profileKey")
        key = str(x52_key())
        tick()
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

func profileTitle(_ key: String) -> String {
    key == "default" ? "Default (all aircraft)" : key.hasPrefix("aircraft-") ? String(key.dropFirst(9)) : key
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
            Text("The mode dial on the stick picks the column, and holding the pinkie switch picks the Pinkie set. When you change a light in another mode, the stick shows that mode for a few seconds.")
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

// kind 0 is a live light rule, kind 1 an alarm
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
                TextField("e.g. sim/cockpit2/controls/parking_brake_ratio, or name[3]", text: ref)
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

struct ClockRow: View {
    @ObservedObject var m: Model
    let number: Int
    var body: some View {
        let h12 = Binding<Bool>(get: { m.get("clock\(number)_12h") != 0 }, set: { m.set("clock\(number)_12h", $0 ? 1 : 0) })
        HStack {
            Text("Clock \(number)").frame(width: 130, alignment: .leading)
            if number == 1 {
                Text("GMT (fixed)").frame(width: 150, alignment: .leading).foregroundStyle(.secondary)
            } else {
                let name = "clock\(number)_offset"
                let offset = Binding<Int>(get: { m.get(name) }, set: { m.set(name, $0) })
                Picker("", selection: offset) {
                    ForEach(Array(stride(from: -720, through: 840, by: 30)), id: \.self) { Text(offsetLabel($0)).tag($0) }
                    if m.get(name) % 30 != 0 { Text(offsetLabel(m.get(name))).tag(m.get(name)) }
                }
                .labelsHidden().frame(width: 150)
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
            Text("On the stick, the Start/Stop and Reset buttons under the screen switch between the three clocks. Function switches to the stopwatch. Inside X-Plane, Clock 1 follows the simulator's GMT.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Divider()
            Toggle("Latched clutch: one press of the clutch (i) turns safe mode on, the next turns it off", isOn: latched)
            Text("Safe mode applies in X-Plane: buttons only show their name on the screen. The clutch and POV lights flash while it is on.")
                .font(.callout).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            Spacer()
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

// MARK: - Window

struct SettingsView: View {
    @ObservedObject var m: Model
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Circle().fill(m.pluginActive ? Color.blue : m.connected ? Color.green : Color.gray).frame(width: 9, height: 9)
                Text(m.statusText)
                Spacer()
                Picker("Profile", selection: Binding(get: { m.key }, set: { m.select($0) })) {
                    ForEach(m.profiles, id: \.self) { Text(profileTitle($0)).tag($0) }
                }
                .frame(width: 280)
            }
            if m.pluginActive {
                Text("Changes you make here are saved, and X-Plane applies them within a second.")
                    .font(.callout).foregroundStyle(.secondary)
            }
            if !m.error.isEmpty {
                Text(m.error).foregroundStyle(.red).fixedSize(horizontal: false, vertical: true)
            }
            TabView(selection: $m.tab) {
                LightsTab(m: m).padding(14).tabItem { Text("Lights") }.tag(0)
                LiveTab(m: m).padding(14).tabItem { Text("Live lights") }.tag(1)
                AlarmsTab(m: m).padding(14).tabItem { Text("Alarms") }.tag(2)
                ScreenTab(m: m).padding(14).tabItem { Text("Screen & clock") }.tag(3)
                TestTab(m: m).padding(14).tabItem { Text("Test") }.tag(4)
            }
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
        .frame(width: 720, height: 640)
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
        loginLine.target = self
        menu.addItem(loginLine)
        menu.addItem(.separator())
        menu.addItem(withTitle: "Quit X52 Pro", action: #selector(quit), keyEquivalent: "q").target = self
        item.menu = menu
        installEditMenu()

        // Opened by hand (not at login): show the window so there is something to see.
        if !CommandLine.arguments.contains("--background") && !launchedAsLoginItem() { openSettings() }
    }

    // An app without a Dock icon has no main menu, and without one Command+C, V, X, A and Z do nothing in a
    // text field. The menu is never shown; its key equivalents are what the fields respond to.
    func installEditMenu() {
        let main = NSMenu()
        let appItem = NSMenuItem()
        main.addItem(appItem)
        let appMenu = NSMenu()
        appMenu.addItem(withTitle: "Close Window", action: #selector(NSWindow.performClose(_:)), keyEquivalent: "w")
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
        NSApp.mainMenu = main
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
            let w = NSWindow(contentRect: .zero, styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
            w.title = "X52 Pro"
            w.contentView = NSHostingView(rootView: SettingsView(m: model))
            w.isReleasedWhenClosed = false
            w.setContentSize(NSSize(width: 720, height: 640))
            w.center()
            window = w
        }
        NSApp.activate(ignoringOtherApps: true)
        window?.makeKeyAndOrderFront(nil)
    }

    @objc func quit() { NSApp.terminate(nil) }

    func applicationWillTerminate(_ note: Notification) { model.stop() }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.accessory)
app.run()
