# Standard aircraft map update report

> Record of the 1.7.1 button maps (#133), written when they were made. The backup, evidence and review folders it mentions were local only and are not in this repository. The version it cites (1.6.7) is the one at the time of the work.

Completed 27 November 2026. Updated 18 nonmilitary aircraft maps: 18 DOCX files and 18 PDFs, covering 21 aircraft profile variants. All final maps have seven pages; 126 pages rendered and visually reviewed. Original photographs, control illustrations, markers, source links and page order were retained. The Stinson photographs were reduced proportionally to keep its legend and sources on the systems page.

9,566 check rows matched; 0 unmatched. Every final DOCX passed tools/check_docx.py. PDFs were exported directly from the final DOCX with bundled LibreOffice, as authorized, without opening Word or printing. Extracted PDF body text matches the DOCX in both directions; there are no extra or missing words. The approved Boeing pilot was retained byte for byte.

| Aircraft map | Profile used | Checked | Matched | Unmatched | Pages rendered and reviewed | Open mapping questions |
|---|---|---:|---:|---:|---:|---|
| Aero-Works Aerolite 103 | aircraft-Aerolite_103.x52 | 438 | 438 | 0 | 7 | None |
| Airbus A330-300 | aircraft-A330.x52 | 542 | 542 | 0 | 7 | None |
| BETA Technologies Alia-250 | aircraft-ALIA-250.x52 | 494 | 494 | 0 | 7 | None |
| Beechcraft Baron 58 | aircraft-Baron_58.x52 | 526 | 526 | 0 | 7 | None |
| Beechcraft King Air C90B | aircraft-C90B.x52 | 536 | 536 | 0 | 7 | None |
| Boeing 737-800 | aircraft-b738.x52 | 515 | 515 | 0 | 7 | None |
| Cessna 172 SP Skyhawk | aircraft-Cessna_172SP.x52; aircraft-Cessna_172SP_G1000.x52; aircraft-Cessna_172SP_seaplane.x52 | 872 | 872 | 0 | 7 | None |
| Cessna Citation X | aircraft-Cessna_CitationX.x52 | 542 | 542 | 0 | 7 | None |
| Cirrus SR22 | aircraft-Cirrus SR22.x52 | 511 | 511 | 0 | 7 | None |
| Cirrus Vision SF50 | aircraft-CirrusSF50.x52 | 513 | 513 | 0 | 7 | None |
| Lancair Evolution | aircraft-N844X.x52 | 506 | 506 | 0 | 7 | None |
| McDonnell Douglas MD-82 | aircraft-MD80.x52 | 537 | 537 | 0 | 7 | None |
| Piper PA-18 Super Cub | aircraft-PA-18-150.x52 | 487 | 487 | 0 | 7 | None |
| Robinson R22 Beta II | aircraft-Robinson R22 Beta II.x52; aircraft-Robinson R22 Beta II_mariner.x52 | 597 | 597 | 0 | 7 | None |
| Schleicher ASK 21 | aircraft-ASK21.x52 | 420 | 420 | 0 | 7 | None |
| Sikorsky S-76C | aircraft-S-76C.x52 | 528 | 528 | 0 | 7 | None |
| Stinson L-5 Sentinel | aircraft-L5_Sentinel.x52 | 491 | 491 | 0 | 7 | None |
| Van's RV-10 | aircraft-RV-10.x52 | 511 | 511 | 0 | 7 | None |

## Scope and shared profiles

Military F-4 and F-14 maps and all personal profiles were excluded. default.x52 is a standard fallback without an existing aircraft map; no new fallback map was invented.

The three Skyhawk variants share one map. The G1000 adds AP FLC on T3 up in Mode 2 and Pinkie layer 5; both cells and the hold/release notes identify the G1000-only action. The analog and seaplane bindings otherwise match. The R22 Beta II and Mariner bindings, settings, LEDs, alarms and screens match except for the profile name. Each additional variant has separate parsed binding, setting, light, rule, alarm and screen check rows in the shared map report.

The checks cover normal and Pinkie layers, Any bindings, separate press/hold/release actions, ordered sequences and waits, relevant settings and runtime defaults, mode names, controls, LEDs and live rules, alarm symbols and rates, page order and fields, systems references and reverse document coverage. Raw profile lines, commands, waits and target DOCX locations are retained in the CSV/JSON evidence. Sequence prose and aircraft-system classification were reviewed separately; the check script does not independently prove the physical effect of an aircraft custom command.

## Source limitations retained

Zero unmatched means that the documented mappings match the checked profiles and runtime; it is not a simulator flight-test result. Aircraft light locations and equipment facts retain the cited manual/model evidence and its uncertainties. The Alia, Aerolite and S-76 lack a complete installed aircraft operating manual in the reviewed material. The Stinson lighting inventory distinguishes its historical recognition-light installation from the installed simulator beacon. Generic brake and parking-brake commands do not establish physical wheel equipment on skid helicopters.

Inclusive conditional ranges that meet at 0.5 are documented as running both matching branches in order. No profile logic was changed to conceal this behavior. Citation X Recognition, C90B Recognition and SR22 Wing inspection are explicitly listed with their current Pinkie assignments. SR22 taxi lighting is identified as sharing the landing circuit.

## Files and protection

Original maps are backed up in the parent backup folder. Per-aircraft evidence, scripts, hashes and reviewed page PNGs are in this review-standard folder; the Boeing pilot evidence is in ../review-737.

The version file advanced separately during the work and currently reads 1.6.7. All protected profile, core runtime, generator and template hashes remain unchanged from the review snapshot. This task made no branch changes, commits, pushes or pull requests. Out-of-scope military maps and the blank mapping template retain their original hashes.
