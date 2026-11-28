# Vim editing

Press `Ctrl+Shift+Space` to switch NeNe Nib to Vim mode, where you write with the same keys as Vim. The first half of this chapter covers the first things to learn if you have never used Vim. The second half is for people who know Vim, and lists in tables the keys and commands you can use in Nib.

> Right after you switch to Vim mode, you are in NORMAL, and the characters you type do not go into the text. Press `i` to enter INSERT, and then what you type is inserted. To go back to Basic mode, press `Ctrl+Shift+Space` again.

## First steps for people new to Vim

### How the modes work

In Vim, the same key does different things depending on the current state. Vim calls these states "modes". To keep them apart from Nib's Basic mode and Vim mode, this chapter writes them as NORMAL, INSERT, and VISUAL, the way they appear on the screen.

The status bar and the shape of the caret change with the state.

| State | What it is for | Status bar shows | Caret shape |
|---|---|---|---|
| NORMAL | You move with keys and give commands such as delete and copy. The characters you type do not go into the text. | `NORMAL` | A block that covers a character |
| INSERT | The characters you type go into the text. You can write just as in Notepad. | `INSERT` | A vertical line |
| VISUAL | You select a range. You can give commands to the selected range. | `VISUAL`, `VISUAL LINE`, `VISUAL BLOCK` | — |

The current state appears to the right of **Basic | Vim** on the left side of the status bar. If you are unsure, look here first.

<!-- Screenshot: NORMAL and INSERT shown on the left side of the status bar (to be added) -->

- When you switch to Vim mode, you always start in NORMAL.
- From any state, press `Esc` to go back to NORMAL.
- The input method (IME) turns off automatically in NORMAL, and turns back on when you enter INSERT.

### Start writing and return to NORMAL

Press one of the following keys in NORMAL to enter INSERT, where you can type text. When you finish writing, press `Esc` to go back to NORMAL.

| To do this | Do this | What happens |
|---|---|---|
| Write before the caret | `i` | Enters INSERT before the character at the caret. |
| Write after the caret | `a` | Enters INSERT after the character at the caret. |
| Make a new line below and write | `o` | Makes an empty line below the current line and enters INSERT there. |
| Make a new line above and write | `O` | Makes an empty line above the current line and enters INSERT there. |
| Write from the start or end of the line | `I` / `A` | Enters INSERT before the first character of the line, or at the end of the line. |
| Go back to NORMAL | `Esc` | Stops writing and goes back to NORMAL. |

### Move the caret

In NORMAL, you move the caret with letter keys. The arrow keys, `Home`, `End`, `PgUp`, and `PgDn` also work.

| To do this | Do this | What happens |
|---|---|---|
| Move left, down, up, or right by one | `h` / `j` / `k` / `l` | Moves one character left, one line down, one line up, or one character right. |
| Move to the next or previous word | `w` / `b` | Moves to the start of the next word or the start of the previous word. |
| Move to the start or end of the line | `0` / `$` | Moves to the first or last character of the line. |
| Move to the first or last line of the document | `gg` / `G` | Moves to the first line or the last line of the document. |
| Move to a specific line | Type `:120` and press `Enter` | Moves to line 120. |

If you type a number before a key, the key is repeated that many times. For example, `5j` moves down five lines.

### Delete, copy, paste, and undo

| To do this | Do this | What happens |
|---|---|---|
| Delete one character | `x` | Deletes the character at the caret. |
| Delete one line | `dd` | Deletes the current line. You can paste the deleted line. |
| Copy one line | `yy` | Copies the current line. |
| Paste | `p` / `P` | Pastes the text you copied or deleted after the caret (below, for lines) or before the caret (above, for lines). |
| Undo | `u` | Undoes the last edit. |
| Redo | `Ctrl+R` | Redoes the edit you undid. |
| Repeat the same edit | `.` | Repeats the last edit at the caret. |

In Vim mode, **Copy** and **Paste** in the right-click menu still use the Windows clipboard, so you can exchange text with other apps. (Internally, they run Vim keys such as `"+y` and `"+p`.)

Text you delete with `dd` or copy with `yy` goes into a place used only inside Nib (Vim calls it a register), not into the Windows clipboard.

### Select a range

| To do this | Do this | What happens |
|---|---|---|
| Select a range of characters | Press `v`, then move | Enters VISUAL and selects as far as you move. |
| Select whole lines | Press `V`, then move | Enters VISUAL LINE and selects whole lines. |
| Delete or copy the selected range | `d` / `y` | Deletes or copies the selected range and goes back to NORMAL. |
| Stop selecting | `Esc` | Clears the selection and goes back to NORMAL. |

If you select a word with a double-click or a line with a triple-click, you enter VISUAL.

### Save and close

In NORMAL, type `:` to show a line at the bottom of the screen where you type commands. Type a command and press `Enter` to run it.

| To do this | Do this | What happens |
|---|---|---|
| Save | `:w` | Saves changes to the current file. You can also save with `Ctrl+S`. |
| Save and close the tab | `:wq` | Saves, and then closes the current tab. |
| Close the tab | `:q` | Closes the current tab. If there are unsaved changes, the tab does not close. |
| Close the tab without saving | `:q!` | Discards the changes and closes the current tab. |
| Cancel the command | `Esc` | Closes the command line. |

### Return to Basic mode

`Esc` does not return you to Basic mode. `Esc` only returns you to NORMAL. To go back to Basic mode, press `Ctrl+Shift+Space` from any state, or click **Basic | Vim** on the left side of the status bar. The text, the undo history, and the caret position stay as they are.

### Keys that differ from Basic mode

Vim mode gives priority to Vim keys, so some keys work differently from Basic mode.

| To do this | Basic mode | Vim mode |
|---|---|---|
| Save | `Ctrl+S` | `Ctrl+S` (the same) or `:w` |
| Undo and redo | `Ctrl+Z` / `Ctrl+Y` | `u` / `Ctrl+R` |
| Open a file | `Ctrl+O` | `Ctrl+Shift+O` (`Ctrl+O` is the Vim key that goes back to where you were before) |
| Close a tab | `Ctrl+W` | `Ctrl+F4` or `:q` (`Ctrl+W` is the Vim key for split windows) |
| New tab | `Ctrl+T` / `Ctrl+N` | `Ctrl+T` (`Ctrl+N` is a Vim key) |
| Bookmark | `Ctrl+D` | `Ctrl+Shift+D` |
| Find, replace, and go to a line | `Ctrl+F` / `Ctrl+H` / `Ctrl+G` | `/`, `:s`, `:120`, and so on (`Ctrl+F`, `Ctrl+H`, and `Ctrl+G` keep their Vim functions) |

Commands that have no key are available from **All Commands** (`F1`) in both modes.

## Reference for Vim users

From here on, this chapter is for people who have used Vim. If you do not know Vim, the first half is enough to use Nib.

Nib's Vim editing is not an embedded copy of Vim; it was built from scratch. It is verified by 6,892 tests (plus 402 for wrapped display) that compare its output with real Vim 9.1 for the same keystrokes.

### Motions

| Category | Keys |
|---|---|
| Characters and lines | `h` `j` `k` `l` `0` `^` `$` `_` `g_` `{N}\|`, `Home` `End` |
| Words | `w` `b` `e` `ge` `W` `B` `E` `gE` |
| To a character in the line | `f` `F` `t` `T` `;` `,` |
| Document and screen | `gg` `G` `{N}%` `H` `M` `L` |
| Matching bracket | `%` |
| Paragraphs and sentences | `{` `}` `(` `)` |
| Wrapped screen lines | `gj` `gk` `g0` `g^` `g$` `gm` `gM` |
| Scrolling | `Ctrl+D` `Ctrl+U` `Ctrl+F` `Ctrl+B` `Ctrl+E` `Ctrl+Y`, `zt` `zz` `zb` `z<CR>` `z.` `z-` `z+` `z^` |
| Horizontal scrolling (when lines are not wrapped) | `zl` `zh` `zL` `zH` `zs` `ze` |

A character written with an accent or similar mark added after it moves as one character, together with its marks.

### Operators and text objects

Operators (commands such as `d` that act on the motion or range that follows) can be combined with motions and text objects (notations such as `iw` that point to a unit such as a word or the inside of brackets). As in Vim, counts are multiplied (`2d3w` deletes six words).

| Category | Keys |
|---|---|
| Operators | `d` `c` `y` `>` `<` `g~` `gu` `gU` |
| Whole lines and to the end of the line | `dd` `cc` `yy` `D` `C` `Y` |
| Single-character edits | `x` `X` `r` `s` `S` `~` |
| Joining lines | `J` `gJ` |
| Pasting | `p` `P` |
| Increasing and decreasing numbers | `Ctrl+A` `Ctrl+X`, `g Ctrl+A` in VISUAL |
| Text objects | `iw` `aw` `iW` `aW`, quotes (`i"`, `a"`, and so on), brackets (`i(`, `a{`, and so on; `b` and `B` also work), `ip` `ap` `is` `as` `it` `at` |
| Entering INSERT | `i` `a` `I` `A` `gI` `o` `O` (a count can be added), `gi` |
| Inside INSERT | `Ctrl+W` `Ctrl+U` `Ctrl+R {register}`, `Ctrl+O` (one NORMAL command) |
| Repeat and undo | `.` (also in VISUAL), `u`, `Ctrl+R` |

When you turn on `:set autoindent` (`ai`), `Enter` in INSERT and `o` `O` `cc` `S` give the next line the same indent as the current line. It is off by default.

### VISUAL and blocks

To select a block (a rectangular selection of the same columns across lines), use `Ctrl+V`.

| Category | Keys |
|---|---|
| Start selecting | `v` `V` `Ctrl+V`, `gv` to go back to the previous selection, `o` `O` to switch the end of the selection |
| Commands on the selection | `d` `x` `y` `c` `r` `J` `gJ` `X` `D` `Y` `C` `R` `s` `>` `<` `~` `u` `U` |
| Replace the selection by pasting | `p` `P` (`P` does not change the register) |
| Commands on a block | `$` `I` `A` `c` `C` `d` `x` `y` `r` `>` `<` `~` `u` `U`, block `p` `P`, `.` |

With block `p` `P`, if the column to paste at falls in the middle of a full-width character (such as a CJK character), Nib does not split the character; it fills the space before it with spaces.

### Registers and macros

| Category | Keys |
|---|---|
| Unnamed register | Used when you do not specify a register |
| Named registers | `"a` to `"z`; append with `"A` to `"Z` |
| Numbered registers | `"0` to `"9` |
| Small deletes and discarding | `"-` / `"_` |
| Windows clipboard | `"+` `"*`, `:set clipboard=unnamed` (shares the unnamed register with the clipboard) |
| Macros | `q{register}` to start recording and `q` to stop, `@{register}` `@@` (a count can be added) |

While a macro is being recorded, something like `recording @a` appears to the right of the mode in the status bar.

### Search and substitute

| Category | Keys and commands |
|---|---|
| Search | `/` `?` `n` `N` `*` `#`, `Ctrl+G` `Ctrl+T` while typing, `:noh` |
| Select a match of the last search | `gn` `gN` (change matches one at a time with `cgn` and `.`) |
| Uppercase and lowercase | `:set ignorecase` / `:set smartcase` |
| Substitute | `:[range]s/find/replace/[flags] [count]` |
| Repeat a substitution | `:&` `:&&` `:~`, `&` `g&` in NORMAL |
| Delete or copy lines | `:[range]d` `:[range]y` |
| Lines that match or do not match a pattern | `:g/find/d` `:v/find/d` `:g/find/s//replace/` |

- `hlsearch` (colors the matches) and `incsearch` (searches while you type) are on by default.
- On the `:` line and the search line, `↑` `↓` bring back commands and text you typed before.
- For the substitute range, you can use `%` `N,M` `.` `$` `'<,'>` `'a,'b` `.,.+N`; for the flags, you can use `g` `i` `I` `n` `e` `&` `c`. Besides `/`, any character that is not a letter can be used as the separator.
- In the replacement, you can use `&`, `\0` to `\9` (groups from `\(` `\)`), `~` `\r` `\u` `\U` `\l` `\L` `\e`.
- A substitution made by one command is undone with one `u`.
- The supported pattern syntax is summarized in [Going further](#going-further-detailed-differences-from-vim) at the end of the chapter.

### Marks and jumps

| Category | Keys |
|---|---|
| Set and return to marks | `m{a-z}`, `'{a-z}` `` `{a-z} `` |
| Special marks | `''` (before the jump), `'.` (where you last changed), `'^` `` `^ `` (where you last left INSERT) |
| Jump list | `Ctrl+O` `Ctrl+I` |
| Change list | `g;` `g,` |

When you delete or add lines above a mark, the mark moves with them.

### easymotion

In NORMAL or VISUAL, press `\\` (`\` twice) followed by one of the keys below, and small labels appear at the upper left of the places on screen you can jump to. Type the letters of a label to move there. Press `Esc` to cancel.

<!-- Screenshot: easymotion labels on the screen (to be added) -->

| Then press | Where labels appear |
|---|---|
| `w` / `b` | The start of each word after or before the caret |
| `e` | The end of each word after the caret |
| `j` / `k` | The first non-blank character of each line below or above the caret |
| `l` | The start of every visible word |
| `f` / `F` / `s` and one character | Each place with that character (`f` after the caret, `F` before it, `s` in both directions) |

- Words are divided the same way as with `w`, so labels also appear in Japanese text.
- After the jump, the position before the jump becomes the position you can go back to with `''`.
- You cannot use it after an operator, as in `d\\w`. When the window is split, labels appear only in the current window.

### Available `:` commands

| Category | Commands |
|---|---|
| Saving and quitting | `:w` `:w name` `:w! name` `:saveas name` `:saveas! name` `:q` `:q!` `:wq` `:x` |
| Go to a line | `:{N}` |
| Substitute and line commands | `:s` `:&` `:&&` `:~` `:d` `:y` `:g` `:v` (see [Search and substitute](#search-and-substitute) above) |
| Clear the search highlighting | `:noh` |
| Key mappings | `:map` `:nmap` `:imap` `:vmap` `:xmap` `:omap`, the `noremap` and `unmap` forms, `:let mapleader=` |
| Settings | `:set` (the table below and the [Settings](08-settings.md) chapter) |
| Tabs, split windows, and files | `:tabnext` `:tabprevious` `:tabnew` `:tabclose` `:tabs`, `:only`, `:e` `:b` `:ls`, `:grep` (the [Files and projects](04-files-and-projects.md) chapter) |

Mappings you make with `:map` are saved in the settings file, and you can use them in Vim mode the next time you start Nib.

`:set` accepts the short Vim names (`hls` `is` `ic` `scs` `rnu` `so` `ws` `cb` `et` `sw` `ts`) and the same forms as Vim (several on one line, as in `:set ic scs`; `name?` `noname?` `name&` `name<` `name:val` `+=` `-=` `^=`; `:se` `:setl` `:setg`). The main settings related to Vim editing are as follows.

| Setting | What it does | Default |
|---|---|---|
| `hlsearch` / `incsearch` | Color the matches / search while you type | On |
| `ignorecase` / `smartcase` | Search without matching uppercase and lowercase | Off |
| `wrapscan` | After searching to the end of the document, continue from the start | On |
| `relativenumber` | Show line numbers as the distance from the caret line | Off |
| `scrolloff=` | The number of lines to keep above and below the caret | 0 |
| `autoindent` | Give the next line the same indent when you start a new line | Off |
| `clipboard=unnamed` | Share the unnamed register with the Windows clipboard | Off |

### Not available yet

The following are not available in the current version.

- Uppercase marks (`'A` to `'Z`) and `:marks`
- `:sort`, and `:g` `:v` with commands other than `d` and `s`
- Read-only registers (`".` `":` `"/` `"%`)
- `R` in NORMAL and `S` in VISUAL
- Entering VISUAL by dragging the mouse in Vim mode
- commentary and surround
- Expanding `%` and `*` in file names for `:w` and similar commands

## Going further: Detailed differences from Vim

### Pattern syntax

Search and `:s` use the same pattern rules (the way you write the text to find). Of the `magic` syntax, the following are available.

- `.` `*` `^` `$` and character sets `[...]`
- Counts for a single character or group: `\+` `\=` `\?` `\{n}` `\{n,}` `\{,m}` `\{n,m}`, and the shortest match `\{-n,m}`
- Alternation `\|`, groups `\(` `\)` and `\%(` `\)`
- `\zs` `\ze`, which reset the start and end of the match

The following are not available yet.

- Switching the syntax with `\v` `\V` `\M`
- Patterns that span lines (`\n`, `\_s`, `\_.`, and so on)
- Backreferences inside the pattern (such as `\(a\)\1`)

When an expanded pattern would become very large (for example, when a single character is repeated more than 1,024 times), Nib shows the same message as Vim's E363 and does not search.

### Confirming substitutions

With the `c` flag, Nib asks at each match: `y` (replace), `n` (skip), `a` (replace all the rest), `q` (quit), or `l` (replace this one and quit). If you add the `c` flag to an `:s` inside `:g`, Nib currently rejects the command with a message. There is no feature that previews the result of a substitution while you type.

### Settings whose values cannot be changed

In Nib, indents are always tab characters, with a width of 8. `:set` accepts values that are the same as the current ones (`noet` `sw=8` `ts=8` `magic`), but rejects different values (such as `et` `sw=4` `nomagic`). `:set number` and `:set all&` are also rejected. A rejected line stops a macro or a `.` repeat at that point.

### Other differences

- `:set clipboard=unnamed` stays in effect until you close the window. It is not saved in the settings file.
- A macro or a `.` repeat can play back up to 100,000 keys in one run. If there are more, the rest are discarded and Nib shows a one-line message.
- In a file that ends with a line break, Nib does not count an empty line after the last line break (the same way Vim counts with `eol`). `G` and `:$` move to the line that has the last line break. The saved contents do not change.
- While you press `Ctrl+O` in INSERT and Nib is waiting for a NORMAL command, the status bar shows `(insert)`.
- Vim messages appear in English, whatever the display language.

Next: [Files and projects](04-files-and-projects.md) covers tabs, **Files and Tabs** (`Ctrl+P`), the Explorer, and searching a project.
