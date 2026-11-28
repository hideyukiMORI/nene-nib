# Themes and appearance

In NeNe Nib, you can change the theme (the set of colors for the text and the window), the font size, the font, and how text is displayed. The easiest way to change any of these is from **Settings**, which opens on the right when you press `Ctrl+,`. This chapter explains how to make changes in **Settings**, and the `:set` and other commands that do the same thing. How to type commands is explained in detail in the [Settings](08-settings.md) chapter.

> By default, the colors follow the Windows light or dark mode. Once you choose a theme yourself, Nib keeps that theme even when you change the Windows mode. To follow Windows again, choose **Match Windows** in the list of themes.

## Choose a theme

Nib comes with 125 themes.

| To do this | Do this | What happens |
|---|---|---|
| Choose a theme from a list | In the **Display** section of **Settings** (`Ctrl+,`), click the value of **Color Scheme** or press `Enter` | The list of themes opens. When you choose one, the theme changes right away. |
| Compare colors as you choose | Press `Ctrl+P` and type `:colorscheme` followed by a space | Theme names are listed. As you move with `↑` / `↓`, the window temporarily takes that theme's colors. Press `Enter` to confirm. |
| Choose a theme by name | In NORMAL in Vim mode, type `:colorscheme dracula` and press `Enter` | The theme you typed is applied. You can complete the name with `Tab`. |
| Check the current theme | In NORMAL in Vim mode, type `:colorscheme` and press `Enter` | The name of the current theme appears. |
| Follow Windows again | Choose **Match Windows** in the **Color Scheme** list, or type `:colorscheme system` | The colors follow the Windows light or dark mode again. |

<!-- Screenshot: the Color Scheme list in Settings (to be added) -->

- In **Color Scheme** in **Settings**, you can also press `←` or `→` to switch to the previous or next theme, one at a time.
- The theme you choose is saved and is still in effect when you start Nib again.
- Theme names stay in English, whatever the display language.

If you are not sure which to pick, try one of these eight.

| Background | Themes (the name to type after `:colorscheme`) |
|---|---|
| Dark | Alloy Core (`alloy-core`), Code Rain (`code-rain`), MonaLisa (`monalisa`), Red (`red`) |
| Light | Rosé Pine Dawn (`rose-pine-dawn`), Everforest Light (`everforest-light`), Gruvbox Light (`gruvbox-light`), GitHub Light (`github-light`) |

## Change the font size

| To do this | Do this | What happens |
|---|---|---|
| Make text larger or smaller | `Ctrl++`, `Ctrl+-`, or turn the mouse wheel while holding `Ctrl` | Changes the font size of the text in 0.5 pt steps. |
| Restore the default size | `Ctrl+0` | Returns to 13.5 pt. |
| Change it in **Settings** | `←` or `→` on **Font Size** in the **Text** section of **Settings** | Changes the size in 0.5 pt steps. |
| Set the size as a number | `:set fontsize=18` | Sets the text to 18 pt. |

You can set the size between 8 and 40 pt. The size you set is saved.

## Change the font

By default, the text font is Cascadia Code. To change it, type the font name and the size together.

| To do this | Do this | What happens |
|---|---|---|
| Change the font | `:set guifont=Consolas:h14` | Sets the text font to Consolas at 14 pt. |

- After `:h`, write the font size in pt. You cannot leave it out.
- You can write a font name that contains spaces as it is.
- **Font** in the **Text** section of **Settings** shows the name of the current font. You cannot change the font in **Settings**, so use `:set guifont=`.

## Show relative line numbers

Line numbers are always shown on the left of the text. There is no setting to hide them.

| To do this | Do this | What happens |
|---|---|---|
| Show line numbers relative to the caret line | `:set relativenumber` (short form `:set rnu`) | The line with the caret shows its actual line number, aligned left. Other lines show how many lines away they are from the caret line. |
| Go back | `:set norelativenumber` (short form `:set nornu`) | All lines show their actual line numbers. |

Relative line numbers are a handy guide when you move by a number of lines in Vim mode, such as `5j`. This setting is displayed the same way in Basic mode.

## Turn word wrap on or off

By default, long lines wrap at the window width. Wrapping affects only the display. No line breaks are added to the text.

| To do this | Do this | What happens |
|---|---|---|
| Turn off wrapping | Turn off **Wrap Lines** in the **Editing** section of **Settings**, or type `:set nowrap` | Long lines stay on one line and extend past the right edge. |
| Turn wrapping back on | Turn on **Wrap Lines**, or type `:set wrap` | Lines wrap at the window width. |

When lines are wrapped, the caret moves differently depending on the mode.

- In Basic mode, `↑` / `↓`, `Home`, `End`, `PgUp`, and `PgDn` treat each line on the screen as one line.
- In Vim mode, `j`, `k`, `0`, and `$` treat each line of the text, before wrapping, as one line. To move by screen lines, use `gj`, `gk`, `g0`, and `g$`.
- The wrap setting applies to all tabs. It is saved and is still in effect when you start Nib again.

## Use syntax coloring

In programs and configuration files, Nib identifies the language from the file extension and colors keywords, strings, and so on. This is called syntax coloring. The colors are set by the theme you have chosen.

Syntax coloring is available for these 25 languages:

C, C++, C#, Rust, Go, Python, JavaScript, TypeScript, JSON, YAML, TOML, INI, XML, HTML, CSS, Markdown, PHP, SQL, Lua, PowerShell, shell, CMake, Dockerfile, GDScript, Godot resources

Java, Kotlin, Dart, Zig, Terraform, Makefile, Batch, Ruby, Swift, Scala, Haskell, Elixir, R, Perl, Vue, and Svelte are identified as languages and shown in the status bar, but they are not colored.

- The language of the current document is shown on the right side of the status bar.
- To set the language again, type something like `:set filetype=python`. You can also write it as `:set ft=python`.
- To turn off colors for the current document only, type `:syntax off`. To turn them back on, type `:syntax on`.
- Code blocks in the Markdown preview use the same syntax coloring. `:syntax off` does not affect the preview.

## Show indent guides and matching brackets

| Feature | Do this | What happens |
|---|---|---|
| Indent guides (vertical lines at each indent level) | Turn on **Indent Guides** in the **Display** section of **Settings**, or type `:set indentguides` | Draws a vertical line at each indent level. Off by default. |
| Matching bracket highlight | Cannot be changed (always on) | Outlines the bracket that pairs with the bracket at the caret. |

By default, indent guides use a different color for each level (rainbow colors), and the line for the level the caret is in is drawn thicker and stronger. You can change both in the **Display** section of **Settings**.

| Settings item | Values | Command |
|---|---|---|
| **Indent Guide Colors** | **Rainbow**, **One color** | `:set indentguidestyle=rainbow`, `:set indentguidestyle=plain` |
| **Rainbow Color Count** | 3 to 6 (default: 4) | `:set indentguidecolorcount=5` and so on |
| **Highlight Caret Level** | **On**, **Off** | `:set indentguidehighlightcurrent`, `:set noindentguidehighlightcurrent` |
| **Indent Width** (the width of each indent guide level) | 2, 4, 8 (default: 4) | `:set indentguidewidth=2` and so on |

**Indent Width** is how many characters apart the indent guide lines are drawn. It is used when Nib cannot work out the width from the indentation of the opened document. The value is used only to place the indent guide lines. It is separate from the indentation of the text itself (always tab characters, 8 characters wide; see the [Vim editing](03-vim.md) chapter), and changing this value does not change the indentation of the text.

## Show the minimap

The minimap is a small overview of the whole document, shown at the right edge of the window. The part you can currently see is highlighted.

- Turn on **Minimap** in the **Features** section of **Settings**, or type `:set minimap` to show it. It is off by default. To hide it, type `:set nominimap`.
- Click the minimap to scroll so that the line you clicked is in the middle of the screen. Drag on it to scroll along with the mouse. The caret does not move.

## Add effects (caret motion and typing sparks)

You can add small effects to caret movement and to typing. Both are off by default, and you can turn each one on or off separately.

| Effect | Turn on | Turn off | What happens |
|---|---|---|---|
| Caret motion | Turn on **Caret Motion** in the **Editing** section of **Settings**, or type `:set caretmotion` | Turn off **Caret Motion**, or type `:set nocaretmotion` | The caret slides smoothly with a short trail when it moves. |
| Typing sparks | Turn on **Typing Sparks** in the **Editing** section of **Settings**, or type `:set caretsparks` | Turn off **Typing Sparks**, or type `:set nocaretsparks` | When you type a character, a few small sparks scatter from where you typed and fade away. |

<!-- Screenshot: caret motion and typing sparks (to be added) -->

- You can change how long the caret takes to move with **Caret Motion Time** in **Settings**, or with a command such as `:set caretmotiontime=200`. The range is 40 to 300 ms (default: 120 ms).
- Sparks fade in about 0.4 seconds. They do not appear when you paste, undo, or move the caret.
- Neither effect appears when Windows **Animation effects** is off, or while the input method (IME) is composing text.

## Going further: Add your own themes and compare themes

You can add up to 128 themes of your own. Put a theme file named `<name>.v1.theme` in the `%LOCALAPPDATA%\NeNeNib\themes` folder and restart Nib. The theme then appears in `:colorscheme` and in the **Color Scheme** list. How to write the file is described in `docs/design/user-theme-format.md` in the GitHub repository.

- If a theme file cannot be read, Nib shows the file name and the reason.
- If the file of your own theme that you had chosen is missing or broken, Nib does not rewrite the settings file, and it does not save changes to settings until you fix the theme and restart.

To compare the colors of the 125 themes, save `docs/design/colorschemes.html` from the GitHub repository and open it in a browser. You can filter by name or brightness, and it opens without an internet connection. The syntax coloring shown in this gallery is only a sample and is not the same as Nib's syntax coloring.

Next: [Settings](08-settings.md) covers the **Settings** panel and the settings commands together.
