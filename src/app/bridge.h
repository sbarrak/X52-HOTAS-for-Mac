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

// Live lights: a light follows an X-Plane dataref. Colours: -1 the mode's colour, 0 off, 1 green,
// 2 amber, 3 red. Operators: 0 >, 1 >=, 2 <, 3 <=, 4 ==, 5 !=. Several rules per light are fine.
int x52_preset_count(void);
const char* x52_preset_name(int i);
int x52_rule_count(void);
int x52_rule_led(int rule);
const char* x52_rule_dataref(int rule);
int x52_rule_op(int rule);
double x52_rule_value(int rule);
int x52_rule_then(int rule);
int x52_rule_else(int rule);
const char* x52_rule_status(int rule);         // what the rule's dataref is doing right now
int x52_rule_add(void);                        // returns the new rule's number
void x52_rule_remove(int rule);
void x52_rule_set(int rule, int led, const char* dataref, int op, double value, int then_color, int else_color);
void x52_rule_preset(int rule, int preset);    // fills in the rule, keeps its light

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
