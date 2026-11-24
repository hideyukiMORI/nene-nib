# ADR 0195 — 構文の役割は 19 に増やして色は既存の 16 欄からの導出の表で決め、言語サーバーの semantic tokens は文書の値として持って字句器の区間の上に重ねる

- 状態: 提案（下調べの席 A2・2026-11-25・実装しない。色の値と見え方は hide が /design の絵で決める・下の「施主に確かめる事」）
- 日付: 2026-11-25
- Issue: #1974（計画の親）・親 #995（LSP）・下調べ `D:/NeNeNib/outputs/20261125/plan-syntax-roles-1974.md`（起点 `66be2a85`・4 つのサーバーの実測は `D:/NeNeNib/scripts/20261125/semantic-probe/`）・前提 ADR 0017（テーマと `SyntaxPalette`）/ 0156（色は桁の区間）/ 0114（本文の文字組みの共有）/ 0160（言語サーバーの入出力）/ 0161（診断は文書の値）/ 0190（要求の表）/ 0193（layout の外に重ねる飾り）
- 規則: FR-021、D11 / D13 / D60 / D62 / D65、ARC-001 / ARC-003 / ARC-010 / ARC-012、CPP-002 / CPP-012、QLT-014

## 文脈

hide の指摘（2026-11-23）は「構文の色の表現力がほかの IDE より狭い」。現物を数えると、狭いのは色の数ではなく**役割の数と、字句器が見分けられる物**である。

- `core::SyntaxRole` は 10 値（`src/core/SyntaxRole.hpp`）。字句器が出すのは 9（8 つの分類と、閉じない文字列などの `error`。`warning` は出さない）。Issue #1974 の本文の「字句器 8」は分類の 8 を数えた物。
- 字句器は表引きと「直後が `(`」で分類するので、変数・引数・フィールド・名前空間・自分で定義した型・enum の値・マクロの使用は全部、区間なし（本文の色）になる。タグは keyword・属性名は type・エスケープは string に間借りしている（ADR 0156 決定 5・18）。
- テーマの `SyntaxPalette` は 16 欄（うち構文の色は 10）で、組み込みの 125 テーマが手書きの値を持つ。利用者のテーマファイルの鍵も同じ 16（`ThemeColorFields.hpp`）。8 つの分類の色が 8 色とも違うテーマは 125 のうち 48 で、number = constant が 53・keyword = operators が 41（採用案の 2 テーマも number = constant）。
- base16 の styling guidelines（ https://github.com/tinted-theming/home/blob/main/styling.md ）は引数・プロパティ・名前空間・マクロ・組み込みの置き場を決めていない。変数は base08 と書くが、テンプレートは本文の色（tinted-vim）と base08（tinted-vscode・tinted-helix）に割れている。
- 言語サーバーの semantic tokens（LSP 3.16〜・ https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_semanticTokens ）を、この機械の 5 つのサーバーで実測した: clangd 19.1.5 は full ＋ delta（range なし）、rust-analyzer 1.100.0 は full ＋ delta ＋ range（delta の要求に実測では full で答えた・解析の前は足りない答えを返して後で refresh を送る）、typescript-language-server 6.1.0 は full ＋ range、gopls 0.24.0 は `initializationOptions` の `semanticTokens` を立てたときだけ full ＋ range、pyright 1.1.414 は出さない。1,400 行前後の文書で 3,000〜4,200 token・答えは 30〜60 KB。行末を越える token と重なる token は 0 件。clangd と typescript-language-server はキーワード・文字列・コメントを出さない。
- 色は layout の外で描くときだけ当てる（brush は 1 本で `SetColor`・ADR 0156 決定 11）。役割が増えても brush と layout の鍵は増えない。太字と斜体は字形が変わるので layout の鍵を割る（ADR 0114）。
- LSP は設定でオン・既定オフで、オフのときの振る舞いと速さは変えない（D60・FR-021）。`settings.v6` は 1.2.0 で閉じ、この計画は 1.2.0 の後。

## 決定

**役割は閉じた enum のまま 19 値に増やす。色は `SyntaxPalette` の 16 欄を増やさず、core の導出の表 1 つ（既存の欄の整数の混色）から引く。字句器は見分けられる役割だけを足して出す。言語サーバーが動いている文書は `textDocument/semanticTokens/full` の答えを文書の値として持ち、core の純関数 1 本が行ごとに字句器の区間の上へ重ねて、今と同じ `ColorSpan` の列にする。**

### 役割と色

1. **`SyntaxRole` に 9 値を足す**（末尾に足す・名前は実装で決めてよい）: `variable` `parameter` `property` `module`（名前空間・`namespace` は予約語）`macro` `tag` `attribute` `escape` `builtin`。既存の 10 値の意味と並びは変えない。
2. **足さない物は既存の役割へ写す。** 自分で定義した型・型引数は `type`、メソッドは `function`、enum の値は `constant`、修飾の語は `keyword`、正規表現は `string`。分けたくなったら 1 値と表に 1 行を足す。
3. **`SyntaxPalette` は 16 欄のまま。** 125 テーマの値・利用者のテーマファイルの鍵（`body.*` の 16）・`theme-sources.json` を変えない。
4. **足した役割の色は core の導出の表 1 つ。** `syntax_color(palette, role)` が、既存の 10 は今までどおり同じ名前の欄を返し、足した 9 は表の行 `{元の欄, 寄せる先の欄, 百分率}` から `mix`（ADR 0017 決定 4 の整数の補間）で作る。表は役割の順に 1 行ずつで、行の数は `static_assert` が役割の数と縛る。呼び手（本文・格子・ミニマップ・プレビュー・印刷・エクスプローラーの印）は今の 1 本を呼ぶだけで、別の対応表を持たない（ARC-001）。
5. **役割を足す段の表の値は「今の色」。** `variable` `parameter` `property` `module` は foreground、`macro` `tag` は keyword、`attribute` は type、`escape` は string、`builtin` は function（百分率 0）。この段では画面が 1 画素も変わらない。**色を変える値は見た目の決めで、hide が /design の絵で選んでから表を書き換える**（ここで決めるのは規則の形で値ではない・ADR 0017 決定 4 と同じ）。
6. **テーマごとの上書きは 1 か所。** 導出で足りないテーマ（まず採用案の `ubuntu-aubergine` と `neutral-light`）は、テーマの表の脇の上書きの表に手書きの値を置く（`derive_ui` と同じく、導出の関数の中にテーマ名の分岐を書かない・ADR 0017 決定 5）。利用者のテーマファイルに任意の鍵を足すかは未決。
7. **既存の 10 の色は動かさない。** Markdown プレビューの見た目（D62: 見出しは keyword・コードは string・リンクは function）と `verify-window.py` の画素の表明がそのまま通る。

### 字句器

8. **字句器だけで見分けられる役割を、言語ごとに足す。** 全言語に効く物から: 文字列の中のエスケープと書式の指定（`escape`）・付け替えだけで済む HTML / XML のタグと属性（`tag` `attribute`）・JSON / YAML / TOML / ini のキーと CSS のプロパティ名（`property`）。次に言語ごと: `@name` `#[...]` `[[...]]`（`attribute`）・`#define` の名前と Rust の `name!`（`macro`）・組み込みの関数と変数の小さな表（`builtin`）・`.` の直後で `(` が続かない名前（`property`）・`::` の直前と `import` / `use` の行の名前（`module`）。
9. **字句器は変数と引数を出さない。** 表と 1 行の形からは決められない。`variable` と `parameter` は semantic tokens だけが出す。
10. **行をまたぐ状態（`LexState`）と遡りの行数は変えない。** 足すのは行の中の分類だけで、ADR 0156 の決定 3・4・8・9 はそのまま。分岐は表（CPP-012）。

### semantic tokens

11. **頼むのは `textDocument/semanticTokens/full` だけ。** client は `textDocument.semanticTokens` に `requests.full = true`（delta と range は名乗らない）・`formats = ["relative"]`・標準の `tokenTypes` と `tokenModifiers` のうち写す物・`overlappingTokenSupport = false`・`multilineTokenSupport = false`・`augmentsSyntaxTokens = true` を、`workspace.semanticTokens.refreshSupport = true` を名乗る。名乗るのは実装した段だけ（`text_document_capabilities` の 1 か所）。
12. **要求は ADR 0190 の表に 1 種類足す。** 文脈は (文書の退避番号, `EditHistory::revision()`)。答えは、同じ文書の revision が同じときだけ置き、違えば捨てて頼み直す。待ちの刻限と取り消しは既存の要求と同じ道。
13. **頼む時機は 4 つ。** 文書をサーバーへ開いたとき・タブを切り替えて戻ったとき（答えが今の revision の物でなければ）・編集が止まってから（`LanguageServerTick` を数える。時計は読まない）・`workspace/semanticTokens/refresh` が来たとき。打鍵ごとには頼まない。refresh の要求には結果 null で答える（今は MethodNotFound で断っている所に 1 つ足す）。
14. **legend の名前 → 役割は core の表 1 つ。** initialize の答えの `semanticTokensProvider.legend.tokenTypes` を、名前の表（標準の 23 と、実測で見た方言: clangd の `concept`・typescript-language-server の `member`・rust-analyzer の `builtinType` `selfKeyword` `typeAlias` `escapeSequence` `formatSpecifier` など）で添字 → `optional<SyntaxRole>` の列にして、子の席ごとに持つ。表に無い名前（`bracket` `operator` `unknown` `punctuation` など）は役割なし＝区間を作らない。
15. **答えは core の純関数が読む。** 整数 5 つ組の列と legend の写しと本文から、`{本文のバイト位置の begin, end, 役割}` の昇順の列を作る。位置の単位（utf-8 / utf-16）は既存の `LspPosition` の 1 本で写す。行末を越える token は行末で切り、前の token と重なる token・長さ 0・範囲の外・5 の倍数でない列は捨てる（失敗は `std::expected`・ARC-010）。
16. **持ち方は診断と同じ「文書の値」。** 置いた後は変えない列への参照（`shared_ptr<const vector<…>>`）を `DocumentState` の側に持ち、タブの切り替えに付いていく。作るのも読むのも UI の糸の上だけ。undo では戻さない（次の答えで直る）。
17. **編集のたびに同じ 1 本でずらす。** 診断の `diagnostics_adjusted` と同じく `offset_after_edit` で位置を写し、編集の範囲に掛かった token は消す（そこは字句器の色に戻る）。次の答えが来たら丸ごと置き換える。
18. **重ねるのは core の純関数 1 本。** 見えている行ごとに、字句器の区間（`lex_line`）とその行に掛かる semantic の区間を「semantic が勝つ」で 1 本の `ColorSpan` の列（昇順・重ならない・ADR 0156 決定 2）にして `LineView.colors` に置く。字句器の区間の途中に semantic の区間が入れば字句器の区間を割る（文字列の中の書式の指定）。renderer・格子・ミニマップ・印刷は変えない。巨大な行の窓・IME の差し込み行は ADR 0156 の決めのまま（差し込みのずらしは今の 1 本を通す）。
19. **修飾子は最初の段では読まない。** `defaultLibrary`（→ `builtin`）・`readonly`・`deprecated` をどう見せるかは見た目の決めで、後の段（施主に確かめる事）。列には bit のまま持っておく。
20. **上限。** サーバーへ送る文書は既存の 4 MiB（D65）。token の数にも上限を置き、越えた答えは置かずに字句器の色のままにして言語サーバーの記録に 1 行残す。値は段 S1 の実測で決める（未決）。
21. **`:syntax off` は semantic の色も消す。** 文書の値 `syntax_enabled` が偽なら頼まず、重ねない。設定の鍵は足さない（`lsp` がオンでサーバーが出すなら受ける）。

### 速さ

22. **LSP オフ・semantic tokens の無い文書の道は今と同じ。** frame は文書の値が空かを 1 回見るだけで、`colors` は字句器の区間そのまま。既存 8 本のベンチ（`.txt`）と基準値には触れない（D60・QLT-014）。
23. **オンの費用は別の基準で見る。** frame ごとに、見えている行の行頭の位置で列を二分探索して、その行の token だけを併合する。`lsp-` のベンチに「semantic tokens を受けた後の打鍵」を 1 本足す（別の基準・記録から始める）。字句器の役割が増えた分は任意のベンチ `key-to-frame-single-cpp` で記録する。

### 太字・斜体・線

24. **役割ごとの太字と斜体は持たない（この ADR）。** 字形が変わり、描画と位置解決で共用する layout の鍵と保持した字形の道を割る（ADR 0114 / 0073）。テーマの値も増える。持つなら別の ADR。
25. **打ち消し線（deprecated）は後の段の候補。** 字形を変えない線なので、layout の外に桁の区間で重ねる道（診断の波線・ADR 0193）で描ける見込み。出すか・見た目は施主に確かめる事。

## 強制

- CPP-002: `SyntaxRole` の `switch`（`syntax_color`・`MatchParen` の `excluded_role`・`PreviewBrushes`）に `default` を書かない。役割を足すとコンパイルで落ちる — **active**（既存）
- 導出の表の行の数と順が役割と合う（`static_assert`）・足した段の値で全テーマの色が今と同じ（`nib_tests --themes` に 1 本）— **planned**（段 R1）
- 字句器の fixture（`LexerTests`）と性質の試験（区間は昇順・重ならない）— **active**（既存）・新しい役割の fixture は **planned**（段 R2）
- 答えの読み・ずらし・重ねの純関数の単体と、契約（revision が違う答えは置かない・LSP オフでは要求が 0・`:syntax off` では頼まない・refresh で頼み直す）— **planned**（段 S1）
- LSP オフの速さ（QLT-014 の 8 本）— **active**（既存。まとめ枝で設計席が回す）
- ui/win32 に色のリテラルを書かない（ADR 0008 決定 8）— 既存の決めのまま

## 失うもの

- 導出の色は sRGB の整数の混色で、知覚的に均一ではない（ADR 0017 と同じ）。テーマの作者が決めた「変数の色」「引数の色」は使えない（出典に在っても写さない）。
- 8 つの分類が元々 6 色以下しか無いテーマ（125 のうち 54）では、導出しても色の幅は広がりにくい。
- semantic の色は言語サーバーの答えの後に付くので、開いた直後と打った直後は字句器の色で、後から色が増える（実測: 初回 40 ms〜2 秒・gopls は 18 秒）。
- 編集に掛かった token は次の答えまで字句器の色に戻る。行の途中の 1 字の挿入でも、その token 1 つは色が落ちる。
- delta と range を使わないので、大きい文書では答えを毎回丸ごと読む（4 MiB の文書の答えの大きさは測っていない）。
- gopls は `initializationOptions` を渡さないと出さないので、段 S1 では色が付かない。pyright も出さない。
- 役割は閉じた enum なので、利用者が規則を書いて色を変える道（VS Code の `semanticTokenColorCustomizations`・Neovim の `@lsp.typemod.*`）は無い。

## 採らなかった案

| 案 | 採らない理由 |
| --- | --- |
| `SyntaxPalette` に欄を足して 125 テーマの値を書く | 1,000 個を超える値を足す。base16 に決まりの無い役割が多く、出典から写せない。テーマファイルの鍵も増える。導出 ＋ 上書きなら判断は上書きの分だけ（ADR 0017 の却下の表と同じ理由） |
| base16 の決まりどおり変数を base08（赤）にする | Nib の欄で base08 に当たるのは `error`。画面の大半の字が赤系になり、流儀も割れている。値は hide が絵で決める |
| TextMate の文法（scope の階層）を持つ | 文法の読み手と scope → 色の規則の 2 つを core に持ち込む。字句器は表と純関数のまま（ADR 0156） |
| tree-sitter を入れる | 外の依存と言語ごとの文法の生成物が増える。今回の目的（役割を増やす）は字句器の表と semantic tokens で足りる |
| サーバーの色だけにする（Zed の `full`） | clangd と typescript-language-server はキーワード・文字列・コメントを出さない。答えが来るまで無色になる |
| full ＋ delta を最初から | 出すのは clangd と rust-analyzer だけで、実測では rust-analyzer が full で答えた。減るのは読む量だけで、edits を当てる道（整列を仮定できない）が増える。実測で要ると分かってから |
| range（見えている範囲）を最初から | clangd が出さない。スクロールのたびに頼む道と、答えの無い範囲の扱いが増える。巨大な文書の段で見直す |
| 打鍵ごとに頼む（Neovim の形） | 1,400 行で 30〜60 KB の答えを打鍵ごとに読む。サーバーの作り直しも打鍵ごとに走る |
| 編集で全部の token を消して答えを待つ | 打つたびに色が明滅する |
| layout に `SetDrawingEffect` や `SetFontWeight` を載せる | 共有の layout が割れ、保持した字形の道を失う（ADR 0156 の却下の表・ADR 0114） |
| 修飾子（readonly → constant）を最初から写す | clangd は `const` の局所変数と引数にも readonly を付ける（見本の variable の token 14 個のうち 8 個）。定数の色だらけになるかは絵で見てから |
| 設定の鍵を足す | `settings.v6` は 1.2.0 で閉じる。鍵を足すと `settings.v7` が要る。まず `lsp` のオン・オフと `:syntax off` で足りるかを見る |

## 未決

- token の数の上限の値と、答えの JSON を読む費用（どの糸で何 ms か）。段 S1 の最初に `nib_probe` で測り、重ければ worker で整数の列まで読む形にする（ADR 0160 の追補）。
- 編集のたびに全 token を写してずらす費用（上限いっぱいのとき）。重ければ行ごとの相対の形で持つ。
- 「編集が止まってから」の tick の数。
- gopls の `initializationOptions`（よく知られたサーバーの表に初期化の値を持たせるか）。basedpyright の対応（この機械に無く未確認）。
- rust-analyzer の `selfKeyword` `lifetime` `attributeBracket` などの方言をどこまで写すか（表の行）。
- 利用者のテーマファイルに、足した役割の任意の鍵（`body.variable` など・無ければ導出）を足すか。
- 型引数・enum の値・ラベルを独立の役割にするか。
- 2 枠で同じ文書を見ているときの持ち方（文書の値なので 1 つで足りる見込み・推測）。

## 施主に確かめる事

見た目に関わるので、設計席が /design の絵（下調べの末尾の見本のコード 4 つが材料）を見せて決めてもらう。

1. **変数を本文の色のままにするか、色を付けるか。** 画面の字の大半が変わる。base16 の流儀も割れている（Vim の雛形は本文の色・VS Code と Helix の雛形は赤）。
2. **引数・プロパティ・名前空間を、変数と別の色にするか**（色数が増えると騒がしくなる）。導出の表の値（どの欄をどの欄へ何 % 寄せるか）と、採用案の 2 テーマ（茄子色のダーク・中立のライト）を手書きの値にするか。
3. **マクロ・タグ・属性・エスケープ・組み込みを、今の間借りの色（keyword・type・string・function）から変えるか。**
4. **標準ライブラリの物（`defaultLibrary`）と readonly を色で分けるか。**
5. **deprecated の打ち消し線を出すか。** 太字・斜体を将来持つか（持つなら別の ADR で、速さと引き換え）。
6. **言語サーバーの答えの後に色が増える見え方でよいか**（開いて 0.1〜2 秒後・打った所は次の答えまで字句器の色）。
7. **semantic tokens だけを切る設定が要るか。** 要るなら `settings.v7` に鍵を足す（v6 は 1.2.0 で閉じる・D91）。

## 段の切り方（子 Issue の案）

| 段 | 中身 | 画面 | 前提 |
| --- | --- | --- | --- |
| R1 | 役割 9 と導出の表（値は今の色）・`switch` の 3 か所 | 変わらない | 1.2.0 の公開の後 |
| R2-a | 字句器: escape・tag / attribute の付け替え・キー（property） | 値を決めるまで変わらない | R1 |
| R2-b | 字句器: 言語ごと（C / C++・Rust・Python・JS / TS から。attribute・macro・builtin・module・`.` の後の property） | 同上 | R1 |
| R3 | 色の値（施主に確かめる事 1〜3）を表と上書きに書く | 変わる | /design の絵と hide の決め。R2 と順は入れ替えられる |
| S1 | semantic tokens の full・答えの読み・文書の値・ずらし・重ね・refresh（clangd・rust-analyzer・typescript-language-server） | 変わる（LSP オンの文書だけ） | R1・未決の実測 |
| S2 | 修飾子（施主に確かめる事 4・5）・gopls の初期化の値・delta か range（実測で要るなら） | 変わる | S1・hide の決め |

## 結果

得られるもの: 言語サーバーが無くても、エスケープ・属性・キー・マクロ・組み込みが見分けられる。言語サーバーが在れば、変数・引数・フィールド・名前空間・自分の型・enum の値に役割が付く（見本の C++ で 7 色 → 13 の役割）。125 テーマとテーマファイルは 1 バイトも変えずに済み、色の決めは表の値の 1 か所に閉じる。renderer・格子・ミニマップ・印刷・プレビューは `syntax_color` の 1 本のまま。
正直に: 実測は 30〜40 行と 900〜1,700 行の見本で 1 回ずつで、揺れと巨大な文書は測っていない。「今の色」の対応は字句器の規則から読んだ物で、製品を動かして確かめてはいない。Web の資料は取得の道具が要約して返した文で確かめた。

## 関連

ADR 0017（決定 1・4・5）・ADR 0156（決定 1・2・7・10・11）・ADR 0161（決定 2・3）・ADR 0190（決定 1・3）・ADR 0193・計画 `docs/plans/2026-10-21-syntax-and-lsp.md`・D60 / D62 / D65・FR-021。
