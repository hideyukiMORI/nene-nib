# NeNe Nib

A fast single-executable text editor for Windows 11 that toggles between ordinary editing and Vim editing.
C++23, plain Win32, Direct2D and DirectWrite, no UI library, no runtime dependency.

> **Status (2026-09-23):** ordinary editing, file open/save, Japanese IME, persisted settings with nine
> built-in themes, and the Vim NORMAL / INSERT / VISUAL core (motions, operators, text objects, search with
> highlighting, `.` repeat, blockwise VISUAL) work. One tab only; the Ctrl+P file/history lists, Markdown preview,
> bookmarks and files above 64 MiB are still planned. Nothing to download yet (Phase 4).

## What works today

Each line is one requirement of [SPECIFICATION.md](SPECIFICATION.md) (FR-NNN); the decisions behind it are in
[`docs/adr/`](docs/adr/README.md). The numbers below are as of `main` on 2026-09-23 and are kept in
`docs/todo/current.md`; the measurements behind them are in `docs/quality/gate-proofs.md`.

- **FR-001 one `.exe`, no runtime dependency** — C++23 with clang-cl, Win32, Direct2D / DirectWrite. Done.
- **FR-002 ordinary editing** — piece table, multi-line, scrolling, selection, Ctrl+C/X/V, Ctrl+Z/Y, click to place the caret. Done.
- **FR-003 Vim editing** — NORMAL / INSERT / VISUAL from scratch, replayed against real Vim 9.1: 1339 oracle-generated
  fixtures in `tests/vim/` are checked by CTest. Implemented: `h j k l 0 $ ^ w b e gg G`, Home / End,
  `f F t T ; ,`, `H M L`, Ctrl-d/u/f/b, PgUp / PgDn, counts (operator × motion), `x r`, `d c y` + motion, `dd cc yy`,
  `D C Y`, `p P` with a typed unnamed register, `i a I A o O` with counts, `.` (also in VISUAL), text objects
  `iw aw iW aW i" a" i' a' i( a( i{ a{ i[ a[ i< a<` and the backtick pair (`b` / `B` aliases), search `/ ? n N * #`
  with `hlsearch` on by default (`:noh`, `:set (no)hlsearch`), virtual columns (Tab = 8, wide = 2), `u` / Ctrl-r,
  `v V` and blockwise `Ctrl-v` (`o O $`, `d x y r`, block `p P`, `.`), literal CR in LF files, Esc everywhere.
  Not yet: `it ip is`, `incsearch`, `:s :g`, named registers, macros `q @`, `J s S R`, `> < gu gU`, autoindent,
  drag-to-select, block `I A c C`, general Ex (`:w :q`, ranges, pipes, history).
- **FR-004 one toggle** — the status bar switches "通常 | Vim". Done.
- **FR-005 tabs in the title bar** — the tab band with one tab and the window controls; multiple tabs are planned.
- **FR-006 Ctrl+P** — lists the settings commands only (themes, `set fontsize=`, `set guifont=`); files, folders,
  bookmarks and history are planned.
- **FR-007 Markdown preview** — planned (md4c is not in the tree yet).
- **FR-008 encodings and line endings** — UTF-8, UTF-8 BOM, Shift_JIS; CRLF / LF kept as read; unsaved mark and
  "save?" prompt. Done.
- **FR-009 tab restore** — waits for multiple tabs.
- **FR-010 bookmarks and jump list** — planned.
- **FR-011 look** — frameless window with Mica title bar, follows the OS light / dark theme (aubergine dark, orange
  accent), one paint per frame. Done.
- **FR-012 Japanese IME** — IMM32; the composition is drawn in place and committed as one intent; Vim NORMAL turns the
  IME off, INSERT turns it back on. Done.
- **FR-013 1 GB files** — planned; files above 64 MiB are not supported yet.
- **FR-014 Per-Monitor v2 DPI** — declared in the manifest; DIP to pixel conversion is integer-only in the core.
- **FR-015 speed** — five Release benchmarks in `eng/measure-speed.py` against per-machine baselines in
  `eng/perf-reference.json` (startup 191 ms, window visible 35 ms, one keystroke 0.9 ms, 16 MiB file 250 ms on the
  reference machine). Run on demand and in `-Full`; the required CI check does not measure.
- **FR-016 colorscheme** — nine built-in themes plus up to 128 user themes from
  `%LOCALAPPDATA%/NeNeNib/themes/<name>.v1.theme`; `:colorscheme <name>` / `system`, Tab completion, Ctrl+P. Done.
- **FR-017 font size** — `Ctrl` + `+` / `-` / `0`, Ctrl + wheel (8–40 pt), `:set fontsize=` and `:set guifont=`,
  persisted in `%LOCALAPPDATA%/NeNeNib/settings.v1`. Done.

## Using the settings commands

In Vim NORMAL, press `:` and enter `colorscheme` to see the current theme, `colorscheme dracula`
to switch, or `colorscheme system` to follow Windows. `Tab` / `Shift+Tab` cycle completions.
Use `set fontsize=18` or `set guifont=Cascadia Code:h18` for the body font. `Enter` applies;
`Esc` cancels. Left/Right, Home/End, Backspace/Delete and single-line `Ctrl+V` edit the command.
Press `Ctrl+P` in either editing mode to choose the same commands from a filtered list.

The settings file format and failure behavior are specified in
[ADR 0020](docs/adr/0020-versioned-editor-settings-and-point-font-size.md); command behavior is in
[ADR 0022](docs/adr/0022-ex-command-line-and-settings-evaluation.md) and
[ADR 0023](docs/adr/0023-command-palette-and-shared-input-session.md).
Put a [user-theme file](docs/design/user-theme-format.md) in the themes folder, restart, then choose it through
the same `colorscheme` command or Ctrl+P list. Invalid themes report their name and reason; a missing or broken
saved theme leaves the original settings intact and blocks settings writes until repaired and restarted. See
[ADR 0025](docs/adr/0025-user-theme-catalog-and-selection.md).

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

For a new Vim fixture group, regenerate just that group with
`python eng/vim-oracle.py --regenerate --only <name-prefix> --reuse-ref <commit>`.
The generator validates the previous inputs, measurement code and Vim environment before reusing
unchanged expectation rows. See [ADR 0026](docs/adr/0026-vim-character-search-and-scoped-oracle.md).
Write one fixture per line; `python eng/vim-oracle.py --format` stores `tests/vim/fixtures.json` in
its one canonical form without running Vim, and CNF-011 refuses any other spelling.

Full verification is opt-in and requires a concrete reason why narrower checks cannot cover the change:

```powershell
pwsh -NoProfile -File ./eng/check.ps1 -Full -Reason 'Concrete scope and reason'
```

It needs Visual Studio Build Tools with the pinned MSVC toolset and the bundled LLVM 19.1.5, CMake, Ninja and
Python 3.12 (`eng/tool-versions.json`). Run `pwsh -NoProfile -File ./eng/bootstrap.ps1` once after cloning.
The Phase 0 measurements can be reproduced with `pwsh -NoProfile -File ./eng/measure-language.ps1`.

## License

MIT. See [LICENSE](LICENSE).
