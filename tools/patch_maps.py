#!/usr/bin/env python3
"""Patches text in the printed button maps (PDF and Word) in place.

The maps in docs/mapping/ and my_profiles/out/mapping/ come from a generator that is not in this repository, so a
change to their wording is made on the files themselves.

Usage:   python3 tools/patch_maps.py <folder> [<folder> ...]      (the rules are in RULES below; edit them first)
Needs:   pip install pypdf reportlab        (use a virtual environment outside the repository)
Checks:  look at a rendered page afterwards, and back the folders up first: the files are overwritten.

How it works
- PDF: the page text is plain strings in the content stream (Tj / TJ), drawn with a subset of the font. A rule's
  `find` text is replaced by `replace` inside that string. The replacement may only use letters the subset already has
  (a shortened sentence, or words that appear elsewhere on the page). Text that needs other letters goes in `append`:
  it is drawn in Helvetica right after the shortened text, same size and colour. The old text is removed, not covered,
  so searching the PDF finds only the new wording.
- Word (.docx): the same `find` -> `replace + append` as a plain replacement in word/document.xml.
- A rule that finds nothing in a file stops the run for that file (the file is not written), so running the tool
  twice on the same folder is safe.
"""
import glob
import io
import os
import sys
import zipfile

import pypdf
from pypdf.generic import ArrayObject, ContentStream, NameObject, TextStringObject
from reportlab.pdfbase.pdfmetrics import stringWidth
from reportlab.pdfgen import canvas

# The change made for the dials (1.3.0): the plugin drives them too (E: zoom; i: speed brake where the aircraft has
# one). Earlier rules (POV 1 and the mouse nub, 1.2.6 / 1.2.7) are already in the files. `pages` is how many times each
# rule must match in a PDF (the three mode pages) and in a Word file.
# Maps with other footer wording were patched one wording at a time with a rule each, no longer than the original so
# the text after it is not overlapped:
#   ultralight, glider: "The plugin drives the stick and throttle; the slider, rotaries stay in X-Plane."
#                    -> "The plugin drives the stick, throttle, rotaries; the slider stays in X-Plane."
#   helicopters:     "The plugin drives the stick; the throttle, slider, rotaries stay in X-Plane."
#                    -> "Plugin: stick, rotaries; throttle, slider stay in X-Plane."
#   my_profiles:     "Stick and throttle axes, rotaries, slider stay in X-Plane."
#                    -> "Stick, throttle, slider stay in X-Plane; plugin: rotaries."
RULES = [
    dict(find="throttle and slider; rotaries stay in X-Plane.", replace="throttle, slider and rotaries.", append="", pages=3),
]


def _text_of(op, operands):
    if op == b"TJ":
        return str(operands[0][0]) if operands and isinstance(operands[0], list) and len(operands[0]) == 1 else None
    return str(operands[0]) if operands else None


def patch_pdf(path):
    w = pypdf.PdfWriter(clone_from=path)
    hits = [0] * len(RULES)
    for pg in w.pages:
        cs = ContentStream(pg.get_contents(), w)
        ops = cs.operations
        color, size, xy = (0, 0, 0), 8.0, (0.0, 0.0)
        overlay, changed = [], False
        for i, (o, op) in enumerate(ops):
            if op == b"rg":
                color = tuple(float(v) for v in o)
            elif op == b"Tf":
                size = float(o[1])
            elif op in (b"TD", b"Td"):
                xy = (float(o[0]), float(o[1]))
            elif op == b"Tm":
                xy = (float(o[4]), float(o[5]))
            elif op in (b"Tj", b"TJ"):
                s = _text_of(op, o)
                if s is None:
                    continue
                for r, rule in enumerate(RULES):
                    if rule["find"] in s:
                        s = s.replace(rule["find"], rule["replace"])
                        ops[i] = ([ArrayObject([TextStringObject(s)])] if op == b"TJ" else [TextStringObject(s)], op)
                        if rule["append"]:
                            overlay.append((xy[0] + stringWidth(s, "Helvetica", size), xy[1], size, color, rule["append"]))
                        hits[r] += 1
                        changed = True
        if changed:
            cs.operations = ops
            pg[NameObject("/Contents")] = cs
        if overlay:
            buf = io.BytesIO()
            c = canvas.Canvas(buf, pagesize=(float(pg.mediabox.width), float(pg.mediabox.height)))
            for x, y, sz, col, text in overlay:
                c.setFillColorRGB(*col)
                c.setFont("Helvetica", sz)
                c.drawString(x, y, text)
            c.save()
            buf.seek(0)
            pg.merge_page(pypdf.PdfReader(buf).pages[0])
    for r, rule in enumerate(RULES):
        assert hits[r] == rule["pages"], (path, rule["find"], hits[r])
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        w.write(f)
    os.replace(tmp, path)


def patch_docx(path):
    zin = zipfile.ZipFile(path)
    items = [(i, zin.read(i.filename)) for i in zin.infolist()]
    zin.close()
    out = []
    for i, data in items:
        if i.filename == "word/document.xml":
            t = data.decode("utf8")
            for rule in RULES:
                n = t.count(rule["find"])
                assert n == rule["pages"], (path, rule["find"], n)
                t = t.replace(rule["find"], rule["replace"] + rule["append"])
            data = t.encode("utf8")
        out.append((i, data))
    tmp = path + ".tmp"
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as z:
        for i, d in out:
            z.writestr(i, d)
    os.replace(tmp, path)


if __name__ == "__main__":
    for d in sys.argv[1:]:
        for f in sorted(glob.glob(os.path.join(d, "*.pdf"))):
            try:
                patch_pdf(f)
                print("pdf ", os.path.basename(f))
            except AssertionError as e:
                print("skipped (no match):", os.path.basename(f), e)
        for f in sorted(glob.glob(os.path.join(d, "*.docx"))):
            try:
                patch_docx(f)
                print("docx", os.path.basename(f))
            except AssertionError as e:
                print("skipped (no match):", os.path.basename(f), e)
