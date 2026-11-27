# Boeing 737-800 pilot check report — 27 November 2026

> Record of the pilot map for the 1.7.1 button maps (#133, #137), written before the other maps were made. The backup, evidence files and scripts it mentions were local only and are not in this repository. The version it cites (1.6.5) and its pending items are as they were at that time; the other civil maps are covered in `Map-update-report.md`.

Updated only `Boeing 737-800 - X52 Pro map.docx` and `.pdf` in `docs/mapping/`.
Current local maps were the layout base. No checkout, restore, patch_maps, source edit,
profile edit, template edit, branch, commit, push, issue or pull request was performed.
Main and working VERSION were confirmed as 1.6.5. Original map files were backed up
in the parent folder before editing. All other maps retain their starting hashes.

| Check | Result |
|---|---|
| Profile | `profiles/aircraft-b738.x52` |
| Profile SHA-256 | `34501f0722d2b8116083f69b1283cbfaadf6865e7716ba4084ded59710b945fa` |
| Installed profile vs repository | Identical bytes |
| Independent check rows | 515 checked / 515 matched / 0 unmatched |
| Raw binding/trigger rows | 120; each retains button, layer, trigger, label, ordered steps and waits |
| Reverse assignment cells | 207 checked, including blanks and compact directional groups |
| Relevant explicit settings / runtime defaults | 30 / 5 |
| Normal and Pinkie light cells | 66 |
| Live rules / alarms | 3 / 8 |
| Screen page-order cells / custom pages / fields | 36 / 3 / 14 |
| Power conditions / systems references | 2 / 21 |
| DOCX structure | `tools/check_docx.py`: problems 0 |
| PDF export | Directly from final DOCX using headless LibreOffice, per revised preference |
| Editable body paragraphs absent from PDF | 0 |
| Reverse PDF word-count differences | 0 extra; 0 missing (recurring headers/footers excluded) |
| Pages rendered and visually inspected | 7 of 7, at 1800-pixel width |
| Pagination | No orphan-only page; no split table rows, isolated headings or visible clipping |
| Native manual reference | Generated after successful `make`, and reviewed; not used to replace the map |
| Make | Exit 0; existing missing-field initializer compiler warnings were emitted |

## Content changes

- Plain modes retain the condensed illustrated first-page table.
- Pinkie layers 4/5/6 have their own full table. Any bindings were checked in all effective layers.
- Tap versus timed hold (400 ms), immediate held commands and release behavior are distinguished.
- Gear, battery, landing-light, autothrottle and runway-turnoff sequences list conditions and waits in English.
- Mode-settle/banner, wheel acceleration and alarm cycle times are stated.
- Position/strobe selector and taxi-brightness steps follow the installed aircraft scripts.
- Mode lights, Pinkie lights, live rules, alarms, screen pages and systems-page control references were checked.
- Original aircraft photos, markers, source links and overall page styling were retained.

## Source interpretation and limits

Landing-light conditions overlap at exactly 0.5, so ON and then OFF run in order at that saved value.
The document states this; the profile was not changed. The source itself is unambiguous.

Identical sequence text is grouped by its applicable modes in the document. The audit nevertheless
has one row for every raw binding/trigger. This compact grouping is a layout choice for pilot review.

Axis curve/dead-zone calibration, full rotary ranges, unused rotary-zoom steps and internal profile
metadata are listed in `check-summary.json` rather than repeated as pilot operating instructions.
This follows the request to leave configured percentages out of the maps. Active axes and inversions
are explained in operating terms. No release binding is configured; command-end behavior is separate.

Macro prose and aircraft selector behavior were reviewed against raw steps, runtime code and the
installed Lua scripts. Automated matching verifies traceability and expected text; it does not
establish that every simulator command succeeds in every aircraft state. No flight test was run.

## Inventory and pending work

24 standard profile files map to 20 existing aircraft maps plus the default fallback, which has no map.
The three Skyhawk variants and two R22 variants share identical bindings and relevant settings,
except their names. See `Inventory.md` for the complete list. Military maps are inventoried but
have not been updated in this pilot.

Only this Boeing pilot is updated. Approval is pending before continuing to other aircraft.

## Reproduce

Run from the project root using a Python with python-docx and pypdf installed:

```sh
python3 docs/mapping/originals/2026-11-27-before-map-update/review-737/audit_737.py 'docs/mapping/Boeing 737-800 - X52 Pro map.docx'
python3 tools/check_docx.py 'docs/mapping/Boeing 737-800 - X52 Pro map.docx'
```

The read-only parser is `parse_profiles.py`; `check-rows.csv` and `check-rows.json` contain the locations
and raw source for each check. `reference.docx` is temporary native manual output, not the final map.
