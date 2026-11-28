# Files and projects

In NeNe Nib, each file you open becomes a tab in the title bar. This chapter describes how to work with tabs, how to find and open files with `Ctrl+P`, how to open a folder and work with a whole project (a folder that holds a set of related files), and how to split the text area into two panes.

> If you do not know where the file you want is, press `Ctrl+P` first. The open tabs, bookmarks, recently closed files, and the files in the same folder as the current file appear together in one list.

## Use tabs

Tabs appear in the title bar at the top of the window.

| To do this | Do this | What happens |
|---|---|---|
| Open a new tab | `Ctrl+T` (also `Ctrl+N` in Basic mode), or `+` on the tab strip | An empty **Untitled** tab opens. |
| Switch tabs | Click a tab | Switches to the document in that tab. |
| Switch in the order you last used them | `Ctrl+Tab` (`Ctrl+Shift+Tab` for the reverse order) | Each time you press `Tab` while holding down `Ctrl`, Nib moves to the next tab in the order you last used them. The tab is chosen when you release `Ctrl`. |
| Close a tab | `Ctrl+F4` (also `Ctrl+W` in Basic mode), the × on the tab, or middle-click the tab | Closes that tab. |
| Choose from the list of tabs | `∨`, which appears when the tabs do not fit on the strip | A list of the open tabs appears. |
| See where a file is | Point to the tab | After a moment, the full path of the file appears in small text below the tab. A long path is shortened in the middle with "…". |

<!-- Screenshot: the tab strip and the small full-path display (to be added) -->

### Use the tab right-click menu

Right-click a tab (or the × on it) to open the following menu. It is the same in Basic mode and Vim mode.

| Item | What happens |
|---|---|
| **Reveal in File Explorer** | Opens the folder in Windows File Explorer with that file selected. |
| **Copy Path** | Copies the full path of the file to the clipboard. |
| **Copy File Name** | Copies the file name to the clipboard. |
| **Save** | Saves changes to the file in that tab. |
| **Save As** | Saves the document in that tab with another name. |
| **Close Tab** | Closes that tab. |
| **Close Other Tabs** | Closes every tab except that one. |
| **Close Tabs to the Right** | Closes the tabs to the right of that tab. |

- An **Untitled** tab has no path, so **Reveal in File Explorer**, **Copy Path**, and **Copy File Name** are dimmed and cannot be chosen.
- When you close several tabs at once, Nib asks whether to save each tab that has unsaved changes. If you choose **Cancel**, Nib stops closing tabs.
- You cannot open this menu from the keyboard (`Shift+F10` and the Menu key open the right-click menu of the text). **Reveal in File Explorer**, **Copy Path**, **Copy File Name**, **Close Other Tabs**, and **Close Tabs to the Right** can also be run on the current tab from **All Commands** (`F1`).

### Tabs when you start Nib again

When you start Nib without specifying a file, the tabs from your last session (saved files only) are restored, with their order, the tab you were viewing, and the caret and scroll positions. For details, see the [Installing and first start](01-install.md) chapter.

## Find files and tabs with `Ctrl+P`

Press `Ctrl+P` to open **Files and Tabs**. The input box is empty, and the following appear together in one list.

- Open tabs
- Bookmarks
- Recently closed files
- Files in the same folder as the current file

<!-- Screenshot: Files and Tabs (Ctrl+P) and the preview area on the right (to be added) -->

When you type part of a file name, fuzzy search narrows the candidates: a name matches if it contains the characters you typed in order, even if they are not next to each other. If nothing is found by file name, Nib also searches folder names.

| To do this | Do this | What happens |
|---|---|---|
| Choose a candidate | `↑` / `↓` | Moves the selection. |
| Open | `Enter` | Opens the selected file. If it is an open tab, Nib switches to that tab. |
| Close | `Esc` | Closes the list. |
| Change where the preview area appears | `Ctrl+Shift+P` while the list is open | The preview area switches in this order: right, below, off. |

- The contents of the selected candidate appear in the preview area to the right of the list. If the window is not wide enough, the preview appears below the list. The layout you choose stays the same the next time you start Nib.
- For an open tab, the preview shows its contents, including unsaved changes. For a file that is not open, Nib reads and shows up to 16 MiB. Files that are in the cloud, such as OneDrive, and are not yet on this PC are not downloaded for the preview.
- Right after the list opens, the input method (IME) is off. To type with it, turn it on before you type.

### Filter with a prefix

Type one symbol at the start of the input box to show only one kind of item.

| Prefix | What appears |
|---|---|
| `#` | Open tabs |
| `*` | Bookmarks |
| `@` | Recently closed files |
| `/` | Files in the same folder as the current file |
| `!` | Full-text search of the project (see [Search the whole project](#search-the-whole-project) below) |
| `:` | Settings commands (described in the [Settings](08-settings.md) chapter) |
| `?` | **All Commands** (the same as `F1`) |

`^`, `&`, and `%` are language server lists. They are described in the [Language servers](06-language-servers.md) chapter.

## Reopen recently closed files and bookmarks

### Recently closed files

Nib keeps up to 100 recently closed files. They appear in **Files and Tabs** (`Ctrl+P`), and you can show only them with `@`. Up to 10 of them also appear under **Recent Files** on the start screen.

If a recently closed file no longer exists, Nib shows a one-line message and removes it from the list.

### Bookmarks

If you bookmark files you open often, you can bring them up quickly with `*` in `Ctrl+P`.

| To do this | Do this | What happens |
|---|---|---|
| Add or remove a bookmark for the current file | `Ctrl+D` in Basic mode, `Ctrl+Shift+D` in Vim mode | "Bookmark added" or "Bookmark removed" appears. |
| Add or remove a bookmark for a file in the list | Select a candidate in **Files and Tabs** (`Ctrl+P`) and press the same key | Adds or removes the bookmark for the selected file without opening it. |

- You can bookmark only saved files. In an **Untitled** tab, "Choose a saved file" appears; save the file first.
- You can add up to 1,024 bookmarks.
- If you move or delete a bookmarked file, the bookmark stays. When you no longer need it, select it with `*` in **Files and Tabs** and remove it with the same key.
- Up to 10 bookmarks also appear under **Bookmarks** on the start screen.
- In Vim mode, `Ctrl+D` keeps its Vim function of moving half a screen.

## Open a folder (the project root)

In Nib, you choose a project folder as the root: the top folder for the Explorer and for full-text search. The Explorer is the file tree pane on the left side of the screen. It is not the same as Windows File Explorer.

| To do this | Do this | What happens |
|---|---|---|
| Open a folder | `Ctrl+Shift+R`, **Open Folder** on the start screen, or **Choose Root Folder** in **All Commands** (`F1`) | A dialog box for choosing a folder appears. The folder you choose becomes the root, and the Explorer appears on the left. |
| Make the current file's folder the root | `:set root=.` | The folder of the current document becomes the root. |
| Specify a folder by path | `:set root=` followed by the folder path | That folder becomes the root. |

- You can also use settings commands by pressing `Ctrl+P` and typing them after `:`.
- The root you choose is saved in the settings and stays the same the next time you start Nib.
- The folder dialog box also shows the installed WSL (Windows Subsystem for Linux) distributions, at the location `\\wsl.localhost\<name>`.

## Use the Explorer

The Explorer is the pane to the left of the text that shows the folders and files under the root in a tree. It is hidden by default.

<!-- Screenshot: the Explorer on the left and a preview tab in italics (to be added) -->

| To do this | Do this | What happens |
|---|---|---|
| Show or hide it | `Ctrl+Shift+E` (or `:set explorer` / `:set noexplorer`) | If it is hidden, Nib shows it and moves focus to the Explorer. If it is shown and focus is in the Explorer, Nib hides it. If it is shown and focus is in the text, Nib only moves focus to the Explorer. |
| Open a file | Click the file's row | Opens the file. Focus stays in the Explorer, so you can keep looking at other files with `↑` / `↓`. |
| Open a file and move to the text | Double-click the file's row, or press `Enter` | Opens the file and moves focus to the text. |
| Expand or collapse a folder | Click the folder's row | Shows or hides the contents of the folder. |
| Change the pane width | Drag the border between the pane and the text | The width changes. Double-click the border to restore the original width. Press `Esc` while dragging to go back to the width before the drag. The width you set stays the same the next time you start Nib. |

While focus is in the Explorer, you can use the following keys. They are the same in both modes.

| Press | What happens |
|---|---|
| `↓` / `↑` (or `j` / `k`) | Moves the selected row. |
| `→` (or `l`) | Expands a folder. |
| `←` (or `h`) | Collapses a folder. On a collapsed folder or a file, moves to the folder above it. |
| `Enter` | For a file, opens it and moves focus to the text. For a folder, expands it. |
| `Esc` | Returns focus to the text and keeps the pane shown. |

### Preview tabs

A tab you open with a single click in the Explorer shows its name in italics. This is a "preview tab": when you click another file next, Nib replaces the contents of the same tab instead of adding a new tab. You can look through files one after another without piling up tabs.

A preview tab becomes a regular tab when you do any of the following:

- Edit the text
- Save
- Double-click in the Explorer, or press `Enter`
- Double-click the tab name

Tabs you open with `Ctrl+P`, `Ctrl+O`, or drag and drop are always regular tabs. Preview tabs are not the same as the Markdown preview ([next chapter](05-markdown.md)).

### How the Explorer looks

- Each file row has a small dot in the color of the file's language.
- A file with unsaved changes has a colored circle.
- Hidden files also appear, in a lighter color.

## Search the whole project

You can search for text in all the files under the root.

1. Press `Ctrl+Shift+F` (it works in both modes; **Search in project** in **All Commands** does the same). **Files and Tabs** opens with `!` already at the start.
2. Type the text to find. Nib searches again each time you type, and the matching lines are added to the list. While it searches, "Searching…" appears with the number of files searched so far.
3. Select a line with `↑` / `↓` and press `Enter`. That file opens, and the caret moves to the line and column of the match.

<!-- Screenshot: the list while searching with Ctrl+Shift+F (to be added) -->

- Nib searches under the Explorer root. If you have not chosen a root, it searches under the folder of the current document. If there is neither, "Nothing to search. Choose a root or search from a saved document" appears.
- The text you type is searched for exactly as typed, and the search is not case-sensitive. If you start with `/`, the text is searched as a pattern written the same way as a Vim search.
- For each line, only the first match is shown. The search stops when 1,000 matches have been found.
- Lines that are not valid UTF-8 (such as lines in a Shift_JIS file) cannot be searched. To search inside a Shift_JIS file, open the file and search with `Ctrl+F`. Searching inside an open document works for Shift_JIS files too.

## Split the text area into panes

You can split the text area into two panes to view distant parts of the same document, or two documents, side by side.

| To do this | Do this | What happens |
|---|---|---|
| Split left and right | `Ctrl+\` | Splits the text area into two side-by-side panes that both show the same document. |
| Split top and bottom | `Ctrl+Shift+\` | Splits the text area into two stacked panes that both show the same document. |
| Move to the other pane | `F6`, or click the pane | Switches the pane you are working in. |
| Close a pane | `Ctrl+Shift+F4` | Closes the current pane and goes back to one pane. The document is not closed. |
| Resize the panes | Drag the border between the panes | The border moves. Double-click it to make the panes the same size again. Press `Esc` while dragging to go back. |

- You can have up to two panes.
- Switching tabs, new tabs, opening files, and `Ctrl+P` apply to the pane you are working in.
- You can also resize the panes with **Enlarge Pane**, **Shrink Pane**, and **Equalize Panes** in **All Commands** (`F1`). You can change how far one step moves with the **Pane Resize Step** setting (default: 5).
- The split and the pane sizes last until you close the window. At the next start, Nib goes back to one pane.

## In Vim mode

In Vim mode, besides the keys above, you can do the same things with the same commands as in Vim. For details, see the [Vim editing](03-vim.md) chapter.

| To do this | Vim command |
|---|---|
| Move between tabs | `gt` / `gT`, `:tabnext` / `:tabprevious` |
| Open or close a tab | `:tabnew` / `:tabclose` |
| See the open tabs in a list | `:tabs`, `:ls` / `:files` / `:buffers`, `:b` (type more after it, as in `:b memo`, to narrow the list) |
| Open the file list | `:e` (the same list as **Files and Tabs**) |
| Search the whole project | `:grep` followed by the text to find |
| Split, move between, or close panes | `Ctrl+W` `s`, `Ctrl+W` `v`, `Ctrl+W` `w`, `Ctrl+W` `c`, `Ctrl+W` `o`, `:only`, and so on |
| Resize the panes | `Ctrl+W` `>`, `Ctrl+W` `<`, `Ctrl+W` `+`, `Ctrl+W` `-`, `Ctrl+W` `=`, `:resize` / `:vertical resize` |

## Not available yet

- You cannot yet rearrange tabs by dragging them.
- The position and size of the window are not kept for the next start.
- In the Explorer, you cannot rename or delete files or create new files.
- If another app changes the contents of a folder, the Explorer does not show the change until you open that folder again.
- The taskbar jump list (the list that appears when you right-click the taskbar icon) does not yet show recently closed files and bookmarks.

## Going further: Limits of the lists and search

| Item | Limit or rule |
|---|---|
| Files in the same folder in **Files and Tabs** | When 8,192 files have been read, a message appears until you close the list (see also the list below). |
| The **Files and Tabs** preview area | For files that are not open, Nib reads up to 16 MiB. For larger files and files that are not text, a message says the file is not previewed. |
| Recently closed files | Up to 100 files. Stored in `%LOCALAPPDATA%\NeNeNib\history.v1`. |
| Bookmarks | Up to 1,024. Stored in `%LOCALAPPDATA%\NeNeNib\bookmarks.v1`. |
| Explorer | Shows up to 2,000 items in one folder. A folder that cannot be read shows "(unreadable)". |
| What full-text search of the project skips | Hidden folders; the `.git`, `node_modules`, `build`, and `out` folders; junctions and symbolic links to folders (both are mechanisms, similar to shortcuts, that point to a folder in another location); files larger than 16 MiB; files that have a NUL (a character whose value is 0) in their first 8 KiB |
| Number of results in full-text search of the project | One per line, up to 1,000 in total |
| One step of pane resizing | 1 to 40, set with the **Pane Resize Step** setting (see also the list below). |

- Files in the same folder in **Files and Tabs** do not include hidden files, system files, or files of kinds known not to be text. Files with no extension and files with unknown extensions do appear.
- If reading or writing the bookmarks fails, Nib keeps the previous list and shows a one-line message.
- You can also change one step of pane resizing with `:set paneresizestep=`.
- In Vim mode, `Ctrl+W` `>`, `<`, `+`, and `-` without a count move the border by one character (or one line), as in Vim. With `:set paneresizestepvim`, they follow **Pane Resize Step**.

Next: [Markdown and preview](05-markdown.md) covers how to view a Markdown document in its formatted form.
