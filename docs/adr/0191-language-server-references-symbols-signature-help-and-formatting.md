# ADR 0191 — 言語サーバーの参照・記号・引数の情報・整形は、ADR 0190 の要求の表に種類を足し、一覧は Ctrl+P・札は hover の札・整形は 1 回の置き換え

- 状態: 提案（実装席 E・2026-11-22・FR-021 の段 L4 / LSP の段 S6。鍵と見せ方は hide の試用の後に受理・D41）
- 日付: 2026-11-22
- Issue: #1947（a 参照）・b〜d は各 PR で起票・親 #1821・前提 ADR 0190（要求の表）/ 0161 の決定 9 の追補（診断の一覧・#1939）/ 0060（Ctrl+P の記号）/ 0078（操作の表・Alt と組んだ鍵は操作にしない）/ 0189（プレビュー欄）
- 規則: FR-021、D60 / D61 / D67 / D82、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-012、CNF-016、QLT-014

## 文脈

ADR 0190 で、要求と答えを id で結ぶ表（`send_lsp_request`・`lsp_method_of`・種類ごとの取り消し・tick の古さ）と、補完・hover・定義へが入った。S5（#1939）で Ctrl+P の一覧に診断の出どころ `^`（`CommandChoiceKind::land`）が入った。残る日常の 4 つ（参照・記号の一覧・引数の情報・整形）を同じ道で足す。

## 決定

1. **要求は ADR 0190 の表に種類を足す。** `LspRequestKind` に `references` `document_symbol` `signature_help` `formatting` `range_formatting` を 1 つずつ足し、`lsp_method_of`・答えの振り分け・古さの捨て方の `switch` を直す（CPP-002）。送る前に未送信の変更を流し、同じ種類の古い要求は `$/cancelRequest` で取り消す（ADR 0190 の決定 1・2 のまま）。答えの待ちの上限も今の tick の古さのまま。
2. **能力の無い機能は頼まない。** `initialize` の答えの `referencesProvider` `documentSymbolProvider` `signatureHelpProvider`（と `triggerCharacters`）`documentFormattingProvider` `documentRangeFormattingProvider` を `ServerAbilities` に持つ。言語サーバーは動いているが能力が無いときは「この言語サーバーは〜に答えません」の 1 行、動いていなければ今の「言語サーバーが動いていません」。client の能力は `references`・`documentSymbol`（`hierarchicalDocumentSymbolSupport=true`）・`signatureHelp`（`signatureInformation.documentationFormat=["plaintext"]`・`parameterInformation.labelOffsetSupport=true`）・`formatting`・`rangeFormatting` を申し出る。
3. **答えの読みは core の純関数。** 参照は定義と同じ `lsp_locations`（`Location[]`・null）で、1,000 件で切る。記号は `DocumentSymbol[]`（階層・子を親の名前つきで平らにする）と `SymbolInformation[]`（平ら・`containerName`）の両方を読む。引数の情報は `SignatureHelp` の `activeSignature` の `label` と、`activeParameter`（署名の欄が先・無ければ全体の欄）の `label` の範囲（文字列なら `label` の中の最初の位置・`[start, end]` なら UTF-16 の位置）。整形は `TextEdit[]`。位置の単位は答えた席の単位（`LspLanding` と同じく答えに添えて写すまで運ぶ）。
4. **a 参照（`textDocument/references`・#1947）。**
   - 鍵: Vim の NORMAL は **`grr`**（Neovim の既定）。Nib の g の表に `r` は無く（`gr{char}` の仮想置換は未実装）、`gr` は次の 1 鍵を取る接頭辞にし、`r` なら参照の問い、ほかの鍵は表に無い鍵と同じに失敗する。engine の歩は `gd` と同じく表に行が無かったときのまま（fixture は不変）で、頼めたら controller が成功の歩に読み替える（S5 の `]d` `[d` `gl` と同じ・#1946 のレビュー R3）。一覧から選んで跳ぶのは鍵の効果の外なので、着地は今までどおり `PlaceCaret` の道。通常モードは **Shift+F12**（VS Code）。操作「参照の一覧」（両モード）。`includeDeclaration: true`。
   - 答えは Ctrl+P の一覧の新しい出どころ（記号 **`&`**・C++ の参照の印。`@` は履歴・`^` は診断）に出す。部品は S5 の診断の一覧と同じ（`CommandChoiceKind::land`・プレビュー欄・選ぶと跳ぶ）。files（記号なし）には混ぜない。ブックマークの対象外。
   - 候補は「ファイル名:行:桁」（ファイル名は root からの相対パス・root が無ければファイル名）。開いているタブの候補は、その本文から行の文を補足に出し、行と桁は本文で写す。**開いていないファイルの候補は行の文を出さず**、行は答えの行、桁は答えの文字の位置 + 1（ASCII の行では同じ）。行の文は選んだときにプレビュー欄（ADR 0189 の `PreviewPort`）で見せる。参照のために読む道は増やさない（設計席の決め 2026-11-22・全文検索の当たりの行の文は木を歩く照合と一緒に作るもので、行を指して読む口が無い）。
   - 選ぶと定義へ移動と同じ着地の 1 本（`ProjectHitLanding` と `land_project_hit`・開いていなければ `open_listed`）。`''` を置く。
   - 答えを使うのは、文脈（文書・revision・キャレット）が同じで、何も待っていないとき（ADR 0190 の追補 3 と同じ）。違えば捨てる。答えが空なら「参照が見つかりません」。
5. **b 記号（`textDocument/documentSymbol`）。** Ctrl+P の一覧の新しい出どころ（記号 **`%`**・Vim の Ex で `%` は今のファイル。この一覧は今の文書の記号だけ）。候補は「種類の印 名前」と補足に親の名前。選ぶと跳ぶ。操作「記号の一覧」（鍵なし・F1 の一覧から）。Vim は **`gO`**（Neovim の既定・Vim の `gO` は help の目次で本文では空き・g の表に `O` は無い）。`workspace/symbol` は入れない。
6. **c 引数の情報（`textDocument/signatureHelp`）。** 頼むのは、Vim の INSERT と通常モードでサーバーの `triggerCharacters` の 1 文字を打ったとき（LSP が動いている文書の真偽の後ろ・ADR 0190 の決定 10）と、操作「引数の情報を表示」（**鍵なし**・F1 の一覧から。VS Code の Ctrl+Shift+Space は D61 の「通常 / Vim の切り替え」が両モードで使っているので触らない・設計席の決め 2026-11-22）。見せ方は hover の札の部品で、キャレットの行の上に `label` を出し、今の引数を強める。次の `)`・Esc・行の移動で消える。補完の一覧と同時なら札が上・一覧が下。lsp がオフ・能力が無いときの打鍵の道は何もしない。
7. **d 整形（`textDocument/formatting` と `rangeFormatting`）。** 操作「文書を整形」「選択範囲を整形」（**どちらも鍵なし**・F1 の一覧から。VS Code の Shift+Alt+F は ADR 0078 の「Alt と組んだ鍵は操作にしない」に当たるので入れない・設計席の決め 2026-11-22）。Vim の `gq` は Nib に無いので触らない。答えの `TextEdit[]` は、全部を覆う 1 区間（最小の始まり〜最大の終わり）の新しい本文を手元で組み、`EditorController::replace` を 1 回だけ呼ぶ（1 つの undo の単位・`Edit.restore` は当てる前のキャレット）。頼んだ時の改訂番号から本文が変わっていたら捨てて 1 行の知らせ。`tabSize` と `insertSpaces` は今の文書の設定から。保存のときの自動の整形は入れない（設定の鍵が要る・`settings.v6` の時）。
8. **文と操作。** 画面の文は `UiMessage` の 3 列（操作の名前と説明は操作の群に表の順・知らせは末尾）、操作は `EditorOperation` の末尾。日本語の文に「面」を使わない。設定の鍵は足さない。

## 強制

- 能力の無い要求を送らない・文脈の照合・一覧の候補と着地: **active**（`nib_tests --language-server` `--lsp-answers`・各段で足す scope）。
- 整形が 1 つの undo の単位で、`u` で元の本文とキャレットに戻る: **active**（性質の試験・段 d）。
- 打鍵の道（段 c）のオフの速さ: **active**（QLT-014・設計席が速さのゲートを回す）。

## 結果

得られるもの: clangd などで参照・記号の一覧・引数の情報・整形が、ADR 0190 と同じ要求の表と、Ctrl+P の一覧・hover の札・`replace` の 1 本を通って使える。

失うもの・残る穴: 開いていないファイルの参照の行の文（プレビュー欄だけ）・引数の情報と整形の鍵（F1 の一覧から）・`workspace/symbol`・保存のときの整形・rename・quick fix（S7）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 開いていないファイルの行の文を `PreviewPort` で 1 ファイルずつ読む | 新しい要求が前の要求をやめる口で、プレビュー欄自身の要求と取り消し合う |
| 参照の行を読む口を `ProjectSearchPort` に足す | 読む道が 1 本増える（ARC-012）。プレビュー欄で見える |
| 引数の情報に Ctrl+Shift+Space | D61 の切り替えの鍵 |
| 整形に Shift+Alt+F | ADR 0078 の Alt の守り（AltGr の配列で誤って走る） |
| 整形の TextEdit を 1 つずつ `replace` する | undo の単位が編集の数だけになる |
