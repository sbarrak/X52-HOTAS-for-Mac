#include "pagetree.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace x52 {

namespace {

// Splits a line into words; "double quoted" text is one word.
bool tokens(const std::string& s, std::vector<std::string>& out, std::string& err) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        if (isspace((unsigned char)s[i])) { ++i; continue; }
        if (s[i] == '"') {
            size_t e = s.find('"', i + 1);
            if (e == std::string::npos) { err = "a quote is not closed"; return false; }
            out.push_back("\"" + s.substr(i + 1, e - i - 1));   // a leading " marks a quoted word
            i = e + 1;
        } else {
            size_t e = i;
            while (e < s.size() && !isspace((unsigned char)s[e])) ++e;
            out.push_back(s.substr(i, e - i));
            i = e;
        }
    }
    return true;
}
bool quoted(const std::string& t) { return !t.empty() && t[0] == '"'; }
std::string unquote(const std::string& t) { return quoted(t) ? t.substr(1) : t; }

bool validId(const std::string& id) {
    if (id.empty()) return false;
    for (char c : id)
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') return false;
    return true;
}

std::string clean(std::string s) {
    for (char& c : s) {
        if (c == '"') c = '\'';
        if (c == '\n' || c == '\r') c = ' ';
    }
    return s;
}

}  // namespace

int PageTree::find(const std::string& id) const {
    for (size_t i = 0; i < pages.size(); ++i)
        if (pages[i].id == id) return (int)i;
    return -1;
}

std::vector<int> PageTree::topLevel() const {
    std::vector<int> out;
    for (size_t i = 0; i < pages.size(); ++i)
        if (pages[i].parent.empty()) out.push_back((int)i);
    return out;
}

std::string serializePages(const PageTree& t) {
    std::ostringstream o;
    if (t.pages.empty()) return "";
    o << "# page <id> \"<title>\" [in <parent id>]    pages of the stick's screen; a page without 'in' is reached with the left wheel\n";
    o << "# row <page id> text \"<words>\" | value \"<label>\" \"<format, e.g. %.0f kg>\" <dataref> | link \"<label>\" <page id>\n";
    for (const UserPage& p : t.pages) {
        o << "page " << p.id << " \"" << clean(p.title) << "\"";
        if (!p.parent.empty()) o << " in " << p.parent;
        o << "\n";
        for (const PageRow& r : p.rows) {
            o << "row " << p.id << " ";
            if (r.kind == RowKind::Text) o << "text \"" << clean(r.text) << "\"";
            else if (r.kind == RowKind::Value)
                o << "value \"" << clean(r.text) << "\" \"" << clean(r.format) << "\" " << (r.dataref.empty() ? "-" : r.dataref);
            else o << "link \"" << clean(r.text) << "\" " << (r.target.empty() ? "-" : r.target);
            o << "\n";
        }
    }
    return o.str();
}

bool parsePageLine(const std::string& rest, PageTree& t, std::string& err) {
    std::vector<std::string> w;
    if (!tokens(rest, w, err)) return false;
    if (w.size() != 2 && w.size() != 4) { err = "page needs: <id> \"<title>\" [in <parent id>]"; return false; }
    UserPage p;
    p.id = w[0];
    p.title = unquote(w[1]);
    if (!validId(p.id)) { err = "a page id is letters, digits, - and _"; return false; }
    if (t.find(p.id) >= 0) { err = "page '" + p.id + "' is defined twice"; return false; }
    if (w.size() == 4) {
        if (w[2] != "in") { err = "expected 'in <parent id>'"; return false; }
        p.parent = w[3];
        if (p.parent == p.id) { err = "a page cannot be inside itself"; return false; }
    }
    t.pages.push_back(p);
    return true;
}

bool parseRowLine(const std::string& rest, PageTree& t, std::string& err) {
    std::vector<std::string> w;
    if (!tokens(rest, w, err)) return false;
    if (w.size() < 3) { err = "row needs: <page id> text|value|link ..."; return false; }
    int pi = t.find(w[0]);
    if (pi < 0) { err = "row for page '" + w[0] + "', which is not defined above it"; return false; }
    PageRow r;
    if (w[1] == "text" && w.size() == 3) {
        r.kind = RowKind::Text;
        r.text = unquote(w[2]);
    } else if (w[1] == "value" && w.size() == 5) {
        r.kind = RowKind::Value;
        r.text = unquote(w[2]);
        r.format = unquote(w[3]);
        r.dataref = w[4] == "-" ? "" : w[4];
    } else if (w[1] == "link" && w.size() == 4) {
        r.kind = RowKind::Link;
        r.text = unquote(w[2]);
        r.target = w[3] == "-" ? "" : w[3];
    } else {
        err = "row is: text \"words\"  |  value \"label\" \"format\" dataref  |  link \"label\" page-id";
        return false;
    }
    t.pages[pi].rows.push_back(r);
    return true;
}

std::string formatPageValue(const std::string& f, double v) {
    std::string out;
    bool used = false;
    for (size_t i = 0; i < f.size(); ++i) {
        if (f[i] != '%') { out += f[i]; continue; }
        if (i + 1 < f.size() && f[i + 1] == '%') { out += '%'; ++i; continue; }
        size_t j = i + 1;
        std::string spec = "%";
        while (j < f.size() && std::string("-+ 0#").find(f[j]) != std::string::npos) spec += f[j++];
        int width = 0, digits = 0;
        while (j < f.size() && isdigit((unsigned char)f[j]) && digits < 2) { spec += f[j]; ++j; ++digits; }
        (void)width;
        if (j < f.size() && f[j] == '.') {
            spec += f[j++];
            int pd = 0;
            while (j < f.size() && isdigit((unsigned char)f[j]) && pd < 2) { spec += f[j++]; ++pd; }
        }
        char conv = j < f.size() ? f[j] : 0;
        if (used || !(conv == 'd' || conv == 'i' || conv == 'f' || conv == 'e' || conv == 'g')) {
            char buf[48];
            snprintf(buf, sizeof buf, "%g", v);
            return buf;   // not a format we trust: show the number plainly
        }
        used = true;
        char buf[96];
        if (conv == 'd' || conv == 'i') snprintf(buf, sizeof buf, (spec + "lld").c_str(), (long long)std::llround(v));
        else snprintf(buf, sizeof buf, (spec + conv).c_str(), v);
        out += buf;
        i = j;
    }
    if (!used) return out;   // a format with no number in it: just the text
    return out;
}

}  // namespace x52
