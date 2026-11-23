# ADR 0191 — 言語サーバーの参照・記号・引数の情報・整形は、ADR 0190 の要求の表に種類を足し、一覧は Ctrl+P・札は hover の札・整形は 1 回の置き換え

- 状態: 提案（実装席 E・2026-11-22・FR-021 の段 L4 / LSP の段 S6。鍵と見せ方は hide の試用の後に受理・D41）
- 日付: 2026-11-22
- Issue: #1947（a 参照）・#1955（b 記号）・c〜d は各 PR で起票・親 #1821・前提 ADR 0190（要求の表）/ 0161 の決定 9 の追補（診断の一覧・#1939）/ 0060（Ctrl+P の記号）/ 0078（操作の表・Alt と組んだ鍵は操作にしない）/ 0189（プレビュー欄）
- 規則: FR-021、D60 / D61 / D67 / D82、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-012、CNF-016、QLT-014

## 文脈

ADR 0190 で、要求と答えを id で結ぶ表（`send_lsp_request`・`lsp_method_of`・種類ごとの取り消し・tick の古さ）と、補完・hover・定義へが入った。S5（#1939）で Ctrl+P の一覧に診断の出どころ `^`（`CommandChoiceKind::land`）が入った。残る日常の 4 つ（参照・記号の一覧・引数の情報・整形）を同じ道で足す。

## 決定

1. **要求は ADR 0190 の表に種類を足す。** `LspRequestKind` に `references` `document_symbol` `signature_help` `formatting` `range_formatting` を 1 つずつ足し、`lsp_method_of`・答えの振り分け・古さの捨て方の `switch` を直す（CPP-002）。送る前に未送信の変更を流し、同じ種類の古い要求は `$/cancelRequest` で取り消す（ADR 0190 の決定 1・2 のまま）。答えの待ちの上限も今の tick の古さのまま（種類ごとの上限は入れない・設計席の決め 2026-11-22）: tick は 30 秒ごと（`WindowTimer.hpp` の `language_server_interval_milliseconds`）で古さ 2 回で捨てるので、参照・記号・整形の待ちは 30〜60 秒で捨てる。段 a（#1947）で `references`、段 b（#1955）で `document_symbol` を足した。残りの 3 つは各段の PR で足す。
2. **能力の無い機能は頼まない。** `initialize` の答えの `referencesProvider` `documentSymbolProvider` `signatureHelpProvider`（と `triggerCharacters`）`documentFormattingProvider` `documentRangeFormattingProvider` を `ServerAbilities` に持つ。言語サーバーは動いているが能力が無いときは「この言語サーバーは〜に答えません」の 1 行、動いていなければ今の「言語サーバーが動いていません」。client の能力は `references`・`documentSymbol`（`hierarchicalDocumentSymbolSupport=true`）・`signatureHelp`（`signatureInformation.documentationFormat=["plaintext"]`・`parameterInformation.labelOffsetSupport=true`）・`formatting`・`rangeFormatting` を申し出る。段 a で `referencesProvider` と client の `references`、段 b で `documentSymbolProvider` と client の `documentSymbol` が入った。残りは各段の PR で足す。
3. **答えの読みは core の純関数。** 参照は定義と同じ `lsp_locations`（`Location[]`・null）で、1,000 件で切る。記号は `DocumentSymbol[]`（階層・子を親の名前つきで平らにする）と `SymbolInformation[]`（平ら・`containerName`）の両方を読む。引数の情報は `SignatureHelp` の `activeSignature` の `label` と、`activeParameter`（署名の欄が先・無ければ全体の欄・どちらも無ければ 0。引数の数の外は 0（LSP 3.17 の文）、`null`（3.18 の「今の引数なし」）は強めない）の `label` の範囲（文字列なら、署名の `label` の最初の `(` の後から前の引数の一致の終わりを起点に順に探した位置・`[start, end]` なら UTF-16 の位置・#1968 のレビュー c-R3）。整形は `TextEdit[]`。位置の単位は答えた席の単位（`LspLanding` と同じく答えに添えて写すまで運ぶ）。
4. **a 参照（`textDocument/references`・#1947）。**
   - 鍵: Vim の NORMAL は **`grr`**（Neovim の既定）。Nib の g の表に `r` は無く（`gr{char}` の仮想置換は未実装）、`gr` は次の 1 鍵を取る接頭辞にし、`r` なら参照の問い、ほかの鍵は表に無い鍵と同じに失敗する。engine の歩は `gd` と同じく表に行が無かったときのまま（fixture は不変）で、頼めたら controller が成功の歩に読み替える（S5 の `]d` `[d` `gl` と同じ・#1946 のレビュー R3）。一覧から選んで跳ぶのは鍵の効果の外（下の着地の 1 本）。限界: Vim の `gr<C-v>x` は `<C-v>` の後の文字を読むが、Nib は `<C-v>` で失敗して `x` が別の命令として走る（仮想置換が無いため）。通常モードは **Shift+F12**（VS Code）。操作「参照の一覧」（両モード）。`includeDeclaration: true`。
   - 答えは Ctrl+P の一覧の新しい出どころ（記号 **`&`**・C++ の参照の印。`@` は履歴・`^` は診断）に出す。部品は S5 の診断の一覧と同じ（`CommandChoiceKind::land`・プレビュー欄・選ぶと跳ぶ）。files（記号なし）には混ぜない。ブックマークの対象外。
   - 候補は「ファイル名:行:桁」（ファイル名は root（設定のルート・無ければ今の文書のフォルダ）からの相対パス・その外ならパスの全体）。開いているタブの候補は、その本文から行の文を補足に出し、行と桁は本文で写す。**開いていないファイルの候補は行の文と桁を出さず**「ファイル名:行」だけ（答えの文字の位置は答えた席の単位で、ASCII でない行では本文の桁とずれるため・設計席の決め 2026-11-22・#1955。跳んだ先は開いた本文で写すので正しい）。行の文は選んだときにプレビュー欄（ADR 0189 の `PreviewPort`）で見せる。参照のために読む道は増やさない（設計席の決め 2026-11-22・全文検索の当たりの行の文は木を歩く照合と一緒に作るもので、行を指して読む口が無い）。
   - 選ぶと定義へ移動と同じ着地の 1 本（`ProjectHitLanding` と `land_project_hit`・開いていなければ `open_listed`）。`''` を置く。
   - 答えを使うのは、文脈（文書・revision・キャレット）が同じで、何も待っていないとき（ADR 0190 の追補 3 と同じ）。違えば捨てる。答えが空なら「参照が見つかりません」。誤りの答え（`ContentModified` など）も定義と同じく空の答えとして扱う。窓が非アクティブの間に届いた答えは一覧を開かずに控え、`&` で開けば見える（#1954 のレビュー R5）。
   - 答えは受け取りの時点で 1,000 件（`lsp_reference_limit`）に切り、切ったときは「参照 N 件のうち 1000 件を出しています」を 1 行知らせる。控えた最後の答えは次の答えまで `&` で開けるが、答えを頼んだ文書を言語サーバーが閉じたら（タブを閉じた・lsp をオフにした・席が止まった）捨てる。`&` を手で打つと最後の参照の答えが出るのはそのまま（設計席の決め 2026-11-22）。候補（当たり）は一覧が `&` を見せるときにだけ作り、同じ一覧の間は 1 回だけ作って持つ（ほかの記号で開いた一覧・頼んでいない答えでは 1 件も作らない・`settle_listed_answers`・#1958 のレビュー R1）。作るときもルートは 1 回、ファイルとタブは URI が変わったときだけ引く。答えの行が巨大な行（バイト数が `giant_line_threshold` を超える）なら行の文を出さず行だけの当たり（開いているタブでも題名は「相対パス:行」で桁を付けない・#1958 の確かめレビュー M1）にし、位置の変換で行頭から行を写さない（着地は選んだときに 1 回だけ写す）。始まりと終わりが別の行の範囲は、終わりを変換せず始まりの行の行末に切る（L1）。一覧の下の案内の欄に、何を頼んだ答えか（頼んだ所の「相対パス:行 の参照」）を出す（#1958）。候補が 0 なら一覧の中に頼み方の 1 行（ADR 0060 の 2026-11-22 の追補）。
5. **b 記号（`textDocument/documentSymbol`・#1955）。** Ctrl+P の一覧の新しい出どころ（記号 **`%`**・Vim の Ex で `%` は今のファイル。この一覧は今の文書の記号だけ）。参照 `&` と記号 `%` は検索欄の案内に出さない（入口は鍵・ADR 0060 の 2026-11-22 の追補）。操作「記号の一覧」（鍵なし・F1 の一覧から）。Vim は **`gO`**（Neovim の既定・Vim の `gO` は help の目次で本文では空き・g の表に `O` は無い。`gd` と同じく engine の歩は表に行が無かったときのままで、頼めたら controller が成功の歩に読み替える・`grr` と同じ）。`workspace/symbol` は入れない。
   - 読みは core の `lsp_document_symbols` の 1 本（再帰せず手元の積みで歩く・上限 1,000 件の `lsp_symbol_limit`）。名前か位置の欠けた記号は、その子ごと捨てる。跳ぶ先は `DocumentSymbol` の `selectionRange`（無ければ `range`）、`SymbolInformation` の `location.range`。
   - 候補は「種類の印 名前」（印は LSP の `SymbolKind` の名前の小文字・`function` `class` `field` など。LSP の用語なので日本語・中国語の画面でも訳さない（Vim の報せと同じ扱い・設計席の決め 2026-11-22）・知らない値は `symbol`）と、補足に親の名前（階層の親・平らな答えは `containerName`）。並びは文書の順（親の直後に子）。プレビュー欄と選ぶ道は参照と同じ部品（`PaletteReference` の列・`land_location`）。
   - 答えを使う条件・窓が非アクティブのときの控え・答えを頼んだ文書を言語サーバーが閉じたら控えを捨てるのは、参照と同じ。空なら「記号が見つかりません」、能力が無ければ「この言語サーバーは記号に答えません」。1,000 件で切ったら「記号 N 件のうち 1000 件を出しています」を 1 行知らせる（読みは上限の後は記号を作らず数だけ数え、子に親の名前を写すのも上限の残りの数まで・#1958 のレビュー L1・L5）。
   - **`%` は今の文書の今の改訂の記号だけ**（#1958 のレビュー R2・設計席の決め 4）。手で `%` を打ったとき、控えた答えがアクティブな文書のものでないか、頼んだ後に文書が変わっていれば（改訂番号で比べる）見せず、今の文書について `gO` と同じ道で頼み直す。届いた答えは開いている `%` の一覧の候補にする。頼めなければ一覧の中の空の 1 行。候補（当たり）を作るのは参照と同じく `%` を見せるときだけで、巨大な行の扱いも参照と同じ。
   - 限界: 同じ名前の記号（多重定義・別の名前空間）は候補の題名が同じで、補足の親の名前とプレビューで見分ける（レビュー L2・試用で判断）。平らな答え（`SymbolInformation`）は `location.range` しか無いので、着地は名前でなく宣言の頭で、プレビューの強調も行末まで伸びる（L3・clangd は階層で答える）。参照の答えを窓が非アクティブの間に控えた後で `&` で開くと「N 件のうち」の知らせは出ない（知らせは答えが届いて開いたときだけ・a のレビューの小さな点）。
6. **c 引数の情報（`textDocument/signatureHelp`）。** 頼むのは、Vim の INSERT と通常モードでサーバーの `triggerCharacters` の 1 文字を打ったとき（LSP が動いている文書の真偽の後ろ・ADR 0190 の決定 10）と、操作「引数の情報を表示」（**鍵なし**・F1 の一覧から。VS Code の Ctrl+Shift+Space は D61 の「通常 / Vim の切り替え」が両モードで使っているので触らない・設計席の決め 2026-11-22）。見せ方は hover の札の部品で、キャレットの行の上に `label` を出し、今の引数を強める。次の `)`・Esc・行の移動で消える。補完の一覧と同時なら札が上・一覧が下。lsp がオフ・能力が無いときの打鍵の道は何もしない。
   - 段 c（#1959）の形: 能力は `signatureHelpProvider`（object）とその `triggerCharacters`・`retriggerCharacters`（`ServerAbilities.signature_help` `signature_triggers` `signature_retriggers`）。client は `signatureHelp.signatureInformation`（`documentationFormat=["plaintext"]`・`parameterInformation.labelOffsetSupport=true`）を申し出る。要求は `LspRequestKind::signature_help` で、`context` は `triggerKind`（打った文字は 2・操作は 1）・`triggerCharacter`・`isRetrigger`（札が出ているか）。札が出ている間は `retriggerCharacters` でも頼み直す。
   - 札は hover の札と**別の欄**（controller の `signature_tip_`・`SignatureTip{document, line, signature}`）で、打鍵では消えない（設計席の答え 2026-11-22）。消すのは意図の出口の `settle_signature_tip` の 1 か所（文書が替わった（タブの切り替え）・キャレットが答えの行を離れた・挿入を抜けた（Vim の INSERT でなくなった）・入力行（Ctrl+P の一覧）・右クリックのメニュー・設定の面・窓が非アクティブ・hover と診断の札が出た（診断の札は hover と同じ答えの欄）・言語サーバーが running でなくなった）と、`)` を打った（trigger か、消す前に札が出ていて retrigger なら頼み直し、入れ子の外側の札はその答えが戻す・`)` が retrigger だけのサーバーも同じ・c-R1）・通常モードの Esc（`CancelSelection`）・空の答え。照合で捨てた答え（打ち続けて文脈が変わった）は今の札を残す。キャレットの行が見えている行に無い間（通常モードのホイール）は、消さずに隠す（`shown_signature` が frame の行を見る・c-R2'）。札の行は改行の索引の `line_containing` で引き、行頭から歩かない（c-R2）。手で頼んで空なら「引数の情報がありません」（手で頼んだ印は次の要求が下ろす・照合で捨てた答えの印を残さない・c-R5）、能力が無ければ「この言語サーバーは引数の情報に答えません」。
   - 読みは core の `lsp_signature_help`（`LspSignatureHelp.cpp`・`label` は 1,024 バイトで切る）。描くのは hover の札と同じ `HoverTipLayout`（別の 1 本 `signature_tip_layout_`）・書式・`paint_tab_tip` で、置き場は core の `signature_tip_rect`（補完の一覧が無いか重ならなければ `hover_tip_rect` と同じ。重なれば、キャレットの一覧と反対の側（一覧が上ならキャレットの下）、次に一覧の向こう側（一覧が上なら一覧の上・下なら一覧の下）の順に、領域に収まり一覧とキャレットの行に掛からない所。どこにも入らなければ一覧を優先して札を出さない・#1968 のレビュー c-M1）。補完の一覧の箱は同じ frame で先に描いた `completion_layout_` を読む。今の引数は太字（`HoverTipLayout::emphasized` が保持した layout の太字の範囲だけを付け替える・文と幅が同じなら layout を作り直さない）。
   - 打鍵の道: 入口は補完の自動の面（ADR 0190 の決定 5 の #1962 の追補）と同じ `typed_for_language` の 1 本で、その `running` の枝で `open_triggered_completion` の後に `settle_typed_signature` を呼ぶ。通常モードの `accept(InsertText)` も Vim の `accept(VimKeyPress)` も「言語サーバーが起きている」の真偽 1 つの後ろで、Vim の打った文字の見分け（出どころが打った鍵・再生の外・入力行なし・INSERT・1 鍵の引数待ちでない・流す前のキャレットの直後にその文字がそのまま入った）は #1962 のもの。`map` の右辺・録画の再生・`.`・`:normal`・Ctrl-R とその後のレジスタの名前の鍵（1 鍵の引数の待ち）・IME の確定（`CommitText`）は頼まず、左辺の待ちの後にまとめて流れた文字（`inoremap jk <Esc>` の `j(` の `(`）は頼む。能力の無い席では欄を 1 つ読んで返る。
   - 限界: 括弧を自動で閉じる `map`（`inoremap ( ()`）では、`(` が右辺から入るので自動では頼まない（操作「引数の情報を表示」で頼める・#1968 のレビュー c-L2）。
7. **d 整形（`textDocument/formatting` と `rangeFormatting`）。** 操作「文書を整形」「選択範囲を整形」（**どちらも鍵なし**・F1 の一覧から。VS Code の Shift+Alt+F は ADR 0078 の「Alt と組んだ鍵は操作にしない」に当たるので入れない・設計席の決め 2026-11-22）。Vim の `gq` は Nib に無いので触らない。答えの `TextEdit[]` は、全部を覆う 1 区間（最小の始まり〜最大の終わり）の新しい本文を手元で組み、`EditorController::replace` を 1 回だけ呼ぶ（1 つの undo の単位・`Edit.restore` は当てる前のキャレット）。頼んだ時の改訂番号から本文が変わっていたら捨てて 1 行の知らせ。`tabSize` と `insertSpaces` は今の文書の設定から。保存のときの自動の整形は入れない（設定の鍵が要る・`settings.v6` の時）。
8. **文と操作。** 画面の文は `UiMessage` の 3 列（操作の名前と説明は操作の群に表の順・知らせは末尾）、操作は `EditorOperation` の末尾。日本語の文に「面」を使わない。設定の鍵は足さない。

## 強制

- a 参照の能力の無い要求を送らない・文脈の照合・一覧の候補と着地・上限: **active**（`nib_tests --lsp-references`）。
- b 記号の同じ守り（読み・能力・文脈の照合・一覧の候補と着地・`gO`）: **active**（`nib_tests --lsp-symbols`）。
- c 引数の情報の同じ守り（読み・能力・自動の頼み・札の出る間と消える条件（`)`・Esc・行の移動・INSERT を抜ける・空の答え・タブの切り替え・窓が非アクティブ・Ctrl+P の一覧・右クリックのメニュー・設定・hover の札・照合で捨てた答えは札を残す・ホイールで隠す）・補完の一覧と重ならない置き場・lsp がオフと能力の無い席で打鍵の道（通常モードと Vim の INSERT）が言語サーバーに触れない契約）: **active**（`nib_tests --lsp-signature`）。
- d 整形の同じ守り: **planned**（段 d の PR で、足した scope を名指しして active に書き換える）。
- 整形が 1 つの undo の単位で、`u` で元の本文とキャレットに戻る: **planned**（性質の試験・段 d の PR で active にする）。
- 打鍵の道（段 c）のオフの速さ: **planned**（QLT-014・段 c の PR の後に設計席が速さのゲートを回して active にする）。

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
