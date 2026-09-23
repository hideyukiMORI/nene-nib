# いまのタスク — NeNe Nib

> GitHub Issue が正。ここは要約であり、Markdown のチェックリストをタスク状態として扱わない。
> 更新は実測でだけ行う。検証は差分から選び、関連入力が不変の成功結果を再利用する（QLT-001 / QLT-012・[ADR 0021](../adr/0021-diff-scoped-verification-and-result-reuse.md)）。
> Issue ごとの経緯は[日報](../reports/)、コマンドと数字は [gate-proofs](../quality/gate-proofs.md)。ここには書かない（Issue #124）。

## 運用（2026-09-23 施主指示）

- 背景席は仕事の種類で `model` を明示する（実装＝Opus・下ごしらえ＝Sonnet・機械作業＝Haiku・裁定と受理は設計席）。繰り返す手順は `eng/` のスクリプト（[ADR 0038](../adr/0038-model-per-seat-and-scripted-preparation.md)）。
- 実装席は probe → 実装 → 差し戻し対応を**それぞれ新しい席**にし（`SendMessage` の使い回しと `fork` は禁止）、道具出力を `out/` へ落とし、最終報告は 30 行以内。依頼書の型は [`docs/templates/implementation-seat-brief.md`](../templates/implementation-seat-brief.md)（[ADR 0039](../adr/0039-implementation-seat-per-step-and-small-tool-output.md)）。

## いまの Issue と次の順

| 順 | Issue | 状態 |
| --- | --- | --- |
| 1 | add バッファの chunk 化（ADR・未起票） | 16 MiB の 1 打鍵は 0.9 ms なので急がない |
| 2 | マクロ `q @`（ADR・未起票） | `.` の記録の型を再利用 |
| 3 | Tab の画素幅（ADR 0034 の 8 桁と DirectWrite の tab stop・未起票） | |
| 4 | `VimStep.cpp` の鍵の表の切り出し（ADR 0042 決定 6・次に表を触る Issue で） | |
| 5 | 複数タブ → Ctrl+P のファイル/履歴統合 → 一般 Ex | |

2026-09-23 に統合: #124・#131・#141・#130・#144・#146（usage の集計）・#117（`^M` の描画・ADR 0040）・#151・#147（C1 の 4 桁）・#152・#148（incsearch・ADR 0041・既定オンは施主決定 D18）・#162・#160（単体テストの分割・ADR 0042）・#165・#140（`assert_uncovered`・`measure`）・#168（Ctrl-G / Ctrl-T・ADR 0043）。open な Issue は 0。

順は設計席の案で hide 未確認。open の一覧は `gh issue list --state open` が正。

## いまの数字（main・2026-09-23）

| 項目 | 値 | 正本 |
| --- | --- | --- |
| Vim fixture | 1339 件 | `tests/vim/VimFixtures.hpp` の 5 行目（CNF-010） |
| 既定の `nib_tests` | 13712 checks・scope 21（1 scope = 1 翻訳単位・表は `NibTests.cpp`・ADR 0042） | [gate-proofs 5-aw](../quality/gate-proofs.md)。前後比較は `python eng/protected-diff.py --base <ref> --build`（#130・`out/protected/<短い SHA>.json`） |
| ADR | 0043 まで | [`docs/adr/README.md`](../adr/README.md) |
| 見た目の確認 | `python eng/verify-window.py [--open <file>] [--vim] --capture <dir> --keys "<鍵>"` → PNG を Read で見る・`eng/compare-frames.py --regions --expect`。撮影は同じ機械で 1 席ずつ（覆われると `covered` で終了 1・#140） | #131・[gate-proofs 5-al](../quality/gate-proofs.md) |
| 実機用 Release | `pwsh -NoProfile -File eng/build-release.ps1 -Ref main` → `build/release-<短い SHA>/NeNeNib.exe` と `out/release/<短い SHA>.json`（起動は設計席） | [ADR 0038](../adr/0038-model-per-seat-and-scripted-preparation.md) 決定 5・#129 |
| 席の消費 | `python eng/usage-report.py --since <日付>` → 席ごとの turns・最大文脈・cache_read・seat_tokens | #146・[gate-proofs 5-an](../quality/gate-proofs.md) |
| 速さ（実機） | 起動 191 ms・窓 35 ms・1 打鍵 0.9 ms・16 MiB 250 ms | `eng/perf-reference.json`（ADR 0016） |

既知の既存失敗: `eng/test-conformance.py` の `test_verification_policy.py` の一部は cp932 の端末で pwsh の出力が読めず落ちることがある（道具側は #106 で直した。残れば別 Issue）。

## 動かないもの

64 MiB 超のファイル・文字コードと改行の手動切り替え・IME の再変換と TSF 固有の機能・
VISUAL の `p u ~ > < J I A gv` と `X D C Y`・ドラッグで VISUAL・矩形の `c I A C > < J ~` と VISUAL の中の `p`・`virtualedit`・
autoindent・名前つきレジスタ（`"a` の接頭辞と `"ap`）・マクロの中の Ex と録画中の表示・`J s S R`・r の制御文字・Ctrl-e/y・検索の `:s` `:g`・履歴・offset・`\v` `\c` `\(` `\|` `\{`・`ignorecase`・
テキストオブジェクト `it ip is`・一般 Ex（`:w` / `:q`、範囲、パイプ、履歴）・
複数タブ・Ctrl+P のファイル/フォルダ/ブックマーク/履歴統合・Markdown プレビュー・折り返し・横スクロール・ドラッグ選択。

## 段階

| 段階 | 状態 |
| --- | --- |
| Phase 0 言語の実測 | ✅ 2026-09-15（114 記録・`docs/quality/phase0-results.json`） |
| Phase 1 文書とゲートの足場 | ✅ 2026-09-15（Issue #1・PR #2） |
| Phase 2 negative proof | ✅ 2026-09-15（26 反例・`docs/quality/gate-proofs.md`） |
| Phase 3 縦切り | 🔲 進行中。#3 窓（ADR 0007）✅ → #5 見た目（ADR 0008）✅ → #7 編集（ADR 0009）✅ → #11 ファイル（ADR 0010）✅ → #16 速さ（ADR 0011）✅ → #19 起動の内訳 ✅ → #22 Vim の最初の縦切り（ADR 0012）✅ → #24 窓を先に見せる（ADR 0013）✅ → #28 IME（ADR 0014）✅ → #31 タブの帯（D16）✅ → #30 計測器の揺れ ✅ → #36 欠測の言い方 ✅ → #44 生成物の SHA（CNF-010）✅ → #43 Vim の 2 本目（ADR 0015）✅ → #47 CI の速さの基準値（ADR 0016）✅ → #52 カラーテーマ C1（ADR 0017）✅ → #53 VISUAL（ADR 0018）✅ → #58 画面移動 ✅ → #60〜#70 設定と C2〜C4b ✅ → #72 / #76 / #79 / #84 / #87 / #93 / #100 / #91 / #108 / #112 / #85 / #123 Vim の縦切り ✅（ADR 0026〜0037）→ **#129 / #130 / #131 下ごしらえのスクリプト化（ADR 0038）進行中** |
| Phase 4 公開 | 🔲 |
