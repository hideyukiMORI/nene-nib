# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

[Issue #3](https://github.com/hideyukiMORI/nene-nib/issues/3) — 起動して枠なし窓に Direct2D で 1 行描き、OS のライト／ダークに従う（最初の縦切り）。ADR 0007。
[Issue #1](https://github.com/hideyukiMORI/nene-nib/issues/1) は PR #2 で main へ統合済み。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`。Loupe の 19 ケースを再現し、clang-cl・C++23・SIMD・並行性・DirectX・md4c・Vim を足した） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2・フルゲートと CI 緑） |
| Phase 2 negative proof | ✅ 2026-09-15（25 反例・`docs/quality/gate-proofs.md`。ruleset と CI の実行証拠は PR #2 で取った） |
| Phase 3 縦切り 1 本 | 🔲 進行中（Issue #3・ADR 0007。枠なし窓に Direct2D で 1 行・OS のライト／ダーク） |
| Phase 4 公開 | 🔲 |

## 動くもの

（まだ無い。検査基盤の緑は Nib が動くことを示さない）

## 動かないもの

（製品コードが無い）

## 次の 1 手

Issue #3 の実装をフルゲート・実機確認（`eng/verify-window.py`）・CI で通し、ARC-002 / ARC-003 / ARC-007 / CPP-013 / QLT-009 / CNF-007 を active にして squash merge する。
その次は見た目の縦切り（自前タイトルバーとタブ・ステータスバーのトグル・Mica・茄子色のダーク D11）で、実装の前に `/design` で案を作って施主が選ぶ（施主指示 2026-09-15）。
