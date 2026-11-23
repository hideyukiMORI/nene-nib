# ADR 0194 — 言語サーバーの名前の変更とクイックフィックスは、WorkspaceEdit を先に全部検査し、ファイルごとにタブへ切り替えて `replace` を 1 回ずつ当てる

- 状態: 提案（ADR の席 A1・2026-11-24・FR-021 の LSP の段 S7。鍵と見せ方は hide の試用の後に受理・D41。下の「hide に決めてほしいこと」は設計席が仮に決めた物）
- 日付: 2026-11-24
- Issue: #2002・親 #1821・下調べ `D:/NeNeNib/outputs/20261124/plan-lsp-s7.md`（main `9e8adbf5`・5 つのサーバーの実測は `D:/NeNeNib/scripts/20261124/s7-probe/`）・前提 ADR 0190（要求の表）/ 0191（決定 7 の整形・PR #1997）/ 0056（ほかのタブは不変の束）/ 0059（`reach_tab`）/ 0060（Ctrl+P の記号）/ 0078（操作の表）/ 0159（裏の読み）/ 0160（席・4 MiB）/ 0161（診断）
- 規則: FR-021、D60 / D65 / D67 / D82 / D104、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-012、CNF-016、QLT-014

## 文脈

S6 までで、言語サーバーへの要求は ADR 0190 の表の 1 本、答えの読みは core の純関数、本文への当て方は整形（#1997）の `lsp_text_edits` → `formatted_span` → `replace` の 1 回（1 つの undo の単位）になった。残る S7 の名前の変更（`textDocument/rename` ＋ `prepareRename`）とクイックフィックス（`textDocument/codeAction` ＋ `codeAction/resolve`・`workspace/executeCommand`・サーバーからの `workspace/applyEdit`）は、どれも答えが `WorkspaceEdit`（**複数のファイル**への `TextEdit` の束）である。

今の Nib には、アクティブでない文書の本文を変える道が無い（ADR 0056 の決定と却下の表「`with_edit()` にタブの位置の引数を足す」）。ほかのタブはまだ読んでいないこともあり（ADR 0059）、開いていないファイルもある。

窓を出さない調べ（下調べ §2）で、5 つのサーバーの答えの形が分かった。

- 名前の変更は、clangd・gopls・pyright・rust-analyzer のどれも**開いていないファイル**への編集を返す。
- 版（`version`）で古さを判定できるサーバーは一部だけ。clangd と ts は版の無い `changes` で返す。pyright は開いている文書にも null を返す。gopls は開いていないファイルに 0 を返す。
- クイックフィックスは、client が `codeActionLiteralSupport` を名乗らないと rust-analyzer が 0 件を返し、clangd は Command（`clangd.applyFix` → `workspace/applyEdit`）だけを返す。
- `resolveSupport` を名乗らないと、gopls は Command に落ちる。
- `disabledSupport` を名乗ると、ts が使えない refactor を 15 件ほど返す。
- ts の「Move to a new file」は、**まだ無いファイルへの TextEdit** を `applyEdit` で送ってくる。

## 決定

1. **段を 2 つに分ける。**
   - S7 a は、`WorkspaceEdit` を当てる土台（決定 3〜6）と名前の変更（決定 7）。
   - S7 b は、クイックフィックス（決定 8〜10）。
   - 受け入れ条件の案は末尾の「段の切り方」に書いた。
2. **要求は ADR 0190 の表に種類を足す。**
   - `LspRequestKind` に `prepare_rename` `rename`（S7 a）と `code_action` `code_action_resolve` `execute_command`（S7 b）を末尾に 1 つずつ足し、写し先の `switch` を直す（CPP-002）。
   - 送る前に未送信の変更を流すこと、同じ種類の古い要求を `$/cancelRequest` で取り消すこと、待ちの上限が tick の古さ（30〜60 秒）であることは、ADR 0190 の決定 1・2 のまま。
   - 文脈 `LspRequestContext`（文書・改訂番号・キャレット）に、**その席が `didOpen` している文書ごとの「退避番号と改訂番号」の写し**を添える（名前の変更とクイックフィックスだけ・決定 4 の照合に使う）。
3. **能力は実装した段でだけ名乗る**（`text_document_capabilities` の 1 か所・`initialize_params` の `workspace` は同じ所に 1 つ足す・ADR 0190 の 2026-11-23 の追補 #1961）。
   - **S7 a で名乗る物:**
     - `textDocument.rename = {prepareSupport: true}`。
     - `workspace.workspaceEdit = {documentChanges: true, failureHandling: "abort"}`。
     - 名乗らない物: `resourceOperations`（作る・名前を変える・消す）、`changeAnnotationSupport`、`normalizesLineEndings`、`prepareSupportDefaultBehavior`。
     - `failureHandling` に `abort` を選ぶ理由: 決定 5 で先に全部を検査するので、実際には全部当たるか 1 つも当たらないかになる。それでも、名乗るのは保証できる一番弱い形にとどめる。
   - **S7 b で名乗る物:**
     - `textDocument.codeAction` の 4 つ:
       - `codeActionLiteralSupport.codeActionKind.valueSet`（`""` `quickfix` `refactor` `refactor.extract` `refactor.inline` `refactor.rewrite` `source` `source.organizeImports` `source.fixAll`）。
       - `isPreferredSupport: true`。
       - `dataSupport: true`。
       - `resolveSupport.properties = ["edit"]`。
     - `workspace.applyEdit: true`。
     - `workspace.executeCommand = {}`。
     - **`disabledSupport` は名乗らない。**
   - サーバーの側の能力は `ServerAbilities` に持つ。
     - `renameProvider`（true か object）と `prepareProvider`。
     - `codeActionProvider`（true か object）と `resolveProvider`。
     - `executeCommandProvider.commands`（文字列の列）。
   - 能力が無ければ「この言語サーバーは〜に答えません」の 1 行を出す（ADR 0191 の決定 2 と同じ表引き）。
4. **答えの読みは core の純関数で、古さの照合は 2 段にする。**
   - **読み:** `lsp_workspace_edit(JsonValue) -> std::expected<LspWorkspaceEdit, LspWorkspaceEditFailure>`。
     - `documentChanges` があればそれを使い、`changes` は捨てる（LSP の文）。
     - 同じ URI の `TextDocumentEdit` が 2 つ以上あれば、届いた順につなぐ。
     - `AnnotatedTextEdit` は `TextEdit` として読み、`annotationId` は読まない。
     - **resource operation（`kind` を持つ項目）が 1 つでもあれば `resource_operation`、`needsConfirmation: true` の注釈を指す編集があれば `needs_confirmation` を返し、どちらも答え全体を断る。**
     - 失敗は閉じた enum `LspWorkspaceEditFailure`（`malformed` `resource_operation` `needs_confirmation` `too_many_files`）で返す（ARC-010・CPP-005）。
     - ファイルの数の上限は `lsp_workspace_edit_file_limit`（**32**・hide に決めてほしいこと 2）。
     - ファイルごとの `TextEdit` の列の読みは `lsp_text_edits` の 1 本を共用する（ARC-012）。
   - **照合（当てる前に 1 回だけ）:**
     - (a) 頼んだ文書の文書と改訂番号が文脈と同じ（キャレットは問わない。名前の入力行を開いている間に動かないため）。`workspace/applyEdit` には頼んだ文書が無いので、(a) は飛ばす。
     - (b) 編集の先の各ファイルについて、その席が `didOpen` している文書なら、改訂番号が頼んだときの写しと同じ。
     - (c) 答えに版があり、かつ (b) の文書なら、版が今の改訂番号と同じ。
     - 席が開いていない文書の版は照らさない。gopls の 0 と pyright の null は、ここで無害になる。
     - どれかが違えば 1 つも当てず、「答えが届く前に文書が変わったので、当てませんでした」の 1 行を出す（整形の `notice_formatting_stale` と同じ形の文）。
5. **当てる道は 1 本で、`apply_workspace_edit` が検査 → 当てる → 戻るの順に進める。名前の変更・クイックフィックスの `edit`・resolve の `edit`・`workspace/applyEdit` は、どれもこの 1 本を通る**（ARC-001）。
   - **検査（何も変えない）:**
     - 入力の途中でない（`formatting_interrupts` を `server_edit_interrupts` に名前を広げて共用する）。
     - 決定 4 の照合が通る。
     - 各ファイルの本文が手元にある。
       - アクティブの文書は、そのまま使う。
       - 読み込み済みの脇の束は `parked_bundle_at` で読む。
       - まだ読んでいないタブは `reach_tab`（`reached` でなければ断る）。
       - 開いていないファイルは、開く道と同じ読みの port（`DocumentReadRequest`・上限 4 MiB）で読む。製品の読みは 64 MiB 以下を同期で揃えるが、port が裏の読みを返したら断る。読めない・無い・4 MiB 超も断る。
     - 開いていない・まだ読んでいないファイルが巨大な行の問い（`asks_grid`）に当たるなら断る。当てる途中で表示の問いを立てないため。
     - 各ファイルの編集を `formatted_span` に通す（newText の改行は整形と同じく文書の形へ揃える）。写せない位置・重なる編集があれば断る。
     - URI が `file:` でない場合も断る（`file_path_of_uri`）。
     - **1 つでも断る理由があれば 1 つも当てず、理由ごとの 1 行を出す。**
   - **当てる:** `WorkspaceEdit` の順にファイルを 1 つずつ処理する。
     - そのファイルのタブがあれば、`reach_tab` を通した後で切り替えの 1 本（`leave_document` → `with_switched` → `enter_document`）で行く。
     - 無ければ、開く道の最後の段 `place_opened`（固定のタブ・`OpenedAs::pinned`）で開く。
     - `replace` を 1 回呼ぶ（`FormattingPieces`・`Edit.restore` は当てる前のキャレット・`remember_formatted_unit`。脇の表は退避番号ごとなので、そのまま複数の文書で使える）。今の文書と同じ結果のファイルは飛ばす。
     - 全部を当てたら、頼んだタブへ同じ切り替えの 1 本で戻る（`workspace/applyEdit` のときは、当てる前にアクティブだったタブへ戻る）。
     - 1 つの文書につき undo の単位は 1 つ。ファイルをまたぐ undo は無い（限界）。
     - **ディスクには書かない。変えたファイルは未保存（●）のまま残し、保存は使う人がする。**
     - frame は意図の終わりの 1 回だけなので、途中の切り替えは描かない。
   - **知らせ:** 当てたら「N 個のファイルの M か所を変えました」を 1 行出す（1 ファイルなら「M か所を変えました」）。何も変わらなければ何も出さない。
   - **切り替えて当てる形を採る理由:** 現物を読んで、ほかのタブを束のまま書き換える案（却下の表）より正典を崩さないと判断した。
     - 編集はいつもアクティブな文書の `replace` の 1 本を通る（ADR 0056 のまま）。
     - 切り替えの前に `reach_tab` を呼ぶ（ADR 0059 のまま）。
     - `didChange` は今の「意図の途中で切り替えた文書」の道（ADR 0161 の決定 1）を通る。
     - 新しく開いたファイルは今の開く道と同じ `didOpen` の後に `didChange` になる。
     - 自動退避は改訂番号で変わったタブを拾う（ADR 0137）。
   - **切り替えの費用**（受け入れる・失うもの）:
     - 使った順（Ctrl+Tab）は「頼んだタブが先頭、変えたタブがその後ろ」に並び替わる（`with_switched` の `touch_active`）。
     - 頼んだ文書の選択はキャレットへ畳まれる。
     - Markdown のプレビューの要求が切り替えごとに出る。
     - 帯の送り（`reveal_active_tab`）が最後の文書に合う。
     - Vim のジャンプの列とマークは各文書の束のまま動かない（`''` を置かない）。
6. **サーバーからの `workspace/applyEdit` は、Nib が送った `workspace/executeCommand` の答えを待っている間だけ受ける。**
   - 受けたら決定 5 の 1 本で当て、`{applied: true}` を返す。
   - 断ったら `{applied: false, failureReason: <断った理由の英語の 1 行>}` を返す。理由はサーバーの記録に残る物で、画面の文ではないので CNF-016 の外。
   - 待っていないときに来た要求（頼んでいない編集）は、当てずに `applied: false` を返す。
   - 答える所は `receive_lsp(slot, const LspRequest&)` の今の 1 か所で、`workspace/applyEdit` の分岐を 1 つ足す（ほかの要求は今のまま MethodNotFound）。
7. **名前の変更（S7 a）。**
   - **鍵:**
     - 通常モードは **F2**（VS Code）。
     - Vim の NORMAL は **`grn`**（Neovim の既定）。今の接頭辞 `gr`（`VimPrefix::g_references`）の次の鍵に `n` を足す。engine の歩は `grr` と同じく表に行が無かったときのまま（fixture は不変）で、頼めたら controller が成功の歩に読み替える。
     - 操作「名前を変更」は両モードで F1 の一覧にも載せる（`EditorOperation` の末尾・`OperationKey` に `f2` を足し、ui の `function_chord` に F2 を写す）。
   - **頼み方:**
     - `prepareProvider` があれば `prepareRename` を頼む。答えの読みは core の `lsp_rename_target`（`Range` / `{range, placeholder}` / `{defaultBehavior}` / null）。
       - 答えの範囲の文字（placeholder があればそれ）を初めの文字にして、入力行を開く。
       - null と誤りの答えは「ここは名前を変えられません」（誤りの答えにサーバーの message があれば、それを訳さずに 1 行で出す。rust-analyzer の索引の途中の `content modified` など）。
       - 答えを使うのは、文書・改訂番号・キャレットが同じで、何も待っていないときだけ（ADR 0190 の追補 3）。
     - `prepareProvider` が無ければ、キャレットの下の語（補完の語の文字の規則）を初めの文字にして、すぐ開く。
   - **入力行:** 既存の入力行（ステータスバーの左）に `InputLinePrompt::rename`（「新しい名前: 」）を 1 値足す。`SearchLine::opened_replace_with` と同じく初めの文字を入れて開く。
     - Enter で `rename` を頼む。
     - Esc でやめる。
     - 空・初めの文字と同じ名前なら頼まない。
     - 新しい箱は作らない。
   - **答え:**
     - `WorkspaceEdit` は決定 5 で当てる。
     - null・空の答えは「名前を変える所が見つかりません」。
     - 誤りの答えは、サーバーの message を 1 行で出す。
8. **クイックフィックス（S7 b）の頼み方。**
   - **鍵:**
     - 通常モードは **Ctrl+.**（VS Code）。
     - Vim の NORMAL は **`gra`**（`gr` の次の鍵に `a`）。
     - 操作「クイックフィックス」（両モード・F1 の一覧）。`OperationKey` に `period` を足し、ui の `symbol_chord` に `VK_OEM_PERIOD` を写す。
     - Vim の VISUAL の `gra` は入れない（Nib の VISUAL に `gr` が無い）。VISUAL と通常モードの選択は Ctrl+. で範囲として頼める。
   - **範囲:** キャレット（範囲は空）、または選択（VISUAL は描く選択と同じ範囲・整形の `formatting_range` と同じ規則で、選択が空ならキャレット）。
   - **`context`:**
     - `diagnostics` は、範囲に掛かる診断（キャレットなら、キャレットを含む診断）。
     - `triggerKind` は 1（手で頼んだ）。
     - `only` は送らない（refactor と source も出る）。
     - 診断の無い所でも頼める。
   - **診断の値に足す物:** 診断をサーバーへ戻すため、`core::Diagnostic` に `code` の JSON の型（数か文字列か）と `data`（JSON のまま・4 KiB を超えたら持たない）を足す。ts は数の `code` で照らし、rust-analyzer は `data` を使うことがある。今は `code` を文字列にしか持たず、`data` を捨てている。
9. **クイックフィックスの見せ方は Ctrl+P の一覧の新しい出どころ**（記号 **`=`**・hide に決めてほしいこと 3）。
   - 部品は参照 `&` と記号 `%` と同じにする（答えが届いたら開く・`PaletteMarkHint::never`・`settle_listed_answers`）。記号の表に 1 行と、候補の列に 1 種類を足す（ADR 0060）。
   - 候補は `title`。補足は `kind`（LSP の用語なので訳さない・ADR 0191 の決定 5 の記号の種類と同じ扱い）で、`isPreferred` の項目には補足の頭に印を付ける。
   - 並びはサーバーの順のまま。
   - Command だけの項目（`Command` の literal）も出す（hide に決めてほしいこと 4）。
   - 空なら「ここで使える直し方はありません」。
   - 答えを使う条件・窓が非アクティブの間の控え・答えを頼んだ文書を席が閉じたら控えを捨てることは、参照と同じ（ADR 0191 の決定 4）。
   - キャレットの下の箱（ADR 0169 の補完の一覧）は使わない（却下の表）。
10. **クイックフィックスを選んだ後は、`edit` → `command` の順に進める**（LSP の文）。
    - 選んだ時点で、文書と改訂番号が答えの文脈と同じでなければ当てない（決定 4 の (a)）。
    - `edit` があれば決定 5 で当てる。
    - `edit` が無く `data` があり、`resolveProvider` があれば、`codeAction/resolve` を頼み、届いた `edit` を当てる（文脈の照合は同じ）。
    - 当てた後（または `edit` が無ければすぐ）に `command` があれば、`workspace/executeCommand` を送る。
    - 答えを待つ間に届いた `workspace/applyEdit` は決定 6 で当てる。
    - 誤りの答えはサーバーの message を 1 行で出す。
11. **古い答えと入力の途中の判定は、整形と同じ 1 本を使う。**
    - `formatting_interrupts` を `server_edit_interrupts` に、`formatting_refusal` の照合を「サーバーの編集を当ててよいか」の 1 本に名前を広げる。
    - 整形・名前の変更・クイックフィックス・`workspace/applyEdit` が共用する。
    - 答えは合図（`WorkCompleted`）の中で届くので、入力行・面・IME の変換・Vim の待ちの間は当てない。
    - `workspace/applyEdit` が入力の途中に届いたら `applied: false` を返す（Nib の側で待たない）。
12. **設定の鍵は足さない。LSP がオフ・言語サーバーが動いていない文書の打鍵の道には何も足さない**（ADR 0190 の決定 10・D60）。
    - F2・Ctrl+.・`grn`・`gra` は、言語サーバーが動いていなければ今の「言語サーバーが動いていません」の 1 行を出す。
    - Vim の `grn` `gra` は、`grr` と同じく表に無い鍵の失敗のまま。
13. **文と操作。**
    - 画面の文は `UiMessage` の 3 列に足す。操作の名前と説明は操作の群に表の順で、知らせは末尾に置く（CNF-016）。
    - 操作は `EditorOperation` の末尾に足す。
    - 日本語の文に「面」を使わない。
    - サーバーの返す `title` と message は訳さない。

## 強制

- S7 a の守り: **planned**（段 S7 a の PR で、足した scope を名指しして active に書き換える）。
  - `lsp_workspace_edit` の読み（両形・resource operation・注釈・上限）。
  - 照合の 2 段。
  - 検査で 1 つでも断れば 1 つも変えないこと。
  - 開いていないファイルがタブになり、未保存で残ること。
  - 文書ごとに undo の単位が 1 つで、`u` で元に戻ること。
  - 元のタブへ戻ること。
  - 能力の無い席では頼まないこと。
  - `prepareRename` の読み。
  - 入力行の Enter / Esc。
- S7 b の守り: **planned**（段 S7 b の PR で active にする）。
  - codeAction の読み（Command と CodeAction の両方）。
  - `context.diagnostics` に戻す形（code の型・data）。
  - Ctrl+P の出どころ。
  - `edit` → `command` の順。
  - resolve。
  - 頼んでいない `workspace/applyEdit` への `applied: false`。
  - 入力の途中の `applied: false`。
- オフの打鍵の道と速さ: **planned**（QLT-014・各段の PR の後に設計席が速さのゲートを回す）。

## 結果

得られるもの:

- clangd・gopls・pyright・rust-analyzer・ts で、名前の変更（開いていないファイルを含む）とクイックフィックス・refactor が、F2 / Ctrl+.（VS Code）と `grn` / `gra`（Neovim）で使える。
- 複数のファイルの編集は、今の編集の 1 本（アクティブな文書の `replace`）だけを通り、未保存のまま使う人に見える。

失うもの・残る穴:

- ファイルをまたぐ 1 回の undo は無い。ファイルごとに `u` で戻す。
- 当てた後の使った順が変わる。
- 頼んだ文書の選択が畳まれる。
- resource operation（ファイルを作る・名前を変える・消す）を含む編集は丸ごと断る。ts の「Move to a new file」や、rust-analyzer のモジュールのファイルの名前の変更などが該当する。
- 32 を超えるファイルにまたがる編集は断る。
- 裏の読みになったファイル（製品では 64 MiB 超で、4 MiB の上限で先に断る）は断る。
- 開いていないファイルの、ディスクと索引のずれは照らせない（版が無い）。
- Command の結果で窓や URL を開く項目（gopls の Browse…）は、Nib では何も起きない（`window/showDocument` は MethodNotFound）。
- Vim の VISUAL の `gra` は無い。
- `changeAnnotations` の確かめの問いは無い（断る）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| ほかのタブを切り替えずに束のまま書き換える | `EditorState` に位置を取る編集の関数を足すことになる（ADR 0056 の却下の表・CLAUDE.md の「タブの位置を引数に取る編集の関数を足さない」）。マーク・もう 1 つの枠・履歴・`didChange` の写しを 2 本目に書く（ARC-012） |
| 開いていないファイルはディスクへ直接書く | 保存の道（`encoded_parts`・一時ファイル・ADR 0135）が 2 本目になり、undo も無い。使う人が見ていない所でファイルが変わる |
| 開いているタブだけに当て、開いていないファイルを含む編集は断る | 実測で 4 つのサーバーの名前の変更が開いていないファイルを含む。多くの名前の変更が使えない |
| 当ててから検査し、途中で失敗したら戻す（`transactional` / `undo`） | 戻すのに undo の単位を逆にたどる 2 本目の道が要る。先に全部を検査すれば要らない |
| 版（`version`）だけで古さを判定する | clangd と ts は版の無い `changes`、pyright は開いている文書にも null（実測）。頼んだときの改訂番号の写しが要る |
| `disabledSupport` を名乗り、使えない項目を灰色で出す | ts が 15 件ほどの使えない refactor を並べる（実測）。選べない行が並ぶだけ |
| クイックフィックスをキャレットの下の補完の箱（ADR 0169）に出す | 箱は INSERT の語の補完の作り（接頭辞で絞る・Vim では選ぶたびに本文へ入れる）で、NORMAL・VISUAL で開く道が無い。種類を足すと鍵の分岐が全部 2 通りになる。Ctrl+P の一覧は参照 `&`・記号 `%` で同じ「答えが届いたら開く」の部品が既にある |
| 名前の入力に新しい箱（VS Code のキャレットの上の小さな欄）を作る | 入力行の部品（`SearchLine`・`InputLinePrompt`）で足りる（ARC-012） |
| resource operations を名乗る | ファイルを作る・名前を変える・消すは、エクスプローラーのファイル操作（#1014・未実装）と同じ道で作るべきで、今は道が無い |
| 頼んでいない `workspace/applyEdit` も当てる | 使う人の操作の外で本文が変わる |

## hide に決めてほしいこと

設計席が仮に決めた物です。どれも、試用の後に変えられます。

1. **変えたファイルを保存しないで残す（推し）か、VS Code のように自動で保存するか。**
   - 何が変わるか: 名前の変更で、開いていなかったファイルを Nib が開いて書き換えた後、保存するかどうか。
   - 残す（推し）: ● が付いたタブが並ぶ。使う人が中身を見てから保存できる。Ctrl+Z で戻せる。
     - 悪い点: 保存し忘れると、ビルドが古い中身を見る。
   - 自動で保存: VS Code の既定（`files.refactoring.autoSave`）。
     - 悪い点: 見ていないファイルが変わる。保存の道を名前の変更から呼ぶことになる。
   - 推し: 残す（依頼書の方向のまま）。
2. **一度に変えるファイルの上限: 32（推し）。**
   - 何が変わるか: これを超える名前の変更は、1 つも変えずに「N 個のファイルにまたがるので変えませんでした」と出す。
   - 上限を上げると、タブがその数だけ増える（帯は横に送られる）。
   - 開いていないファイルは画面を出す前に読むので、大きいファイルが多いと一瞬止まる（1 つ 4 MiB まで）。
   - 推し: 32（帯の送りと Ctrl+P の `#` で追える数）。
3. **クイックフィックスの一覧の記号: `=`（推し）。**
   - 何が変わるか: Ctrl+P の一覧で、行頭に打つとクイックフィックスの答えを見せる記号。鍵（Ctrl+. と `gra`）から開くので、ふだんは打たない。
   - 空いている記号は `= ~ $ + > ;`。
   - `=` は Vim で「整える」の命令なので、「直す」に近い。
   - 推し: `=`。
4. **サーバーの Command だけの項目（gopls の「Browse documentation…」など）も一覧に出すか。**
   - 出す（推し）: サーバーが出した物を全部見せる。
     - 良い点: clangd の古い形や gopls の一部の直しは Command で動く。
     - 悪い点: 選んでも何も起きない項目が混ざる（窓や URL を開く物）。
   - 出さない: 一覧がすっきりする。
     - 悪い点: Command で動く直しも消える。
   - 推し: 出す（何も起きなかったときは 1 行も出さない）。
5. **鍵: 名前の変更は F2 と `grn`、クイックフィックスは Ctrl+. と `gra`（推し）。**
   - 何が変わるか: VS Code と Neovim の既定の鍵と同じになる。今の Nib では、どれも空いている。
   - 気になる点: 中国語（簡体字）の Microsoft Pinyin の IME は、Ctrl+. を句読点の全角 / 半角の切り替えに使う（Ctrl+Space の D104 と同じ種類）。2 つ目の鍵を今は足さず、F1 の一覧からも選べる形にしておく。
   - 推し: この 4 つ。
6. **操作の名前: 「名前を変更」「クイックフィックス」（推し）。**
   - VS Code の日本語の表示（「シンボルの名前変更」「クイック フィックス」）に近く、短い方。
   - 一覧には refactor（「変数へ取り出す」など）も出るので、「直し方とリファクタリング」の案もある（長い）。
   - 推し: 「名前を変更」「クイックフィックス」。

## 段の切り方（子 Issue の案）

**S7 a — WorkspaceEdit を当てる土台と名前の変更**

- 受け入れ条件:
  - `lsp_workspace_edit` と `lsp_rename_target` の読みが、試験の表で両形（`changes` / `documentChanges`）・版の null と数・resource operation・注釈・上限・崩れた形を縛る。
  - 決定 4 の照合の 2 段と決定 5 の検査が、1 つでも断れば 1 つも変えない。替え玉のサーバーで、アクティブ・脇の束・まだ読んでいないタブ・開いていないファイル・無いファイル・4 MiB 超・重なる編集・版の違い・改訂の違いを試す。
  - 開いていないファイルが固定のタブになり ● のまま残る。文書ごとに `u` 1 回で元の本文とキャレットへ戻る。頼んだタブへ戻る。「N 個のファイルの M か所」の 1 行を出す。
  - F2・`grn`（fixture は不変）・入力行の初めの文字（prepare の範囲・placeholder・キャレットの語）・Enter / Esc・空と同じ名前で頼まない。
  - 能力の無い席・動いていない席では頼まない。`initialize` の期待（`verify_abilities`）に `rename` と `workspace.workspaceEdit` を足す。
  - `formatting_interrupts` の改名で、整形の試験が変わらない。
  - 操作を足したので、`--command-palette` `--shell-registration` `--every-scope` と `nib_probe_expectations` を回す。
- 前提: PR #1997（整形）が main に入っていること。

**S7 b — クイックフィックス**

- 受け入れ条件:
  - codeAction の読み（`Command` と `CodeAction`・`isPreferred`・`data`・`edit` と `command` の両方）。
  - `core::Diagnostic` の `code` の型と `data` の往復。publish → 値 → `context.diagnostics` で同じ JSON になる。
  - Ctrl+P の `=` の出どころ（候補・空の 1 行・控え・控えを捨てる）。
  - 選ぶと `edit` → `command` の順に進む。resolve で `edit` を引く。
  - 頼んだ executeCommand の間の `workspace/applyEdit` は当てて `applied: true`、頼んでいない時と入力の途中は `applied: false`。
  - Ctrl+.・`gra`・VISUAL の範囲。
  - 能力（literal・isPreferred・data・resolve・applyEdit・executeCommand）を `initialize` の期待に足す。
  - 窓を出さない調べ（`s7-probe.py`）で、5 つのサーバーの 1 つずつで直しが当たることを PR 本文に記録する。
- 前提: S7 a。
