# 完了報告 — PR #1946（#1939 LSP 段 S5）のレビュー R3 の差し戻し対応（席 B2）

Issue / 規則 ID: #1939・ADR 0161 の段 S5 の追補・ADR 0060 の `^` の追補・ARC-010（期待される失敗は例外にしない）・ARC-007（<atomic> を使わない）・CPP-002

## 変更したファイルと振る舞い

1. 推奨 1: `show_diagnostic_tip` が札を置くとき、全部の席の hover の要求を `cancel_lsp_requests` で取り消す。後から届いた古い答えは id が表に無いので捨てられ、札は消えない・上書きされない。
2. 推奨 2: 診断の列を `SharedDiagnostics`（`std::shared_ptr<const std::vector<Diagnostic>>`・新しい `src/application/SharedDiagnostics.hpp`）で持つ。`EditorState::diagnostics()` は `std::span` を返し、`DocumentState.diagnostics` も同じ参照。打鍵ごとの `with_*` は参照 1 つを写すだけ。`language_server_edit` はずらした列を 1 回だけ作って参照を差し替える（前は 2 回の深い写し）。作るのも読むのも UI の糸だけ（worker へ渡さない）。文の上限を 4,096 → 3,840 バイトにし、1 件 4,096 バイト × 1,000 件 ≤ 4 MiB を `static_assert` で守る。
3. 推奨 3: `]d` `[d` で跳べた・`gl` で札を出せたときは、controller の `vim_effect_landed_` を見て `stepped_vim` が engine の失敗を成功の歩に読み替える（engine は不変・fixture は不変）。マクロと map の右辺の残りが流れる。診断が無ければ失敗のまま。
4. 推奨 4: 鍵の効果の中の跳びは `PlaceCaret` を呼ばず `move_caret_to`（`placed_caret` の code point の単位）＋ `settle_vim_caret`。INSERT の `Ctrl-o` の始め直しは鍵の出口の 1 回だけ。操作（F8・一覧）は従来どおり `PlaceCaret`。
5. 軽い: F8 / Shift+F8 は IME の変換中は何もしない。`preview_target` の診断の帯の位置は範囲を確かめる（`diagnostic_preview_target` に分けた・関数の大きさの lint）。`PaletteDiagnostic` の注釈の根拠を直した。負の整数の `code` を 10 進にする（`JsonNumber.integer`）。
6. 文書: ADR 0161 の追補に持ち方・成功の歩・置き方・hover の取り消し・Ctrl+P の費用・`3"a]d` の限界・足した契約、ADR 0060 の追補に「どの記号で開いても作る」を 1 行。

新しい契約（`--diagnostic-jump` に 5 関数・35 → 50 checks）: `verify_tip_outlives_hover`・`verify_keys_succeed_in_macros`（`qa]dxq` `@a`・診断なしの打ち切り・`nnoremap Q ]dx`）・`verify_insert_pause_jump`（`i<C-o>]dZ<Esc>` と `u`）・`verify_f8_states`（IME・INSERT・オペレータ待ち）・`verify_negative_code`。

## 実行した検証コマンドと結果（Debug・作業木の build）

- nib_tests `--diagnostic-jump` 50・`--lsp-diagnostics` 112・`--language-server` 222・`--lsp-answers` 88・`--command-palette` 904・`--vim-text-objects` 2648 pass
- nib_tests `--every-scope` 2,442,824 checks pass・引数なし 2,427,889 checks pass（fixture の再生 `verify_vim_fixtures` を含む・前の 2,427,874 から本件の 15 だけ増えた）
- `ctest -R nib_probe_expectations` 1/1 pass
- nib_window_tests `--hover-tip` 18・`--diagnostics` 7・`--standard-keys` 75・`--vim-keys` 9 で 0 failures
- `eng/conformance.py --build-dir build` 0 violation・`eng/symbols.py --build-dir build --require core application` 0 violation・`eng/format-check.ps1` 2268 files formatted

## 費用の測定（推奨 2・軽い 2・目安）

使い捨ての作業木 `D:\NeNeNib\worktrees\lsp-s5-cost-before`（387fba61）と `lsp-s5-cost-after`（本件の差分）で、`tests/unit/DiagnosticJumpTests.cpp` を測定のファイル（`D:\NeNeNib\scripts\20261122\b2-diagnostic-cost\DiagnosticJumpTests.cpp`・steady_clock・リポジトリには入れない）に置き換え、Release の `nib_tests --diagnostic-jump` を 1 回ずつ（各 2 周）。1,000 行の文書・先頭に 1 字 × 200 打鍵（frame まで）・Ctrl+P を開いて閉じる × 20。同じ機械でほかの席がビルド中なので目安。

| 場合 | 1 打鍵 前 | 1 打鍵 後 | Ctrl+P 前 | Ctrl+P 後 |
| --- | --- | --- | --- | --- |
| 診断なし | 0.055〜0.058 ms | 0.079〜0.084 ms | 0.16〜0.23 ms | 0.23〜0.25 ms |
| 1,000 件・短い文 | 0.52〜0.72 ms | 0.47〜0.56 ms | 1.7〜1.8 ms | 2.1〜2.5 ms |
| 1,000 件・各約 3,000 バイト | 3.0〜5.2 ms | 1.3〜1.4 ms | 3.8〜6.0 ms | 2.5〜2.7 ms |

診断なしの差（約 25 µs）は本件の道に触れない場合なので機械の揺れと読む（後の測定はほかの席のビルドと重なった）。

ドキュメント・スキーマの変更: ADR 0161・ADR 0060 の追補（上記）。設定の鍵・色のトークン・UiMessage・EditorOperation は足していない。

Waivers: none

## 残るリスク・設計席へ

- 長い文の 1,000 件で、後ろの全部がずれる位置の 1 打鍵はまだ 1.3 ms（ずらした列を作るときの文の写し）。D52 の打鍵 1 ms は lsp オフの基準で、lsp オンは D60 の別の基準。さらに削るなら文を共有の文字列にする（`Diagnostic` の型を変える・範囲を越えるので別 Issue の候補）。
- 成功の歩の読み替えは controller の旗 1 つで、engine の不変条件（本文・Vim の状態は成功でも失敗でも同じ）に触れない。fixture は再生で違い 0（引数なしの実行）。
- 使い捨ての作業木 `lsp-s5-cost-before` と `lsp-s5-cost-after`（どちらも detached・追跡ファイルを書き換えた測定用）が残っている。片付けは設計席の判断で。
