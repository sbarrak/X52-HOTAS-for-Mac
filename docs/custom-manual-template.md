# Your own manual template

**Export Current Template...** (the menu next to the profile picker) fills a Word file with your profile. The app ships with a
standard template, the layout of the standard aircraft maps. If you want a different look (your own fonts, colours,
logo, wording, a shorter or longer manual) you make a template of your own in Word and tell the app to use it. You
never need the command line.

A template is an ordinary Word file (`.docx`). Wherever the manual should show something from your profile, the
template has a **content control** with a **tag**. The app replaces each control with the profile's text, keeping the font
and size you gave it. Everything else in the file is yours to change.

## 1. Get a copy of the standard template

1. In the app, open the menu next to the profile picker (the circle with the three dots).
2. Choose **Save a Built-In Template Blank Copy...** and pick a place to save it.

(The same file is `templates/X52-Manual-Template.docx` in the project.)

## 2. Open it in Word and show the Developer tab

1. Open your copy in Word.
2. Show the **Developer** tab: Word > Settings > Ribbon & Toolbar > tick **Developer** (on Windows: File > Options >
   Customize Ribbon > Developer).
3. On the Developer tab turn on **Design Mode**. Every spot the app fills now shows as a control with its name, such as
   `profile.name` or `stick.a_tap.m1`.

You can change anything that is not a control: the title, the pictures, fonts, colours, margins, page order, the
wording of headings and notes. Format a control's text the way you want the value to look (font, size, bold).

## 3. Show a value from the profile

1. Put the cursor where the value goes.
2. Developer tab > **Plain Text Content Control**.
3. Select the control, click **Properties**, and type the tag in the **Tag** box (the **Title** can be anything; it is
   only a label for you).
4. Format the text of the control.

Some tags (the full list is in [manual-data.md](manual-data.md)):

| Tag | Shows |
|---|---|
| `profile.name` | the profile's name |
| `profile.file` | the profile's file name |
| `mode.1.name`, `mode.2.name`, `mode.3.name` | the names of the three modes |
| `stick.a_tap.m1` | what a tap on button A does in mode 1 (`m2`, `m3` for the other modes) |
| `throttle.mw_forward.m2` | what the scroll wheel forward does in mode 2 |
| `pinkie_stick.h2_up.m3` | what hat 2 up does in mode 3 with the Pinkie held |
| `light.a.m1` | the colour of button A's light in mode 1, as a picture |
| `alarm.1.name`, `alarm.1.m1`, `alarm.1.rate` | the first alarm: its name, its colour in mode 1, its rate |

A table cell is just a paragraph with a control in it. To add a row for another control, copy the cell controls of a
similar row and change the tags.

## 4. Repeat a table row for every entry of a list

For tables with a changing number of rows (the live lights, the user's own screen pages, or a plain list of all the
buttons):

1. Make one row, with a content control in each cell.
2. Select the **whole row**, insert a **Rich Text Content Control** around it, and give it the tag `repeat:` followed by
   the list name, such as `repeat:rules`. The controls inside use the row's tags (`rule.light`, `rule.condition`, ...).
3. The lists are in [manual-data.md](manual-data.md): `rules`, `user_pages`, `stick_buttons`, `throttle_buttons`.

Add `:used_plain` or `:used_pinkie` to leave out the buttons with nothing bound, for example
`repeat:stick_buttons:used_plain`.

## 5. Light pictures

Put a Plain Text control with a light tag (`light.a.m1`, `alarm.2.m3`, `rule.active`) in a table cell. The app puts the
right light picture in it (steady, flashing, two colours, the mode's own colour, or off); you do not type anything.
Centre the paragraph and use normal line spacing so the row grows to fit the picture. The pictures come with the app;
you cannot replace them from the template.

## 6. The aircraft picture

The standard template has a grey "AIRCRAFT IMAGE" box in the page header. Double-click the header, right-click the
picture and choose **Change Picture**, then pick a picture of your aircraft. The app keeps it as it is.

## 7. Use your template

1. Menu next to the profile picker > **Choose manual template...**, and pick your `.docx` file. The app remembers it.
2. Menu > **Export Current Template...** and save the result. Open it in Word. For a PDF use File > Save as PDF there.
3. To go back to the standard template: menu > **Use the built-in manual template**.

If your template uses a tag the app does not know (a typo, or a field from a newer version), the exported file has an
empty spot there and the app lists the unknown tags after the export.

## If something goes wrong

- **A spot is empty in the export:** the tag is misspelled, or the control has no tag. Check the Tag in **Properties**.
  Tags are written in lower case with dots and underscores, exactly as in [manual-data.md](manual-data.md).
- **"This template needs a newer version of the app":** the template says it needs newer data than this app has. Update the
  app, or use a template made for this version.
- **Word cannot open the exported file:** this should never happen; please report it with the template. You can check a
  file with `python3 tools/check_docx.py file.docx` (it needs only Python).
- **Keep the file a `.docx`.** Do not save the template as `.dotx`, `.doc` or `.pages`; the app only reads `.docx`.
- **Pictures or fonts differ from what you expect:** the export keeps your template's fonts. If a font is missing on
  another computer, Word replaces it there, as for any Word file.
