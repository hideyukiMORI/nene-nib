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
   - 追補（#1210）: 編集で診断をずらし `didChange` の変更を積むのは `language_server_edit` の 1 本で、本文を置く 3 か所（`replace`・undo・redo）が本文を置く前に呼ぶ（undo / redo は `replace` を通らないため）。

4. **見せ方は下線と件数。** `LineView` に `std::vector<core::DiagnosticSpan> diagnostics`（`DiagnosticSpan{Column begin; Column end; DiagnosticSeverity severity;}`・`colors` と同じ桁・昇順・重なりは重い方だけ）を 1 欄足し、frame を作る所が見えている行の診断だけ写す（`colors` と同じ道・格子の画面行も `GridLineRows` の写し）。renderer は IME の `underline_runs` の道で **波線**（error は `SyntaxPalette.error`・warning は `.warning`・information と hint は `muted`・太さ 1 DIP・振幅 1 DIP・色のトークンは足さない）。ステータスバーの右端に 5 つ目の項目「E2 W1」（件数が 0 なら項目を出さず幅も取らない・枠は `write_status` の保持に 1 枠足して最大 12・録画と排他なので録画時は 9・ADR 0154 の形）。キャレットの行の診断の文は、キャレットが診断の区間の上にあるときだけ知らせの欄に出す（ほかの知らせがあればそちら・frame から作る値で状態に積まない・256 バイトで切る）。
   - 追補（L1c・#995 の ui の段）:
     - **件数と文は application が frame に載せる（R7-1）。** 型は core の `DiagnosticCounts{errors; warnings; completeness}`（`DiagnosticsCount` の打ち切り・status_items_for と同じ core の値なので core に置く）。数えるのは `diagnostic_counts(span<const Diagnostic>, DiagnosticsCount)` の 1 本で、frame の `diagnostic_counts` と窓のステータスバーの hit test（`EditorController::diagnostic_counts()`）が同じ値を引く。ui は数え直さない。キャレットの上の文は `diagnostic_notice(diagnostics, caret)`（区間 `[begin, end)` の中・空の区間はその位置・重い方が勝つ）で、`command_message` があれば作らない。文を出す frame は案内を消す（知らせと同じ・`GuideContext::hidden`）。外から来た文を 1 行にするのは `notice_line` の 1 本（最初の改行の前・制御文字は空白・256 バイトを code point の境目で切る・空と UTF-8 でない文は出さない）。
     - **5 つ目の項目は言語の右（いちばん右）。** 文字列は core の `diagnostic_count_item`（「E2」「W1」「E2 W1」・打ち切りは末尾に「+」・0 件は値なし）。`DisplayText` は空を持てないので、4 項目の配列（`status_item_count = 4`）には入れず、`StatusBarLayout.diagnostics` の矩形を足した。件数が無ければ右端の幅 0 の矩形で間の余白も取らず、あれば幅 96 DIP（上限 1,000 件の「E500 W500+」まで）で、ほかの 4 項目はその左へ寄る。配置は `status_bar_layout(width, height, dpi, counts)` の 1 本（3 引数は件数 0 と同じ配置で、帯の高さだけを読む所が使う）。畳む順は言語 → 件数（R3-1）: 件数を置いた後の右端で言語を畳み、それでも `:` の入力欄の右端が `command_input_minimum_dip` に届かなければ件数も幅 0 にする（ADR 0154 の決定 6 の不変条件を守る）。renderer の描画とステータスバーのクリックの hit test は同じ counts を渡す。
     - **波線。** 太さ 1 DIP・振幅 1 DIP（山から谷まで 2 DIP）・周期 4 DIP の三角波を、ui の `draw_diagnostic_wave`（`DiagnosticWave.hpp`）の 1 本で引く。画素の列ごとに太さの矩形を 1 つ塗り（同じ高さが続く列は 1 つにまとめる）、座標は整数の画素なので中間色が出ない。位相は画素の絶対の x で決め、隣り合う区間と行でつながる。下端は IME の下線と同じ（行の下端から `caret_inset`）。本文の行は区間を色の区間と同じ写し（`span_ranges`・IME の差し込み行は差し込みの後ろをずらす）で UTF-16 の範囲にし、run ごとに引く。行末の外の区間（空の診断）は行末の位置から 1 桁（ブロックの最小の幅 7 DIP）。格子は升の範囲の矩形の下端に同じ関数で引く。色は core の `diagnostic_color`（error は本文の `error`・warning は `warning`・information と hint は UI の `muted`・トークンは足さない）。診断の無い行は `empty()` の分岐 1 つで抜ける。
     - **録画中は文を出さない（R3-3）。** マクロの録画中は `recording @a` を隠さないよう、`add_diagnostics` が文を作らない（優先の式はこの 1 か所で、renderer は `command_message` か `diagnostic_message` の値のある方を読むだけ・両方が値を持つことは無い）。文が出ている間にトグルの位置をクリックしても、`command_message` のときと同じく何もしない（`EditorController::diagnostic_message_shown()`）。
     - **格子の行末の 1 升。** `grid_diagnosed_line` が行末へ切り詰めた空の範囲は、行の最後の画面行で行末の外の 1 升 `[starts.back(), starts.back() + 1)` にする（本文の表示の「行末の外に 1 桁」と揃える・R7-1）。
     - **枠。** `write_status` の保持は 12（トグル 2・モード 1・右の 4 項目・件数 1・案内 4）、録画中は 9。件数が 0 の frame は件数の枠を使わない（`status_layout_demand` が `StatusBarLayout.diagnostics` の幅で数える）。

5. **届く道は合図だけ。** `ServerMessage` は `WorkCompleted` → `collect` → `parse_json` → `publishDiagnostics` なら決定 2 の写し → `EditorDelivery.repaint`。ほかの通知（`window/showMessage` `window/logMessage`）は `showMessage` の error / warning だけ 1 行の知らせ、`logMessage` は捨てる。要求の答え（`initialize` 以外）は L1 では無い。
   - 追補（L1c）: 写しは core の `lsp_shown_notice` の 1 本（`type` が 1 か 2 の数・`message` が文字列のときだけ `notice_line` で 1 行）。controller は既存の `with_command_message` に置くだけで、次の使う人の意図で消える（ほかの知らせと同じ）。Info（3）・Log（4）・欄の欠け・`window/logMessage` は捨てる。

6. **打鍵の道の費用。** on のときに足すのは `apply` の終わりの `didChange` の組み立てと `send`（文字列の確保と `WriteFile` の発行・数十 µs の見込み・ベンチで測る）。診断の写し・ずらしは `collect` と `replace` の中で、見えている行の写しは frame の `colors` と同じ走り。off のときは分岐 1 つも足さない（port を呼ぶ所に入らない）。

7. **ベンチ（D60・`lsp-` の接頭辞・任意・別の基準値）。** `lsp-single-key`・`lsp-burst-200`（替え玉のサーバー exe を起こして `initialize` が返った後・診断は返さない設定）・`lsp-diagnostics-ready`（替え玉が 100 件の診断を即返す設定で、打鍵を止めてから下線が frame に載るまで・参考値）。clangd での実測は hide の実機で記録だけ（版と `compile_commands.json` の有無を書く）。基準値の採用は L1 の試用の後に hide。
   - 追補（L1d・#1216）: 3 本は `eng/measure-speed.py` の `OPTIONAL_BENCHES`（`LSP_BENCHES`）で、`--record` だけを受ける（`--check` `--adopt` `--values` は拒否・`eng/perf-reference.json` は説明だけで機械の値を持たない）。替え玉は測る exe と同じ build の `nib_fake_language_server.exe`（`--silent` は did* に黙る・`--diagnostics <N>` は `didOpen` / `didChange` ごとに同じ uri と version で N 件を 1 通・1 行 1 件の 0〜4 桁・error と warning を交互）。設定は道具の profile `out/speed/profile-lsp` の `settings.v4` に `lsp=on` と `lsp.cpp.command="<替え玉>" <引数>` を `window_driver.write_settings` で書き、文書は `key-to-frame-single-cpp` と同じ `single-cpp-1000.cpp`。**「サーバーが起きた」の節目**は製品に無いので、替え玉が窓の子プロセスとして見える（`window_driver.child_names`）まで待ってから窓のスレッドが静かになるのを待つ（替え玉は `initialize` に即答し、controller はその答えで `didOpen` を送る）。単打と 200 打鍵はその後に `key-to-frame-single` / `-burst-200` と同じ打鍵・同じ節目（同じ 1 試行）。**`lsp-diagnostics-ready` は近似**: 製品は診断の節目を持たないので、ui に節目を足さず、静かになってから送る 1 打鍵の `input_received` から**その後の 2 枚目の `frame_presented`** までを値にする（1 枚目は打鍵の frame、2 枚目は `publishDiagnostics` → `WorkCompleted` → `repaint` の frame。暖機の 2 枚目が測る打鍵より後なら、または 2 枚目が無ければ欠測）。サーバーの答えが打鍵の frame より先に着いて 1 枚に重なると、2 枚目は無関係な後の frame になる。これを拾わないよう、1 枚目と 2 枚目の間隔が `LSP_READY_GAP_MS`（100 ms）を越えた試行は欠測にし、試行ごとの `[1 枚目, 2 枚目]`（打鍵からの ms）を記録の `framesMs` に残す（#1217 のレビュー R3-1。根拠: 替え玉はマイクロ秒で答え、席の確認では診断の frame は打鍵の frame の約 1 ms 後、`--silent` でも出る無関係な frame は 300 ms 以上後）。替え玉は `--diagnostics` をほかの振る舞いの引数と併せると何も読まずに 4 で終わる（R1-1）。参考値のまま基準にしない。

8. **契約。** core: `--lsp-diagnostics`（position の写し・UTF-16 の代理対・範囲の外・1,000 件の上限・`EditAdjust` でのずらし）。application: `--language-server` に「`didOpen` の条件（on・command・4 MiB）・`didChange` の束ね（マクロの 1 意図で 1 通・`version`）・`didClose`・古い `version` の診断のずらし・2 言語目で前を閉じる・off で 0 通」。ui: `nib_window_tests` に波線の画素（WIC・`underline_runs` の既存の比較の形）と件数の枠。`verify_quiet_paths`: off で port 0 回・on でも切り替えと保存で 0 通（`didSave` は送らない・限界）。

9. **限界（L2 以降・別 ADR）。** 補完・hover・定義・code action・`relatedInformation`・`textDocument/didSave`・複数のサーバーの同時起動・診断の一覧の面（Ctrl+P の `!` など）・診断の間を跳ぶ鍵（`]d` `[d`）。

## 強制

- 診断の位置の写しとずらしが core の純関数 1 本ずつ: **active**（`eng/symbols.py`・契約）。#995 の core の段で `lsp_range_to_offsets` と `lsp_published_diagnostics`（`LspDiagnostics.hpp`）・`diagnostics_adjusted` と `diagnostic_spans`（`DiagnosticEdits.hpp`）を足し、契約 `nib_tests --lsp-diagnostics`（既定実行にも載る・固定 seed の性質の試験を含む）が守る。
- off のときの port の呼び出し 0 回: **active**（#1210・`nib_tests --history` の `verify_quiet_paths` が既定の設定（off）で起動・開く・切り替え・保存・打鍵の後に port の呼ばれた回数 0 を見る。`--language-server` の `verify_quiet_when_off` が off の打鍵・合図・tick で 0 回、`verify_did_close` が on でも開いた文書の間の切り替えと保存で 0 通を見る）。
- off の 8 本に退行なし: **active**（QLT-014）。
- `lsp-` のベンチの基準値: **planned**（試用の後に hide が採用・D60）。
- 件数・文・知らせ・波線の ui（決定 4・5 の追補）: **active**（L1c）。core は `nib_tests --status-items`（件数の 5 形・幅 0 の配置・hit test）と `--lsp-diagnostics`（`notice_line`・`diagnostic_notice`・`lsp_shown_notice`・`diagnostic_counts`）、application は `--language-server`（件数と `+` と文が frame に載る・区間の外で文が無い・`command_message` が勝つ・`showMessage` の error / warning だけ知らせ・行末の 1 桁と 1 升）、ui は `nib_window_tests --diagnostics`（WIC の画素で波の形と格子の error の色）と `--status`（12 枠・録画 9 枠）。

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
