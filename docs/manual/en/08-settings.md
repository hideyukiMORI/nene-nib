# Settings

You can change NeNe Nib's settings in two ways. You can use the **Settings** panel, which opens on the right when you press `Ctrl+,` and lets you see the items and change them with the mouse or the keyboard. Or you can use `:set` and related commands, which also reach items that are not in **Settings**. This chapter explains how to use both, and lists the main settings.

> Items that are not in **Settings** (such as case-insensitive search) are changed with settings commands. You can use commands in Basic mode too: press `Ctrl+P` and type `:`.

## Use the Settings panel

| To do this | Do this | What happens |
|---|---|---|
| Open **Settings** | `Ctrl+,` (or **Settings** in **All Commands** (`F1`)) | **Settings** opens on the right of the window. The text stays visible. |
| Choose an item | `↑` / `↓` (or `Tab`) | The chosen item is outlined. |
| Turn an item on or off | Press `Space` or click the switch | The setting changes right away. |
| Change a value one step at a time | `←`, `→` | Values such as the color scheme or the font size change to the previous or next value. |
| Choose from a list of values | Click the value, or press `Enter`, `Space`, `Alt+↓`, or `F4` | A list opens. Choose a value with `↑` / `↓` and press `Enter` to apply it. |
| Go to a section | `Ctrl+1` to `Ctrl+6` | Moves to the **Display**, **Text**, **Editing**, **Panes**, **Features**, or **Language Server** section. |
| Find an item | Type in **Search settings** at the top | Only items whose name, description, or `:set` name matches are shown. |
| Go back to the settings you had when you opened it | Click **Revert** | Restores the state from when you opened **Settings**. |
| Close | `Esc`, `Ctrl+,`, **Close**, ✕, or click outside **Settings** | Closes **Settings** and saves your settings. |

<!-- Screenshot: the whole Settings panel (to be added) -->

- Changes appear on the screen right away. They are saved when you close **Settings**.
- **Settings** also closes and saves when you click the text, type, or switch tabs. Scrolling the text with the mouse wheel, or turning the wheel while holding `Ctrl`, does not close it.
- While you are searching for an item, `Space` types a space in the search text.
- A list of values shows up to 10 rows, and the current value has a ✓. `Esc`, `Alt+↑`, and `F4` close only the list. `Tab` applies the chosen value and closes the list. Type the first letter of a value to move to a value that starts with it.

### Find items by section

| Section | Items |
|---|---|
| **Display** | **Color Scheme**, **Display Language**, **Shortcut Guide**, **Indent Guides** and their colors and width |
| **Text** | **Font** (the font name; display only), **Font Size** |
| **Editing** | **Wrap Lines**, **Auto Indent**, **Caret Motion**, **Caret Motion Time**, **Typing Sparks** |
| **Panes** | **Explorer** (the file tree pane), **Finder Preview** (the preview in **Files and Tabs**), **Pane Resize Step**, **Vim Pane Resize Step** |
| **Features** | **Matching Brackets** (always on), **Minimap**, **Language Server (LSP)** |
| **Language Server** | A language server for each language (explained in the [Language servers](06-language-servers.md) chapter) |

### Balance features and speed

Above the **Features** section, you see "Estimated time per keystroke with these settings". This is an estimate of the time from one key press until the result appears on the screen. It is a base time measured on the developer's PC, plus the time for each feature you have turned on. The lower the estimate, the more quickly the screen keeps up with your typing, so it helps you decide which features to turn on. Each feature row shows its effect on speed as a number of ● marks. The number of ● marks is also based on numbers measured on the developer's PC. These are not times measured on your PC.

You can switch features as a group with the three buttons below it.

| Button | What happens |
|---|---|
| **Speed First** | Turns on only the features that do not affect speed. |
| **Recommended** | Turns on features up to those with a small effect. This is the default. |
| **All On** | Turns on all features. |

## Use settings commands

Where you type a command depends on the mode.

| Mode | Do this |
|---|---|
| Vim mode | In NORMAL, press `:`, and `:` appears at the bottom of the window. Then type something like `set ignorecase` and press `Enter`. |
| Basic mode | Press `Ctrl+P` and type `:` to show the list of commands. Then type something like `set ignorecase` to narrow the list, and press `Enter`. |

The `:` list in **Files and Tabs** works in Vim mode too. You do not have to remember every command: type `:set` and the available commands are listed. You can also type a command with a value, such as `:set scrolloff=3`, and press `Enter` to run it.

- On the `:` line in Vim mode, you can complete command and theme names with `Tab` and `Shift+Tab`. Press `Esc` to stop typing.
- Add `?` at the end, as in `:set ignorecase?`, to see the current value.
- Add `&` at the end, as in `:set ignorecase&`, to restore the default value.
- You can put two or more settings on one line, as in `:set ic scs`.
- Many settings can also be typed with the same short names as in Vim, such as `ic` (`ignorecase`).
- Settings commands take effect right away. Settings that are saved are saved at that moment.

## Search without matching case

By default, search matches uppercase and lowercase exactly. To search without matching case, turn on `ignorecase`. This setting is not in **Settings** or in **All Commands**, so you change it with a command.

### Change it in Basic mode

1. Press `Ctrl+P`.
2. Type `:set ignorecase`. "set ignorecase" remains in the list.
3. Press `Enter`.

Searches with `Ctrl+F`, `F3`, and `Shift+F3`, and the text you search for in replace with `Ctrl+H`, now ignore case. To go back, follow the same steps and choose `:set noignorecase`.

### Change it in Vim mode

In NORMAL, type `:set ignorecase` (short form `:set ic`) and press `Enter`. Searches with `/`, `?`, `n`, and `N` now ignore case. To go back, type `:set noignorecase` (`:set noic`).

### Match case only when you type uppercase (smartcase)

If you turn on `smartcase` (short form `scs`) in addition to `ignorecase`, search matches case only when the search text contains at least one uppercase letter. For example, `nib` matches both `Nib` and `NIB`, while `Nib` matches only `Nib`. `smartcase` works only when `ignorecase` is on.

| Search text | `ignorecase` only | `ignorecase` and `smartcase` |
|---|---|---|
| `nib` | Matches `nib`, `Nib`, and `NIB` | Matches `nib`, `Nib`, and `NIB` |
| `Nib` | Matches `nib`, `Nib`, and `NIB` | Matches only `Nib` |

Both settings are saved and are still in effect when you start Nib again. Project search with `Ctrl+Shift+F` always ignores case, whatever these settings are.

## Look up the main settings

Settings that have a name in the "Settings item" column can also be changed in **Settings**.

### Search

| To do this | Command | Settings item | Default |
|---|---|---|---|
| Search without matching case | `:set ignorecase` (`ic`) | None | Off |
| Match case only when you type uppercase | `:set smartcase` (`scs`) | None | Off |
| In Vim mode search, highlight every match | `:set hlsearch` (`hls`), `:set nohlsearch` | None | On |
| In Vim mode search, show matches while you type | `:set incsearch` (`is`), `:set noincsearch` | None | On |
| In Vim mode search, continue from the beginning after reaching the end of the document | `:set wrapscan` (`ws`), `:set nowrapscan` | None | On |

### Display

| To do this | Command | Settings item | Default |
|---|---|---|---|
| Wrap long lines | `:set wrap`, `:set nowrap` | **Wrap Lines** | On |
| Show line numbers relative to the caret line | `:set relativenumber` (`rnu`) | None | Off |
| Always keep N lines visible above and below the caret | `:set scrolloff=3` (`so=3`) | None | 0 |
| Show shortcut hints in empty documents and the status bar | `:set guide`, `:set noguide` | **Shortcut Guide** | On |
| Change the display language | `:set language=ja`, `en`, `zh-hans`, `system` | **Display Language** | **Match Windows** |
| Change the theme | `:colorscheme <name>` | **Color Scheme** | **Match Windows** |
| Change the font size | `:set fontsize=18` | **Font Size** | 13.5 pt |
| Change the font | `:set guifont=<name>:h<size>` | None (**Font** is display only) | Cascadia Code |
| Show the Explorer on the left | `:set explorer`, `:set noexplorer` | **Explorer** | Off |

### Editing and features

| To do this | Command | Settings item | Default |
|---|---|---|---|
| In Vim mode, copy the indent of the previous line on a new line | `:set autoindent` (`ai`) | **Auto Indent** | Off |
| Use language servers | `:set lsp`, `:set nolsp` | **Language Server (LSP)** | Off |
| In Vim mode, use the Windows clipboard for copy and paste | `:set clipboard=unnamed` | None | Off |

The settings listed here are saved and are still in effect when you start Nib again. However, these four are not saved. They return to their defaults when you start Nib again.

- `hlsearch`
- `incsearch`
- `wrapscan`
- `clipboard`

- `scrolloff` is a number from 0 to 999, and it works in both Basic mode and Vim mode.
- `autoindent` works on line breaks in INSERT in Vim mode, and with `o`, `O`, `cc`, and `S`.
- The settings for themes, font size, font, wrapping, line numbers, and effects are explained in the [Themes and appearance](07-themes.md) chapter.
- You can also change the display language with **Choose Display Language** in **All Commands** (`F1`). `:set language?` shows the current setting and the language actually in use.
- `:set number` and `:set nonumber` are not available. Line numbers are always shown.

## Save settings

- Settings you change in **Settings** are saved when you close **Settings**.
- Settings you change with `:set` or `:colorscheme` are saved at that moment.
- If settings cannot be saved, a one-line message appears. If they cannot be saved when you close **Settings**, Nib tries again when you close the window.
- Settings are saved in `settings.v6` in `%LOCALAPPDATA%\NeNeNib`. The location is explained at the end of the [Installing and first start](01-install.md) chapter.

## Going further: Use settings files from older versions

- If there is no `settings.v6`, Nib reads the settings file of an older version, in this order: `settings.v5` from 1.1.0, then `settings.v4` from 1.0.0. It does not rewrite the older file it read, and it creates `settings.v6` the first time you change a setting.
- If you change settings in 1.1.0 after `settings.v6` has been created, the change goes into `settings.v5` and does not reach 1.2.0. The same is true the other way around.
- The settings file can be up to 16 KiB.
- If the settings file is broken, Nib shows a message and starts with the settings left unchanged, and it does not save settings until you fix the file and restart.
- Key mappings you create with `:map` in Vim mode are also saved in `settings.v6`.

Next: [Large files and encodings](09-large-files-and-encodings.md) covers how Nib handles large files and encodings such as Shift_JIS.
