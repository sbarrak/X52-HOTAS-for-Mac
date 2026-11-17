// The "Program buttons" window. A small immediate-mode UI drawn with X-Plane's own
// text and OpenGL rectangles: every frame draws the whole window and records which
// rectangles are clickable; a mouse click runs the action of the rectangle it lands on.
#include <OpenGL/gl.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <set>
#include <sstream>

#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"

#include "../core/keys.h"
#include "../mac/keysend.h"
#include "app.h"

namespace x52 {

namespace {

struct Rect {
    int l, t, r, b;   // X-Plane window coordinates: t is above b
    bool has(int x, int y) const { return x >= l && x < r && y >= b && y < t; }
    int w() const { return r - l; }
};

struct Hit {
    Rect r;
    std::function<void()> fn;
};
struct ScrollArea {
    Rect r;
    int* value;
    int max;
};

const int kPad = 10, kRow = 22, kGap = 6;
const float kWhite[3] = {0.95f, 0.95f, 0.95f};
const float kDim[3] = {0.62f, 0.66f, 0.70f};
const float kAmber[3] = {1.0f, 0.75f, 0.25f};
const float kRed[3] = {1.0f, 0.45f, 0.40f};
const float kGreen[3] = {0.45f, 0.90f, 0.50f};
const float cPanel[4] = {0.10f, 0.12f, 0.14f, 0.85f};
const float cButton[4] = {0.24f, 0.28f, 0.33f, 1.0f};
const float cButtonOn[4] = {0.16f, 0.46f, 0.72f, 1.0f};
const float cField[4] = {0.05f, 0.06f, 0.07f, 1.0f};
const float cFieldFocus[4] = {0.08f, 0.16f, 0.26f, 1.0f};
const float cSelected[4] = {0.16f, 0.36f, 0.56f, 0.9f};
const float cPressed[4] = {0.30f, 0.85f, 0.40f, 1.0f};
const float kDim4[4] = {0.62f, 0.66f, 0.70f, 1.0f};
const float cTag[4] = {0.18f, 0.20f, 0.24f, 1.0f};

XPLMWindowID win = nullptr;
std::vector<Hit> hits;
std::vector<ScrollArea> scrolls;
int fontH = 10;

enum Tab { TabButtons, TabLights, TabLive, TabSettings };
int tab = TabButtons;
int selButton = 1;
int selLayer = kLayerAny;
bool follow = true;
long seenSerial = 0;
int listScroll = 0;
int lightsMode = 0;
bool lightsPinkie = false;   // editing the colours used while the pinkie is held
std::string liveValueText;   // the number as typed, until it parses
bool confirmDrop = false;

// command picker
bool pickerOpen = false;
bool pickerHold = false;
int pickerStep = -1;   // -1 = add a new step
std::string pickerQuery;
std::string pickerResultsFor = "\x01";
std::vector<int> pickerResults;
int pickerScroll = 0;

// keyboard focus: a text field being edited, or a key being recorded
int focusId = 0;
std::function<std::string()> focusGet;
std::function<void(const std::string&)> focusSet;
bool capturing = false;
std::function<void(const std::string&)> captureDone;

// text cursor of the field being edited: `caret` is where typing goes, `anchor` the other end of the
// selection (the same number = nothing selected). Positions count characters; only plain ASCII is typed.
int caret = 0, anchor = 0;
int fieldTextX = 0;      // where the focused field's text starts, set while drawing
int fieldScroll = 0;     // first character shown when the text is wider than the field
int clickX = 0;          // x of the mouse press being handled
bool dragSelecting = false;

// ---------------------------------------------------------------- clipboard

}  // namespace
#ifdef X52_NO_GL
std::string uiTestClipboard;
#endif
namespace {

std::string clipboardGet() {
#ifdef X52_NO_GL
    return uiTestClipboard;
#else
    std::string out;
    if (FILE* p = popen("/usr/bin/pbpaste", "r")) {
        char buf[512];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0 && out.size() < 4096) out.append(buf, n);
        pclose(p);
    }
    return out;
#endif
}

void clipboardSet(const std::string& s) {
#ifdef X52_NO_GL
    uiTestClipboard = s;
#else
    if (FILE* p = popen("/usr/bin/pbcopy", "w")) {
        fwrite(s.data(), 1, s.size(), p);
        pclose(p);
    }
#endif
}

// ---------------------------------------------------------------- drawing helpers

void fill(const Rect& r, const float c[4]) {
#ifdef X52_NO_GL   // unit tests run without a graphics context
    (void)r;
    (void)c;
#else
    XPLMSetGraphicsState(0, 0, 0, 0, 1, 0, 0);
    glColor4f(c[0], c[1], c[2], c[3]);
    glBegin(GL_QUADS);
    glVertex2i(r.l, r.b);
    glVertex2i(r.l, r.t);
    glVertex2i(r.r, r.t);
    glVertex2i(r.r, r.b);
    glEnd();
#endif
}

int textW(const std::string& s) {
    return (int)XPLMMeasureString(xplmFont_Proportional, s.c_str(), (int)s.size());
}

std::string fit(const std::string& s, int maxW) {
    if (maxW <= 0) return "";
    if (textW(s) <= maxW) return s;
    std::string t = s;
    while (!t.empty() && textW(t + "...") > maxW) t.pop_back();
    return t + "...";
}

void text(int x, const Rect& row, const std::string& s, const float* col = kWhite) {
    if (s.empty()) return;
    float c[3] = {col[0], col[1], col[2]};
    int y = row.b + (row.t - row.b - fontH) / 2 + 2;
    XPLMDrawString(c, x, y, const_cast<char*>(s.c_str()), nullptr, xplmFont_Proportional);
}

void click(const Rect& r, std::function<void()> fn) { hits.push_back({r, std::move(fn)}); }

void button(const Rect& r, const std::string& label, std::function<void()> fn, bool on = false) {
    fill(r, on ? cButtonOn : cButton);
    std::string s = fit(label, r.w() - 8);
    text(r.l + (r.w() - textW(s)) / 2, r, s);
    click(r, std::move(fn));
}

void clearFocus() {
    if (focusId || capturing) XPLMTakeKeyboardFocus(0);
    focusId = 0;
    focusGet = nullptr;
    focusSet = nullptr;
    capturing = false;
    captureDone = nullptr;
}

void setFocus(int id, std::function<std::string()> get, std::function<void(const std::string&)> set) {
    focusId = id;
    focusGet = std::move(get);
    focusSet = std::move(set);
    capturing = false;
    caret = anchor = focusGet ? (int)focusGet().size() : 0;
    fieldScroll = 0;
    XPLMTakeKeyboardFocus(win);
}

// Character position under window x, for the focused field's text.
int caretAt(const std::string& v, int x) {
    int best = fieldScroll, bestD = 1 << 30;
    for (int i = fieldScroll; i <= (int)v.size(); ++i) {
        int d = std::abs(fieldTextX + textW(v.substr(fieldScroll, i - fieldScroll)) - x);
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void field(const Rect& r, int id, std::function<std::string()> get, std::function<void(const std::string&)> set,
           const std::string& placeholder = "", const float* col = kWhite, std::function<void()> onFocus = nullptr) {
    bool focused = focusId == id;
    fill(r, focused ? cFieldFocus : cField);
    std::string v = get();
    if (!focused) {
        if (v.empty()) text(r.l + 6, r, fit(placeholder, r.w() - 12), kDim);
        else text(r.l + 6, r, fit(v, r.w() - 12), col);
    } else {
        int n = (int)v.size(), avail = r.w() - 14;
        caret = std::max(0, std::min(caret, n));
        anchor = std::max(0, std::min(anchor, n));
        if (fieldScroll > caret) fieldScroll = caret;
        while (fieldScroll < caret && textW(v.substr(fieldScroll, caret - fieldScroll)) > avail) ++fieldScroll;
        int end = fieldScroll;
        while (end < n && textW(v.substr(fieldScroll, end + 1 - fieldScroll)) <= avail) ++end;
        fieldTextX = r.l + 6;
        int lo = std::max(std::min(caret, anchor), fieldScroll), hi = std::min(std::max(caret, anchor), end);
        if (lo < hi)
            fill({fieldTextX + textW(v.substr(fieldScroll, lo - fieldScroll)), r.t - 2,
                  fieldTextX + textW(v.substr(fieldScroll, hi - fieldScroll)), r.b + 2}, cSelected);
        text(fieldTextX, r, v.substr(fieldScroll, end - fieldScroll), col);
        int cx = fieldTextX + textW(v.substr(fieldScroll, caret - fieldScroll));
        fill({cx, r.t - 3, cx + 1, r.b + 3}, cButtonOn);
    }
    int tx = r.l + 6;
    click(r, [id, get, set, onFocus, tx] {
        if (onFocus) onFocus();
        setFocus(id, get, set);
        fieldTextX = tx;
        caret = anchor = caretAt(get(), clickX);
    });
}

// Lays out equally high cells left to right on one row.
struct RowCursor {
    int x, t;
    Rect take(int w) {
        Rect r{x, t, x + w, t - kRow};
        x += w + kGap;
        return r;
    }
};

// A list that opens under a button, for choices with more than a couple of values.
struct Drop {
    bool open = false;
    Rect anchor{0, 0, 0, 0};
    std::vector<std::string> items;
    int current = -1;
    bool anyTab = false;   // stays open on every tab (the profile picker); other lists only live on the Live lights tab
    std::function<void(int)> pick;
} drop;

void openDrop(const Rect& anchor, std::vector<std::string> items, int current, std::function<void(int)> pick) {
    drop.open = true;
    drop.anyTab = false;
    drop.anchor = anchor;
    drop.items = std::move(items);
    drop.current = current;
    drop.pick = std::move(pick);
}

// Drawn last so it sits on top and its rectangles are hit first; a press anywhere else just closes it.
void drawDrop(int L, int T, int R, int B) {
    if (!drop.open) return;
    click({L, T, R, B}, [] { drop.open = false; });
    int w = drop.anchor.w();
    for (const std::string& it : drop.items) w = std::max(w, textW(it) + 24);
    int h = (int)drop.items.size() * kRow;
    int top = drop.anchor.b - 1;
    if (top - h < B + kPad) top = std::min(T - kPad, drop.anchor.t + h);   // no room below: open upwards
    Rect box{drop.anchor.l, top, drop.anchor.l + w, top - h};
    fill({box.l - 1, box.t + 1, box.r + 1, box.b - 1}, kDim4);
    for (int i = 0; i < (int)drop.items.size(); ++i) {
        Rect row{box.l, box.t - i * kRow, box.r, box.t - (i + 1) * kRow};
        button(row, drop.items[i], [i] {
            auto pick = drop.pick;
            drop.open = false;
            if (pick) pick(i);
        }, i == drop.current);
    }
}

// ---------------------------------------------------------------- profile editing

const Binding& shownBinding() {
    static const Binding none;
    const Binding* b = gApp->profile.exact(selButton, selLayer);
    return b ? *b : none;
}

void mutate(const std::function<void(Binding&)>& fn) {
    Binding& b = gApp->profile.edit(selButton, selLayer);
    fn(b);
    gApp->profileChanged();
}

Action& actionOf(Binding& b, bool hold) { return hold ? b.hold : b.press; }

std::string summary(const Binding* b) {
    if (!b) return "";
    if (!b->label.empty()) return b->label;
    if (!b->press.empty()) {
        const Step& s = b->press[0];
        std::string t = s.kind == StepKind::Wait ? "wait" : s.text;
        if (s.kind == StepKind::Dataref) {
            size_t sl = t.rfind('/');
            if (sl != std::string::npos) t = t.substr(sl + 1);
        }
        size_t slash = t.rfind('/');
        if (s.kind == StepKind::Command && slash != std::string::npos) t = t.substr(slash + 1);
        return b->press.size() > 1 ? t + " +" + std::to_string(b->press.size() - 1) : t;
    }
    return b->holdLabel.empty() ? "(hold)" : b->holdLabel;
}

void openPicker(bool hold, int step) {
    pickerOpen = true;
    pickerHold = hold;
    pickerStep = step;
    pickerQuery.clear();
    pickerScroll = 0;
    setFocus(9000, [] { return pickerQuery; }, [](const std::string& s) {
        pickerQuery = s;
        pickerScroll = 0;
    });
}

void applyPickedCommand(const std::string& name) {
    bool hold = pickerHold;
    int step = pickerStep;
    mutate([=](Binding& b) {
        Action& a = actionOf(b, hold);
        if (step >= 0 && step < (int)a.size()) a[step].text = name;
        else a.push_back({StepKind::Command, name, 0});
    });
    pickerOpen = false;
    clearFocus();
}

// X-Plane virtual key -> our key name ("" if we cannot type that key).
std::string vkName(unsigned char vk) {
    if (vk >= XPLM_VK_0 && vk <= XPLM_VK_9) return std::string(1, (char)('0' + (vk - XPLM_VK_0)));
    if (vk >= XPLM_VK_A && vk <= XPLM_VK_Z) return std::string(1, (char)('a' + (vk - XPLM_VK_A)));
    if (vk >= XPLM_VK_F1 && vk <= XPLM_VK_F16) return "f" + std::to_string(vk - XPLM_VK_F1 + 1);
    if (vk >= XPLM_VK_NUMPAD0 && vk <= XPLM_VK_NUMPAD9) return "kp" + std::to_string(vk - XPLM_VK_NUMPAD0);
    switch (vk) {
        case XPLM_VK_SPACE: return "space";
        case XPLM_VK_RETURN:
        case XPLM_VK_ENTER: return "return";
        case XPLM_VK_TAB: return "tab";
        case XPLM_VK_BACK: return "backspace";
        case XPLM_VK_DELETE: return "delete";
        case XPLM_VK_LEFT: return "left";
        case XPLM_VK_RIGHT: return "right";
        case XPLM_VK_UP: return "up";
        case XPLM_VK_DOWN: return "down";
        case XPLM_VK_PRIOR: return "pageup";
        case XPLM_VK_NEXT: return "pagedown";
        case XPLM_VK_HOME: return "home";
        case XPLM_VK_END: return "end";
        case XPLM_VK_MULTIPLY: return "kp*";
        case XPLM_VK_ADD: return "kpplus";
        case XPLM_VK_SUBTRACT: return "kp-";
        case XPLM_VK_DECIMAL: return "kp.";
        case XPLM_VK_DIVIDE: return "kp/";
        case XPLM_VK_NUMPAD_ENT: return "kpenter";
        case XPLM_VK_EQUAL: return "=";
        case XPLM_VK_MINUS: return "-";
        case XPLM_VK_RBRACE: return "]";
        case XPLM_VK_LBRACE: return "[";
        case XPLM_VK_QUOTE: return "'";
        case XPLM_VK_SEMICOLON: return "semicolon";
        case XPLM_VK_BACKSLASH: return "\\";
        case XPLM_VK_COMMA: return ",";
        case XPLM_VK_SLASH: return "/";
        case XPLM_VK_PERIOD: return ".";
        case XPLM_VK_BACKQUOTE: return "`";
    }
    return "";
}

// ---------------------------------------------------------------- Buttons tab

void drawButtonList(const Rect& area) {
    App& a = *gApp;
    fill(area, cPanel);
    int rows = std::max(1, (area.t - area.b) / kRow);
    int maxScroll = std::max(0, kMaxButton - rows);
    listScroll = std::max(0, std::min(listScroll, maxScroll));
    scrolls.push_back({area, &listScroll, maxScroll});

    for (int i = 0; i < rows; ++i) {
        int button = listScroll + i + 1;
        if (button > kMaxButton) break;
        Rect row{area.l, area.t - i * kRow, area.r, area.t - (i + 1) * kRow};
        if (button == selButton) fill(row, cSelected);
        if (a.engine.isDown(button)) fill({row.l + 2, row.t - 6, row.l + 8, row.b + 6}, cPressed);

        bool reserved = a.profile.isReserved(button);
        char num[8];
        snprintf(num, sizeof num, "%2d", button);
        text(row.l + 14, row, num, kDim);
        text(row.l + 38, row, buttonName(button), reserved ? kDim : kWhite);

        std::string right;
        const float* col = kDim;
        if (reserved) {
            right = button == a.profile.shiftButton && a.profile.pinkieShift ? "shift"
                    : button == a.profile.clutchButton && a.profile.clutchSafe ? "safe mode"
                    : "mode switch";
        } else if (const Binding* b = a.profile.find(button, selLayer)) {
            right = summary(b);
            col = b->layer == selLayer ? kGreen : kDim;   // dim = inherited from "Any mode"
        }
        int nameEnd = row.l + 38 + textW(buttonName(button)) + 10;
        right = fit(right, row.r - 8 - nameEnd);
        text(row.r - 8 - textW(right), row, right, col);

        click(row, [button] {
            selButton = button;
            pickerOpen = false;
        });
    }
}

void drawSteps(RowCursor& rc, const Rect& panel, bool hold) {
    const Binding& b = shownBinding();
    const Action& act = hold ? b.hold : b.press;

    Rect head{panel.l, rc.t, panel.r, rc.t - kRow};
    char title[96];
    if (hold) snprintf(title, sizeof title, "When held longer than %.1f s (optional)", gApp->profile.holdMs / 1000.0);
    else snprintf(title, sizeof title, "When pressed");
    text(panel.l, head, title, kAmber);
    rc.t -= kRow + 2;

    rc.x = panel.l;
    Rect lab = rc.take(110);
    text(lab.l, lab, "Name on screen:", kDim);
    Rect f = rc.take(std::min(260, panel.r - rc.x));
    field(
        f, 100 + (hold ? 1 : 0), [hold] { return hold ? shownBinding().holdLabel : shownBinding().label; },
        [hold](const std::string& s) {
            mutate([&](Binding& bb) { (hold ? bb.holdLabel : bb.label) = s.substr(0, kMfdCols); });
        },
        "up to 16 characters");
    rc.t -= kRow + kGap;

    for (int i = 0; i < (int)act.size(); ++i) {
        const Step& s = act[i];
        rc.x = panel.l;
        Rect tag = rc.take(46);
        fill(tag, cTag);
        const char* tname = s.kind == StepKind::Command ? "CMD" : s.kind == StepKind::Key ? "KEY"
                          : s.kind == StepKind::Dataref ? "DATA" : "WAIT";
        text(tag.l + (tag.w() - textW(tname)) / 2, tag, tname, kDim);

        int rightButtons = 2 * (26 + kGap);
        int contentW = panel.r - rc.x - rightButtons;
        if (s.kind == StepKind::Command) {
            Rect c = rc.take(contentW);
            fill(c, cField);
            text(c.l + 6, c, fit(s.text, c.w() - 12));
            click(c, [hold, i] { openPicker(hold, i); });
        } else if (s.kind == StepKind::Dataref) {
            Rect c = rc.take(std::max(80, contentW - 78 - 70 - 2 * kGap));
            bool known = datarefReport(s.text).compare(0, 9, "Not found") != 0;
            field(
                c, 2000 + (hold ? 500 : 0) + i,
                [hold, i] {
                    const Action& aa = hold ? shownBinding().hold : shownBinding().press;
                    return i < (int)aa.size() ? aa[i].text : std::string();
                },
                [hold, i](const std::string& v) {
                    std::string t;
                    for (char ch : v) if (!isspace((unsigned char)ch)) t += ch;
                    mutate([&](Binding& bb) {
                        Action& aa = actionOf(bb, hold);
                        if (i < (int)aa.size()) aa[i].text = t;
                    });
                },
                "dataref, e.g. sim/cockpit/..., or name[3]", known ? kWhite : kAmber);
            const char* opNames[3] = {"Set", "Toggle", "Add"};
            button(rc.take(78), opNames[(int)s.op], [hold, i] {
                mutate([&](Binding& bb) {
                    Action& aa = actionOf(bb, hold);
                    if (i < (int)aa.size()) aa[i].op = (DatarefOp)(((int)aa[i].op + 1) % 3);
                });
            });
            Rect v = rc.take(70);
            if (s.op == DatarefOp::Toggle) {
                char buf[48];
                snprintf(buf, sizeof buf, "%g / %g", s.value, s.value2);
                text(v.l + 4, v, fit(buf, v.w() - 8), kDim);
            } else {
                field(
                    v, 3000 + (hold ? 500 : 0) + i,
                    [hold, i] {
                        const Action& aa = hold ? shownBinding().hold : shownBinding().press;
                        if (i >= (int)aa.size()) return std::string();
                        char buf[48];
                        snprintf(buf, sizeof buf, "%g", aa[i].value);
                        return std::string(buf);
                    },
                    [hold, i](const std::string& txt) {
                        char* end = nullptr;
                        double val = strtod(txt.c_str(), &end);
                        if (txt.empty() || *end) return;   // not a number yet: keep the old value
                        mutate([&](Binding& bb) {
                            Action& aa = actionOf(bb, hold);
                            if (i < (int)aa.size()) aa[i].value = val;
                        });
                    },
                    s.op == DatarefOp::Add ? "amount" : "value");
            }
        } else if (s.kind == StepKind::Key) {
            Rect c = rc.take(std::max(80, contentW - 80 - kGap));
            KeyCombo k;
            bool valid = parseKey(s.text, k);
            field(
                c, 1000 + (hold ? 500 : 0) + i,
                [hold, i] {
                    const Action& aa = hold ? shownBinding().hold : shownBinding().press;
                    return i < (int)aa.size() ? aa[i].text : std::string();
                },
                [hold, i](const std::string& v) {
                    mutate([&](Binding& bb) {
                        Action& aa = actionOf(bb, hold);
                        if (i < (int)aa.size()) aa[i].text = v;
                    });
                },
                "e.g. shift+f1", valid ? kWhite : kRed);
            bool recordingThis = capturing && focusId == -(1000 + (hold ? 500 : 0) + i);
            button(rc.take(80), recordingThis ? "Press a key" : "Record", [hold, i] {
                focusId = -(1000 + (hold ? 500 : 0) + i);
                capturing = true;
                captureDone = [hold, i](const std::string& combo) {
                    mutate([&](Binding& bb) {
                        Action& aa = actionOf(bb, hold);
                        if (i < (int)aa.size()) aa[i].text = combo;
                    });
                };
                XPLMTakeKeyboardFocus(win);
            }, recordingThis);
        } else {
            auto bump = [hold, i](int delta) {
                mutate([&](Binding& bb) {
                    Action& aa = actionOf(bb, hold);
                    if (i < (int)aa.size()) aa[i].ms = std::max(50, std::min(10000, aa[i].ms + delta));
                });
            };
            button(rc.take(26), "-", [bump] { bump(-50); });
            Rect v = rc.take(70);
            std::string ms = std::to_string(s.ms) + " ms";
            text(v.l + (v.w() - textW(ms)) / 2, v, ms);
            button(rc.take(26), "+", [bump] { bump(50); });
        }

        rc.x = panel.r - rightButtons + kGap;
        button(rc.take(26), "^", [hold, i] {
            if (i > 0)
                mutate([&](Binding& bb) {
                    Action& aa = actionOf(bb, hold);
                    if (i < (int)aa.size()) std::swap(aa[i], aa[i - 1]);
                });
        });
        button(rc.take(26), "x", [hold, i] {
            clearFocus();
            mutate([&](Binding& bb) {
                Action& aa = actionOf(bb, hold);
                if (i < (int)aa.size()) aa.erase(aa.begin() + i);
            });
        });
        rc.t -= kRow + kGap;
    }

    rc.x = panel.l;
    button(rc.take(100), "+ Command", [hold] { openPicker(hold, -1); });
    button(rc.take(100), "+ Keystroke", [hold] {
        mutate([&](Binding& bb) { actionOf(bb, hold).push_back({StepKind::Key, "", 0}); });
    });
    button(rc.take(100), "+ Dataref", [hold] {
        mutate([&](Binding& bb) {
            Step st;
            st.kind = StepKind::Dataref;
            actionOf(bb, hold).push_back(st);
        });
    });
    button(rc.take(100), "+ Wait", [hold] {
        mutate([&](Binding& bb) { actionOf(bb, hold).push_back({StepKind::Wait, "", 200}); });
    });
    rc.t -= kRow + kPad;
}

void drawPicker(const Rect& panel) {
    App& a = *gApp;
    RowCursor rc{panel.l, panel.t};
    Rect title = rc.take(panel.w() - 90 - kGap);
    text(title.l, title, std::string("Choose a command for ") + buttonName(selButton), kAmber);
    button(rc.take(90), "Cancel", [] {
        pickerOpen = false;
        clearFocus();
    });
    rc.t -= kRow + kGap;

    rc.x = panel.l;
    field(
        rc.take(panel.w()), 9000, [] { return pickerQuery; },
        [](const std::string& s) {
            pickerQuery = s;
            pickerScroll = 0;
        },
        "Type to search, e.g. gear, flaps, autopilot heading");
    rc.t -= kRow + kGap;

    if (pickerQuery != pickerResultsFor) {
        pickerResultsFor = pickerQuery;
        pickerResults.clear();
        std::vector<std::string> words;
        std::stringstream ss(pickerQuery);
        std::string w;
        while (ss >> w) {
            for (char& c : w) c = (char)tolower((unsigned char)c);
            words.push_back(w);
        }
        for (int i = 0; i < (int)a.commands.size(); ++i) {
            bool all = true;
            for (const std::string& word : words)
                if (a.commands[i].haystack.find(word) == std::string::npos) {
                    all = false;
                    break;
                }
            if (all) pickerResults.push_back(i);
        }
    }

    // Commands from add-on aircraft are not in X-Plane's list; let them be typed in full.
    if (pickerQuery.find('/') != std::string::npos && pickerQuery.find(' ') == std::string::npos) {
        rc.x = panel.l;
        std::string q = pickerQuery;
        button(rc.take(panel.w()), "Use exactly:  " + q, [q] { applyPickedCommand(q); });
        rc.t -= kRow + kGap;
    }

    Rect list{panel.l, rc.t, panel.r, panel.b};
    fill(list, cPanel);
    int rows = std::max(1, (list.t - list.b) / kRow);
    int maxScroll = std::max(0, (int)pickerResults.size() - rows);
    pickerScroll = std::max(0, std::min(pickerScroll, maxScroll));
    scrolls.push_back({list, &pickerScroll, maxScroll});
    if (pickerResults.empty()) {
        Rect row{list.l, list.t, list.r, list.t - kRow};
        text(row.l + 8, row, a.commands.empty() ? "X-Plane's command list could not be read; type the full name."
                                                : "Nothing matches.", kDim);
    }
    for (int i = 0; i < rows && pickerScroll + i < (int)pickerResults.size(); ++i) {
        const CommandInfo& c = a.commands[pickerResults[pickerScroll + i]];
        Rect row{list.l, list.t - i * kRow, list.r, list.t - (i + 1) * kRow};
        int half = list.w() / 2;
        text(row.l + 8, row, fit(c.name, half - 16));
        text(row.l + half, row, fit(c.description, half - 8), kDim);
        std::string name = c.name;
        click(row, [name] { applyPickedCommand(name); });
    }
}

void drawEditor(const Rect& panel) {
    App& a = *gApp;
    RowCursor rc{panel.l, panel.t};
    Rect title{panel.l, rc.t, panel.r, rc.t - kRow};
    text(title.l, title, std::to_string(selButton) + "  " + buttonName(selButton) + "   -   " + layerName(selLayer));
    rc.t -= kRow + kGap;

    if (a.profile.isReserved(selButton)) {
        const char* why = selButton == a.profile.shiftButton && a.profile.pinkieShift
                              ? "The pinkie switch is the Shift key: hold it to reach Mode 1-3 + Pinkie."
                          : selButton == a.profile.clutchButton && a.profile.clutchSafe
                              ? "The clutch is the safe mode: while held, buttons only show their name."
                              : "This is a position of the mode dial. It selects the mode.";
        Rect r{panel.l, rc.t, panel.r, rc.t - kRow};
        text(r.l, r, why, kDim);
        Rect r2{panel.l, rc.t - kRow, panel.r, rc.t - 2 * kRow};
        if (!(selButton >= a.profile.modeButtons[0] && selButton <= a.profile.modeButtons[2]))
            text(r2.l, r2, "You can turn that off in the Settings tab to program it like any button.", kDim);
        return;
    }

    const Binding* exact = a.profile.exact(selButton, selLayer);
    const Binding* any = a.profile.exact(selButton, kLayerAny);
    if (selLayer != kLayerAny && (!exact || exact->empty()) && any && !any->empty()) {
        Rect r{panel.l, rc.t, panel.r, rc.t - kRow};
        text(r.l, r, fit("Nothing set for this mode yet, so 'Any mode' applies: " + summary(any), panel.w()), kDim);
        rc.t -= kRow + kGap;
    }

    drawSteps(rc, panel, false);
    drawSteps(rc, panel, true);

    rc.x = panel.l;
    if (exact && !exact->empty())
        button(rc.take(240), std::string("Clear this button in ") + layerName(selLayer), [] {
            clearFocus();
            mutate([](Binding& bb) {
                bb.press.clear();
                bb.hold.clear();
                bb.label.clear();
                bb.holdLabel.clear();
            });
        });
}

void drawButtonsTab(const Rect& area) {
    RowCursor rc{area.l, area.t};
    Rect lab = rc.take(104);
    text(lab.l, lab, "Programming for:", kDim);
    static const char* const shortNames[kLayerCount] = {"Any mode", "Mode 1", "Mode 2", "Mode 3",
                                                        "1 + Pinkie", "2 + Pinkie", "3 + Pinkie"};
    for (int l = 0; l < kLayerCount; ++l) {
        if (l >= 4 && !gApp->profile.pinkieShift) break;
        button(rc.take(82), shortNames[l], [l] {
            selLayer = l;
            pickerOpen = false;
        }, selLayer == l);
    }
    if (selLayer >= 4 && !gApp->profile.pinkieShift) selLayer = kLayerAny;
    button(rc.take(128), follow ? "[x] Follow the stick" : "[  ] Follow the stick", [] { follow = !follow; }, follow);

    int top = area.t - kRow - kPad;
    Rect list{area.l, top, area.l + 300, area.b};
    drawButtonList(list);
    Rect panel{list.r + 2 * kPad, top, area.r, area.b};
    if (pickerOpen) drawPicker(panel);
    else drawEditor(panel);
}

// ---------------------------------------------------------------- Lights tab

void drawLightsTab(const Rect& area) {
    App& a = *gApp;
    RowCursor rc{area.l, area.t};
    Rect lab = rc.take(104);
    text(lab.l, lab, "Lights in:", kDim);
    for (int m = 0; m < 3; ++m)
        button(rc.take(82), modeLabel(m + 1, lightsPinkie), [m] { lightsMode = m; }, lightsMode == m);
    button(rc.take(150), lightsPinkie ? "Pinkie held: on" : "Pinkie held: off", [] { lightsPinkie = !lightsPinkie; }, lightsPinkie);
    rc.t -= kRow + kGap;
    rc.x = area.l;
    Rect now = rc.take(380);
    text(now.l, now, "On the stick now: " + modeLabel(a.engine.mode(), a.engine.shift()), kDim);
    button(rc.take(260), "Copy these colours to the other modes", [] {
        Profile& p = gApp->profile;
        for (int m = 1; m <= 3; ++m)
            for (int l = 0; l < kLedCount; ++l) p.ledAt(m, lightsPinkie, l) = p.ledAt(lightsMode + 1, lightsPinkie, l);
        gApp->profileChanged();
    });
    rc.t -= kRow + kPad;

    for (int l = 0; l < kLedCount; ++l) {
        rc.x = area.l;
        Rect name = rc.take(150);
        text(name.l, name, ledTitle(l));
        Color cur = a.profile.ledAt(lightsMode + 1, lightsPinkie, l);
        auto set = [l](Color c) {
            gApp->profile.ledAt(lightsMode + 1, lightsPinkie, l) = c;
            gApp->profileChanged();
        };
        button(rc.take(70), "Off", [set] { set(Color::Off); }, cur == Color::Off);
        if (ledIsSingleColor(l)) {
            button(rc.take(70), "On", [set] { set(Color::Green); }, cur != Color::Off);
        } else {
            button(rc.take(70), "Green", [set] { set(Color::Green); }, cur == Color::Green);
            button(rc.take(70), "Amber", [set] { set(Color::Amber); }, cur == Color::Amber);
            button(rc.take(70), "Red", [set] { set(Color::Red); }, cur == Color::Red);
        }
        rc.t -= kRow + 4;
    }
    rc.t -= kPad;

    struct Bright {
        const char* name;
        int Profile::*member;
    };
    for (const Bright& br : {Bright{"Light brightness", &Profile::ledBrightness},
                             Bright{"Screen brightness", &Profile::mfdBrightness}}) {
        rc.x = area.l;
        Rect name = rc.take(150);
        text(name.l, name, br.name);
        int Profile::*mem = br.member;
        auto bump = [mem](int d) {
            int& v = gApp->profile.*mem;
            v = std::max(0, std::min(127, v + d));
            gApp->profileChanged();
        };
        button(rc.take(26), "-", [bump] { bump(-16); });
        Rect v = rc.take(60);
        std::string pct = std::to_string((a.profile.*mem) * 100 / 127) + " %";
        text(v.l + (v.w() - textW(pct)) / 2, v, pct);
        button(rc.take(26), "+", [bump] { bump(16); });
        rc.t -= kRow + 4;
    }

    if (a.device.connected() && !a.device.isPro()) {
        Rect r{area.l, rc.t - kPad, area.r, rc.t - kPad - kRow};
        text(r.l, r, "This stick is a standard X52: button colours need the X52 Pro.", kAmber);
    }
}

// ---------------------------------------------------------------- Live lights tab

std::string numberText(double v) {
    std::ostringstream o;
    o << v;
    return o.str();
}

std::string ruleSummary(const LightRule& r) {
    if (!r.active()) return "(no dataref yet)";
    return r.dataref + " " + compareName(r.op) + " " + numberText(r.value);
}

constexpr int kRuleDetailLines = 10;   // lines a rule takes when opened, below its header line
std::set<int> liveOpen, liveChecked;  // rules shown opened / ticked for "Remove selected"
int liveScroll = 0;

void liveReset() {   // the numbers of the rules changed
    liveOpen.clear();
    liveChecked.clear();
    liveValueText.clear();
    clearFocus();
}

void drawLiveTab(const Rect& area) {
    App& a = *gApp;
    RowCursor rc{area.l, area.t};
    Rect head{area.l, rc.t, area.r, rc.t - kRow};
    text(head.l, head, "Live lights follow the simulator and replace the mode colour. One rule per light; alarms (set in the X52 Pro app) override them.", kDim);
    rc.t -= kRow + kGap;
    button(rc.take(120), "Add rule", [] {
        int free = firstFreeLed(gApp->profile.rules);
        if (free < 0) {
            gApp->notice = "Every light already has a rule. A light follows only one rule: remove one first.";
            return;
        }
        LightRule fresh;
        fresh.led = free;
        gApp->profile.rules.push_back(fresh);
        liveOpen.insert((int)gApp->profile.rules.size() - 1);
        gApp->profileChanged();
    });
    button(rc.take(170), "Remove selected", [] {
        std::vector<LightRule> keep;
        for (size_t k = 0; k < gApp->profile.rules.size(); ++k)
            if (!liveChecked.count((int)k)) keep.push_back(gApp->profile.rules[k]);
        gApp->profile.rules = keep;
        liveReset();
        gApp->profileChanged();
    });
    bool allTicked = !a.profile.rules.empty() && liveChecked.size() == a.profile.rules.size();
    button(rc.take(120), allTicked ? "Select none" : "Select all", [allTicked] {
        liveChecked.clear();
        if (!allTicked)
            for (size_t k = 0; k < gApp->profile.rules.size(); ++k) liveChecked.insert((int)k);
    });
    rc.t -= kRow + kGap;

    const int lineH = kRow + 4;
    Rect list{area.l, rc.t, area.r, area.b};
    int cap = std::max(1, (list.t - list.b) / lineH);
    int total = 0;
    for (size_t k = 0; k < a.profile.rules.size(); ++k) total += 1 + (liveOpen.count((int)k) ? kRuleDetailLines : 0);
    int maxScroll = std::max(0, total - cap);
    liveScroll = std::max(0, std::min(liveScroll, maxScroll));
    scrolls.push_back({list, &liveScroll, maxScroll});
    if (a.profile.rules.empty()) {
        Rect r{list.l, list.t, list.r, list.t - kRow};
        text(r.l + 6, r, "No live lights yet. Press Add rule.", kDim);
        return;
    }

    int line = 0;   // index of the line being laid out, counted from the top of the whole list
    auto rowAt = [&](int ln) -> bool {   // is this line on screen? (then rc.x/rc.t say where)
        if (ln < liveScroll || ln >= liveScroll + cap) return false;
        rc.t = list.t - (ln - liveScroll) * lineH;
        rc.x = list.l;
        return true;
    };
    for (size_t k = 0; k < a.profile.rules.size(); ++k) {
        const int idx = (int)k;
        const LightRule r = a.profile.rules[k];
        bool open = liveOpen.count(idx) != 0;
        if (rowAt(line)) {
            Rect whole{list.l, rc.t, list.r, rc.t - kRow};
            fill(whole, open ? cSelected : cPanel);
            click(whole, [idx] {
                if (liveOpen.count(idx)) liveOpen.erase(idx);
                else liveOpen.insert(idx);
                liveValueText.clear();
                clearFocus();
            });
            Rect tick{whole.l + 4, whole.t, whole.l + 40, whole.b};
            bool on = liveChecked.count(idx) != 0;
            text(tick.l, tick, on ? "[x]" : "[  ]");
            click(tick, [idx, on] {
                if (on) liveChecked.erase(idx);
                else liveChecked.insert(idx);
            });
            text(whole.l + 48, whole, open ? "v" : ">");
            text(whole.l + 68, whole, ledTitle(r.led));
            text(whole.l + 200, whole, fit(ruleSummary(r), whole.r - whole.l - 210), r.active() ? kWhite : kDim);
        }
        ++line;
        if (!open) continue;

        auto edit = [idx](const std::function<void(LightRule&)>& fn) {
            if (idx < (int)gApp->profile.rules.size()) fn(gApp->profile.rules[idx]);
            gApp->profileChanged();
        };
        const int x0 = list.l + 40;
        if (rowAt(line)) {
            RowCursor er{x0, rc.t};
            Rect lab = er.take(120);
            text(lab.l, lab, "Light", kDim);
            Rect lb = er.take(180);
            int curLed = r.led;
            button(lb, ledTitle(r.led), [lb, edit, curLed, idx] {
                std::vector<std::string> names;
                for (int l = 0; l < kLedCount; ++l) names.push_back(ledTitle(l));
                openDrop(lb, names, curLed, [edit, idx](int i) {
                    if (ruleUsingLed(gApp->profile.rules, i, idx) >= 0) {
                        gApp->notice = std::string(ledTitle(i)) + " already follows another rule, and a light follows only one. Pick a free light.";
                        return;
                    }
                    edit([i](LightRule& x) { x.led = i; });
                });
            });
        }
        ++line;
        for (int pr = 0; pr < rulePresetCount(); pr += 2) {
            if (rowAt(line)) {
                RowCursor er{x0, rc.t};
                if (pr == 0) {
                    Rect lab = er.take(120);
                    text(lab.l, lab, "Start from", kDim);
                } else {
                    er.take(120);
                }
                for (int c = 0; c < 2 && pr + c < rulePresetCount(); ++c) {
                    int pi = pr + c;
                    button(er.take(300), rulePreset(pi).name, [pi, edit] {
                        edit([pi](LightRule& x) {
                            int led = x.led;
                            x = rulePreset(pi).rule;
                            x.led = led;
                        });
                        liveValueText.clear();
                    });
                }
            }
            ++line;
        }
        if (rowAt(line)) {
            RowCursor er{x0, rc.t};
            Rect lab = er.take(120);
            text(lab.l, lab, "Dataref", kDim);
            field(er.take(560), 3000 + idx, [idx] { return gApp->profile.rules[idx].dataref; },
                  [edit](const std::string& v) {
                      std::string clean;   // a dataref never contains blanks
                      for (char c : v)
                          if (!isspace((unsigned char)c)) clean += c;
                      edit([&clean](LightRule& x) { x.dataref = clean; });
                  },
                  "sim/cockpit2/...  or  name[3] for one item of a list");
        }
        ++line;
        if (rowAt(line)) {   // what X-Plane makes of the name, so a typo or a wrong kind of dataref is visible at once
            RowCursor er{x0, rc.t};
            er.take(120);
            Rect st{er.x, rc.t, list.r, rc.t - kRow};
            std::string rep = datarefReport(r.dataref);
            text(st.l, st, fit(rep, st.w()), rep.compare(0, 6, "Found.") == 0 ? kGreen : kAmber);
        }
        ++line;
        if (rowAt(line)) {
            RowCursor er{x0, rc.t};
            Rect lab = er.take(120);
            text(lab.l, lab, "When value is", kDim);
            Rect ob = er.take(54);
            int curOp = (int)r.op;
            button(ob, compareName(r.op), [ob, edit, curOp] {
                std::vector<std::string> names;
                for (int c = 0; c < kCompareCount; ++c) names.push_back(compareName((Compare)c));
                openDrop(ob, names, curOp, [edit](int i) { edit([i](LightRule& x) { x.op = (Compare)i; }); });
            });
            field(er.take(110), 4000 + idx,
                  [idx, r] { return focusId == 4000 + idx ? liveValueText : numberText(r.value); },
                  [edit](const std::string& v) {
                      liveValueText = v;
                      char* end = nullptr;
                      double d = strtod(v.c_str(), &end);
                      if (!v.empty() && !*end) edit([d](LightRule& x) { x.value = d; });
                  }, "number", kWhite, [r] { liveValueText = numberText(r.value); });
        }
        ++line;
        if (rowAt(line)) {
            RowCursor er{x0, rc.t};
            for (int which = 0; which < 2; ++which) {
                Rect w = er.take(which == 0 ? 120 : 150);
                text(w.l, w, which == 0 ? "Light shows" : "if not true", kDim);
                int cur = which == 0 ? r.thenColor : r.elseColor;
                std::string name = cur < 0 ? "mode colour" : ruleColorName(cur);
                Rect cb = er.take(110);
                button(cb, name, [cb, edit, which, cur] {
                    std::vector<std::string> names{"mode colour"};
                    for (int c = 0; c <= 3; ++c) names.push_back(ruleColorName(c));
                    openDrop(cb, names, cur < 0 ? 0 : cur + 1, [edit, which](int i) {
                        edit([which, i](LightRule& x) { (which == 0 ? x.thenColor : x.elseColor) = i == 0 ? kRuleMode : i - 1; });
                    });
                });
            }
        }
        ++line;
        // spare lines keep every opened rule exactly kRuleDetailLines tall
        line += kRuleDetailLines - (1 + (rulePresetCount() + 1) / 2 + 1 + 1 + 1 + 1);
    }
}

// ---------------------------------------------------------------- Settings tab

void drawSettingsTab(const Rect& area) {
    App& a = *gApp;
    RowCursor rc{area.l, area.t};
    auto check = [&](bool value, const std::string& label, std::function<void()> toggle) {
        rc.x = area.l;
        button(rc.take(36), value ? "[x]" : "[  ]", toggle, value);
        Rect r = rc.take(area.r - rc.x);
        text(r.l, r, label);
        rc.t -= kRow + kGap;
    };
    check(a.profile.pinkieShift, "Pinkie switch works as Shift (adds Mode 1-3 + Pinkie)", [] {
        gApp->profile.pinkieShift = !gApp->profile.pinkieShift;
        gApp->profileChanged();
    });
    check(a.profile.clutchSafe, "Clutch (i) is a safe mode: while held, buttons only show their name on the screen", [] {
        gApp->profile.clutchSafe = !gApp->profile.clutchSafe;
        gApp->profileChanged();
    });
    check(a.profile.clutchLatched, "Latched clutch: one press turns safe mode on, the next turns it off", [] {
        gApp->profile.clutchLatched = !gApp->profile.clutchLatched;
        gApp->profileChanged();
    });
    check(a.profile.com833, "COM fine tuning from the stick steps 8.33 kHz channels (off: 25 kHz)", [] {
        gApp->profile.com833 = !gApp->profile.com833;
        gApp->profileChanged();
    });

    check(a.testMode, "Test mode: the stick's screen names whatever you move or press (buttons do nothing meanwhile)", [] {
        gApp->setTestMode(!gApp->testMode);
    });

    rc.x = area.l;
    Rect lab = rc.take(150);
    text(lab.l, lab, "Hold time");
    auto bumpHold = [](int d) {
        int& v = gApp->profile.holdMs;
        v = std::max(100, std::min(5000, v + d));
        gApp->profileChanged();
    };
    button(rc.take(26), "-", [bumpHold] { bumpHold(-50); });
    Rect v = rc.take(70);
    std::string ms = std::to_string(a.profile.holdMs) + " ms";
    text(v.l + (v.w() - textW(ms)) / 2, v, ms);
    button(rc.take(26), "+", [bumpHold] { bumpHold(50); });
    rc.t -= kRow + kGap;

    rc.x = area.l;
    lab = rc.take(150);
    text(lab.l, lab, "Page on the stick's screen");
    button(rc.take(26), "<", [] { gApp->stepPage(-1); });
    v = rc.take(130);
    std::string pageLabel = fit(a.pageTitle(a.page), v.w() - 8);
    text(v.l + (v.w() - textW(pageLabel)) / 2, v, pageLabel);
    button(rc.take(26), ">", [] { gApp->stepPage(1); });
    rc.t -= kRow + kGap;

    rc.x = area.l;
    lab = rc.take(150);
    text(lab.l, lab, "Typing keyboard keys");
    bool allowed = keysAllowed();
    Rect st = rc.take(210);
    text(st.l, st, allowed ? "allowed by macOS" : "not allowed by macOS yet", allowed ? kGreen : kAmber);
    if (!allowed) button(rc.take(150), "Ask macOS to allow", [] { requestKeyAccess(); });
    rc.t -= kRow + kGap;

    std::string held;
    for (int b = 1; b <= kMaxButton; ++b)
        if (a.engine.isDown(b)) held += (held.empty() ? "" : ",  ") + std::to_string(b) + " " + buttonName(b);
    Rect r{area.l, rc.t, area.r, rc.t - kRow};
    text(r.l, r, "Held on the stick now:", kDim);
    text(r.l + 150 + kGap, r, fit(held.empty() ? "-" : held, area.r - r.l - 160));
    rc.t -= kRow + kGap;

    // Mirror of the stick's own screen.
    r = {area.l, rc.t, area.r, rc.t - kRow};
    text(r.l, r, "On the stick's screen:", kDim);
    Rect mirror{area.l + 150 + kGap, rc.t, area.l + 150 + kGap + 190, rc.t - kMfdLines * (kRow - 4) - 8};
    fill(mirror, cField);
    for (int i = 0; i < kMfdLines; ++i) {
        Rect line{mirror.l, mirror.t - 4 - i * (kRow - 4), mirror.r, mirror.t - 4 - (i + 1) * (kRow - 4)};
        text(line.l + 8, line, a.screen.l[i], a.device.connected() ? kGreen : kDim);
    }
    rc.t = mirror.b - kGap;

    r = {area.l, rc.t, area.r, rc.t - kRow};
    text(r.l, r, "Profile file:", kDim);
    std::string path = a.store.pathFor(a.profileKey());
    while (path.size() > 8 && textW("..." + path) > area.r - r.l - 160) path.erase(0, 1);
    text(r.l + 150 + kGap, r, path == a.store.pathFor(a.profileKey()) ? path : "..." + path);
    rc.t -= kRow + kPad;

    const char* notes[] = {
        "Buttons you program here should be set to 'Do nothing' in X-Plane's own Joystick settings,",
        "otherwise both X-Plane and this plugin will act on them. Axes and the main POV hat stay with X-Plane.",
    };
    for (const char* n : notes) {
        r = {area.l, rc.t, area.r, rc.t - kRow};
        text(r.l, r, n, kDim);
        rc.t -= kRow - 4;
    }
}

// ---------------------------------------------------------------- window callbacks

void draw(XPLMWindowID w, void*) {
    App& a = *gApp;
    hits.clear();
    scrolls.clear();
    int L, T, R, B;
    XPLMGetWindowGeometry(w, &L, &T, &R, &B);
    XPLMGetFontDimensions(xplmFont_Proportional, nullptr, &fontH, nullptr);

    if (a.pressSerial != seenSerial) {
        seenSerial = a.pressSerial;
        if (follow && !pickerOpen && !focusId && !capturing && !a.profile.isReserved(a.lastPressed)) {
            selButton = a.lastPressed;
            int rows = std::max(1, (T - B - 5 * kRow - 6 * kPad) / kRow);
            if (selButton - 1 < listScroll) listScroll = selButton - 1;
            if (selButton - 1 >= listScroll + rows) listScroll = selButton - rows;
        }
    }

    Rect area{L + kPad, T - kPad, R - kPad, B + kPad};

    // status line
    Rect status{area.l, area.t, area.r, area.t - kRow};
    std::string stick = a.input.connected()
                            ? std::string("Stick connected") + (a.device.connected() ? "" : " (screen and lights not reachable)")
                            : "Stick not found - plug in the X52";
    text(status.l, status, stick, a.input.connected() && a.device.connected() ? kGreen : kAmber);
    std::string mode = "Mode " + std::to_string(a.engine.mode()) + (a.engine.shift() ? " + Pinkie" : "") +
                       (a.engine.clutch() ? "   SAFE" : "");
    text(status.l + 330, status, mode);

    RowCursor pr{0, area.t};
    if (!a.manualKey.empty()) {
        confirmDrop = false;   // a profile picked here or on the stick is in use: the aircraft's own is left alone
        std::string p = fit("Profile: " + App::profileTitle(a.manualKey) + " (picked)", 400);
        text(area.r - textW(p), status, p);
    } else if (a.usingAircraftProfile) {
        std::string lab = confirmDrop ? "Really delete it?" : "Delete, use Default";
        pr.x = area.r - 170;
        button(pr.take(170), lab, [] {
            if (confirmDrop) {
                gApp->dropAircraftProfile();
                gApp->notice = "Profile deleted, Default is in use. Press Undo delete to bring it back.";
            }
            confirmDrop = !confirmDrop;
        }, confirmDrop);
        std::string p = fit("Profile: " + a.profile.name, 260);
        text(area.r - 170 - kGap - textW(p), status, p);
    } else {
        confirmDrop = false;
        std::string lab = a.aircraft.empty() ? "" : fit("Make a profile for " + a.aircraft, 260);
        int bw = lab.empty() ? 0 : std::min(270, textW(lab) + 20);
        if (bw) {
            pr.x = area.r - bw;
            button(pr.take(bw), lab, [] { gApp->makeAircraftProfile(); });
        }
        int ub = 0;   // a deleted profile of this aircraft can come back
        if (a.canUndoDrop()) {
            ub = 110;
            RowCursor ur{area.r - bw - (bw ? kGap : 0) - ub, area.t};
            button(ur.take(ub), "Undo delete", [] {
                gApp->undoDropAircraftProfile();
                gApp->notice.clear();
            }, true);
            ub += kGap;
        }
        std::string p = "Profile: Default (all aircraft)";
        text(area.r - bw - (bw ? kGap : 0) - ub - kGap - textW(p), status, p);
    }

    // profile picker, at the right end of the tab row
    {
        Rect pick{area.r - 190, area.t - kRow - kGap, area.r, area.t - 2 * kRow - kGap};
        button(pick, "Change profile...", [pick] {
            std::vector<std::string> keys = gApp->store.list();
            std::vector<std::string> items = {"Automatic (aircraft, else Default)"};
            int current = gApp->manualKey.empty() ? 0 : -1;
            for (size_t i = 0; i < keys.size(); ++i) {
                items.push_back(keys[i] == "default" ? "Default (all aircraft)" : App::profileTitle(keys[i]));
                if (keys[i] == gApp->manualKey) current = (int)i + 1;
            }
            openDrop(pick, items, current, [keys](int i) { gApp->chooseProfile(i == 0 ? "" : keys[i - 1]); });
            drop.anyTab = true;
        });
    }

    // tabs
    RowCursor tc{area.l, area.t - kRow - kGap};
    static const char* const tabNames[] = {"Buttons", "Lights", "Live lights", "Settings"};
    for (int t = 0; t < 4; ++t)
        button(tc.take(110), tabNames[t], [t] {
            tab = t;
            pickerOpen = false;
            clearFocus();
        }, tab == t);

    // notice line at the bottom
    int bottom = area.b;
    if (!a.notice.empty()) {
        Rect n{area.l, area.b + kRow, area.r - 70, area.b};
        text(n.l, n, fit(a.notice, n.w()), kAmber);
        button({area.r - 64, area.b + kRow, area.r, area.b}, "OK", [] { gApp->notice.clear(); });
        bottom += kRow + kGap;
    }

    Rect content{area.l, area.t - 2 * kRow - kGap - kPad, area.r, bottom};
    if (tab == TabButtons) drawButtonsTab(content);
    else if (tab == TabLights) drawLightsTab(content);
    else if (tab == TabLive) drawLiveTab(content);
    else drawSettingsTab(content);
    if (tab != TabLive && !drop.anyTab) drop.open = false;
    drawDrop(L, T, R, B);
}

int mouse(XPLMWindowID, int x, int y, XPLMMouseStatus status, void*) {
    if (status == xplm_MouseDrag) {   // dragging inside a text field selects
        if (dragSelecting && focusId > 0 && focusGet) caret = caretAt(focusGet(), x);
        return 1;
    }
    if (status != xplm_MouseDown) {
        dragSelecting = false;
        return 1;
    }
    clickX = x;
    std::function<void()> fn;
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.has(x, y)) {
            fn = it->fn;
            break;
        }
    clearFocus();
    if (fn) fn();   // may take the focus again (text fields, Record)
    dragSelecting = focusId > 0;
    return 1;
}

int wheel(XPLMWindowID, int x, int y, int, int clicks, void*) {
    drop.open = false;   // the rows move under it
    for (const ScrollArea& s : scrolls)
        if (s.r.has(x, y)) *s.value = std::max(0, std::min(s.max, *s.value - clicks * 2));
    return 1;
}

XPLMCursorStatus cursor(XPLMWindowID, int, int, void*) { return xplm_CursorDefault; }

void key(XPLMWindowID, char ch, XPLMKeyFlags flags, char vkey, void*, int losingFocus) {
    if (losingFocus) {
        focusId = 0;
        focusGet = nullptr;
        focusSet = nullptr;
        capturing = false;
        captureDone = nullptr;
        return;
    }
    if (!(flags & xplm_DownFlag)) return;
    unsigned char vk = (unsigned char)vkey;

    if (drop.open && vk == XPLM_VK_ESCAPE) {
        drop.open = false;
        return;
    }

    if (capturing) {
        if (vk == XPLM_VK_ESCAPE) {
            clearFocus();
            return;
        }
        std::string name = vkName(vk);
        if (name.empty()) return;   // a modifier on its own, or a key we cannot type: keep waiting
        std::string combo;
        if (flags & xplm_ControlFlag) combo += "ctrl+";
        if (flags & xplm_OptionAltFlag) combo += "alt+";
        if (flags & xplm_ShiftFlag) combo += "shift+";
        auto done = captureDone;
        if (done) done(combo + name);
        clearFocus();
        return;
    }

    if (!focusId || !focusGet || !focusSet) return;
    if (vk == XPLM_VK_RETURN || vk == XPLM_VK_ENTER || vk == XPLM_VK_ESCAPE || vk == XPLM_VK_TAB) {
        clearFocus();
        return;
    }
    std::string s = focusGet();
    const int n = (int)s.size();
    caret = std::max(0, std::min(caret, n));
    anchor = std::max(0, std::min(anchor, n));
    const bool cmd = (flags & xplm_ControlFlag) != 0, shift = (flags & xplm_ShiftFlag) != 0, alt = (flags & xplm_OptionAltFlag) != 0;
    const int lo = std::min(caret, anchor), hi = std::max(caret, anchor);
    const bool hasSel = lo != hi;

    auto commit = [&](const std::string& v, int newCaret) {
        auto set = focusSet;   // the setter may rebuild the profile; keep our copy alive
        set(v);
        int len = focusGet ? (int)focusGet().size() : 0;   // the setter may have trimmed or cut it
        caret = anchor = std::max(0, std::min(newCaret, len));
    };
    auto replaceSelection = [&](const std::string& ins) { commit(s.substr(0, lo) + ins + s.substr(hi), lo + (int)ins.size()); };
    auto moveTo = [&](int pos) {
        caret = std::max(0, std::min(pos, n));
        if (!shift) anchor = caret;
    };
    auto wordLeft = [&] {
        int i = caret;
        while (i > 0 && s[i - 1] == ' ') --i;
        while (i > 0 && s[i - 1] != ' ' && s[i - 1] != '/' && s[i - 1] != '_') --i;
        return i;
    };
    auto wordRight = [&] {
        int i = caret;
        while (i < n && (s[i] == ' ' || s[i] == '/' || s[i] == '_')) ++i;
        while (i < n && s[i] != ' ' && s[i] != '/' && s[i] != '_') ++i;
        return i;
    };

    if (cmd) {   // Command (Control) shortcuts
        if (vk == XPLM_VK_A) {
            anchor = 0;
            caret = n;
        } else if (vk == XPLM_VK_C) {
            if (hasSel) clipboardSet(s.substr(lo, hi - lo));
        } else if (vk == XPLM_VK_X) {
            if (hasSel) {
                clipboardSet(s.substr(lo, hi - lo));
                replaceSelection("");
            }
        } else if (vk == XPLM_VK_V) {
            std::string in;
            for (char c : clipboardGet()) {
                if (c == '\n' || c == '\r' || c == '\t') c = ' ';
                if (c >= 32 && c < 127 && c != '"') in += c;
            }
            while (!in.empty() && in.back() == ' ' && in.size() > 1) in.pop_back();   // a copied line often ends in a blank
            if (!in.empty()) replaceSelection(in);
        } else if (vk == XPLM_VK_LEFT) {
            moveTo(0);
        } else if (vk == XPLM_VK_RIGHT) {
            moveTo(n);
        } else if (vk == XPLM_VK_BACK) {   // delete everything before the cursor
            if (hasSel) replaceSelection("");
            else commit(s.substr(caret), 0);
        }
        return;
    }

    switch (vk) {
        case XPLM_VK_LEFT:
            if (hasSel && !shift) caret = anchor = lo;
            else moveTo(alt ? wordLeft() : caret - 1);
            return;
        case XPLM_VK_RIGHT:
            if (hasSel && !shift) caret = anchor = hi;
            else moveTo(alt ? wordRight() : caret + 1);
            return;
        case XPLM_VK_UP:
        case XPLM_VK_HOME: moveTo(0); return;
        case XPLM_VK_DOWN:
        case XPLM_VK_END: moveTo(n); return;
        case XPLM_VK_BACK:
            if (hasSel) replaceSelection("");
            else if (caret > 0) commit(s.substr(0, caret - 1) + s.substr(caret), caret - 1);
            return;
        case XPLM_VK_DELETE:
            if (hasSel) replaceSelection("");
            else if (caret < n) commit(s.substr(0, caret) + s.substr(caret + 1), caret);
            else if (caret > 0) commit(s.substr(0, caret - 1), caret - 1);   // the Mac "delete" key may arrive as this one
            return;
        default: break;
    }
    if (ch >= 32 && ch < 127 && ch != '"') replaceSelection(std::string(1, ch));
}

}  // namespace

void uiCreate() {
    if (win) return;
    int sl, st, sr, sb;
    XPLMGetScreenBoundsGlobal(&sl, &st, &sr, &sb);
    int w = 900, h = 640;
    XPLMCreateWindow_t p;
    memset(&p, 0, sizeof p);
    p.structSize = sizeof p;
    p.left = sl + (sr - sl - w) / 2;
    p.right = p.left + w;
    p.top = st - (st - sb - h) / 2;
    p.bottom = p.top - h;
    p.visible = 0;
    p.drawWindowFunc = draw;
    p.handleMouseClickFunc = mouse;
    p.handleRightClickFunc = mouse;
    p.handleKeyFunc = key;
    p.handleCursorFunc = cursor;
    p.handleMouseWheelFunc = wheel;
    p.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    p.layer = xplm_WindowLayerFloatingWindows;
    win = XPLMCreateWindowEx(&p);
    XPLMSetWindowPositioningMode(win, xplm_WindowPositionFree, -1);
    XPLMSetWindowResizingLimits(win, 820, 560, 1600, 1200);
    XPLMSetWindowTitle(win, "X52 Pro - Program buttons");
}

void uiDestroy() {
    if (!win) return;
    clearFocus();
    XPLMDestroyWindow(win);
    win = nullptr;
    hits.clear();
    scrolls.clear();
}

void uiToggle() {
    if (!win) return;
    bool show = !XPLMGetWindowIsVisible(win);
    if (!show) clearFocus();
    XPLMSetWindowIsVisible(win, show ? 1 : 0);
    if (show) XPLMBringWindowToFront(win);
}

}  // namespace x52
