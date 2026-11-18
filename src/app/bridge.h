// Plain C doorway from the Swift app to the C++ Agent. Strings returned here stay
// valid until the next call of the same function.
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// X-Plane 12 installs found on this Mac, one folder per line (best first).
const char* x52_find_xplane(void);
int x52_valid_xplane(const char* dir);
// 0 not installed, 1 installed but different from the app's copy, 2 up to date.
int x52_plugin_state(const char* xplane_dir, const char* bundled_xpl);
// Empty string on success, otherwise what went wrong.
const char* x52_install_plugin(const char* xplane_dir, const char* bundled_xpl);

void x52_start(const char* xplane_dir, const char* fallback_dir, const char* key);
void x52_stop(void);
void x52_tick(void);

int x52_connected(void);
int x52_is_pro(void);
int x52_plugin_active(void);
int x52_mode(void);
int x52_shift(void);                           // 1 while the pinkie switch is held
int x52_revision(void);
const char* x52_dir(void);
const char* x52_error(void);

int x52_led_count(void);
const char* x52_led_title(int led);
int x52_led_single(int led);
int x52_get_led(int mode, int led, int pinkie);   // mode 1..3; 0 off, 1 green, 2 amber, 3 red
void x52_set_led(int mode, int led, int pinkie, int color);   // pinkie 1: the colours used while it is held
void x52_copy_mode(int from_mode, int pinkie); // this mode's lights to the other two

// Conditional lights (rules): a light follows X-Plane datarefs. Colours: -1 the mode's colour, 0 off,
// 1 green, 2 amber, 3 red. Operators: 0 >, 1 >=, 2 <, 3 <=, 4 ==, 5 !=. One rule per light; they
// override the mode colours, and alarms override them.
int x52_preset_count(void);
const char* x52_preset_name(int i);
int x52_rule_count(void);
int x52_rule_led(int rule);
int x52_rule_then(int rule);
int x52_rule_else(int rule);
int x52_rule_add(void);                        // the new rule's number (on the first free light), -1 when every light has one
void x52_rule_remove(int rule);
// Returns -1 when applied, or the number of another rule that already uses that light (nothing is changed).
int x52_rule_set(int rule, int led, int then_color, int else_color);
int x52_rule_conflict(int rule);               // another rule on the same light (older files), or -1
void x52_rule_preset(int rule, int preset);    // fills in the rule, keeps its light

// Alarms: the highest priority, in order; the first one that is triggered decides its light.
// Effect: 0 steady, 1 blink, 2 alternate colour / colour 2. Speed: 0 slow (1 s a cycle), 1 fast (0.5 s).
int x52_alarm_count(void);
const char* x52_alarm_name(int alarm);
int x52_alarm_led(int alarm);
int x52_alarm_effect(int alarm);
int x52_alarm_fast(int alarm);
int x52_alarm_color(int alarm);                // 0..3
int x52_alarm_color2(int alarm);
int x52_alarm_add(void);                       // returns the new alarm's number
void x52_alarm_remove(int alarm);
void x52_alarm_move(int alarm, int direction); // -1 up (more important), 1 down
void x52_alarm_set(int alarm, const char* name, int led, int effect, int fast, int color, int color2);
int x52_alarm_preset_count(void);
const char* x52_alarm_preset_name(int i);
int x52_alarm_add_preset(int preset);          // adds a copy of the template, returns its number
void x52_alarm_try(int alarm);                 // shows the alarm on the stick for a few seconds
int x52_alarm_tried(void);                     // the alarm being shown that way, or -1

// What a rule (kind 0) or an alarm (kind 1) waits for: condition `which` 0 is the first, 1 the second.
// Join: 0 none (one condition), 1 and, 2 or.
const char* x52_trg_ref(int kind, int index, int which);
int x52_trg_op(int kind, int index, int which);
double x52_trg_value(int kind, int index, int which);
int x52_trg_negate(int kind, int index, int which);
int x52_trg_join(int kind, int index);
void x52_trg_set(int kind, int index, int which, const char* dataref, int op, double value, int negate);
void x52_trg_set_join(int kind, int index, int join);
const char* x52_trg_status(int kind, int index, int which);   // what the dataref is doing right now

// Settings by name: led_brightness, mfd_brightness (0..127), clock2_offset,
// clock3_offset (minutes from GMT), clock1_12h..clock3_12h, date_format, clutch_latched.
// Master switches: the screen (which 0) or the lights (which 1) are on only while their trigger is true.
// They use the trigger calls above with kind 2 and the index 0 (screen) or 1 (lights); no dataref means always on.
void x52_master_try(int which);                // switches it off on the stick for a few seconds
int x52_master_tried(void);                    // the one being shown off, or -1

// Brightness that follows a dataref: which 0 screen, 1 lights. The value runs from in_low to in_high and the
// brightness from out_low to out_high (0..127). No dataref: the fixed brightness slider applies.
const char* x52_bright_ref(int which);
double x52_bright_in_low(int which);
double x52_bright_in_high(int which);
int x52_bright_out_low(int which);
int x52_bright_out_high(int which);
void x52_bright_set(int which, const char* dataref, double in_low, double in_high, int out_low, int out_high);
const char* x52_bright_status(int which);      // what the dataref is doing right now
void x52_bright_try(int which, int level);     // 0 dim end, 1 bright end, for a few seconds
int x52_bright_tried(void);                    // -1 none

// ---- Button assignments, edited from the app. button 1..39, layer 0 any mode, 1..3 mode, 4..6 mode + pinkie.
// hold 0 is "when pressed", hold 1 "when held". Step kinds: 0 command, 1 keystroke, 2 wait, 3 dataref.
// Dataref ops: 0 set, 1 toggle, 2 add. Numbers (which): 0 value, 1 second value (toggle), 2 minimum, 3 maximum (add).
const char* x52_button_name(int button);
const char* x52_layer_name(int layer);
int x52_button_reserved(int button);           // mode dial, pinkie and clutch cannot be programmed
const char* x52_bind_summary(int button, int layer);   // what the button does there, "" when nothing is assigned
int x52_bind_exists(int button, int layer);
void x52_bind_clear(int button, int layer);
const char* x52_bind_label(int button, int layer, int hold);
void x52_bind_set_label(int button, int layer, int hold, const char* label);
int x52_step_count(int button, int layer, int hold);
int x52_step_kind(int button, int layer, int hold, int step);
const char* x52_step_text(int button, int layer, int hold, int step);
int x52_step_ms(int button, int layer, int hold, int step);
int x52_step_op(int button, int layer, int hold, int step);
double x52_step_num(int button, int layer, int hold, int step, int which);
int x52_step_add(int button, int layer, int hold, int kind);   // the new step's number, -1 if refused
void x52_step_remove(int button, int layer, int hold, int step);
void x52_step_move(int button, int layer, int hold, int step, int direction);   // -1 up, 1 down
void x52_step_set(int button, int layer, int hold, int step, const char* text, int ms, int op,
                  double value, double value2, double lo, double hi);

// The buttons this profile programs, in button order, one line each: "number<TAB>name<TAB>layers<TAB>slot<TAB>assigned".
// Layers are the layers' names joined by ", ". These are the ones to set to Do nothing in X-Plane's Settings >
// Joystick. slot is X-Plane's own number for the button (-1 until the plugin has seen it pressed in X-Plane),
// assigned is the command X-Plane has on it according to its joystick settings file ("" when not known),
// "sim/none/none" meaning Do nothing. slot is the number X-Plane's Joystick screen shows ("Btn 2"); -1 unknown. Read-only: nothing is ever changed in X-Plane's files.
const char* x52_bound_buttons(void);
const char* x52_assignment_source(void);       // which of X-Plane's joystick files the assignments were read from

// Import what X-Plane has on the stick's buttons into a NEW profile (never over an existing one, and nothing in
// X-Plane is changed). X-Plane's joystick profiles, one name per line, the one for the aircraft last loaded first;
// the last line is X-Plane's current settings.
const char* x52_xplane_profiles(void);
const char* x52_import_suggest(const char* xplane_profile);   // a free name for the new profile
// Creates the profile (buttons with a command in X-Plane become "Any mode" assignments) and switches to it.
// Returns how many buttons were imported, or -1 with the reason in x52_import_message().
int x52_import_profile(const char* xplane_profile, const char* new_name);
const char* x52_import_message(void);

// X-Plane's own lists, read from Resources/plugins/Commands.txt (kind 0) and DataRefs.txt (kind 1) of the
// X-Plane folder in use, so they can be searched without X-Plane running. Result: one line per match,
// "name<TAB>info<TAB>1 or 0 (writable)", best match first.
int x52_catalog_size(int kind);
const char* x52_catalog_search(int kind, const char* query, int max);

// ---- The user's own screen pages (see src/core/pagetree.h). Pages are numbered in file order. A page with
// parent "" is reached with the left wheel; otherwise only a link row opens it. Row kinds: 0 text, 1 value, 2 link.
int x52_page_count(void);
const char* x52_page_id(int page);
const char* x52_page_title(int page);
const char* x52_page_parent(int page);         // the parent's id, "" for a top-level page
// The pages as the editor lists them, one line each: "page<TAB>depth": top-level pages in wheel order, each
// followed by the pages inside it (depth 1, 2, ...); pages whose parent is missing come last at depth 0.
const char* x52_pages_outline(void);
int x52_page_add(const char* title, const char* parent);   // the new page's number (its id comes from the title)
void x52_page_remove(int page);                // also its sub-pages; links to any of them are cleared
void x52_page_move(int page, int direction);   // -1 / 1: past the previous / next page with the same parent
// Returns 1 when applied; 0 when the parent would put the page inside itself or does not exist (nothing changed).
int x52_page_set(int page, const char* title, const char* parent);
int x52_row_count(int page);
int x52_row_kind(int page, int row);
const char* x52_row_text(int page, int row);
const char* x52_row_format(int page, int row);
const char* x52_row_dataref(int page, int row);
const char* x52_row_target(int page, int row);
int x52_row_add(int page, int kind);           // the new row's number, -1 if refused
void x52_row_remove(int page, int row);
void x52_row_move(int page, int row, int direction);
void x52_row_set(int page, int row, int kind, const char* text, const char* format, const char* dataref, const char* target);
// The left wheel: every page it can reach, in order, one line each: "token<TAB>title<TAB>hidden 1/0<TAB>built-in 1/0".
// Moving or hiding one saves the whole order in the profile (wheel_order, wheel_hidden).
// token is a built-in page's key ("com1", "adf", ...) or "@<page id>".
const char* x52_wheel_entries(void);
void x52_wheel_move(const char* token, int direction);
void x52_wheel_hide(const char* token, int hidden);
const char* x52_builtin_preview(const char* token, int line);   // a built-in page with the sim's values blank
void x52_pages_restore_default(void);          // also forgets the left wheel's order and hidden pages          // no user pages: the stick shows the built-in pages only
// Line 0..2 of the page as the stick shows it, the first row under the cursor. Live numbers when X-Plane sends
// them (see x52_pages_watch), otherwise a made-up 1234.5; x52_page_preview_live says which.
const char* x52_page_preview(int page, int line);
int x52_page_preview_live(int page);
void x52_pages_watch(int on);                  // ask X-Plane for the pages' datarefs (while the editor is open)
const char* x52_pages_source(void);            // the page and row lines of the profile file, as saved
const char* x52_format_example(const char* format, double value);

int x52_get(const char* name);
void x52_set(const char* name, int value);

// Every analog control the stick reports, one line each: "name<TAB>percent<TAB>value text<TAB>raw<TAB>moving 1/0<TAB>usage".
// Settings by name for the flight axes: axis_override, axis_<roll|pitch|yaw|throttle>_deadzone (0..40 %),
// _expo (0..100 %) and _invert; for the right wheel: dial_accel and dial_factor (2..50); throttle_plugin (the plugin drives the throttle on its own), slider_flaps (the slider
// sets the flap lever) and slider_invert (invert the flaps: slider towards you = deployed).
const char* x52_axes(void);

const char* x52_profiles(void);                // keys, one per line
// Ready-made profiles (standard) are never changed. Any change to one waits (x52_copy_needed) for a name:
// x52_copy_save puts it, and the changes after it, into a new profile and switches to it ("" on success, else why);
// x52_copy_cancel drops them. x52_copy_ask asks for a name without a change waiting ("Save as new profile").
int x52_standard(void);                        // the profile being edited is a ready-made one
int x52_profile_standard(const char* key);
int x52_copy_needed(void);
void x52_copy_ask(void);
const char* x52_copy_suggest(void);
const char* x52_copy_save(const char* name);
void x52_copy_cancel(void);
const char* x52_key(void);
void x52_select(const char* key);

const char* x52_screen(int line);
const char* x52_held(void);
const char* x52_moved(void);
void x52_test(int on);
void x52_text(const char* l0, const char* l1, const char* l2);
void x52_clear_text(void);

#ifdef __cplusplus
}
#endif
