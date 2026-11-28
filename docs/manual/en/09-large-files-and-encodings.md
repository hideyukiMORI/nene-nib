# Large files and encodings

NeNe Nib can open large files such as logs and data. You can open UTF-8 files up to 1 GiB, and you can keep writing in other tabs while a file loads. Besides UTF-8, Nib handles the Shift_JIS and UTF-16 encodings. This chapter explains how to open large files, how files with very long lines are displayed, and how to choose the encoding and line ending.

> Nib detects the encoding of a file automatically when you open it. There is no step for choosing an encoding when you open a file. The encoding you choose in the status bar is the encoding used when you save.

## Open large files

Open a large file the same way as any other file: with `Ctrl+O` (`Ctrl+Shift+O` in Vim mode) or by drag and drop.

| File size | What happens |
|---|---|
| Up to 64 MiB | The file is loaded and opened right away. |
| Over 64 MiB, up to 1 GiB (UTF-8) | The file is loaded in the background. The status bar shows the progress, such as "Loading: filename 37%", and the tab opens when loading is done. You can keep writing in other tabs in the meantime. |
| Over 64 MiB (Shift_JIS, UTF-16) | The file cannot be opened. Nib shows "Shift_JIS / UTF-16 files larger than 64 MiB cannot be opened." |
| Over 1 GiB | The file cannot be opened. Nib shows "Files larger than 1 GB cannot be opened." (meaning 1 GiB). |

MiB and GiB are sizes based on units of 1,024 (1 MiB is 1,024 × 1,024 bytes).

- To stop loading partway, close the window. `Esc` does not stop it.
- If you open the same file again while it is loading, loading does not start over. It continues.

With large files, these operations take time.

- While saving, and while searching the whole document, the window stops responding until the operation is done.
- Documents larger than 64 MiB are not autosaved every 30 seconds (autosave copies unsaved changes to another place in case Nib quits unexpectedly). For such a document, "Documents larger than 64 MiB are not autosaved" appears once at the bottom of the window. Save often with `Ctrl+S`.

## Open files with very long lines

Some files have a single line of more than 16,384 characters with no line breaks, such as logs written on one line or JSON with the line breaks removed. When you open such a file, Nib first asks "This file has a very long line with no line breaks. Do you want to open it in grid view?"

<!-- Screenshot: the "This file has a very long line with no line breaks. Do you want to open it in grid view?" dialog box (to be added) -->

| Answer | What happens |
|---|---|
| **Yes** | The file opens in grid view. Grid view arranges characters in cells of equal width and uses a monospaced font. Long lines also wrap at the window width. `grid` appears on the right side of the status bar. |
| **No** | The file opens in the normal view. Only the long lines are not wrapped, and a mark appears next to their line numbers. Only the area around the caret is laid out and displayed, so even long lines do not slow things down. |

- You are asked only once for each document.
- To change the view after opening the file, press `Ctrl+P`, type `:`, and choose `set display=grid` (grid view) or `set display=text` (normal view). In Vim mode, you can also type a command such as `:set display=grid` in NORMAL.
- You can use grid view for documents without long lines too. It is useful when you want to see characters lined up at the same width.
- Long lines do not get syntax coloring.

## Turn word wrap on or off

By default, long lines wrap at the window width. How to turn wrapping off and back on is explained in the [Themes and appearance](07-themes.md) chapter.

## Understand encodings

An encoding is the rule for turning characters into the bytes of a file. Nib handles these encodings.

| Encoding | Shown in the status bar as | Description |
|---|---|---|
| UTF-8 | `UTF-8` | New documents are saved in this format. |
| UTF-8 (with BOM) | `UTF-8 BOM` | UTF-8 with a BOM (a mark of a few bytes that indicates the encoding) at the start. |
| Shift_JIS | `Shift_JIS` | A Japanese encoding. Nib handles the extended form used in Windows (CP932). |
| UTF-16 (little-endian) | `UTF-16 LE` | A format that represents each character with 2 bytes (or 4 bytes). Little-endian is the byte order that puts the lower byte of the 2 bytes first. Only files with a BOM are supported. |
| UTF-16 (big-endian) | `UTF-16 BE` | The byte order is the reverse of UTF-16 LE: the higher byte comes first. Only files with a BOM are supported. |

### Check how the encoding is detected

Nib detects the encoding of a file you open in this order.

1. If the file starts with a UTF-16 BOM, it opens as UTF-16.
2. If the file starts with a UTF-8 BOM, it opens as UTF-8 (with BOM).
3. If it can be read as valid UTF-8, it opens as UTF-8.
4. If it can be read as Shift_JIS, it opens as Shift_JIS.
5. Otherwise, Nib shows "The encoding could not be detected." and does not open the file.

- UTF-32 files cannot be opened. Nib shows "UTF-32 files are not supported. Save the file again as UTF-8 or UTF-16."
- Encodings that are not in the table above, such as EUC-JP and JIS (ISO-2022-JP), are not supported.
  - An EUC-JP file can usually be read as Shift_JIS, so it opens as Shift_JIS and the text is garbled. If part of it cannot be read as Shift_JIS, Nib shows "The encoding could not be detected." and does not open it.
  - A JIS file consists only of bytes in the same range as ASCII letters and digits, so it opens as UTF-8 and the text is garbled.
- A file you open is saved in the encoding it had when you opened it.

### Change the encoding used for saving

| To do this | Do this | What happens |
|---|---|---|
| Choose an encoding | Click the encoding (such as `UTF-8`) on the right side of the status bar | You can choose the encoding to save with from a list. |
| Do the same from **All Commands** | **Choose Encoding** in **All Commands** (`F1`) | The same list appears. |
| Do the same in Vim mode | `:set fenc=cp932` and so on | Choose from `utf-8`, `utf-8-bom`, `cp932` (Shift_JIS), `utf-16le`, and `utf-16be`. |

<!-- Screenshot: the list that appears when you click the encoding in the status bar (to be added) -->

- Choosing an encoding does not change the text on the screen. It changes the format used the next time you save.
- When you choose one, "● " appears in front of the tab name, and it disappears when you save. If you have not changed the text, it also disappears when you choose the original encoding again.
- Choosing an encoding cannot be undone with `Ctrl+Z`. To go back, choose the original encoding again.
- To add a BOM, choose `UTF-8 BOM` and save. To remove it, choose `UTF-8` and save.

Some characters, such as emoji, cannot be written in Shift_JIS. If you try to save a document that contains such characters in Shift_JIS, Nib asks "Some characters cannot be saved in this encoding. Do you want to save as UTF-8?" Choose **Yes** to save as UTF-8, or **No** to cancel saving.

## Change line endings

There are two line ending formats: `CRLF`, the standard on Windows, and `LF`, the standard on macOS and Linux.

- A file you open is saved with the line ending format it had when you opened it.
- New documents are saved with `CRLF`.
- To change the format, click the line ending (`CRLF` or `LF`) on the right side of the status bar, or choose **Choose Line Ending** in **All Commands** (`F1`). In Vim mode, you can also change it with `:set ff=dos` (`CRLF`) or `:set ff=unix` (`LF`).
- As with the encoding, choosing a format does not change the text on the screen. The line ending format changes when you save. It cannot be undone with `Ctrl+Z`.

## Going further: Compare how fast files open

These are the results of comparing NeNe Nib and Notepad (Notepad 11.2609 on Windows 11) under the same conditions on the developer's PC. The version measured was NeNe Nib 1.1.0, not the current 1.2.0.

| Measurement | NeNe Nib 1.1.0 | Notepad |
|---|---:|---:|
| From start until the text of a 200-line file appears | 100 ms | 450 ms |
| From opening a 16 MiB file until its text appears | 117 ms | 908 ms |
| Memory after opening a 16 MiB file | 30.4 MiB | 88.7 MiB |

ms means milliseconds (thousandths of a second). For a 16 MiB file, Nib opens it in about one eighth of the time Notepad takes and uses about one third of the memory.

The numbers are the median of 20 runs (10 runs for memory), measured on one PC with a Core i9-10850K, an RTX 3090, and Windows 11, with default settings and the same window size.

## Going further: Check size limits and encoding details

With large files, these features have limits.

| Feature | Limit | When the limit is exceeded |
|---|---|---|
| Opening (UTF-8, UTF-8 BOM) | 1 GiB | The file cannot be opened. |
| Opening (Shift_JIS, UTF-16) | 64 MiB | The file cannot be opened. |
| Size at which loading moves to the background | Over 64 MiB | The status bar shows the progress. |
| Autosave | 64 MiB | The document is not autosaved. |
| Printing | 16 MiB | Nib shows "Printing is limited to 16 MiB". |
| Preview in **Files and Tabs** | 16 MiB | Nib shows "Too large to preview (up to 16 MiB)". |
| Line length (asks about grid view) | 16,384 characters | When you open the file, Nib asks whether to use grid view. |

Encodings follow these detailed rules.

- A file that has a UTF-8 BOM but is not valid UTF-8 after it is not read again as Shift_JIS. Nib shows "The encoding could not be detected." and does not open it.
- Project search (`Ctrl+Shift+F`) does not search lines that are not valid UTF-8 (such as lines in Shift_JIS files). It also does not search files larger than 16 MiB.
- Language servers handle only UTF-8 files without a BOM. See the [Language servers](06-language-servers.md) chapter for details.
- The line ending format is decided by the first line break in the file. If there is a CR before the first line break, the format is `CRLF`; otherwise, it is `LF`. A file with no line breaks is treated as `CRLF`. If other kinds of line breaks are mixed in later, they are saved as the same bytes unless you choose the line ending format again.

Next: [Troubleshooting](10-troubleshooting.md) covers common problems and how to fix them.
