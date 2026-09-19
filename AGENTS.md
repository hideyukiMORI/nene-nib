# NeNe Nib — Agent Guide

Short English entry point. The authoritative handbook is [CLAUDE.md](CLAUDE.md) (Japanese);
the normative documents live in [docs/](docs/).

## Project identity

- Product: NeNe Nib — a fast single-executable text editor for Windows 11 that toggles between ordinary and Vim editing
- Language / UI: C++23 (clang-cl 19.1.5, MSVC toolset for the linker and SDK) / plain Win32 with Direct2D and DirectWrite
- Package root: `nenenib`
- Governing principle: **one meaning, one canonical implementation path, enforced by machines**

## Required reading before changing production code

0. `SPECIFICATION.md`
1. `docs/ARCHITECTURE_CONSTITUTION.md`
2. `docs/PROJECT_LAYOUT.md`
3. `docs/CODING_RULES.md`
4. `docs/QUALITY_GATES.md`
5. `docs/DEVELOPMENT_WORKFLOW.md`
6. `docs/COMMIT_CONVENTIONS.md`
7. `docs/GLOSSARY.md`

Then the active issue, the relevant accepted ADRs, and any active waivers.

## Agent rules

- Do not invent a second implementation path because it is locally convenient.
- Do not weaken a gate to make a change pass. Fix the code instead.
- Do not mark a rule `active` in `docs/QUALITY_GATES.md` before its enforcement exists.
- Do not read the current time, randomness, locale, environment, or files, and do not create threads, outside `src/adapters/win32`.
- Do not write `default` / `else` / `_` in a branch over a closed set; it disables exhaustiveness checking.
- Do not read `std::optional` through `*` or `->`; use `value()` / `value_or()`.
- Do not write SIMD intrinsics outside `src/core/simd/`, and always keep a scalar fallback.
- Do not add suppressions, lint baselines, or tool exclusions without an active waiver.
- Do not claim a command passed unless it was actually executed.
- Prefer the smallest change that fully follows the canonical path.

## Change-scoped verification

- Select the smallest checks that can catch regressions in the changed behavior and its direct dependencies/callers. Explain that risk for every selected check; otherwise do not run it.
- Reuse successful results while the implementation, tests, relevant dependencies and required environment remain unchanged. A new owner, phase, documentation edit or commit SHA alone is not a reason to repeat verification at push, review or merge.
- Documentation/comment/rule changes need no application runtime tests. Hook/tool changes need only focused checks of that tool.
- Full verification is exceptional: explain the scope and why narrower checks cannot cover it before using `pwsh -NoProfile -File ./eng/check.ps1 -Full -Reason '...'`. No routine approval request is required. Calling it without these arguments fails before running tools.
- Fix failures caused by this change. Record demonstrably unrelated existing failures with evidence in a separate issue and continue the current work. Never retry until green or start unrelated repairs/full reruns.
- Record scope, regression rationale, command/result/evidence and reuse rationale in the PR. CI checks Git conventions, this record and whitespace; hooks stay lightweight and do not repeat product tests.

The normative policy is QLT-001 / QLT-012 in `docs/QUALITY_GATES.md` and ADR 0021.
It supersedes older full-gate/final-HEAD instructions in reports, handoffs and ADRs.

## Required completion report

Issue and rule IDs, files and behavior changed, verification commands and results,
documentation or schema changes, active waiver IDs (or `none`), remaining risks.

Investigation-only requests do not authorize editing, committing, pushing, or opening PRs.
