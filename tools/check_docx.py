#!/usr/bin/env python3
"""Checks Word files for what makes Word refuse them: XML that is not well-formed (such as an undeclared prefix),
control characters, duplicate picture ids, missing relationship targets. Standard library only.

    python3 tools/check_docx.py file.docx [more.docx ...]
"""
import sys, zipfile, re, xml.dom.minidom as m
bad=0
for f in sys.argv[1:]:
    z=zipfile.ZipFile(f)
    print("==",f.split('/')[-1], len(z.namelist()),"parts")
    for n in z.namelist():
        data=z.read(n)
        if n.endswith(('.xml','.rels')):
            try: m.parseString(data)
            except Exception as e: print("  NOT WELL-FORMED:",n,e); bad+=1
            if re.search(rb'[\x00-\x08\x0b\x0c\x0e-\x1f]',data): print("  ILLEGAL CONTROL CHARS in",n); bad+=1
    d=z.read("word/document.xml").decode()
    ids=re.findall(r'<wp:docPr id="(\d+)"',d)
    if len(ids)!=len(set(ids)): print("  DUPLICATE docPr ids:", len(ids), len(set(ids))); bad+=1
    rels=z.read("word/_rels/document.xml.rels").decode()
    relids=re.findall(r'Id="([^"]+)"',rels)
    if len(relids)!=len(set(relids)): print("  DUPLICATE rel ids"); bad+=1
    for rid in set(re.findall(r'r:(?:embed|id)="([^"]+)"',d)):
        if rid not in relids: print("  MISSING REL",rid); bad+=1
    for t in re.findall(r'Target="([^"]+)"',rels):
        if not t.startswith(("http","/")) and __import__("posixpath").normpath("word/"+t) not in z.namelist(): print("  MISSING TARGET",t); bad+=1
    print("  sdt left:",d.count("<w:sdt"), " drawings:",d.count("<w:drawing>"))
print("problems:",bad)
