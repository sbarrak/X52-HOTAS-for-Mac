// x52manual: fills a manual template with a profile, from the command line (for trying a template out; the app does the same
// from its Export manual button).
//   x52manual <profile.x52> <template.docx> <output.docx>
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
    if (argc != 4) {
        std::cerr << "usage: x52manual <profile.x52> <template.docx> <output.docx>\n";
        return 2;
    }
    std::string text, tmpl;
    if (!slurp(argv[1], text)) { std::cerr << "cannot read " << argv[1] << "\n"; return 1; }
    if (!slurp(argv[2], tmpl)) { std::cerr << "cannot read " << argv[2] << "\n"; return 1; }
    Profile p;
    std::string err;
    if (!parse(text, p, &err)) { std::cerr << "profile: " << err << "\n"; return 1; }
    FillResult r = fillDocx(tmpl, buildManualData(p, X52_VERSION));
    if (!r.ok) { std::cerr << r.error << "\n"; return 1; }
    std::ofstream o(argv[3], std::ios::binary);
    o << r.bytes;
    for (const std::string& t : r.unknownTags) std::cerr << "warning: the template uses \"" << t << "\", which this app does not know\n";
    std::cout << "Wrote " << argv[3] << "\n";
    return o ? 0 : 1;
}
