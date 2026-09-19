# NeNe Nib

A fast single-executable text editor for Windows 11 that toggles between ordinary editing and Vim editing.
C++23, plain Win32, Direct2D and DirectWrite, no UI library, no runtime dependency.

> **Status (2026-09-20):** ordinary editing, file open/save, Japanese IME and the first Vim slices
> (motions, operators, visual selection and viewport navigation) work. Body font size, font family and
> theme settings persist between launches and can be changed from the Vim command line or Ctrl+P.
> Multiple tabs and the Ctrl+P file/history lists are still planned.
> Nothing to download yet (Phase 4).

Use `Ctrl` + `+` / `-` to change body size, `Ctrl+0` to reset to 13.5 pt, or `Ctrl` + mouse wheel
in either editing mode (8–40 pt). The title and status bar keep their size.
In Vim NORMAL, press `:` and enter `colorscheme` to see the current theme, `colorscheme dracula`
to switch, or `colorscheme system` to follow Windows. `Tab` / `Shift+Tab` cycle completions.
Use `set fontsize=18` or `set guifont=Cascadia Code:h18` for the body font. `Enter` applies;
`Esc` cancels. Left/Right, Home/End, Backspace/Delete and single-line `Ctrl+V` edit the command.
This settings command line does not yet support ranges, pipes, history, `:w` or `:q`.

Press `Ctrl+P` in either editing mode to choose a settings command. Type part of a theme name
(for example `drac`) to filter, use Up/Down or Tab/Shift+Tab to select, and press Enter or click
to choose. Font-setting candidates fill the input so a value can be entered before execution.
Esc, Ctrl+C, Ctrl+P again or a click outside closes the list and preserves the body selection.
This first command palette does not yet list files, folders, bookmarks or history.

Settings live in `%LOCALAPPDATA%/NeNeNib/settings.v1`, created on the first setting change.
The format and failure behavior are specified in
[ADR 0020](docs/adr/0020-versioned-editor-settings-and-point-font-size.md); command behavior is in
[ADR 0022](docs/adr/0022-ex-command-line-and-settings-evaluation.md) and
[ADR 0023](docs/adr/0023-command-palette-and-shared-input-session.md).

## What it will be

- One `.exe`, starts instantly, draws with the GPU (Direct2D / DirectWrite, DXGI flip model, Mica).
- Ordinary editing like Notepad, or Vim editing (vanilla Vim defaults first), switched by one key.
  The Vim engine is written from scratch and verified against a real headless Vim.
- Tabs in the title bar, `Ctrl+P` for open files, bookmarks, history and the current folder, a Markdown preview.
- UTF-8 by default with Shift_JIS detection, 1 GB files through memory mapping, Japanese IME from day one.

The requirements are in [SPECIFICATION.md](SPECIFICATION.md) (Japanese).

## How it is built

This repository follows the AYANE strict policy: **one meaning, one canonical implementation path, enforced by machines.**

- `docs/ARCHITECTURE_CONSTITUTION.md` — the rules (ARC-NNN) and how far C++23 / clang-cl reaches for each of them
- `docs/CODING_RULES.md` — the approved subset of C++ (CPP-NNN)
- `docs/QUALITY_GATES.md` — what is enforced right now (active / planned / impossible), the only source of truth
- `docs/quality/phase0-results.json` — the 114 compiler, linker and tool measurements the rules are based on
- `docs/adr/` — decisions: clang-cl over cl, Direct2D over GDI, one UI thread plus one worker, an own Vim engine, a speed gate

Select the smallest checks for the changed behavior and its direct dependencies/callers, and record
the regression rationale and results in the PR. Reuse successful results when the relevant inputs
are unchanged; CI validates Git conventions, verification records and whitespace without repeating
product tests. Documentation-only changes need no application tests. See `docs/QUALITY_GATES.md`.

Full verification is opt-in and requires a concrete reason why narrower checks cannot cover the change:

```powershell
pwsh -NoProfile -File ./eng/check.ps1 -Full -Reason 'Concrete scope and reason'
```

It needs Visual Studio Build Tools with the pinned MSVC toolset and the bundled LLVM 19.1.5, CMake, Ninja and
Python 3.12 (`eng/tool-versions.json`). Run `pwsh -NoProfile -File ./eng/bootstrap.ps1` once after cloning.
The Phase 0 measurements can be reproduced with `pwsh -NoProfile -File ./eng/measure-language.ps1`.

## License

MIT. See [LICENSE](LICENSE).
