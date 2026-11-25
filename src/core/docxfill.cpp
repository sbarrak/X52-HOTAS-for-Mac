#include "docxfill.h"

#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <sstream>

#include "zipfile.h"

namespace x52 {

namespace {
const size_t npos = std::string::npos;

struct Scope {
    const ManualData* data;
    const ManualRow* row;   // the entry being repeated, or null
    std::vector<std::string>* unknown;
    const IconSet* icons;
    std::vector<std::string>* iconsUsed;   // the pictures put into the part so far (each once)
    bool pictures;                         // false in headers and footers: they have no picture relationships of their own
};

// "<w:sdt" followed by '>' or ' ': a content control, not <w:sdtPr> or <w:sdtContent>.
size_t findOpen(const std::string& x, size_t pos) {
    for (;;) {
        size_t p = x.find("<w:sdt", pos);
        if (p == npos) return npos;
        char n = p + 6 < x.size() ? x[p + 6] : '\0';
        if (n == '>' || n == ' ') return p;
        pos = p + 6;
    }
}

// Index just after the </w:sdt> that closes the control opened at `open`; npos when it never closes.
size_t findEnd(const std::string& x, size_t open) {
    int depth = 0;
    size_t pos = open;
    for (;;) {
        size_t o = findOpen(x, pos), c = x.find("</w:sdt>", pos);
        if (c == npos) return npos;
        if (o != npos && o < c) {
            ++depth;
            pos = o + 6;
        } else {
            --depth;
            pos = c + 8;
            if (depth == 0) return pos;
        }
    }
}

std::string tagOf(const std::string& sdt) {
    size_t a = sdt.find("<w:sdtPr>"), b = sdt.find("</w:sdtPr>");
    if (a == npos || b == npos) return "";
    size_t t = sdt.find("<w:tag w:val=\"", a);
    if (t == npos || t > b) return "";
    t += 14;
    size_t e = sdt.find('"', t);
    return e == npos ? "" : sdt.substr(t, e - t);
}

std::string contentOf(const std::string& sdt) {
    size_t a = sdt.find("<w:sdtContent>"), b = sdt.rfind("</w:sdtContent>");
    if (a == npos || b == npos || b < a) return "";
    a += 14;
    return sdt.substr(a, b - a);
}

std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    return o;
}

size_t findRun(const std::string& x, size_t pos) {
    for (;;) {
        size_t p = x.find("<w:r", pos);
        if (p == npos) return npos;
        char n = p + 4 < x.size() ? x[p + 4] : '\0';
        if (n == '>' || n == ' ') return p;
        pos = p + 4;
    }
}

// The control's own text formatting: the properties of its first run, without the grey placeholder style.
std::string runProps(const std::string& content, size_t from) {
    size_t r = findRun(content, from);
    if (r == npos) return "";
    size_t openEnd = content.find('>', r);
    if (openEnd == npos || content.compare(openEnd + 1, 7, "<w:rPr>") != 0) return "";
    size_t end = content.find("</w:rPr>", openEnd);
    if (end == npos) return "";
    std::string props = content.substr(openEnd + 1, end + 8 - (openEnd + 1));
    const std::string placeholder = "<w:rStyle w:val=\"PlaceholderText\"/>";
    size_t ph = props.find(placeholder);
    if (ph != npos) props.erase(ph, placeholder.size());
    return props;
}

// A value may carry formatting for parts of it (see manual.h): \x01<RRGGBB>[,<size>]\x02 starts a coloured part,
// \x03 ends it. A run with that colour (and size, in half-points) replaces the control's own where it applies.
struct Seg {
    std::string text, color;
    int size = 0;
    std::string icon;   // a picture instead of text: \x04<name>\x05
};

std::vector<Seg> parseStyled(const std::string& v) {
    std::vector<Seg> out;
    Seg cur;
    auto flush = [&]() {
        if (!cur.text.empty()) out.push_back(cur);
        cur.text.clear();
    };
    for (size_t i = 0; i < v.size(); ++i) {
        char c = v[i];
        if (c == '\x01') {
            size_t e = v.find('\x02', i);
            if (e == npos) break;
            flush();
            std::string spec = v.substr(i + 1, e - i - 1);
            size_t comma = spec.find(',');
            cur.color = spec.substr(0, comma);
            cur.size = comma == npos ? 0 : atoi(spec.c_str() + comma + 1);
            i = e;
        } else if (c == '\x04') {
            size_t e = v.find('\x05', i);
            if (e == npos) break;
            flush();
            Seg pic;
            pic.icon = v.substr(i + 1, e - i - 1);
            out.push_back(pic);
            i = e;
        } else if (c == '\x03') {
            flush();
            cur.color.clear();
            cur.size = 0;
        } else {
            cur.text += c;
        }
    }
    flush();
    if (out.empty()) out.push_back(Seg());
    return out;
}

// Puts `value` into the attribute of the first <tag .../> of props, or adds the element when there is none.
std::string setAttr(std::string props, const std::string& tag, const std::string& value) {
    std::string elem = "<" + tag + " w:val=\"" + value + "\"/>";
    size_t a = props.find("<" + tag + " ");
    if (a != npos) {
        size_t e = props.find("/>", a);
        if (e != npos) return props.replace(a, e + 2 - a, elem);
    }
    if (props.empty()) return "<w:rPr>" + elem + "</w:rPr>";
    size_t close = props.rfind("</w:rPr>");
    return close == npos ? props : props.insert(close, elem);
}

std::string styledProps(const std::string& props, const Seg& s) {
    std::string p = props;
    if (!s.color.empty()) p = setAttr(p, "w:color", s.color);
    if (s.size > 0) {
        p = setAttr(p, "w:sz", std::to_string(s.size));
        p = setAttr(p, "w:szCs", std::to_string(s.size));
    }
    return p;
}

// The control's text as a run (or paragraph, when the control holds paragraphs).
// The relationship of the n-th picture used: rId901, rId902, ...
std::string iconRelId(int n) { return "rId" + std::to_string(901 + n); }

std::string pictureRun(const std::string& name, int id, const std::string& rid) {
    const std::string emu = "139700";   // 11 pt
    return "<w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\"><wp:extent cx=\"" + emu + "\" cy=\"" + emu + "\"/>"
           "<wp:docPr id=\"" + std::to_string(id) + "\" name=\"Light icon " + name + "\"/>"
           "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
           "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"><pic:pic>"
           "<pic:nvPicPr><pic:cNvPr id=\"0\" name=\"" + name + ".png\"/><pic:cNvPicPr/></pic:nvPicPr>"
           "<pic:blipFill><a:blip r:embed=\"" + rid + "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
           "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" + emu + "\" cy=\"" + emu + "\"/></a:xfrm>"
           "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>";
}

std::string render(const std::string& content, const std::string& value, const Scope& scope) {
    bool block = content.find("<w:p>") != npos || content.find("<w:p ") != npos;
    std::string ppr;
    size_t from = 0;
    if (block) {
        size_t a = content.find("<w:pPr>"), b = content.find("</w:pPr>");
        if (a != npos && b != npos && b > a) {
            ppr = content.substr(a, b + 8 - a);
            from = b + 8;
        }
    }
    std::string props = runProps(content, from);
    std::string runs;
    static int pictureId = 9000;
    for (const Seg& seg : parseStyled(value)) {
        if (!seg.icon.empty()) {
            bool have = false;
            if (scope.icons) have = scope.icons->find(seg.icon) != scope.icons->end();
            if (!scope.pictures || !have) {
                std::string tag = "icon:" + seg.icon;
                if (std::find(scope.unknown->begin(), scope.unknown->end(), tag) == scope.unknown->end()) scope.unknown->push_back(tag);
                continue;
            }
            auto at = std::find(scope.iconsUsed->begin(), scope.iconsUsed->end(), seg.icon);
            if (at == scope.iconsUsed->end()) at = scope.iconsUsed->insert(scope.iconsUsed->end(), seg.icon);
            runs += pictureRun(seg.icon, ++pictureId, iconRelId((int)(at - scope.iconsUsed->begin())));
            continue;
        }
        runs += "<w:r>" + styledProps(props, seg);
        size_t start = 0;
        for (;;) {
            size_t nl = seg.text.find('\n', start);
            runs += "<w:t xml:space=\"preserve\">" + escape(seg.text.substr(start, nl == npos ? npos : nl - start)) + "</w:t>";
            if (nl == npos) break;
            runs += "<w:br/>";
            start = nl + 1;
        }
        runs += "</w:r>";
    }
    return block ? "<w:p>" + ppr + runs + "</w:p>" : runs;
}

bool lookup(const Scope& s, const std::string& tag, std::string& out) {
    if (s.row) {
        auto it = s.row->find(tag);
        if (it != s.row->end()) {
            out = it->second;
            return true;
        }
    }
    auto it = s.data->values.find(tag);
    if (it == s.data->values.end()) return false;
    out = it->second;
    return true;
}

// "repeat:stick_buttons:used_pinkie": only the rows whose yes/no field ending in ".used_pinkie" says yes.
bool rowPasses(const ManualRow& row, const std::string& filter) {
    if (filter.empty()) return true;
    const std::string suffix = "." + filter;
    for (const auto& kv : row)
        if (kv.first.size() > suffix.size() && kv.first.compare(kv.first.size() - suffix.size(), suffix.size(), suffix) == 0)
            return kv.second == "yes";
    return true;   // the row has no such field: show it
}

std::string process(const std::string& xml, const Scope& scope) {
    std::string out;
    size_t pos = 0;
    for (;;) {
        size_t open = findOpen(xml, pos);
        size_t end = open == npos ? npos : findEnd(xml, open);
        if (end == npos) {
            out += xml.substr(pos);
            return out;
        }
        out += xml.substr(pos, open - pos);
        std::string sdt = xml.substr(open, end - open), tag = tagOf(sdt), content = contentOf(sdt);
        if (tag.compare(0, 7, "repeat:") == 0) {
            std::string listName = tag.substr(7), filter;
            size_t colon = listName.find(':');
            if (colon != npos) {
                filter = listName.substr(colon + 1);
                listName = listName.substr(0, colon);
            }
            auto list = scope.data->lists.find(listName);
            if (list == scope.data->lists.end()) {
                if (std::find(scope.unknown->begin(), scope.unknown->end(), tag) == scope.unknown->end()) scope.unknown->push_back(tag);
            } else {
                for (const ManualRow& row : list->second)
                    if (rowPasses(row, filter)) out += process(content, Scope{scope.data, &row, scope.unknown, scope.icons, scope.iconsUsed, scope.pictures});
            }
        } else if (!tag.empty()) {
            std::string value;
            if (!lookup(scope, tag, value)) {
                if (std::find(scope.unknown->begin(), scope.unknown->end(), tag) == scope.unknown->end()) scope.unknown->push_back(tag);
                value.clear();
            }
            out += render(content, value, scope);
        } else {
            out += sdt;   // someone else's content control: left alone
        }
        pos = end;
    }
}

bool isWordPart(const std::string& name) {
    if (name.compare(0, 5, "word/") != 0 || name.size() < 9 || name.compare(name.size() - 4, 4, ".xml") != 0) return false;
    std::string base = name.substr(5);
    return base == "document.xml" || base.compare(0, 6, "header") == 0 || base.compare(0, 6, "footer") == 0;
}
}  // namespace

int templateSchema(const std::string& templateBytes) {
    std::vector<ZipEntry> parts;
    if (!readZip(templateBytes, parts, nullptr)) return 0;
    for (const ZipEntry& e : parts) {
        if (e.name != "docProps/custom.xml") continue;
        size_t n = e.data.find("name=\"X52TemplateSchema\"");
        if (n == npos) return 0;
        size_t v = e.data.find("<vt:i4>", n);
        if (v == npos) return 0;
        return atoi(e.data.c_str() + v + 7);
    }
    return 0;
}

IconSet loadIconSet(const std::string& dir) {
    IconSet icons;
    DIR* d = opendir(dir.c_str());
    if (!d) return icons;
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name.size() < 5 || name.compare(name.size() - 4, 4, ".png") != 0) continue;
        std::ifstream f(dir + "/" + name, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        if (f) icons[name.substr(0, name.size() - 4)] = ss.str();
    }
    closedir(d);
    return icons;
}

FillResult fillDocx(const std::string& templateBytes, const ManualData& data, const IconSet& icons) {
    FillResult r;
    std::vector<ZipEntry> parts;
    std::string err;
    if (!readZip(templateBytes, parts, &err)) {
        r.error = "The template is not a Word file (" + err + ").";
        return r;
    }
    bool hasDocument = false;
    for (const ZipEntry& e : parts) hasDocument = hasDocument || e.name == "word/document.xml";
    if (!hasDocument) {
        r.error = "The template is not a Word document.";
        return r;
    }
    int needs = templateSchema(templateBytes);
    if (needs > kManualSchema) {
        r.error = "This template needs a newer version of the app (manual data schema " + std::to_string(needs) +
                  "; this app has " + std::to_string(kManualSchema) + ").";
        return r;
    }
    std::vector<std::string> used;
    for (ZipEntry& e : parts)
        if (isWordPart(e.name)) e.data = process(e.data, Scope{&data, nullptr, &r.unknownTags, &icons, &used, e.name == "word/document.xml"});
    if (!used.empty()) {   // the pictures: their files, a relationship for each, and the png type
        std::string rels;
        for (size_t n = 0; n < used.size(); ++n) {
            const std::string& name = used[n];
            parts.push_back({"word/media/light_" + name + ".png", icons.at(name)});
            rels += "<Relationship Id=\"" + iconRelId((int)n) + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/light_" + name + ".png\"/>";
        }
        for (ZipEntry& e : parts) {
            if (e.name == "word/_rels/document.xml.rels") {
                size_t end = e.data.rfind("</Relationships>");
                if (end != npos) e.data.insert(end, rels);
            } else if (e.name == "[Content_Types].xml" && e.data.find("Extension=\"png\"") == npos) {
                size_t end = e.data.rfind("</Types>");
                if (end != npos) e.data.insert(end, "<Default Extension=\"png\" ContentType=\"image/png\"/>");
            }
        }
    }
    r.bytes = writeZip(parts);
    r.ok = true;
    return r;
}

}  // namespace x52
