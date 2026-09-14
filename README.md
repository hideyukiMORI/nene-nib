# NeNe Nib

A fast single-executable text editor for Windows 11 that toggles between ordinary editing and Vim editing.
C++23, plain Win32, Direct2D and DirectWrite, no UI library, no runtime dependency.

> **Status (2026-09-15):** Issue #1 only. The language was measured, the rules and the single gate are in place,
> and there is no product code yet. Nothing to download (Phase 4).

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

The only definition of done, locally and in CI:

```powershell
pwsh -NoProfile -File ./eng/check.ps1
```

It needs Visual Studio Build Tools with the pinned MSVC toolset and the bundled LLVM 19.1.5, CMake, Ninja and
Python 3.12 (`eng/tool-versions.json`). Run `pwsh -NoProfile -File ./eng/bootstrap.ps1` once after cloning.
The Phase 0 measurements can be reproduced with `pwsh -NoProfile -File ./eng/measure-language.ps1`.

## License

MIT. See [LICENSE](LICENSE).
