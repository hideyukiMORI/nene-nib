# Markdown and preview

While you write a Markdown document (a document format that uses symbols such as `#` and `**` for headings and bold), you can check how it will look in the preview. This chapter describes how to show the preview, what the preview can display, and pasting into Markdown documents.

> To close the preview, press `Ctrl+Shift+M` a few times. Each press switches in this order: right (or bottom) pane, full view, closed.

## Show the preview

| To do this | Do this | What happens |
|---|---|---|
| View it next to the text | `Ctrl+Shift+M` | The preview appears in a second pane (on the right, or at the bottom). The text stays on the left (or at the top, when the preview is at the bottom), so you can check the result as you write. |
| View it in full | Press `Ctrl+Shift+M` again while viewing side by side, or choose **Markdown Preview (Full)** in **All Commands** (`F1`) | The preview fills the whole text area. The tab strip and the status bar stay. |
| Go back to the view before full view | `Esc` | Leaves the full preview and goes back to the view you had before (see the list below the table). |
| Close the preview | Press `Ctrl+Shift+M` again while viewing in full | The preview closes, and only the text remains. Nib does not go back to the side-by-side view. |

- If you switched to full view from the side-by-side view, `Esc` takes you back to the text with the preview next to it. If you went straight to full view from **All Commands** or with `:preview full`, `Esc` closes the preview and leaves only the text.

<!-- Screenshot: the text on the left and the preview on the right (to be added) -->

- When you change the text, the preview updates shortly afterward.
- When you scroll the text, the preview follows to the same place.
- You cannot move into the preview pane with `F6` or by clicking. "Cannot move into the preview pane" appears.
- In the full preview, you can select text by dragging or with `Shift` and the arrow keys, and copy it with `Ctrl+C`. You cannot edit. If you type, "You cannot edit while previewing. Press Esc to go back" appears.
- In Vim mode, you can also switch with `:preview side` (side by side), `:preview full` (full view), and `:preview off` (close).
- The preview does not stay open between sessions. When Nib starts, the preview is always closed.

## What the preview can display

The preview follows CommonMark, the common Markdown specification. Besides headings, paragraphs, bold, italics, bulleted lists, numbered lists, quotes, code, links, and images, it can display the following.

| Syntax | Example | How it looks in the preview |
|---|---|---|
| Tables | A `\| Name \| Value \|` row and a `\|---\|---\|` separator row | A table with horizontal rules only; the header row is bold (for column alignment, see below the table). |
| Notes (GitHub alerts) | A quote that starts with `> [!NOTE]` | A colored line on the left, with the kind's name in bold on the first line. There are five kinds: `NOTE`, `TIP`, `IMPORTANT`, `WARNING`, and `CAUTION`. The names stay in English. |
| Task lists | `- [ ] To do` / `- [x] Done` | Check boxes (see [Tick check boxes](#tick-check-boxes) below). |
| Strikethrough | `~~deleted text~~` | The text is struck through. |
| Bare URLs | `https://example.com` | They become links. |
| Character references (an HTML notation that represents one character with a name enclosed in `&` and `;`) | `&copy;` and others | Every HTML named character reference becomes its character (© for `&copy;`). |
| Front matter (a section at the start of a document that holds its title, date, and so on) | From a `---` line at the start of the document to the next `---` line | Not read as headings or paragraphs of the text; shown as a single-color block. |
| Fenced code (code enclosed by `` ``` `` lines before and after) | Code with a language name after `` ``` `` | Colored with the same syntax coloring as the text. |

In the table separator row, write `:---` to align that column left, `:---:` to center it, or `---:` to align it right.

<!-- Screenshot: the preview of a table, a note, and check boxes (to be added) -->

### Colors in fenced code

The first word after `` ``` `` decides which language to color. You can use a language name (such as `python`), an extension (such as `py` or `ts`), or another name for the language (such as `c++`). Code in an unknown language, and code with no language, stays in one color.

### Images

Image files on your PC are displayed in the flow of the text.

- The supported formats are PNG, JPEG, GIF (only the first frame), and BMP.
- Images larger than the pane are shrunk to fit the pane width. Small images are never enlarged.
- Write the image location as a path relative to the document's folder, or as a path that starts with the drive.
- Images on the internet (those starting with `http` or `https`) are not downloaded. Like images that cannot be read, they are shown as `[Image: description text]`.

### Wide tables

A table that is wider than the pane has a thin bar below it. You can scroll just that table sideways in any of these ways.

- Drag the bar
- Click the bar's track
- Hold down `Shift` and turn the mouse wheel over the table
- Use the horizontal wheel (or a horizontal touchpad gesture) over the table

## Work in the preview

### Tick check boxes

Click a check box in the preview to switch `[ ]` and `[x]` in the text. This works both side by side and in full view. One `Ctrl+Z` undoes it.

In full view, you can also switch it by pressing `Space` while the caret in the text is on the task line. If there is no task on that line, "There is no task on this line" appears.

### Open links

When you click a link in the preview, it does not open right away; a small confirmation box appears just below the link. The box shows the real destination, with the server name in bold.

<!-- Screenshot: the confirmation box below a link (to be added) -->

| Press | What happens |
|---|---|
| `Enter` | Opens the link. |
| `Esc` | Closes the box without opening the link. |
| `Ctrl+C` | Copies the destination to the clipboard. |

| Destination | How it opens |
|---|---|
| Starts with `http` or `https` | Opens in the default browser. |
| Starts with `mailto:` | Opens a new message in the default mail app. |
| A text file | Opens in a new Nib tab. |
| A program, or a destination of an unknown kind | Does not open. "This link can't be opened" appears; copy the destination and check it yourself. |

When you click a link in the form `#heading` (a link to a heading in the same document), no confirmation box appears; the caret in the text moves to the line of that heading. Heading names are matched the same way as on GitHub. If the heading is not found, "No such heading in this document" appears. In Vim mode, you can go back to the position before the move with `''`.

## Pasting HTML as Markdown

When you paste text copied from a browser or a word processor into a Markdown document with `Ctrl+V`, Nib converts headings, bold, links, and bulleted lists to Markdown symbols. To paste only the text, use `Ctrl+Shift+V`. For details, see [Paste text from a web page](02-basic-editing.md#paste-text-from-a-web-page) in the [Basic editing](02-basic-editing.md) chapter.

## Syntax coloring

Coloring of the text in Markdown documents and in program files is described in the [Themes and appearance](07-themes.md) chapter. `:syntax off` does not affect fenced code in the preview.

## Not available yet

- You cannot search the text in the preview.
- A link to a heading in another document (in the form `other.md#sec`) only opens that document; it does not move to the heading.

## Going further: Details of the preview

| Item | Rule |
|---|---|
| Size that can be previewed | Documents up to 16 MiB. For larger documents, a message such as "The preview is limited to 16 MiB" appears. |
| Parser | Documents are read with md4c 0.5.2, and Nib draws the display itself. |
| Colors in fenced code | Long code is colored in chunks of 64 lines. |
| Links to headings | When heading names are compared, only ASCII letters are treated as the same in uppercase and lowercase. For uppercase full-width Latin letters, Greek letters, and similar characters, write them in the link in the same uppercase as the heading. |
| Long paragraphs | A very long paragraph takes a little time the first time it is displayed. After that, scrolling does not stall. |

- If a heading contains uppercase full-width Latin letters, Greek letters, or similar characters, and the link writes them in lowercase, the heading is not found.

Next: [Language servers](06-language-servers.md) covers how to use completion, going to definitions, and other features with language servers.
