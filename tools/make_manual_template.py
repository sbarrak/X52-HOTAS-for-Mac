#!/usr/bin/env python3
"""Makes the first version of the manual template: templates/X52-Manual-Template.docx.

Standard library only (no packages). Run it once to start a template; after that the .docx is the file you edit in
Word and this script is not needed again. Every spot the app fills is a Word content control with a Tag (Developer
tab > Properties); the tags are listed in docs/manual-data.md.

    python3 tools/make_manual_template.py [output.docx]
"""
import sys
import zipfile
from xml.sax.saxutils import escape

NS = 'xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" ' \
     'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
SCHEMA = 1   # the lowest manual data schema this template needs (docs/manual-data.md)

_ids = [1000]


def next_id():
    _ids[0] += 1
    return _ids[0]


def run(text, size=None, bold=False, color=None):
    props = ""
    if bold:
        props += "<w:b/>"
    if color:
        props += '<w:color w:val="%s"/>' % color
    if size:
        props += '<w:sz w:val="%d"/><w:szCs w:val="%d"/>' % (size, size)
    return '<w:r>%s<w:t xml:space="preserve">%s</w:t></w:r>' % ("<w:rPr>%s</w:rPr>" % props if props else "", escape(text))


def control(tag, size=None, bold=False, alias=None):
    """An inline content control the app replaces with text."""
    return ('<w:sdt><w:sdtPr><w:alias w:val="%s"/><w:tag w:val="%s"/><w:id w:val="%d"/><w:text/></w:sdtPr>'
            '<w:sdtContent>%s</w:sdtContent></w:sdt>') % (escape(alias or tag), tag, next_id(), run("[%s]" % tag, size, bold))


def para(inner, style=None, jc=None, keep=False):
    ppr = ""
    if style:
        ppr += '<w:pStyle w:val="%s"/>' % style
    if keep:
        ppr += "<w:keepNext/>"
    if jc:
        ppr += '<w:jc w:val="%s"/>' % jc
    return "<w:p>%s%s</w:p>" % ("<w:pPr>%s</w:pPr>" % ppr if ppr else "", inner)


def cell(inner_para, width, shade=None):
    tcpr = '<w:tcW w:w="%d" w:type="dxa"/>' % width
    if shade:
        tcpr += '<w:shd w:val="clear" w:color="auto" w:fill="%s"/>' % shade
    return "<w:tc><w:tcPr>%s</w:tcPr>%s</w:tc>" % (tcpr, inner_para)


BORDERS = "".join('<w:%s w:val="single" w:sz="4" w:space="0" w:color="A6A6A6"/>' % s for s in ("top", "left", "bottom", "right", "insideH", "insideV"))
WIDTHS = [2600, 2320, 2320, 2320]   # button name, then one column per mode (A4 text width with 2 cm margins is 9560)


def table(header_cells, row_tags, list_name):
    grid = "".join('<w:gridCol w:w="%d"/>' % w for w in WIDTHS)
    head = '<w:tr><w:trPr><w:cantSplit/><w:tblHeader/></w:trPr>%s</w:tr>' % "".join(
        cell(para(c, keep=True), w, "D9E2F3") for c, w in zip(header_cells, WIDTHS))
    body_cells = "".join(cell(para(control(t, 16, alias=t)), w) for t, w in zip(row_tags, WIDTHS))
    body = ('<w:sdt><w:sdtPr><w:alias w:val="Rows: %s"/><w:tag w:val="repeat:%s"/><w:id w:val="%d"/></w:sdtPr>'
            '<w:sdtContent><w:tr><w:trPr><w:cantSplit/></w:trPr>%s</w:tr></w:sdtContent></w:sdt>') % (list_name, list_name, next_id(), body_cells)
    return ('<w:tbl><w:tblPr><w:tblStyle w:val="TableGrid"/><w:tblW w:w="%d" w:type="dxa"/><w:tblBorders>%s</w:tblBorders><w:tblLayout w:type="fixed"/></w:tblPr>'
            '<w:tblGrid>%s</w:tblGrid>%s%s</w:tbl>') % (sum(WIDTHS), BORDERS, grid, head, body)


def mode_header(first, pinkie=False):
    cols = [run(first, 16, True)]
    for m in (1, 2, 3):
        c = control("mode.%d.name" % m, 16, True)
        cols.append(c + (run(" + Pinkie", 16, True) if pinkie else ""))
    return cols


def body():
    out = [para(run("X52 Pro button map"), "Title"),
           para(control("profile.name", 28, True), "Subtitle")]
    out.append(para(run("Modes: ", 18, True) + control("mode.1.name", 18) + run("  /  ", 18) +
                    control("mode.2.name", 18) + run("  /  ", 18) + control("mode.3.name", 18)))
    for title, lst in (("Stick", "stick_buttons"), ("Throttle", "throttle_buttons")):
        out.append(para(run(title), "Heading1", keep=True))
        out.append(table(mode_header("Button"), ["button.name", "button.m1", "button.m2", "button.m3"], lst))
    out.append('<w:p><w:r><w:br w:type="page"/></w:r></w:p>')
    out.append(para(run("With the Pinkie held"), "Heading1", keep=True))
    for title, lst in (("Stick", "stick_buttons"), ("Throttle", "throttle_buttons")):
        out.append(para(run(title, 20, True), keep=True))
        out.append(table(mode_header("Button", True), ["button.name", "button.p1", "button.p2", "button.p3"], lst))
    sect = ('<w:sectPr><w:footerReference w:type="default" r:id="rId2"/><w:pgSz w:w="11906" w:h="16838"/>'
            '<w:pgMar w:top="1134" w:right="1134" w:bottom="1134" w:left="1134" w:header="567" w:footer="567" w:gutter="0"/></w:sectPr>')
    return "".join(out) + sect


def footer():
    p = para(run("X52 Pro ", 16, color="808080") + control("app.version", 16) + run("  -  ", 16, color="808080") +
             control("profile.name", 16), jc="center")
    return '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:ftr %s>%s</w:ftr>' % (NS, p)


STYLES = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">'
          '<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="Arial" w:hAnsi="Arial" w:cs="Arial" w:eastAsia="Arial"/>'
          '<w:sz w:val="20"/><w:szCs w:val="20"/><w:lang w:val="en-GB"/></w:rPr></w:rPrDefault>'
          '<w:pPrDefault><w:pPr><w:spacing w:after="80" w:line="259" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults>'
          '<w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:qFormat/><w:rPr><w:rFonts w:ascii="Arial" w:hAnsi="Arial" w:cs="Arial" w:eastAsia="Arial"/></w:rPr></w:style>'
          '<w:style w:type="paragraph" w:styleId="Title"><w:name w:val="Title"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/>'
          '<w:pPr><w:spacing w:after="0"/></w:pPr><w:rPr><w:b/><w:sz w:val="48"/><w:szCs w:val="48"/></w:rPr></w:style>'
          '<w:style w:type="paragraph" w:styleId="Subtitle"><w:name w:val="Subtitle"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/>'
          '<w:rPr><w:color w:val="404040"/><w:sz w:val="28"/><w:szCs w:val="28"/></w:rPr></w:style>'
          '<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:basedOn w:val="Normal"/><w:next w:val="Normal"/><w:qFormat/>'
          '<w:pPr><w:keepNext/><w:spacing w:before="240" w:after="80"/><w:outlineLvl w:val="0"/></w:pPr><w:rPr><w:b/><w:sz w:val="28"/><w:szCs w:val="28"/></w:rPr></w:style>'
          '<w:style w:type="table" w:default="1" w:styleId="TableNormal"><w:name w:val="Normal Table"/><w:uiPriority w:val="99"/><w:semiHidden/>'
          '<w:tblPr><w:tblInd w:w="0" w:type="dxa"/><w:tblCellMar><w:top w:w="0" w:type="dxa"/><w:left w:w="108" w:type="dxa"/>'
          '<w:bottom w:w="0" w:type="dxa"/><w:right w:w="108" w:type="dxa"/></w:tblCellMar></w:tblPr></w:style>'
          '<w:style w:type="table" w:styleId="TableGrid"><w:name w:val="Table Grid"/><w:basedOn w:val="TableNormal"/><w:uiPriority w:val="39"/>'
          '<w:pPr><w:spacing w:after="0" w:line="240" w:lineRule="auto"/></w:pPr><w:tblPr><w:tblBorders>'
          + "".join('<w:%s w:val="single" w:sz="4" w:space="0" w:color="A6A6A6"/>' % s for s in ("top", "left", "bottom", "right", "insideH", "insideV"))
          + '</w:tblBorders></w:tblPr></w:style></w:styles>')

CONTENT_TYPES = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                 '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                 '<Default Extension="xml" ContentType="application/xml"/>'
                 '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
                 '<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>'
                 '<Override PartName="/word/footer1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml"/>'
                 '<Override PartName="/docProps/custom.xml" ContentType="application/vnd.openxmlformats-officedocument.custom-properties+xml"/>'
                 '</Types>')

ROOT_RELS = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
             '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
             '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties" Target="docProps/custom.xml"/>'
             '</Relationships>')

DOC_RELS = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>'
            '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer" Target="footer1.xml"/>'
            '</Relationships>')

CUSTOM = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/custom-properties" '
          'xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">'
          '<property fmtid="{D5CDD505-2E9C-101B-9397-08002B2CF9AE}" pid="2" name="X52TemplateSchema"><vt:i4>%d</vt:i4></property></Properties>' % SCHEMA)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "templates/X52-Manual-Template.docx"
    document = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document %s><w:body>%s</w:body></w:document>' % (NS, body())
    parts = [("[Content_Types].xml", CONTENT_TYPES), ("_rels/.rels", ROOT_RELS), ("word/document.xml", document),
             ("word/_rels/document.xml.rels", DOC_RELS), ("word/styles.xml", STYLES), ("word/footer1.xml", footer()),
             ("docProps/custom.xml", CUSTOM)]
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in parts:
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))   # the same bytes every time
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    print("Wrote", out)


if __name__ == "__main__":
    main()
