# ADR 0161 — 言語サーバーの診断は文書の値で、区間の下線と右端の件数で見せる（LSP 段 L1）

- 状態: 受理（施主 2026-10-28「全部推しでいい」・ADR 0160 の L0 の上。L-1（初版の範囲）と L-4（送らない文書の大きさ）は hide の確認の後。D60: 設定で切り替え・既定オフ・オンの速さは `lsp-` のベンチと別の基準）
- 日付: 2026-10-28
- Issue: #995（L1）・L0 は #994・計画 `docs/plans/2026-10-21-syntax-and-lsp.md` §2.3 / 2.6 / 2.7
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-011、CPP-002 / CPP-005 / CPP-011、QLT-014、ADR 0014（IME の下線の描画）/ 0037（当たりの面）/ 0056（文書の値）/ 0062（合図と `collect`）/ 0070 / 0079 / 0154（ステータスバーの枠）/ 0152（編集で位置をずらす `marks_adjusted`）/ 0156（構文の色の区間 `ColorSpan`）/ 0160

## 文脈

L0（ADR 0160）で port と JSON-RPC と設定が揃う。最初に見せる機能は診断（`textDocument/publishDiagnostics`）で、補完や定義より先に効き目が分かり、サーバーの速さに依らず打鍵の道を変えない。Nib には区間の形（`LineView.colors` の `ColorSpan`・本文の桁・昇順で重ならない）と下線の描画（IME の `underline_runs`）が既にある。

## 決定

1. **文書ごとに `didOpen` / `didChange` / `didClose`。** `lsp` が on・文書の言語に command がある・本文が 4 MiB 以下（L-4・設定に出さない定数）のとき、サーバーが `initialized` に達した後に `didOpen`（全文・`languageId` は `language_name`・`version` は `EditHistory::revision()`）。編集は `TextBuffer::replaced(begin, end, inserted)` の 1 編集が `TextDocumentContentChangeEvent`（range・text）に 1 対 1 で対応するので **増分**で送る。1 つの意図の中の複数の編集（マクロ・`.`・貼り付け）は `apply` の終わりに 1 通の `didChange` に束ね（`contentChanges` の列・順のまま）、`version` は意図の後の `revision()`。タブを閉じたら `didClose`。切り替えでは閉じない（サーバーは複数の文書を持てる）。言語ごとにサーバーは 1 本で、同時に起こすのは 1 言語まで（2 つ目の言語の文書を開いたら前のを `shutdown`・限界）。`root`（`rootUri`）は最初に開いた文書のフォルダ（`compile_commands.json` の場所は command の引数で）。

2. **診断は文書の値。** `DocumentState` に `std::vector<core::Diagnostic> diagnostics`（`Diagnostic{OffsetRange range; DiagnosticSeverity severity /* error warning information hint */; std::string message;}`・公開 aggregate・比較は非メンバー）と `std::uint64_t diagnostics_version`（届いたときの `version`）。`publishDiagnostics` の position（`line` と `character`・`positionEncoding` が `utf-16` なら UTF-16 の単位）は core の純関数 `lsp_range_to_offsets(text, range, encoding)` で本文の offset に写す（行は改行の索引・桁は `Utf16` の 1 本・行や桁が本文の外なら切り詰める）。1 文書の診断は **1,000 件まで**（超えた分は捨てて件数に `+` を付ける・限界）。

3. **編集で位置をずらす 1 本。** 新しい診断が届くまで、古い診断は編集で位置をずらして見せる（マークと同じ `EditAdjust` の規則・ADR 0152・編集の範囲の中の診断は消す）。ずらすのは `replace` の 1 か所（`forget_syntax_before` と同じ所）。届いた診断の `version` が今の `revision()` より古ければ、届いた後の編集でずらしてから置く（サーバーは古い版の位置で返す）。`version` が今より新しいことは無い。

4. **見せ方は下線と件数。** `LineView` に `std::vector<core::DiagnosticSpan> diagnostics`（`DiagnosticSpan{Column begin; Column end; DiagnosticSeverity severity;}`・`colors` と同じ桁・昇順・重なりは重い方だけ）を 1 欄足し、frame を作る所が見えている行の診断だけ写す（`colors` と同じ道・格子の画面行も `GridLineRows` の写し）。renderer は IME の `underline_runs` の道で **波線**（error は `SyntaxPalette.error`・warning は `.warning`・information と hint は `muted`・太さ 1 DIP・振幅 1 DIP・色のトークンは足さない）。ステータスバーの右端に 5 つ目の項目「E2 W1」（件数が 0 なら項目を出さず幅も取らない・枠は `write_status` の保持に 1 枠足して最大 12・録画と排他なので録画時は 9・ADR 0154 の形）。キャレットの行の診断の文は、キャレットが診断の区間の上にあるときだけ知らせの欄に出す（ほかの知らせがあればそちら・frame から作る値で状態に積まない・256 バイトで切る）。

5. **届く道は合図だけ。** `ServerMessage` は `WorkCompleted` → `collect` → `parse_json` → `publishDiagnostics` なら決定 2 の写し → `EditorDelivery.repaint`。ほかの通知（`window/showMessage` `window/logMessage`）は `showMessage` の error / warning だけ 1 行の知らせ、`logMessage` は捨てる。要求の答え（`initialize` 以外）は L1 では無い。

6. **打鍵の道の費用。** on のときに足すのは `apply` の終わりの `didChange` の組み立てと `send`（文字列の確保と `WriteFile` の発行・数十 µs の見込み・ベンチで測る）。診断の写し・ずらしは `collect` と `replace` の中で、見えている行の写しは frame の `colors` と同じ走り。off のときは分岐 1 つも足さない（port を呼ぶ所に入らない）。

7. **ベンチ（D60・`lsp-` の接頭辞・任意・別の基準値）。** `lsp-single-key`・`lsp-burst-200`（替え玉のサーバー exe を起こして `initialize` が返った後・診断は返さない設定）・`lsp-diagnostics-ready`（替え玉が 100 件の診断を即返す設定で、打鍵を止めてから下線が frame に載るまで・参考値）。clangd での実測は hide の実機で記録だけ（版と `compile_commands.json` の有無を書く）。基準値の採用は L1 の試用の後に hide。

8. **契約。** core: `--lsp-diagnostics`（position の写し・UTF-16 の代理対・範囲の外・1,000 件の上限・`EditAdjust` でのずらし）。application: `--language-server` に「`didOpen` の条件（on・command・4 MiB）・`didChange` の束ね（マクロの 1 意図で 1 通・`version`）・`didClose`・古い `version` の診断のずらし・2 言語目で前を閉じる・off で 0 通」。ui: `nib_window_tests` に波線の画素（WIC・`underline_runs` の既存の比較の形）と件数の枠。`verify_quiet_paths`: off で port 0 回・on でも切り替えと保存で 0 通（`didSave` は送らない・限界）。

9. **限界（L2 以降・別 ADR）。** 補完・hover・定義・code action・`relatedInformation`・`textDocument/didSave`・複数のサーバーの同時起動・診断の一覧の面（Ctrl+P の `!` など）・診断の間を跳ぶ鍵（`]d` `[d`）。

## 強制

- 診断の位置の写しとずらしが core の純関数 1 本ずつ: **active**（`eng/symbols.py`・契約）。
- off のときの port の呼び出し 0 回: **planned**（`verify_quiet_paths` の欄・L1 の PR で active）。
- off の 8 本に退行なし: **active**（QLT-014）。
- `lsp-` のベンチの基準値: **planned**（試用の後に hide が採用・D60）。

## 結果

得られるもの: C/C++（clangd）の誤りが打ちながら見える。構文の色と同じ区間の形に載るので描画の道は増えない。
失うもの・残る穴: 同時に 1 言語。診断の文は 1 行で切れる。サーバーの速さは Nib の責任の外（`lsp-diagnostics-ready` は参考値）。4 MiB 超の文書には出ない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 診断を `LineView.colors` の `error` / `warning` の役割で色として塗る | 構文の色と重なると区間の統合が要り、下線の方が構文の色を消さない。`SyntaxRole::error` は閉じていない文字列などの字句器の値のまま |
| 打鍵ごとに `didChange` | マクロ・`.`・貼り付けで 1 意図に数百の編集。束ねれば 1 通 |
| 全文の `didChange`（Full） | 1 MiB の文書で打鍵ごとに 1 MiB を書く。増分は `replaced` の 1 編集と 1 対 1 |
| 診断を状態に積まず届くたびに描くだけ | 編集でずれる。文書の値にして `EditAdjust` でずらすのが ADR 0152 と同じ形 |
