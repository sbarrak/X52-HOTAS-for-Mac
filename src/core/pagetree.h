// Pages of the stick's screen that you write yourself, in a tree: a page has rows, and a row is plain text,
// a dataref value, or a link that opens another page. The built-in radio pages stay as they are (see pages.h);
// user pages come after them in the left wheel's order, with the Buttons page still last. Pure C++.
#pragma once
#include <string>
#include <vector>

namespace x52 {

enum class RowKind { Text, Value, Link };

struct PageRow {
    RowKind kind = RowKind::Text;
    std::string text;                 // Text: the words. Value: the label. Link: the label.
    std::string format = "%g";        // Value: how to show the number, e.g. "%.0f kg" (one %d, %i, %f, %e or %g)
    std::string dataref;              // Value: "name" or "name[3]"
    std::string target;               // Link: id of the page it opens
    bool operator==(const PageRow& o) const {
        return kind == o.kind && text == o.text && format == o.format && dataref == o.dataref && target == o.target;
    }
};

struct UserPage {
    std::string id;                   // letters, digits, - and _
    std::string title;                // shown in the windows, and as the only line of a page without rows
    std::string parent;               // "" for a page the left wheel reaches; otherwise only a link opens it
    std::vector<PageRow> rows;
    bool operator==(const UserPage& o) const {
        return id == o.id && title == o.title && parent == o.parent && rows == o.rows;
    }
};

struct PageTree {
    std::vector<UserPage> pages;
    int find(const std::string& id) const;            // index, or -1
    std::vector<int> topLevel() const;                // pages the left wheel reaches, in file order
    bool operator==(const PageTree& o) const { return pages == o.pages; }
};

// The text form, lines of a profile file:
//   page <id> "<title>" [in <parent id>]
//   row <page id> text "<words>"
//   row <page id> value "<label>" "<format>" <dataref>
//   row <page id> link "<label>" <page id>
std::string serializePages(const PageTree& t);
bool parsePageLine(const std::string& rest, PageTree& t, std::string& err);   // after "page "
bool parseRowLine(const std::string& rest, PageTree& t, std::string& err);    // after "row "

// A number as a format asks for it. Only one conversion (%d %i %f %e %g, with flags, width, precision) is
// allowed; %% is a percent sign. Anything else is shown with %g.
std::string formatPageValue(const std::string& format, double value);

}  // namespace x52
