# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う（「動くもの」は `pwsh -NoProfile -File ./eng/check.ps1` が通ったもの）。

## 現在の Issue

[Issue #1](https://github.com/hideyukiMORI/nene-nib/issues/1) — C++23 / Win32 の実測を Loupe から再現し、厳格規約の足場を置く（Phase 0〜2）。
製品コードは書かない。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`。Loupe の 19 ケースを再現し、clang-cl・C++23・SIMD・並行性・DirectX・md4c・Vim を足した） |
| Phase 1 文書とゲートの足場 | 🔲 進行中（Issue #1） |
| Phase 2 negative proof | 🔲 |
| Phase 3 縦切り 1 本 | 🔲 |
| Phase 4 公開 | 🔲 |

## 動くもの

（まだ無い。検査基盤の緑は Nib が動くことを示さない）

## 動かないもの

（製品コードが無い）

## 次の 1 手

Issue #1 の PR をフルゲートと CI の必須 check が通った状態で Ready にし、施主の確認を受けて squash merge する。
その後の最初の縦切り（Phase 3）は「起動して枠なし窓に Direct2D で 1 行描く」を焦点 Issue にし、そこで
ARC-002 / ARC-003 / ARC-007 / CPP-013 のシンボル検査を `--require core application` で結線する。
