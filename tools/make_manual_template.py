#!/usr/bin/env python3
"""Makes the manual template, templates/X52-Manual-Template.docx, from the empty aircraft profile template.

The empty template (docs/mapping/X52 Pro - Empty aircraft profile template.docx) is the manual's layout: the page
header, the joystick and throttle pictures, the assignment tables with their fixed rows, the display and lights
dashboard. This script turns its blank spots into Word content controls with tags (Developer tab > Properties), which
the app's Export Current Template fills in. Layout, pictures and fonts are not touched.

Standard library only (no packages). After the first run the .docx is the file you edit in Word; run this again only to
start over from the empty template. The tags are listed in docs/manual-data.md.

    python3 tools/make_manual_template.py [source.docx] [output.docx]
"""
import re
import sys
import zipfile
from xml.sax.saxutils import escape

SOURCE = "docs/mapping/X52 Pro - Empty aircraft profile template.docx"
OUTPUT = "templates/X52-Manual-Template.docx"
SCHEMA = 1   # the lowest manual data schema this template needs (docs/manual-data.md)

_ids = [4000]


def next_id():
    _ids[0] += 1
    return _ids[0]


def slug(label):
    return re.sub(r"[^a-z0-9]+", "_", label.lower()).strip("_")


def text_of(xml):
    return "".join(re.findall(r"<w:t[^>]*>([^<]*)</w:t>", xml))


def control(tag, rpr="", placeholder="—"):
    """An inline content control the app replaces with text; rpr is the formatting the text keeps."""
    return ('<w:sdt><w:sdtPr><w:alias w:val="%s"/><w:tag w:val="%s"/><w:id w:val="%d"/><w:text/></w:sdtPr>'
            '<w:sdtContent><w:r>%s<w:t>%s</w:t></w:r></w:sdtContent></w:sdt>') % (escape(tag), tag, next_id(), rpr, placeholder)


def text_run(rpr, text):
    return '<w:r>%s<w:t xml:space="preserve">%s</w:t></w:r>' % (rpr, escape(text))


RPR = r"<w:rPr>(?:(?!</w:rPr>).)*</w:rPr>"   # one run's properties, never across two runs
RUN = re.compile(r"<w:r>(" + RPR + r")?(?:<w:t[^>]*>[^<]*</w:t>)?</w:r>", re.S)


def cell_control(cell, tag):
    """The first run of a table cell (empty or with its label) becomes a control that keeps the run's formatting."""
    m = RUN.search(cell)
    if not m:
        raise SystemExit("no run in cell for " + tag)
    return cell[:m.start()] + control(tag, m.group(1) or "") + cell[m.end():]


def cells_of(row):
    return list(re.finditer(r"<w:tc>.*?</w:tc>", row, re.S))


def rows_of(table):
    return list(re.finditer(r"<w:tr[ >].*?</w:tr>", table, re.S))


def replace_cells(row, tags):
    """tags: one tag (or None to keep the cell) for each cell of the row."""
    out, pos = [], 0
    for m, tag in zip(cells_of(row), tags):
        out.append(row[pos:m.start()])
        out.append(m.group(0) if tag is None else cell_control(m.group(0), tag))
        pos = m.end()
    out.append(row[pos:])
    return "".join(out)


def map_rows(table, tags_for_row):
    """Calls tags_for_row(first cell's text, number of cells) for every body row; it returns the tags of that row."""
    out, pos = [], 0
    for r in rows_of(table)[1:]:
        cells = cells_of(r.group(0))
        row = replace_cells(r.group(0), tags_for_row(text_of(cells[0].group(0)), len(cells)))
        out.append(table[pos:r.start()])
        out.append(row)
        pos = r.end()
    out.append(table[pos:])
    return "".join(out)


def roomy_cols(table, cols):
    """The cells in these columns hold a light icon: centred, with normal line spacing so the line is as tall as the picture."""
    out, pos = [], 0
    for r in rows_of(table)[1:]:
        row, rpos, parts = r.group(0), 0, []
        for i, c in enumerate(cells_of(row)):
            parts.append(row[rpos:c.start()])
            cell = c.group(0)
            if i in cols:
                cell = cell.replace('w:line="200" w:lineRule="exact"', 'w:line="240" w:lineRule="auto"').replace("</w:pPr>", '<w:jc w:val="center"/></w:pPr>', 1)
            parts.append(cell)
            rpos = c.end()
        parts.append(row[rpos:])
        out.append(table[pos:r.start()])
        out.append("".join(parts))
        pos = r.end()
    out.append(table[pos:])
    return "".join(out)


def control_table(table, prefix):
    """Rows labelled with a control ('A tap'): each of the three mode cells gets <prefix>.<label slug>.m<n>."""
    return map_rows(table, lambda label, n: [None] + ["%s.%s.m%d" % (prefix, slug(label), m) for m in (1, 2, 3)])


def numbered_table(table, tag_for):
    """Rows labelled 1, 2, 3...: tag_for(number, index of the cell after the number, starting at 1)."""
    return map_rows(table, lambda label, n: [None] + [tag_for(int(label), c) for c in range(1, n)])


def repeat_table(table, list_name, tags):
    """The first body row is the pattern: its cells take the tags, it is wrapped in a repeat control, the other body rows go."""
    rows = rows_of(table)
    first = replace_cells(rows[1].group(0), tags)
    wrapped = ('<w:sdt><w:sdtPr><w:alias w:val="Rows: %s"/><w:tag w:val="repeat:%s"/><w:id w:val="%d"/></w:sdtPr>'
               '<w:sdtContent>%s</w:sdtContent></w:sdt>') % (list_name, list_name, next_id(), first)
    return table[:rows[1].start()] + wrapped + table[rows[-1].end():]


LIGHTS = {"Fire": "fire", "A": "a", "B": "b", "D": "d", "E": "e", "T1": "t1", "T2": "t2", "T3": "t3",
          "H2 lit hat": "pov", "i clutch": "clutch", "Throttle": "throttle"}


def light_table(table):
    return map_rows(table, lambda label, n: [None] + ["light.%s.m%d" % (LIGHTS[label], m) for m in (1, 2, 3)])


def leaf_tables(xml):
    """(start, end) of every table that holds no other table, in order."""
    spans, stack = [], []
    for m in re.finditer(r"<w:tbl>|</w:tbl>", xml):
        if m.group(0) == "<w:tbl>":
            stack.append([m.start(), False])
            if len(stack) > 1:
                stack[-2][1] = True
        else:
            start, has_inner = stack.pop()
            if not has_inner:
                spans.append((start, m.end()))
    return sorted(spans)


def split_run(xml, old, pieces):
    """Replaces the text run holding `old` by runs and controls (pieces: ('t', text) or ('c', tag)), same formatting."""
    m = re.search(r"<w:r>(" + RPR + r")<w:t xml:space=\"preserve\">%s</w:t></w:r>" % re.escape(old), xml, re.S)
    if not m:
        raise SystemExit("not found in the empty template: " + old)
    rpr = m.group(1)
    new = "".join(text_run(rpr, v) if k == "t" else control(v, rpr) for k, v in pieces)
    return xml[:m.start()] + new + xml[m.end():]


def transform_document(xml):
    spans = leaf_tables(xml)
    if len(spans) != 9:
        raise SystemExit("the empty template has %d tables, expected 9; update this script" % len(spans))
    kinds = ["ControlMode", "ControlMode", "ControlMode", "ControlMode", "No.Mode", "PageFields", "LightMode", "LightCondition", "No.Alarm"]
    for (a, b), want in zip(spans, kinds):
        first = text_of(rows_of(xml[a:b])[0].group(0))
        if not first.startswith(want):
            raise SystemExit("table starting '%s' found where '%s' was expected" % (first[:30], want))
    new = {}
    for i, (a, b) in enumerate(spans):
        t = xml[a:b]
        if i == 0: t = control_table(t, "stick")
        elif i == 1: t = control_table(t, "throttle")
        elif i == 2: t = control_table(t, "pinkie_stick")
        elif i == 3: t = control_table(t, "pinkie_throttle")
        elif i == 4: t = numbered_table(t, lambda n, c: "pages.%d.m%d" % (n, c))
        elif i == 5: t = repeat_table(t, "user_pages", ["page.title", "page.fields"])
        elif i == 6: t = light_table(roomy_cols(t, (1, 2, 3)))
        elif i == 7: t = repeat_table(roomy_cols(t, (2, 3)), "rules", ["rule.light", "rule.condition", "rule.active", "rule.otherwise"])
        else: t = repeat_table(roomy_cols(t, (3, 4, 5)), "alarms", ["alarm.no", "alarm.name", "alarm.light", "alarm.m1", "alarm.m2", "alarm.m3", "alarm.rate"])
        new[(a, b)] = t
    for (a, b) in sorted(new, reverse=True):
        xml = xml[:a] + new[(a, b)] + xml[b:]

    # The mode names in the column headings: "Mode 1 / Takeoff" -> "Mode 1 (" + the mode's name + ")".
    def mode_head(m):
        return text_run(m.group(1), "Mode %s (" % m.group(2)) + control("mode.%s.name" % m.group(2), m.group(1)) + text_run(m.group(1), ")")
    xml = re.sub(r"<w:r>(" + RPR + r")<w:t xml:space=\"preserve\">Mode (\d) / \w+</w:t></w:r>", mode_head, xml, flags=re.S)
    xml = split_run(xml, "Shift enabled in each mode:  Mode 1 ________   Mode 2 ________   Mode 3 ________.   Record the actual shifted assignments and the Any mode actions that remain available.",
                    [("t", "Shift enabled in each mode:  Mode 1 "), ("c", "shift.m1"), ("t", "   Mode 2 "), ("c", "shift.m2"),
                     ("t", "   Mode 3 "), ("c", "shift.m3"), ("t", ".   Record the actual shifted assignments and the Any mode actions that remain available.")])
    xml = split_run(xml, "Hold threshold: __________   Release action: ______________________________________________",
                    [("t", "Hold threshold: "), ("c", "profile.hold_ms"), ("t", "   Release action: "), ("c", "profile.release_actions")])
    xml = split_run(xml, "Screen and light power conditions: ____________________________________________",
                    [("t", "Screen and light power conditions: "), ("c", "profile.power_conditions")])
    return move_growing_tables(xml)


def move_growing_tables(xml):
    """Live lights and Alarms can grow, so they leave the dashboard's right column for a page of their own."""
    close = "</w:tc></w:tr></w:tbl>"   # the end of the dashboard's layout table
    a = max(xml.rfind("<w:p>", 0, xml.index("Live lights")), xml.rfind("<w:p ", 0, xml.index("Live lights")))
    end = xml.index(close, a)
    if xml[end - 6:end] != "</w:p>":
        raise SystemExit("the dashboard no longer ends with a paragraph; update this script")
    block = xml[a:end]
    # the heading of the new page is a copy of the dashboard's own heading, starting a page
    h = xml.index("Display and lights dashboard")
    hs = max(xml.rfind("<w:p>", 0, h), xml.rfind("<w:p ", 0, h))
    heading = xml[hs:xml.index("</w:p>", h) + 6].replace("Display and lights dashboard", "Live lights and alarms")
    pageBreak = "<w:pageBreakBefore/>"
    if "<w:pStyle" in heading[:heading.index("</w:pPr>") if "</w:pPr>" in heading else 0]:
        k = heading.index("/>", heading.index("<w:pStyle")) + 2
        heading = heading[:k] + pageBreak + heading[k:]
    elif "<w:pPr>" in heading:
        heading = heading.replace("<w:pPr>", "<w:pPr>" + pageBreak, 1)
    else:
        heading = heading.replace("<w:p>", "<w:p><w:pPr>" + pageBreak + "</w:pPr>", 1)
    empty = '<w:p><w:pPr><w:spacing w:before="0" w:after="0"/></w:pPr></w:p>'   # a table cell must end with a paragraph
    return xml[:a] + empty + close + heading + block + xml[end + len(close):]


def transform_header(xml):
    for old, tag in (("[Aircraft name]", "profile.name"), ("[Profile filename]", "profile.file")):
        m = re.search(r"<w:r>(" + RPR + r")<w:t>%s</w:t></w:r>" % re.escape(old), xml, re.S)
        if not m:
            raise SystemExit("not found in the header: " + old)
        xml = xml[:m.start()] + control(tag, m.group(1)) + xml[m.end():]
    return xml


CUSTOM = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/custom-properties" '
          'xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">'
          '<property fmtid="{D5CDD505-2E9C-101B-9397-08002B2CF9AE}" pid="2" name="X52TemplateSchema"><vt:i4>%d</vt:i4></property></Properties>' % SCHEMA)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else SOURCE
    out = sys.argv[2] if len(sys.argv) > 2 else OUTPUT
    with zipfile.ZipFile(src) as z:
        parts = [(i.filename, z.read(i.filename)) for i in z.infolist()]
    if "docProps/custom.xml" in [n for n, _ in parts]:
        raise SystemExit("the source already has custom properties; update this script")
    result = []
    for name, data in parts:
        if name == "word/document.xml":
            data = transform_document(data.decode("utf-8")).encode("utf-8")
        elif name == "word/header1.xml":
            data = transform_header(data.decode("utf-8")).encode("utf-8")
        elif name == "[Content_Types].xml":
            data = data.decode("utf-8").replace("</Types>", '<Override PartName="/docProps/custom.xml" ContentType="application/vnd.openxmlformats-officedocument.custom-properties+xml"/></Types>').encode("utf-8")
        elif name == "_rels/.rels":
            data = data.decode("utf-8").replace("</Relationships>", '<Relationship Id="rId90" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties" Target="docProps/custom.xml"/></Relationships>').encode("utf-8")
        result.append((name, data))
    result.append(("docProps/custom.xml", CUSTOM.encode("utf-8")))
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in result:
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))   # the same bytes every time
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    print("Wrote", out)


if __name__ == "__main__":
    main()
