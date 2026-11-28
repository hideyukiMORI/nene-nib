# Language servers

A language server is a separate app, made for one programming language, that reads the source code you write and reports errors, offers completions (suggestions for the rest of a name you have started typing), shows where things are defined, and more. When NeNe Nib is connected to a language server, it shows this information right in the text. Language servers are not included with Nib, so you install the one for your language separately. This chapter uses clangd (the language server for C and C++) as an example to show how to connect a language server and use it.

> Language servers are off by default, so installing one has no effect until you turn them on. Turn on language servers in **Settings**, and choose a server for your language.

## Connect a language server

The easiest way is from **Settings**, which opens on the right when you press `Ctrl+,`.

1. Install the language server for your language. For C and C++, that is clangd.
2. In Nib, press `Ctrl+,` to open **Settings**.
3. In the **Features** section, turn on **Language Server (LSP)**.
4. The **Language Server** section lists one row for each language. When Nib finds an installed language server, the row shows **Found**. Click **Use** on the right.
5. Close **Settings**. Your settings are saved when you close it.
6. Open a file in that language. The language server starts.

<!-- Screenshot: the Language Server section of Settings and the Use button (to be added) -->

Each row in the **Language Server** section shows one of these states.

| State | Meaning |
|---|---|
| **Searching…** | Nib is still searching your PC. |
| **Found** | The language server is installed. Click **Use** to use it for this language. |
| **Not found** | The language server was not found. The second line shows the command that installs it. For Lua and Markdown, instead of a command, you see a note such as "Install …, then reopen the settings to list it here". |
| **Configured** | A language server is set for this language. Click **Remove** to stop using it. |
| **Starting** | The language server is starting. |
| **● Running** | The language server is running. |
| **Stopped** | The language server failed to start several times in a row, so Nib stopped it. You can start it again with `:set lsp`. |

- By default, rows for five languages are shown. To see the other languages, click **Show N more languages**.
- C and C++ share one row. When you click **Use**, both languages use the same clangd.
- If a language server still shows **Not found** after you install it, reopen **Settings**. If you changed PATH (the list of folders where Windows looks for apps), restart Nib.
- You can also turn language servers on and off with `:set lsp` and `:set nolsp`. Settings commands are explained in the [Settings](08-settings.md) chapter.
- Language servers do not work on an **Untitled** document that has no name. Save it once first.

The right side of the status bar shows the language of the open document. While a language server is starting, the status bar also shows its progress.

## See errors and warnings (diagnostics)

When a language server finds an error or a warning, a wavy underline appears at that place in the text. These are called diagnostics.

- The right side of the status bar shows the number of errors and warnings, such as `E2 W1`. `E` is the number of errors, and `W` is the number of warnings.
- When you place the caret on a wavy underline, the left side of the status bar shows the diagnostic message.

<!-- Screenshot: a wavy underline and the E and W counts in the status bar (to be added) -->

| To do this | Basic mode | Vim mode | What happens |
|---|---|---|---|
| Go to the next diagnostic | `F8` | `F8` or `]d` | Moves to the next error or warning. |
| Go to the previous diagnostic | `Shift+F8` | `Shift+F8` or `[d` | Moves to the previous error or warning. |
| See the whole diagnostic message | **Show diagnostic** in **All Commands** (`F1`) | `gl` | Shows the diagnostic message at the caret in a small box. |
| See a list of diagnostics | **List diagnostics** in **All Commands** (`F1`) | Same as Basic mode | Lists the diagnostics of the open tabs in the **Files and Tabs** list (`Ctrl+P`). |

You can also open the list of diagnostics by pressing `Ctrl+P` and typing `^`.

## Complete names

You can choose the rest of a name you have started typing from a list of suggestions. The language server's suggestions come first, followed by words in the open documents. When language servers are off, only the words in the documents are listed.

### Basic mode

| To do this | Do this | What happens |
|---|---|---|
| Open the list of suggestions | `Ctrl+Space` (or `Ctrl+I`) | Lists suggestions that match the partial name before the caret. |
| Choose a suggestion | `↑` / `↓` | The chosen suggestion is highlighted. The text does not change yet. |
| Insert the chosen suggestion | `Enter` or `Tab` | Replaces the partial name with the chosen suggestion. |
| Close the list | `Esc` | Closes the list without inserting anything. |

- If you keep typing while the list is open, the suggestions are narrowed down.
- While a language server is running, typing certain characters, such as `.`, opens the list automatically.
- In a list that opened automatically, `Tab` inserts the chosen suggestion or the top one. `Enter` inserts only a suggestion you chose with `↑` / `↓`. If you press `Enter` without choosing one, you get a normal line break. You can also click a suggestion to insert it.
- If your input method (IME) uses `Ctrl+Space`, use `Ctrl+I` instead.

### Vim mode

Use these keys in INSERT.

| To do this | Do this | What happens |
|---|---|---|
| Open the list of suggestions and insert the next one | `Ctrl+N` | Opens the list and inserts the suggestions one by one. |
| Insert the previous suggestion | `Ctrl+P` | Inserts the suggestions in reverse order. |
| Accept the current suggestion | `Ctrl+Y` | Closes the list. |
| Cancel | `Ctrl+E` | Restores the partial name and closes the list. |

- In INSERT, too, typing one of the language server's trigger characters opens the list automatically. While the list is open, you can choose suggestions with `↑` / `↓`, `PgUp`, and `PgDn`.
- While the list is open, `Ctrl+P` chooses suggestions. It does not open **Files and Tabs**.

<!-- Screenshot: the list of completion suggestions (to be added) -->

## See information about a name and go to its definition

| To do this | Basic mode | Vim mode | What happens |
|---|---|---|---|
| See information about a name | **Show information** in **All Commands** (`F1`) | `K` | Shows the type and description of the name at the caret in a small box. |
| Go to the definition | `F12` | `F12` or `gd` | Moves to where the name at the caret is defined. If it is in another file, that file opens. |
| See parameter info | **Show parameter info** in **All Commands** (`F1`) | Same as Basic mode | Shows the parameters of the function call above the caret line. |

- The information box disappears the next time you press a key, click, or scroll.
- When there are two or more definitions, Nib moves to the first one and tells you so, for example "Definition 1 / 3".
- When you type `(` or `,` after a function name, parameter info appears automatically. The parameter you are typing is highlighted.

## Find where a name is used (references)

| To do this | Basic mode | Vim mode | What happens |
|---|---|---|---|
| See a list of the places that use a name | `Shift+F12` | `Shift+F12` or `grr` | Lists the places that use the name at the caret in the **Files and Tabs** list. |

Choose a place in the list and press `Enter` to go there. The list shows up to 1,000 items. To see the list from your last reference search again, press `Ctrl+P` and type `&`.

## See a list of functions and types (symbols)

| To do this | Basic mode | Vim mode | What happens |
|---|---|---|---|
| See a list of the functions and types in the current document | **List symbols** in **All Commands** (`F1`) | `gO` | Lists the functions, types, and other symbols of the current document in the **Files and Tabs** list. |

Choose an item in the list and press `Enter` to go there. The list shows up to 1,000 items. You can also open the list of symbols by pressing `Ctrl+P` and typing `%`.

## Rename a symbol

You can rename a function or variable, together with every place that uses it, in one step.

| To do this | Basic mode | Vim mode | What happens |
|---|---|---|---|
| Rename a symbol | `F2` | `F2` or `grn` | "New name:" appears at the bottom of the window. Type the new name and press `Enter` to rename it. |

- Places in other files are renamed too. Files that were not open are opened in new tabs, and the changes are left unsaved. Check the "● " in front of each tab name, then save after you have reviewed the changes.
- If even one place cannot be renamed, nothing is changed and a one-line message appears.
- In Vim mode, renaming is not done in INSERT. Press `Esc` to return to NORMAL first.
- To undo the rename, press `Ctrl+Z` (`u` in Vim mode) in each tab that changed.

## Format code

You can ask the language server to tidy up the indentation and spacing. There are no keys for this, so choose it from **All Commands** (`F1`).

| To do this | Do this | What happens |
|---|---|---|
| Format the whole document | **Format document** in **All Commands** (`F1`) | Formats the whole document. |
| Format the selection | **Format selection** in **All Commands** (`F1`) | Formats the selected text. If nothing is selected, formats the caret line. |

You can undo the formatting with one `Ctrl+Z` (`u` in Vim mode). In Vim mode, formatting is not done in INSERT.

## Fix problems with a language server

- When a language server does not support a feature, a one-line message appears, such as "This language server does not answer references". The features you can use differ from one language server to another.
- Choose **Open Language Server Log** in **All Commands** (`F1`) to see the messages from the language server in an **Untitled** tab. In Vim mode, you can also open it with `:LspLog`.
- If a language server fails to start several times in a row, Nib stops it. Fix the cause, then start it again with `:set lsp`.

## Going further: Set up language servers in the settings file

Clicking **Use** in **Settings** writes lines like these to the settings file `settings.v6` (see the end of the [Installing and first start](01-install.md) chapter). You can also write them by hand.

```
lsp=on
lsp.cpp.command=C:\llvm\bin\clangd.exe
```

- `lsp=on` turns language servers on.
- After `lsp.<language>.command=`, write the command that starts the language server. Language names include `c`, `cpp`, `python`, `rust`, `go`, `typescript`, `javascript`, and `cs`. To use the same clangd for C and C++, write two lines: `lsp.c.command=` and `lsp.cpp.command=`.
- Write the executable as a full path that starts with the drive letter. If a folder name contains a space, enclose the path in `"`. You can add arguments after it, separated by spaces.
- You cannot use `.cmd` or `.bat` files. For a language server installed with npm, write the path of `node.exe` followed by the path of the script.
- Close Nib before you edit the file by hand. If you change it while Nib is open, Nib shows "The settings file was changed by another process" and does not overwrite the file.
- If you make a mistake in the format, Nib shows "The settings file format is not valid", and you cannot save settings until you fix it.

The **Language Server** section of **Settings** lists language servers for C / C++, Python, TypeScript / JavaScript, Rust, Go, C#, Lua, shell, JSON, CSS, HTML, YAML, Dockerfile, PHP, and Markdown. The ones that have been tested are clangd, rust-analyzer, gopls, and pyright.

One language server runs for each language, and it keeps running when you switch tabs. C and C++, and TypeScript and JavaScript, which run with the same command, share one language server within the same project. The extent of a project is decided by searching upward from the document's folder for a marker file such as `.git` or `Cargo.toml`.

Renaming has these limits.

- It does not make a change that spans more than 32 files, or a change that creates, renames, or deletes files.
- Among files that are not open, it does not change files that are not UTF-8 without a BOM, or files larger than 4 MiB.
- Names in files that the language server has not read yet are not renamed. If you rename right after the language server starts, check the number of changed files in the message, such as "Places changed: 12 in 3 files".
- If a tab affected by the change has unsaved changes and its contents have not been sent to the language server yet, the whole rename is not done. Examples are a tab that was open before the language server started, a tab handled by a different language server, and a tab that has never been shown. Save that tab, then try again.

Next: [Themes and appearance](07-themes.md) covers how to change colors, fonts, and display.
