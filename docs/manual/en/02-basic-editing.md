# Basic editing

When the left side of the status bar shows `Basic`, NeNe Nib is in Basic mode, and you can write with the same keys as Notepad. This chapter first describes what works the same as in Notepad, and then what is unique to Nib. The [Vim editing](03-vim.md) chapter describes Vim mode.

> If the characters you type do not appear, you may be in Vim mode. Press `Ctrl+Shift+Space` to return to Basic mode.

## Select text

| To do this | Do this | What happens |
|---|---|---|
| Select a range | Hold down `Shift` and press an arrow key | Selects as far as the caret moves. |
| Select a range with the mouse | Drag | Selects the range you drag over. You can also drag backward. If you drag outside the text area, the text scrolls in that direction. |
| Extend the current selection | Hold down `Shift` and click | Keeps the start of the selection and selects up to where you click. |
| Place the caret | Click | The caret moves to where you click. |
| Select a word | Double-click | Selects the word you click. |
| Select a line | Triple-click | Selects the line you click. |
| Select everything | `Ctrl+A` | Selects the whole document. |
| Clear the selection | `Esc` | Clears the selection. |

## Copy, cut, paste, and undo

| To do this | Do this | What happens |
|---|---|---|
| Copy | `Ctrl+C` (or `Ctrl+Insert`) | Copies the selected text to the clipboard. |
| Cut | `Ctrl+X` (or `Shift+Delete`) | Moves the selected text to the clipboard. |
| Paste | `Ctrl+V` (or `Shift+Insert`) | Pastes the clipboard contents. |
| Paste only the text | `Ctrl+Shift+V` | Pastes only the text on the clipboard. |
| Undo | `Ctrl+Z` | Undoes the last edit. |
| Redo | `Ctrl+Y` (or `Ctrl+Shift+Z`) | Redoes the edit you undid. |

The difference between `Ctrl+V` and `Ctrl+Shift+V` is described in [Paste text from a web page](#paste-text-from-a-web-page) below.

## Use the right-click menu

Right-click the text to open a menu with **Cut**, **Copy**, **Paste**, **Delete**, **Select all**, **Find**, and **Replace**. You can also open it with `Shift+F10` or the Menu key on your keyboard.

<!-- Screenshot: the right-click menu on the text (to be added) -->

## Move the caret

| To do this | Do this | What happens |
|---|---|---|
| Move one character at a time | `←` / `→` | Moves one character at a time. |
| Move to the line above or below | `↑` / `↓` | Moves one screen line at a time. |
| Move to the start or end of the line | `Home` / `End` | Moves to the start or end of the screen line. |
| Move one screen at a time | `PgUp` / `PgDn` | Scrolls one screen at a time, and the caret moves by the same amount. |
| Move to the start or end of the document | `Ctrl+Home` / `Ctrl+End` | Moves to the start or end of the document. |
| Move by words | `Ctrl+←` / `Ctrl+→` | Stops at word boundaries. |

By default, long lines wrap at the window width. Wrapping affects only the display; no line breaks are added to the text. `↑`, `↓`, `Home`, and `End` treat each wrapped screen line as one line.

Nib finds word boundaries the same way Notepad does.

- `Ctrl+→` moves past a word and the spaces after it, and stops at the start of the next word. At the end of a line, it stops once before the line break.
- `Ctrl+←` moves the same way in the opposite direction.
- In Japanese text, it stops where the kind of character changes, such as between kanji, hiragana, and katakana. A run of kanji is treated as one word.
- `_` is treated as a symbol, so in `foo_bar` it stops before and after `_`. A double-click selects all of `foo_bar` as one word.

### At the ends of the document and with a selection

Nib follows standard Windows behavior:

- If the caret is on the last line of the document (when the line is wrapped, its last screen line) and you press `↓`, the caret moves to the end of the document.
- If the caret is on the first line of the document (when the line is wrapped, its first screen line) and you press `↑`, the caret moves to the start of the document.
- If a range is selected and you press `←` without `Shift`, the caret moves to the start of the selection and the selection is cleared. With `→`, the caret moves to the end of the selection.
- While a range is selected, `↑`, `Home`, and `PgUp` move from the start of the selection, and `↓`, `End`, and `PgDn` move from the end of the selection.

## Delete text

| To do this | Do this | What happens |
|---|---|---|
| Delete the character before the caret | `Backspace` | Deletes one character before the caret. |
| Delete the character after the caret | `Delete` | Deletes one character after the caret. |
| Delete the previous word | `Ctrl+Backspace` | Deletes back to where `Ctrl+←` would stop. |
| Delete the next word | `Ctrl+Delete` | Deletes up to where `Ctrl+→` would stop. |

- If a range is selected, each of these keys deletes the selected range.
- Pressing `Ctrl+Backspace` at the start of a line deletes the line break between it and the previous line. From the start of an indented line, it deletes the indent, the line break, and any empty lines above, all at once. One `Ctrl+Z` brings it all back.
- In the search box and in **Files and Tabs** (`Ctrl+P`), you cannot yet move or delete by words.

## Paste text from a web page

Text you copy from a browser or a word processor is on the clipboard in two forms: plain text, and HTML that includes headings and links. In Nib, `Ctrl+V` pastes differently depending on the kind of document you have open.

| Open document | What `Ctrl+V` pastes |
|---|---|
| Markdown (a document format that uses symbols such as `#` and `**` for headings and bold; files such as `.md`) | Converts the HTML to Markdown and pastes it. Headings become `#`, bold becomes `**…**`, links become the text in `[ ]` followed by the destination in `( )`, and bulleted lists become `- `. |
| HTML, JavaScript, TypeScript | Pastes the HTML with its tags. |
| Other documents | Pastes only the text. |

- `Ctrl+Shift+V` pastes only the text in every kind of document. Use it when you do not want to bring formatting into a Markdown document.
- To paste HTML with its tags in any document, choose **Paste HTML Source** in **All Commands** (`F1`). This command has no key.
- If the HTML cannot be converted to Markdown, Nib pastes only the text.
- Whichever way you paste, one `Ctrl+Z` undoes it.

## Find, replace, and go to a line

| To do this | Do this | What happens |
|---|---|---|
| Find text | `Ctrl+F` | **Find:** appears at the bottom of the screen. Type the text to find and press `Enter` to move to the next match. |
| Find next | `F3` | Moves to the next match of the last text you searched for. |
| Find previous | `Shift+F3` | Moves to the previous match of the last text you searched for. |
| Replace | `Ctrl+H` | Type the text to find, and then type the replacement text when prompted. |
| Go to a line | `Ctrl+G` | Type a line number after **Line:** and press `Enter` to move to that line. |
| Close the input box | `Esc` | Closes **Find:** and the other input boxes. |

<!-- Screenshot: the Find: input box and the colored matches (to be added) -->

### Find text

- Matches are shown in the text while you are still typing.
- Matches are colored, and the one at the caret is outlined.
- `Enter` in **Find:** moves to the next match without closing the input box. When you are done, close it with `Esc`.
- The text you type is searched for exactly as typed. `.` and `*` also find those characters themselves.
- The search is case-sensitive. The [Settings](08-settings.md) chapter describes how to search without matching case.
- When nothing is found, and when the search wraps from the end of the document back to the start, a one-line message appears in English at the bottom of the screen.

### Replace text

1. Press `Ctrl+H`. **Find:** already contains the text you searched for last time.
2. Check the text to find and press `Enter`.
3. After **Replace "<text to find>" with:**, type the replacement text.
4. Press one of the following.

| Press | What happens |
|---|---|
| `Enter` | Replaces the current match and moves to the next one. Keep pressing `Enter` to replace matches one at a time. |
| `Ctrl+Enter` | Replaces every match in the document. One `Ctrl+Z` brings them all back. |

The replacement text is also inserted exactly as typed.

### Go to a line

After **Line:**, type a line number, or a line and column in the form `line:column` (such as `120:5`). If you type a number larger than the number of lines in the document, Nib moves to the last line.

### In Vim mode

In Vim mode, `Ctrl+F`, `Ctrl+H`, and `Ctrl+G` keep their Vim functions. To do the same things, choose **Find**, **Replace**, or **Go to Line** in **All Commands** (`F1`). The text you search for is shared by the two modes, so you can continue a search from Basic mode with `n` in Vim mode.

## Other Notepad commands

| To do this | Do this | What happens |
|---|---|---|
| Insert the date and time | `F5` | Inserts the current date and time at the caret. |
| Make text larger or smaller | `Ctrl++` / `Ctrl+-` / hold down `Ctrl` and turn the mouse wheel | Changes the size of the text in steps of 0.5 pt, between 8 and 40 pt. |
| Restore the text size | `Ctrl+0` | Returns the text to its default size. |
| Print | **Print** in **All Commands** (`F1`) | Choose a printer and print the current document. |

Printing has no key. In Notepad, `Ctrl+P` prints, but in Nib, `Ctrl+P` opens **Files and Tabs**.

## Going further: Characters with accents and dakuten

Some characters are written with an accent added after them as a separate character. Japanese text can do the same with dakuten (the voicing mark on kana): for example, "が" can be written as "か" followed by a combining dakuten. Variation selectors, which choose the glyph of a kanji such as one in a person's name, are also added after the preceding character in the same way.

In Basic mode, Nib handles these characters as follows.

| Press | What happens |
|---|---|
| `←` / `→` | Moves past the base character together with the accents and dakuten after it and the first variation selector, as one character. |
| `Delete` | Deletes the same group at once. Any variation selectors that follow are deleted with it. |
| `Backspace` | Deletes accents and dakuten one at a time. For "が", it deletes only the dakuten and leaves "か". A variation selector is deleted together with the character before it. |

Emoji joined to look like one (such as family emoji), emoji with a skin tone, and flags are not yet treated as one character.

Next: [Vim editing](03-vim.md) covers what you can do after switching to Vim mode with `Ctrl+Shift+Space`.
