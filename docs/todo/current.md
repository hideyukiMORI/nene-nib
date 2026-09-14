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
見た目の縦切り Issue #5（ADR 0008・PR #6）は main へ統合済み。
編集の縦切り Issue #7（ADR 0009）はフルゲートと実機確認を通し、PR で統合する。その次はファイルの開閉と保存（Ctrl+O / Ctrl+S・D8 の文字コード判別・改行の保持）で、`/design` は挟まない。
カラーテーマ（D13）は `docs/plans/2026-09-15-colorschemes.md` の C1〜C4 の順で、C1（テーマの模型と組み込み 9 テーマ・core だけ）はいつでも着手できる。
