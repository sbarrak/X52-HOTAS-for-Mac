// x52manual: fills a manual template with a profile, from the command line (for trying a template out; the app does the same
// from its Export manual button).
//   x52manual <profile.x52> <template.docx> <output.docx> [icon folder]
// The icon folder (the light pictures) is the template's folder + /light-icons when not given.
#include <fstream>
#include <iostream>
#include <sstream>

#include "../src/core/docxfill.h"
#include "../src/core/manual.h"

using namespace x52;

static bool slurp(const char* path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

int main(int argc, char** argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: x52manual <profile.x52> <template.docx> <output.docx> [icon folder]\n";
        return 2;
    }
    std::string text, tmpl;
    if (!slurp(argv[1], text)) { std::cerr << "cannot read " << argv[1] << "\n"; return 1; }
    if (!slurp(argv[2], tmpl)) { std::cerr << "cannot read " << argv[2] << "\n"; return 1; }
    Profile p;
    std::string err;
    if (!parse(text, p, &err)) { std::cerr << "profile: " << err << "\n"; return 1; }
    std::string key = argv[1];   // the file name without its folder and ".x52"
    size_t slash = key.find_last_of('/');
    if (slash != std::string::npos) key = key.substr(slash + 1);
    if (key.size() > 4 && key.compare(key.size() - 4, 4, ".x52") == 0) key.resize(key.size() - 4);
    std::string iconDir;
    if (argc == 5) {
        iconDir = argv[4];
    } else {
        std::string tp = argv[2];
        size_t s = tp.find_last_of('/');
        iconDir = (s == std::string::npos ? std::string(".") : tp.substr(0, s)) + "/light-icons";
    }
    IconSet icons = loadIconSet(iconDir);
    if (icons.empty()) std::cerr << "warning: no light icons in " << iconDir << "\n";
    FillResult r = fillDocx(tmpl, buildManualData(p, X52_VERSION, key), icons);
    if (!r.ok) { std::cerr << r.error << "\n"; return 1; }
    std::ofstream o(argv[3], std::ios::binary);
    o << r.bytes;
    for (const std::string& t : r.unknownTags) std::cerr << "warning: the template uses \"" << t << "\", which this app does not know\n";
    std::cout << "Wrote " << argv[3] << "\n";
    return o ? 0 : 1;
}
