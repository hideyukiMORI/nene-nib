# ADR 0190 — 言語サーバーの補完・hover・定義へは、id で結んだ要求と文脈つきの答えで、面は ADR 0169 の補完の面と小さな札

- 状態: 提案（設計席 2026-11-18・FR-021 の段 L2 / L3。鍵と見せ方は hide の試用の後に受理・D41）
- 日付: 2026-11-18
- Issue: #1821（L2 / L3 の親）・前提 ADR 0160（L0）/ 0161（L1）/ 0169（INSERT の補完の面）・計画 `docs/plans/2026-10-21-syntax-and-lsp.md` §2.3 / 2.5 / 2.6
- 規則: FR-021 / FR-024、D60 / D65 / D67 / D82、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-012、CNF-016、QLT-014

## 文脈

L0 で port・JSON・枠、L1 で診断が入った。要求と答えを id で結ぶ表（`LanguageServerLink.pending`・`LspRequestKind`）は `initialize` と `shutdown` の 2 種類だけで、答えの文脈（どの文書のどの版のどの位置への問いか）を持たない。`initialize` は client の能力に `positionEncodings` しか書かず、サーバーの能力は `positionEncoding` しか読まない。

補完の面・鍵・状態は ADR 0169 が決めた（Ctrl+P の部品・キャレットの下・240 DIP・8 行・`CompletionSource` の閉じた enum に `language_server`）。順は「L2 の ADR で決める」と委ねられている。`gd` と `K` は今どちらも Nib に無い（g の表にも通常の表にも行が無い）。

## 決定

1. **要求の種類を足し、答えは文脈で照合する。** `LspRequestKind` に `completion` `hover` `definition` を足し、`PendingLspRequest` に文脈 `LspRequestContext{AutosaveId document; std::uint64_t revision; Offset at;}`（`initialize` と `shutdown` は文脈なし・閉じた和型で持つ）を添える。hover と定義の答えを使うのは「同じ文書がアクティブ・`revision()` が同じ・キャレットが同じ」ときだけで、違えば捨てる（知らせも出さない）。補完の答えは、要求を出した補完の面（`CompletionSession` の通し番号を文脈に持つ）がまだ開いていて同じ文書なら使い、候補は今の接頭辞で手元で絞り直す（打つたびに要求し直さない・答えが `isIncomplete` のときだけ、次の打鍵で古い要求を取り消して出し直す）。同じ種類の新しい要求を出すときは古い要求に `$/cancelRequest` を送り、表から外す（遅れた答えは id が表に無いので捨てられる）。答えの待ちの上限は今の `LanguageServerTick` の古さ（`language_server_stale_ticks`）のまま（短いタイマーは足さない・文脈の照合で古い答えは使われない）。
2. **送る前に未送信の変更を流す。** 要求の直前に、その文書の `flush_language_server_changes` を呼ぶ（`didChange` の後に要求が届く順を守る）。`didOpen` していない文書（無題・4 MiB 超・言語が違う・サーバーが動いていない）では要求しない。そのときの鍵の振る舞いは決定 5 / 7 / 8。
3. **能力の申し出と保持。** `initialize` の client の能力に `textDocument.completion`（`completionItem.snippetSupport=false`・`documentationFormat=["plaintext"]`）・`hover`（`contentFormat=["plaintext","markdown"]`）・`definition`（`linkSupport=true`）を書く。答えの `capabilities` から `completionProvider`（と `triggerCharacters`）・`hoverProvider`・`definitionProvider` を読み、`LanguageServerLink` に `ServerAbilities` として持つ。持たない能力の要求は送らない。
4. **答えの読みは core の純関数。** `lsp_completion_items(JsonValue) -> expected<LspCompletionList, LspAnswerFailure>`（`CompletionItem[]` と `CompletionList{isIncomplete, items}` の両方・`textEdit`（`TextEdit` と `InsertReplaceEdit` は insert の範囲）・`insertText`・`label`・`kind`・`detail`・`sortText`・`filterText`。500 件で切る）、`lsp_hover_text(JsonValue) -> std::optional<std::string>`（`MarkupContent`・`MarkedString`・その配列を平文に。Markdown は囲みの ``` の行を外すだけ・40 行と 4 KiB で切る）、`lsp_locations(JsonValue)`（`Location`・`Location[]`・`LocationLink[]`（`targetSelectionRange`）・null）。`LspDiagnostics.cpp` の無名の `position_member` `range_member` は公開の 1 本（`LspJsonRead.hpp`）に上げて診断と共用する（ARC-012）。URI → パスは core の `file_path_of_uri`（`file:` だけ・百分率の復号は `ImageReference.cpp` の無名の 1 本を公開へ移して共用）。`additionalTextEdits`・`command`・`resolve` は使わない（限界）。スニペット形式（`insertTextFormat=2`）の項目は `$0` `${1:x}` の印を外した平文を入れる。
5. **補完の面と鍵（ADR 0169 の面のまま）。** 候補の順は **LSP が先・本文の語が後**（LSP の中は `sortText`（無ければ `label`）の順・Vim の `complete` の順は本文の語の中で守る）。面は開いた時点の本文の語ですぐ出し、LSP の答えが来たら同じ列の先頭へ足す（`CommandPalette::extended` と同じ形・選んでいる行は動かさない）。鍵:
   - **Vim の INSERT**: ADR 0169 の Ctrl-n / Ctrl-p / Ctrl-e / Ctrl-y / Esc のまま（Vim の振る舞いの正本は oracle・LSP の候補は Vim に無いので、LSP がオフか動いていない文書では ADR 0169 の形と 1 文字も違わない）。Ctrl-x Ctrl-o（omni）は Ctrl+X が切り取りに当たっているので入れない（限界）。
   - **通常モード（D82・Windows の標準）**: **Ctrl+Space** で開く（VS Code・Visual Studio と同じ）。↑ ↓ で選び、**Enter と Tab で確定**、Esc で閉じる（本文はそのまま）。文字を打つと面は開いたまま接頭辞で絞り直し、語の文字でない文字を打つと閉じる。通常モードでは選ぶたびに本文へ入れない（確定のときに入れる・Windows の標準の形）。
   - **自動で開く**: 通常モードで、LSP が動いている文書でサーバーの `triggerCharacters` の 1 文字を打ったときだけ開く（`.` `->` `::` など）。Vim の INSERT では自動で開かない（Vim に無い）（→ 2026-11-23 の追補（#1962）で、Vim の INSERT でも開くと改めた）。LSP がオフのときの打鍵の道に分岐を足さない（D67・決定 10）。
   - Ctrl+Space は鍵の表（`operation_bindings`）の操作「補完」の行で、F1 の一覧にも載る。Vim の INSERT の Ctrl+Space（Ctrl-@）は今のまま譲る（行を通常モードだけにする）。
   - 確定で本文に入れるのは 1 編集（`replace` の 1 か所・undo の 1 単位・Vim の INSERT では INSERT の 1 単位の中）。範囲は `textEdit` があればその範囲（キャレットの行の中に限る・外なら接頭辞の置き換えに落とす）、無ければ接頭辞（語の頭からキャレット）を `insertText` か `label` で置き換える。
6. **hover の札。** 答えの平文を、キャレットの上（入らなければ下）に小さな札で出す。部品はタブの札（#1789 の `TabTipBox` `TabTipPaint`）の形を使い回し、最大幅 480 DIP・最大 12 行（越えたら末尾に「…」）。色は `panel` / `panel_border` / `text`（色のトークンは足さない）。札は次の打鍵・クリック・スクロール・タブの切り替え・窓の非アクティブ化で消える（frame の値 `EditorFrame.hover`・ui は読むだけ）。答えが空なら札を出さず、ステータスバーに 1 行（`UiMessage` の新しい名前）。マウスを載せて出す hover は入れない（`WM_MOUSEMOVE` の道を重くしない・後続）。
7. **定義へ。** 答えの最初の場所へ跳ぶ。同じ文書なら `''` のマークを置いてキャレットを移す（Vim の `gd` と同じくジャンプリストに残す）。別のファイルなら全文検索の着地（`ProjectHitLanding` と `land_project_hit`）と同じ道で `open_document` → 着地（開いた文書のジャンプリストに置く・ファイルをまたいで戻る列は無い・限界）。場所が 2 つ以上なら最初へ跳んで「1 / N」を 1 行知らせる。見つからなければ 1 行知らせる。
8. **鍵（L3）。**
   - **Vim の NORMAL**: `gd` = 定義へ（LSP が動いている文書だけ。動いていなければ今と同じく失敗する。Vim の局所の定義 `gd` `gD` は後続で、そのときは「LSP が無ければ局所」に落とす・計画 L-2）。`K` = hover（同じく LSP が動いている文書だけ。Vim の `keywordprg` は入れない）。表は `VimKeyTable` の g の表と通常の表に 1 行ずつ。engine は効果 `VimEffect` の閉じた値（`language_definition` `language_hover`）を返すだけで、要求を出すのは controller（engine は port に触れない・ADR 0051 の形）。
   - **通常モード**: **F12** = 定義へ（Visual Studio・VS Code と同じ）、**Ctrl+K Ctrl+I** は 2 打の和音で表に無いので入れず、操作「情報を表示」は鍵なしで F1 の一覧から選べる形にする（鍵は試用の後に hide が決める）。どちらも `EditorOperation` に 1 値ずつ（「定義へ移動」「情報を表示」）。
9. **文と訳。** 新しい知らせ（「定義が見つかりません」「情報がありません」「言語サーバーが動いていません」「候補が N 件の 1 番目へ」など）と操作の名前は `UiMessage` に足し、3 列に同じ位置で 1 行ずつ（CNF-016）。サーバーの返す文（hover・`detail`）は訳さない。
10. **速さ。** LSP がオフのとき、打鍵の道に新しい分岐を足さない（補完の面が閉じていて LSP が動いていなければ、今の 8 本の道と同じ命令列・`triggerCharacters` の照合は「LSP が動いている文書」の 1 つの真偽の後ろ）。オフの 8 本は今の基準値で退行なし（QLT-014）。オンの費用は任意のベンチ `lsp-completion-open`（Ctrl+Space から面の最初の frame まで・替え玉のサーバー）を足して PR 本文に記録する（基準値は試用の後）。
11. **試験。** core: 答えの読み 3 本と URI の写しの表の試験（`--lsp-answers`）。application: 替え玉の `ScriptedLanguageServer` に「送った要求の id を読む」補助を足し、文脈の照合（版が変わった答え・別のタブの答え・取り消し）・候補の順・確定の範囲・`gd` の 2 つの着地・hover の札が frame に載ることを `--language-server` に足す。adapters の替え玉 exe に `--completion N` `--hover` `--definition <path>` を足す。ui: `nib_window_tests` に面と札の画素を 1 つずつ。Vim の oracle: LSP の候補は Vim に無いので fixture は足さない（ADR 0169 の fixture が Vim の形を守る）。

## 強制

- 文脈の照合・候補の順・確定の範囲・着地: **active**（`nib_tests --language-server` `--lsp-answers`）。
- 能力の無い要求を送らない: **active**（替え玉の送った列の照合）。
- オフの 8 本: **active**（QLT-014・PR のゲート）。
- オンの速さ: **planned**（任意のベンチ・基準値は試用の後）。

## 結果

得られるもの: clangd などで、通常モードは VS Code と同じ鍵（Ctrl+Space・F12）、Vim は Ctrl-n の面に LSP の候補・`gd`・`K` が効く。答えの文脈を照合するので、打っている間に遅れて届いた答えが違う場所に入らない。
失うもの・残る穴: `additionalTextEdits`（自動の import）・`resolve`・スニペットの跳び先・署名の help・マウスの hover・ファイルをまたいで戻る列・Ctrl-x Ctrl-o・参照の一覧（`gr`）・名前の変更は入れない（後続）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 答えを待つ短いタイマー（数百 ms）を足す | 文脈の照合で古い答えは使われないので、待ちの上限は今の古さの数え方で足りる。タイマーを増やすと合図の道が増える |
| 補完の面を LSP 用に別に作る | ADR 0169 の決定 5（鍵が衝突する） |
| 通常モードでも Ctrl-n / Ctrl-p | Ctrl+N は新しいタブ、Ctrl+P はファイルの一覧（D71・Windows の標準） |
| Vim の INSERT で自動で開く | Vim に無い。Vim のモードは本物の Vim が正本（→ 2026-11-23 の追補（#1962）で取り下げた） |
| マウスを載せて hover | `WM_MOUSEMOVE` の道に要求を足すと、LSP がオンの全部の動きに費用が乗る。後続で測ってから |

## 追補（2026-11-18・#1851・設計席）: 補完の面・hover・定義の実装とレビューで決めた形

PR #1844（#1833）と PR #1843（#1834）の実装と独立レビュー（`D:/NeNeNib/outputs/20261118/review-1844.md` `review-1843.md`）を受けて、決定 4〜7 を次の形に固める。

1. **打鍵で自動に開く面の候補は LSP だけ。** 語の表（ADR 0169）の候補は、使う人が補完を呼んだとき（操作「補完」・Vim の INSERT の Ctrl-n）にだけ列に入る。LSP がオフの文書では、打鍵で面は開かない。
2. **面の行のクリックは確定。** 押した行と離した行が同じときだけ動く。面の上で始まった押しの列の 2 回目・3 回目は本文へ渡さない。面の上の右ボタンと中ボタンは面を閉じるだけ。
3. **定義へ跳ぶのは、何も待っていないときだけ。** 答えが届いた時点で、入力行・面・右クリックのメニュー・IME の変換が開いておらず、Vim は NORMAL でオペレータ・回数・レジスタの指定・次の鍵・`map` の待ちが無い（通常モードは選択の起点が問いを出したときのまま）ときだけ跳ぶ。違えば知らせずに捨てる。答えは合図の道で届くので、使う人の操作を割らない。
4. **`gd` と `K` の engine の歩は、いつも「表に行が無い鍵」と同じ状態・同じ失敗の印**で、効果だけが LSP への問い。engine は LSP が動いているかを知らず、port に触れない。NORMAL・回数なし・オペレータなしのときだけ問いになる。`:normal!` と再生（`.` `@`）は `gd` `K` の後ろの鍵を捨てる（答えは後から届くので、後ろの鍵を先に走らせない）。本物の Vim の `gd`（局所の宣言へ跳ぶ）と `K`（`keywordprg`）にはせず、LSP が無ければ今までどおり失敗（限界・fixture には入れない）。
5. **学び。** 操作（`EditorOperation`）を 1 つ足したら、F1 の一覧の行数を数える検査（`--command-palette` `--shell-registration`）が動く。依頼書の検証にこの 2 つと `--every-scope` を書く（#1844 で数え漏れが 2 件、後ろの段 #1850 で見つかった）。
6. **補完の 2 つ目の鍵は `Ctrl+I`（通常モードだけ・施主決定 D104）。** `Ctrl+Space` は簡体字の IME のオン・オフに取られる環境があるので、逃げ道を 1 つ持つ。`Ctrl+I` は文字としては Tab（0x09）で届くので、操作を走らせた後の文字を本文へ入れない。Vim のモードには足さない。

## 追補（2026-11-23・#1961・設計席の決め・施主の試用で出た不具合）: 決定 3 の能力に診断と同期を足す

決定 3 の `initialize` の `textDocument` の能力は補完・hover・定義だけで、`publishDiagnostics` を名乗っていなかった。typescript-language-server 6.1.0 は、client がこの能力を名乗らないと `textDocument/publishDiagnostics` を送らない（窓を出さない調べで 0 通 → 名乗ると 1 通）。

- **名乗るのは実装してある物だけ**: `textDocument.synchronization` は `{"didSave":true}`（保存の後の `didSave` は ADR 0161 の 2026-11-20 の追補で送っている・`willSave` `willSaveWaitUntil` は送らないので書かない・`dynamicRegistration` は書かない）。`textDocument.publishDiagnostics` は `{"versionSupport":true}`（`version` は `lsp_published_diagnostics` が読み、送った版と照らしている）。`relatedInformation` `tagSupport` `codeDescriptionSupport` は読まないので書かない（ADR 0161 の決定 9 の限界のまま）。`workspace` の能力（`workspaceFolders` `configuration`）は実装していないので名乗らない。段 S6 の参照・記号・引数の情報・整形は、それぞれの PR が自分の能力を足す。書く所は `text_document_capabilities` の 1 か所のまま。
- **ほかのサーバー**: 同じ能力の前後で、pyright・gopls・clangd は誤りの行を足した文書に同じ診断を 1 通ずつ返し、rust-analyzer は前後とも同じ（25 秒の間は空の通知 1 通・型の誤りは `cargo check` の後）で、振る舞いは変わらない（窓を出さない道具 `D:\NeNeNib\scripts\20261123\k-1961-caps\caps-probe.py`・試用のプロジェクトのファイルは書き換えない）。
- **URI の綴り**: typescript-language-server と pyright は `file:///d%3A/...`（ドライブが小文字・`:` が `%3A`）で返す。前からパスへ写して照合しており（ADR 0161 の 2026-11-20 の追補）、`--language-server` の `verify_diagnostics_uri_forms` にこの綴り（`file:///c%3A/work/main.cpp`）を足した。
- **契約**: `--language-server` の `verify_abilities` の `initialize` の期待に `synchronization` と `publishDiagnostics` を足した。

## 追補（2026-11-23・#1962・施主の決め）: Vim の INSERT でも、打鍵で補完の面を自動で開く

決定 5 の「Vim の INSERT では自動で開かない（Vim に無い）」を改める。施主が gopls で `fmt.` と打って何も出ず「動いていない」と見えた。LSP をオンにするのは開発者で、今の Vim 使いの多くは補完が自動で出る環境に慣れている（施主の決め 2026-11-23）。却下した選択肢の表の「Vim の INSERT で自動で開く」の行は、この追補で取り下げる。

1. **engine は LSP を知らないまま。** engine の歩と効果は変えない。照合するのは controller で、通常モードの `InsertText` と Vim の INSERT の 1 文字の両方が入口 `typed_for_language(打った文字)` の 1 か所を通る（引数の情報の段 S6c もここに足す）。照合は `open_triggered_completion` の 1 本（通常モードと共用）。
2. **使う人が打った鍵だけ。** Vim の側の入口は打った鍵の意図 `accept(VimKeyPress)` だけで、再生（`.` `@`）・`:normal!`（`press_vim_keys`）・IME の確定（`type_as_vim_keys`）はここを通らない。さらに、打った鍵を流す間に engine へ渡した**最後の鍵**が「出どころ typed（`map` の右辺の単位は `KeyMapEmit` の `mapped` で `typed_unit` が mapped にする）・再生の外・INSERT で 1 鍵の引数（`Ctrl-r` のレジスタの名前・`Ctrl-v`）を待っていない文字の鍵」で、鍵の後も INSERT で、その 1 文字が流す前のキャレットの直後にそのまま入ったときだけ渡す（`deliver_vim_key` の頭の `watched_vim_key`・言語サーバーが running の間だけ見張る・レビュー #1965 の推奨 1）。`map` の右辺・`<C-r>.`・貼り付けで入った文字は渡さず、左辺の待ちの後に一度に流れた鍵（`inoremap jk <Esc>` で `j.`）の `.` は打った文字として渡す。限界: 待ちが `MapTimeout` で流れた文字と、待ちと一度に流れた鍵の最後でない文字（`inoremap .. ->` を持つ窓の `.f` の `.`）は渡さない（前者は打った鍵の意図の外で流れ、後者は最後の鍵だけを見る）。
3. **自動で開いた面は engine の外。** 面（`CompletionSession` の `by_hand` が偽）は controller が持ち、通常モードの自動の面と同じ道（`settle_completion` → `completion_holds` → `refresh_completion`）で、語の文字で絞り直し、語の文字でない文字・キャレットの後退で閉じる（次の trigger なら開き直す）。最初は何も選ばない（面の選んでいる行は rows の外・本文を変えない）。
   - **Ctrl-n / Ctrl-p**: 面が出ていれば、面の候補（今の接頭辞で絞った LSP の候補）を engine の写し（`VimCompletionOffer`）にして ADR 0169 の形（選ぶたびに本文へ入れる）へ移る。以後は engine が持つ面（`by_hand` を立てる・要求は出し直さない）。LSP の候補を engine の語に直すのは手で開いた面と同じ `vim_completion_text` の 1 本（ARC-001）で、比べる要求の範囲は面が最後に要求を出したときの接頭辞（`CompletionSession.requested`）から作る。どちらの面から移っても、同じ答えの同じ候補が入る / 入らないは同じ。
   - **Ctrl-y / Ctrl-e**: 何も選んでいないので、本文を変えずに面を閉じるだけで Vim に渡さない（録画にも積まない。今の Nib の engine は補完が閉じていれば Ctrl-y / Ctrl-e で何もしないが、本物の Vim の `i_CTRL-Y` `i_CTRL-E` は上下の文字を写すので、積むと Nib がそれを入れたときに再生の結果が打ったときと変わり、本物の Vim の再生ともずれる）。面が出ている間は、この 2 つの鍵は `map` の表より先に面が取る（`inoremap <C-e> <End>` は面が出ている間だけ効かない。左辺の待ちが積まれていれば待ちは残り、時間切れで流れる。どちらも稀）。
   - **Esc・Tab・文字・Backspace**: Vim のまま流す（Esc は NORMAL へ・Tab は文字を入れて、出口で面が閉じる）。Enter を含むほかの特殊鍵（矢印など）は面を閉じてから流す（Enter は Vim のまま改行を入れる。面を閉じるのが流す前か出口かの違いだけで、結果は同じ）。（→ Enter と Tab は 2026-11-24 の追補（#1971）で確定の鍵に改めた）
4. **答えを待つ間**（面はまだ出ていない）の鍵は全部 Vim のまま。Ctrl-n は待っていた自動の要求を取り消して ADR 0169 の本文の語の補完を開く。INSERT を抜けたら（Esc）・IME の変換が始まったら（`ComposeText` は面を閉じる意図）取り消す。
5. **undo の単位・`.` の中身・レジスタ・録画の中身**は、面が開いた・閉じた・答えが届いたことで変わらない（自動の面から移った補完の録画は 2026-11-24 の追補（#1971）の決定 5）。Ctrl-n で確定した語は ADR 0169 の道のまま「打った文字 ＋ 確定で入った文字」が `.` の記録に残る（engine の `completion_typed_keys`）。
6. **速さ。** LSP がオフ（言語サーバーが 1 つも起きていない）のとき、Vim の打鍵の道に足すのは `accept(VimKeyPress)` の真偽 1 つの読みと、engine へ流す鍵ごとの `deliver_vim_key` の真偽 1 つ（`typed_watch_`・倒れたまま）の読みだけ（決定 10）。起きていても今の文書の言語サーバーが running でなければ見張らず、1 文字ごとに文字列を作らない。
7. **限界。** 自動で開いた面の行のクリックは面を閉じるだけ（Ctrl-n を経ない確定は engine の補完の外で、`.` の記録に載せる道が無い）。（→ 2026-11-24 の追補（#1971）で、Ctrl-n で engine の補完へ移してから確定すると改めた）
8. **試験。** `nib_tests --completion-face` の Vim の側（`VimInsertCompletionTests.cpp`）に、要求 1 通と答えで面が開くこと・絞り・Ctrl-n / Ctrl-e / Ctrl-y / Enter / 語の文字でない文字・`->`・待つ間の Ctrl-n と Esc の取り消し・再生（`.` `@` `:normal!`）で要求 0 通・LSP がオフで port に触れないことを足した。見分けの条件は、`map` の右辺（`map.i.x=.` で `ox`）と `<C-r>.` で要求 0 通・`inoremap jk <Esc>` の待ちの後の `.` で要求 1 通・録画の中に面を閉じる Ctrl-e / Ctrl-y が積まれないこと（レジスタ `a` が `ox.y.<Esc>`）で縛る（レビュー #1965 の推奨 2）。Vim の fixture・fuzz は変わらない（oracle は LSP を持たない）。

## 追補（2026-11-24・#1971・施主の決め 2026-11-23）: 自動で開いた面は、行のクリックと Enter / Tab で確定する

2026-11-23 の追補（#1962）の決定 3（自動で開いた面の Enter と Tab は Vim のまま）と決定 7（行のクリックは閉じるだけ）を改める。施主の決め（2026-11-23 朝・「推しでいいと思う」）: triggerCharacters で開いた面（通常モードと Vim の INSERT）で、**行をクリック → その行で確定・Tab → 選んでいればその候補、選んでいなければいちばん上の候補で確定・Enter → Ctrl-n / Ctrl-p（通常モードは ↑ ↓）で選んだときだけ確定（選んでいなければ今までどおり改行を入れて面を閉じる）**。手で開いた面（Ctrl+Space・Vim の Ctrl-n / Ctrl-p で開いた面）の鍵とクリックは変えない。

1. **「選んだか」は面の印。** 面の状態 `CompletionSession` に `triggered`（打鍵で自動に開いたこと・Vim で engine の補完へ移った後も立ったまま）を足す。engine の外の自動の面（`by_hand` が偽）は、使う人が選ぶ行を動かす（`stepped`）までどの行も選ばずに出す（`awaits_choice`・通常モードも Vim と同じ見た目にした）。通常モードの最初の ↓ は先頭・↑ は最後の行・PageDown は面の行数ぶん・PageUp は先頭を選ぶ。ホイールは窓だけを送って選ばない。打って絞り直すと「選んでいない」に戻る（`narrow` が `stepped` を下ろす今の形）。Vim の engine の補完へ移った面の「選んでいる」は engine の位置が候補の上にあること（輪の端の元の接頭辞は選んでいない）。
2. **通常モード。** 窓は Enter と Tab を鍵の種類 `CompletionCommitKey` つきで `completion_commits` に問い、選んでいない自動の面の Enter は偽（窓は面を閉じて改行を本文へ通す）。Tab と行の名指しの無い確定は `selected`（選んでいなければ先頭）の 1 本で、行のクリックは今の `ChooseCompletion{row}` のまま。先頭の候補が打った語と同じ綴りなら、Tab の確定は本文を変えないので `completion_commits` は偽で、窓は面を閉じて Tab の文字を通す（#1844 の L1 の今の形のまま）。Shift+Tab は前のまま面が受けない（修飾つきの鍵）。
3. **Vim の確定は「Ctrl-n で選んで確定」の 1 本。** 2 本目の確定の道を作らない。engine の外の面の行のクリックと Tab は、面の候補を Ctrl-n で engine の補完へ移し（`offer_auto_vim_completion` の今の道）、engine の列の同じ候補を面の行のクリックの 1 歩（`vim_completion_chosen`）で入れる。engine の補完へ移った面の Enter と Tab は、選んでいる候補があれば Ctrl-y と同じに確定して閉じる（`close_completion` の `vim_completion_settled`・鍵は Vim に渡さない）。元の接頭辞を選んでいれば Tab は先頭の候補をクリックの 1 歩で入れ、Enter は Vim のまま流れる（確定する語が無いので改行だけ）。engine の語に直せない候補（`textEdit` の範囲が直せない・決定 3 の Ctrl-n と同じ判定 `auto_vim_rows`）の行のクリックは面を閉じるだけ。Tab は直せる候補のいちばん上で、1 つも無ければ Vim のまま。選んでいる候補が元の接頭辞と同じ綴りなら、Enter と Tab は確定に読まず Vim に渡す（改行と Tab が空振りしない・決定 2 と同じ考え）。確定するのは修飾の無い Tab だけで、Shift+Tab は前のまま（engine へは同じ Tab の文字で渡るので、窓が先に面を閉じる・`tab_closes_completion`）。
4. **`map` より確定が先。** Enter と Tab の確定は打った鍵の入口 `typed_vim_key_with_language` の頭の `triggered_confirm_took`（`auto_vim_completion_took` より先）で、`map` の表を引く前に読む（`inoremap <Tab> ...` があっても、面が出ていれば確定・面が無ければ map のまま）。左辺の待ちが積まれている間は確定に読まず、今までどおり表を引く（待ちの鍵が先に流れる順を崩さない・`rhs_unit_step` の順のまま）。
5. **録画・`.`・undo（D109）。** 確定は INSERT の undo の 1 単位の中（置き換えは `EditBoundary::absorb`）。engine の外の面の行のクリックと Tab は、Ctrl-n が先頭の語を入れる置き換えと選んだ語の置き換えの 2 編集で、どちらも同じ単位に吸収され `u` 1 回で戻る。`.` の記録は ADR 0169 の道のまま「打った文字 ＋ 確定で入った文字」（engine の `completion_settled` が足す）。マクロの録画は、打鍵で自動に開いた面の補完の鍵（Ctrl-n・Ctrl-p・Ctrl-e・Ctrl-y と、確定の Enter・Tab）を積まず、確定した語を打った本文として積む: controller が補完の鍵を出どころ `mapped`（engine がマクロに積まない出どころ・`map` の右辺と同じ）で流し、確定の前（補完の鍵でない鍵の前・Ctrl-y・Enter / Tab の確定・クリック・engine の外からの閉じ）に `vim_completion_typed` の鍵の列を `vim_macro_typed` で積む。再生（`.` `@` `:normal!`）では面を開かず確定もせず、記録した文字を流す（engine は LSP を知らないまま・oracle と食い違う振る舞いは足していない・足した core の口は `vim_completion_ends` と `vim_completion_typed` の 2 つの読み）。手で開いた面の録画は今のまま（鍵をそのまま積む・Vim の形）。
6. **速さ。** LSP がオフの文書の打鍵の道に足したのは、`step_vim` の頭の `completion_` の 1 つの欄の読みだけ（自動の面は言語サーバーが running の文書でしか開かない）。
7. **限界。** 左辺の待ちが積まれている間の Enter と Tab は確定しない（決定 4）。Ctrl-y / Ctrl-e は今のまま待ちに依らず面が取る。待ちの鍵が先に流れて面が閉じれば、続く Ctrl-n は手で開く補完になる（鍵をそのまま録る）。右辺に補完の鍵を持つ `map`（`inoremap ,n <C-n>` など）で自動の面から確定した録画は、左辺と確定した語の両方を持つので、再生で本文の語の補完が開いて結果が変わり得る（`inoremap ,y <C-y>` は語が左辺より先に積まれて順も入れ替わる・まれで、左辺を捨てると右辺のほかの鍵が消えるので直さない）。改行を含む候補（複数行の `insertText`）は録画に改行の打鍵として残り、`autoindent` がオンだと再生で字下げが付いて確定したときと違う本文になり得る（`.` の記録にも ADR 0169 から同じ形がある）。言語サーバーが止まって起きている数が 0 になった後に残った engine の面では、Enter と Tab は確定に読まず Vim のまま（打った鍵の入口が言語サーバーの真偽の後ろ・本文は壊れない）。手で開いた面（Vim の Ctrl-n）の行のクリックは前からマクロに残らない（手で開いた面は Vim の形のまま・この追補は触らない）。
8. **試験。** `nib_tests --completion-face` に `CompletionConfirmTests.cpp` を足した: 通常モード（選ばずに出る・Enter は確定しない・Tab は先頭・↓ ↑ の最初の 1 歩・ホイール・クリック）、Vim の INSERT（Tab の 3 形・Enter の選んだ / 選んでいない 2 形・クリックの 2 形・`.` の記録・`u` の 1 単位・`.` の再生で要求 0 通）・マクロの録画（確定の 5 形と Ctrl-e の取り消しの後にレジスタが打った本文と Esc だけ・`@a` の再生で同じ本文・要求 0 通・面を開かない）・手で開いた面の Enter と Tab が変わらないこと・`inoremap <Tab>` より確定が先なこと。レビュー R3 の M1 で、通常モードの PageDown / PageUp の最初の 1 歩と打った語と同じ綴りの先頭での Tab・直せない候補のクリックと Tab・左辺の待ちの間の Tab と `inoremap <CR>`（選んでいない面は map・選んだ面は確定）・engine の外からの閉じと Ctrl-n の後の Esc の録画と再生・元の接頭辞と同じ綴りの候補での Enter・Shift+Tab を足した（Shift+Tab の窓の判定は `nib_window_tests --completion` の `tab_closes_completion`）。#1962 の試験（`VimInsertCompletionTests.cpp`）は変えずに通る。Vim の fixture と fuzz の固定 seed は変わらない（oracle は LSP を持たない）。

強制: 上の振る舞いは **active**（`nib_tests --completion-face`）。
