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
int x52_get(const char* name);
void x52_set(const char* name, int value);

const char* x52_profiles(void);                // keys, one per line
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
