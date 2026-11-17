#include "pageview.h"

#include <algorithm>

namespace x52 {

namespace {

constexpr int kMaxDepth = 8;

struct Shown {
    enum Kind { Back, Text, Value, Link } kind;
    const PageRow* row;
};

const UserPage* currentPage(const PageTree& t, const std::string& topId, const PageNav& nav) {
    int i = t.find(nav.stack.empty() ? topId : nav.stack.back());
    return i < 0 ? nullptr : &t.pages[i];
}

std::vector<Shown> shownRows(const UserPage& p, bool inSub) {
    std::vector<Shown> out;
    if (inSub) out.push_back({Shown::Back, nullptr});
    for (const PageRow& r : p.rows)
        out.push_back({r.kind == RowKind::Text ? Shown::Text : r.kind == RowKind::Value ? Shown::Value : Shown::Link, &r});
    return out;
}

std::string cut(const std::string& s, size_t n) { return s.size() > n ? s.substr(0, n) : s; }

std::string rowText(const Shown& s, const std::function<bool(const std::string&, double&)>& read) {
    const size_t avail = kMfdCols - 1;   // the first column is for the cursor
    if (s.kind == Shown::Back) return "< Back";
    const PageRow& r = *s.row;
    if (s.kind == Shown::Text) return cut(r.text, avail);
    if (s.kind == Shown::Link) {
        std::string label = cut(r.text, avail - 2);
        return label + std::string(avail - 1 - label.size(), ' ') + ">";
    }
    double v = 0;
    std::string value = !r.dataref.empty() && read && read(r.dataref, v) ? formatPageValue(r.format, v) : "--";
    value = cut(value, avail);
    size_t room = avail - value.size();
    std::string label = cut(r.text, room > 0 ? room - 1 : 0);
    return label + std::string(room - label.size(), ' ') + value;   // the value is right-aligned
}

}  // namespace

Lines formatUserPage(const PageTree& t, const std::string& topId, PageNav& nav, const std::function<bool(const std::string&, double&)>& read) {
    Lines out;
    const UserPage* p = currentPage(t, topId, nav);
    if (!p) return out;
    std::vector<Shown> rows = shownRows(*p, !nav.stack.empty());
    if (rows.empty()) {
        out.l[0] = cut(p->title, kMfdCols);   // a page without rows says what it is
        return out;
    }
    int n = (int)rows.size();
    nav.cursor = std::max(0, std::min(nav.cursor, n - 1));
    int first = std::max(0, std::min(nav.cursor - 1, n - kMfdLines));
    for (int i = 0; i < kMfdLines && first + i < n; ++i)
        out.l[i] = std::string(first + i == nav.cursor ? ">" : " ") + rowText(rows[first + i], read);
    return out;
}

void pageTreeInput(const PageTree& t, const std::string& topId, PageNav& nav, Wheel w) {
    const UserPage* p = currentPage(t, topId, nav);
    if (!p) return;
    std::vector<Shown> rows = shownRows(*p, !nav.stack.empty());
    int n = (int)rows.size();
    if (n == 0) return;
    nav.cursor = std::max(0, std::min(nav.cursor, n - 1));
    if (w == Wheel::Up) {
        nav.cursor = std::max(0, nav.cursor - 1);
    } else if (w == Wheel::Down) {
        nav.cursor = std::min(n - 1, nav.cursor + 1);
    } else {
        const Shown& s = rows[nav.cursor];
        if (s.kind == Shown::Back) {
            nav.stack.pop_back();
            nav.cursor = nav.cursors.empty() ? 0 : nav.cursors.back();
            if (!nav.cursors.empty()) nav.cursors.pop_back();
        } else if (s.kind == Shown::Link && t.find(s.row->target) >= 0 && (int)nav.stack.size() < kMaxDepth) {
            nav.cursors.push_back(nav.cursor);
            nav.stack.push_back(s.row->target);
            nav.cursor = 0;
        }
    }
}

}  // namespace x52
