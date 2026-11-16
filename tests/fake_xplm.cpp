#include "fake_xplm.h"

#include <cstring>

#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMMenus.h"
#include "XPLMPlanes.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

FakeSim& sim() {
    static FakeSim s;
    return s;
}

namespace {
XPLMCreateWindow_t winParams;
bool haveWindow = false;
XPLMFlightLoop_f loopFn = nullptr;
std::map<std::string, XPLMCommandRef> commandRefs;
std::map<XPLMCommandRef, std::string> commandNames;
std::map<XPLMCommandRef, XPLMCommandCallback_f> handlers;
int fakeWindow;

XPLMCommandRef refFor(const std::string& name) {
    auto it = commandRefs.find(name);
    if (it != commandRefs.end()) return it->second;
    XPLMCommandRef r = (XPLMCommandRef)(new int(0));
    commandRefs[name] = r;
    commandNames[r] = name;
    return r;
}
}  // namespace

void FakeSim::frame() {
    if (loopFn) loopFn(0.016f, 0.016f, 0, nullptr);
    drawn.clear();
    if (haveWindow && windowVisible) winParams.drawWindowFunc((XPLMWindowID)&fakeWindow, winParams.refcon);
}
bool FakeSim::sees(const std::string& text) const {
    for (const DrawnText& d : drawn)
        if (d.text.find(text) != std::string::npos) return true;
    return false;
}
bool FakeSim::clickText(const std::string& text, int nth, bool exact) {
    for (const DrawnText& d : drawn)
        if ((exact ? d.text == text : d.text.find(text) != std::string::npos) && nth-- == 0) {
            winParams.handleMouseClickFunc((XPLMWindowID)&fakeWindow, d.x + 2, d.y + 3, xplm_MouseDown, winParams.refcon);
            winParams.handleMouseClickFunc((XPLMWindowID)&fakeWindow, d.x + 2, d.y + 3, xplm_MouseUp, winParams.refcon);
            frame();
            return true;
        }
    return false;
}
void FakeSim::key(char ch, int vkey, int flags) {
    winParams.handleKeyFunc((XPLMWindowID)&fakeWindow, ch, (XPLMKeyFlags)(flags | xplm_DownFlag), (char)vkey, winParams.refcon, 0);
    frame();
}
void FakeSim::type(const std::string& s) {
    for (char c : s) {
        int vk = 0;
        if (c >= 'a' && c <= 'z') vk = XPLM_VK_A + (c - 'a');
        else if (c >= '0' && c <= '9') vk = XPLM_VK_0 + (c - '0');
        else if (c == ' ') vk = XPLM_VK_SPACE;
        key(c, vk);
    }
}

// ---- the XPLM functions the plugin uses

extern "C" {

void XPLMDebugString(const char*) {}
void XPLMEnableFeature(const char*, int) {}
void XPLMGetPrefsPath(char* out) { strcpy(out, (sim().root + "/Output/preferences/Set X-Plane.prf").c_str()); }
void XPLMGetSystemPath(char* out) { strcpy(out, (sim().root + "/").c_str()); }
void XPLMGetNthAircraftModel(int, char* file, char* path) {
    strcpy(file, sim().aircraftFile.c_str());
    strcpy(path, (sim().root + "/Aircraft/" + sim().aircraftFile).c_str());
}

namespace {
std::vector<std::string> refNames;
double refValue(XPLMDataRef r) {
    size_t i = (size_t)(intptr_t)r - 1;
    return i < refNames.size() && sim().datarefs.count(refNames[i]) ? sim().datarefs[refNames[i]] : 0;
}
}  // namespace
XPLMDataRef XPLMFindDataRef(const char* name) {
    if (!sim().datarefs.count(name)) return nullptr;
    for (size_t i = 0; i < refNames.size(); ++i)
        if (refNames[i] == name) return (XPLMDataRef)(intptr_t)(i + 1);
    refNames.push_back(name);
    return (XPLMDataRef)(intptr_t)refNames.size();
}
int XPLMGetDatai(XPLMDataRef r) { return (int)refValue(r); }
float XPLMGetDataf(XPLMDataRef r) { return (float)refValue(r); }
double XPLMGetDatad(XPLMDataRef r) { return refValue(r); }
XPLMDataTypeID XPLMGetDataRefTypes(XPLMDataRef) { return xplmType_Float | xplmType_Int; }
int XPLMGetDatavf(XPLMDataRef r, float* out, int, int) { *out = (float)refValue(r); return 1; }
int XPLMGetDatavi(XPLMDataRef r, int* out, int, int) { *out = (int)refValue(r); return 1; }

XPLMCommandRef XPLMCreateCommand(const char* name, const char*) { return refFor(name); }
XPLMCommandRef XPLMFindCommand(const char* name) {
    if (commandRefs.count(name)) return commandRefs[name];
    for (const std::string& k : sim().knownCommands)
        if (k == name) return refFor(name);
    return nullptr;
}
void XPLMRegisterCommandHandler(XPLMCommandRef c, XPLMCommandCallback_f h, int, void*) { handlers[c] = h; }
void XPLMUnregisterCommandHandler(XPLMCommandRef c, XPLMCommandCallback_f, int, void*) { handlers.erase(c); }
void XPLMCommandBegin(XPLMCommandRef c) {
    ++sim().begins[commandNames[c]];
    if (handlers.count(c)) handlers[c](c, xplm_CommandBegin, nullptr);
}
void XPLMCommandEnd(XPLMCommandRef c) {
    ++sim().ends[commandNames[c]];
    if (handlers.count(c)) handlers[c](c, xplm_CommandEnd, nullptr);
}
void XPLMCommandOnce(XPLMCommandRef c) {
    ++sim().onces[commandNames[c]];
    if (handlers.count(c)) {
        handlers[c](c, xplm_CommandBegin, nullptr);
        handlers[c](c, xplm_CommandEnd, nullptr);
    }
}

void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f f, float, void*) { loopFn = f; }
void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f, void*) { loopFn = nullptr; }

XPLMMenuID XPLMFindPluginsMenu(void) { return nullptr; }
int XPLMAppendMenuItem(XPLMMenuID, const char*, void*, int) { return 0; }
XPLMMenuID XPLMCreateMenu(const char*, XPLMMenuID, int, XPLMMenuHandler_f, void*) { return nullptr; }
void XPLMDestroyMenu(XPLMMenuID) {}
void XPLMRemoveMenuItem(XPLMMenuID, int) {}

XPLMWindowID XPLMCreateWindowEx(XPLMCreateWindow_t* p) {
    winParams = *p;
    haveWindow = true;
    sim().windowVisible = p->visible != 0;
    return (XPLMWindowID)&fakeWindow;
}
void XPLMDestroyWindow(XPLMWindowID) { haveWindow = false; }
void XPLMSetWindowPositioningMode(XPLMWindowID, XPLMWindowPositioningMode, int) {}
void XPLMSetWindowResizingLimits(XPLMWindowID, int, int, int, int) {}
void XPLMSetWindowTitle(XPLMWindowID, const char*) {}
void XPLMGetWindowGeometry(XPLMWindowID, int* l, int* t, int* r, int* b) {
    *l = winParams.left;
    *t = winParams.top;
    *r = winParams.right;
    *b = winParams.bottom;
}
int XPLMGetWindowIsVisible(XPLMWindowID) { return sim().windowVisible; }
void XPLMSetWindowIsVisible(XPLMWindowID, int v) { sim().windowVisible = v != 0; }
void XPLMBringWindowToFront(XPLMWindowID) {}
void XPLMTakeKeyboardFocus(XPLMWindowID w) { sim().hasKeyboardFocus = w != nullptr; }
void XPLMGetScreenBoundsGlobal(int* l, int* t, int* r, int* b) {
    *l = 0;
    *t = 1080;
    *r = 1920;
    *b = 0;
}

void XPLMSetGraphicsState(int, int, int, int, int, int, int) {}
void XPLMDrawString(float*, int x, int y, const char* s, int*, XPLMFontID) { sim().drawn.push_back({x, y, s}); }
float XPLMMeasureString(XPLMFontID, const char*, int n) { return 6.0f * n; }
void XPLMGetFontDimensions(XPLMFontID, int* w, int* h, int* d) {
    if (w) *w = 6;
    if (h) *h = 10;
    if (d) *d = 0;
}

}  // extern "C"
