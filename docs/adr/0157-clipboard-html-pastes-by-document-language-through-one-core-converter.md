# ADR 0157 — クリップボードの HTML は文書の言語で貼り方が決まり、変換は core の純関数 1 本

- 状態: 提案（hide の試用の後に受理）
- 日付: 2026-10-22
- Issue: #774（Markdown の段 7）・親 #763
- 規則: FR-007、ARC-001 / ARC-003 / ARC-007 / ARC-010 / CPP-002 / CPP-004 / CPP-005 / CPP-011 / CPP-012、ADR 0009（クリップボードの port・決定 5）/ 0035（矩形の鍵 Ctrl+V・決定 9）/ 0051（`"+` `"*`）/ 0055（貼り付けの改行）/ 0078（操作の表）/ 0134（クリップボードの失敗）/ 0153（断りの音）/ 0154（文書の言語）
- 計画: `docs/plans/2026-10-21-markdown-preview.md` §3.5 と §4 の段 7。施主決定: M-7（変換は Markdown の文書だけ）・M-8（リッチコピーは範囲外）・M-9（「HTML のまま貼り付け」の鍵は Ctrl+Alt+V・鍵なしでも可）

## 文脈

ブラウザやワープロからコピーした文章は、クリップボードに文字（`CF_UNICODETEXT`）と HTML（"HTML Format"・`CF_HTML`）の両方で置かれる。今の Ctrl+V は文字だけを読むので、Markdown の文書に貼ると見出し・強調・リンク・リストが消える。文書の言語は #776（ADR 0154）で文書の値になった。

## 決定

1. **貼り方は文書の言語で決まる（表 1 つ）。** Ctrl+V（通常モードと Vim INSERT の OS の貼り付け）は、文書の `Language` が `markdown` で、クリップボードに HTML があれば HTML → Markdown に変換して貼る。`html` の文書なら HTML の fragment をそのまま貼る。ほかの言語か HTML が無ければ今までどおり `CF_UNICODETEXT` の文字だけ。判定は application の 1 か所 `paste_kind_of(core::Language, bool has_html) -> PasteKind { text, markdown_from_html, html_as_is }`（`src/application/PasteKind.hpp`・閉じた enum・言語の `switch` に `default` 無し）。
2. **Ctrl+Shift+V は常に文字だけ。** 操作 `core::EditorOperation::paste_plain`（名前「文字だけを貼り付け」・鍵 `KeyChord{true, true, OperationKey::v}`・`OperationModes::both`・F1 の一覧に載る）。`OperationKey` に `v`（表示名 `V`）を足し、ui の `operation_chord` が Ctrl つきの `'V'` を写す。Shift なしの Ctrl+V は表に行を置かず、ui の今の貼り付けの道（`press_control_key`）のまま。実行は意図 `ClipboardAction{ClipboardOperation::paste_plain}` で、controller は今の文字の読み（`paste_text`）を通す。Vim の NORMAL / VISUAL では何もしない（INSERT だけ・controller の `pastes_from_os`）。鍵で押した Vim の NORMAL / VISUAL の Ctrl+Shift+V は、今までどおり矩形の入口（ADR 0035 の決定 9）に流す（ui の `keeps_block_key` が表の `paste_plain` より先に Vim の Ctrl の表へ渡す）。
3. **「HTML のまま貼り付け」は操作 1 つ。** `core::EditorOperation::paste_html`（名前「HTML のまま貼り付け」）。`KeyChord` に Alt の欄が無い（Alt と組んだ鍵は ui が写さない・ADR 0078 の決定 2）ので、表の行は鍵なし（`std::nullopt`）で、F1 の一覧と Ctrl+P の `?` からだけ実行する（M-9 の「鍵なしでも可」）。Ctrl+Alt+V は、表に Alt の列を足す ADR 0078 の変更と一緒に後続。意図は `ClipboardAction{ClipboardOperation::paste_html}`。どの文書でも fragment をそのまま貼る。HTML が無ければ文字だけ貼って「HTML が無いので文字だけを貼りました」を 1 行知らせる（情報・`reject` を通さず鳴らさない・ADR 0153）。Vim の NORMAL / VISUAL では何もしない。
4. **port は読みの口を 1 つ足す。** `ClipboardPort::html() -> std::expected<std::optional<std::string>, ClipboardFailure>`（HTML が無ければ値なし・fragment だけを UTF-8 で返す）。adapters の `Win32ClipboardAdapter::html` が `RegisterClipboardFormatW(L"HTML Format")`（`ClipboardCalls.register_format`・試験は替え玉）の中身を読み、見出しの解釈は純関数 `html_fragment_of(std::string_view clipboard_bytes) -> std::optional<std::string_view>`（`src/adapters/win32/HtmlClipboardFormat`）の 1 本。`StartFragment:` / `EndFragment:` は UTF-8 のバイト位置で、見出しは最初の `<` より前だけを読む。見出しが無い・数でない・範囲が逆か外れていれば値なし（HTML が無いのと同じ）。fragment が正しい UTF-8 でなければ `unsupported_format`。`OpenClipboard` の失敗は文字の読みと同じ値（ADR 0134）。controller は `html()` の失敗を「HTML が無い」と同じに扱い、文字の読みへ戻る。
5. **変換は core の純関数 1 本。** `[[nodiscard]] std::expected<std::string, HtmlFailure> markdown_from_html(std::string_view fragment)`（`src/core/HtmlToMarkdown`）。自前の最小の字句器（タグ・引用符つきの属性・注釈 `<!-- -->`・宣言 `<!…>` `<?…>`・実体参照・空白の畳み込み）で、外部ライブラリは入れない（md4c は逆向き）。字句器は `HtmlMarkdownWriter`（積みを持つ 1 型）に開き・閉じ・文字を渡す。要素の名前 → `HtmlTag` は字句器の表 1 つ（CPP-012）。属性と実体参照は `html_attribute` / `html_decoded`（`HtmlAttribute`）の 1 対で、名前つきの実体は Markdown の実体の表（`markdown_entity_text`・ADR 0151 の決定 7）を使う（`&amp; &lt; &gt; &quot; &nbsp;` と数値 `&#NNN;` `&#xHH;`）。`HtmlFailure` は閉じた enum（`malformed`・`too_large`）。上限は `html_fragment_limit`（16 MiB）で、超えたら `too_large`。入れ子の深さの上限は `html_depth_limit`（64）で、超える開きタグは `malformed`（レビュー R1・R2）。閉じタグの探索（開いた要素の列を内側から見る）と行頭の印（引用の `> ` とリストの字下げ）の長さはこの深さで抑えられ、対応しない閉じタグの列も線形で終わる。出す Markdown の上限は `html_markdown_limit`（16 MiB）で、深い入れ子の中で `br` を繰り返して出力が膨らむと `too_large`。
6. **対応する要素と出す記法。** `h1`〜`h6` → `#`×n と空白・`p` → 段落（前後に空行）・`br` → 行末の 2 空白と改行・`strong` / `b` → `**…**`・`em` / `i` → `*…*`・`a href` → 角括弧の文字の直後に丸括弧の href（`[文字]` `(href)`・href の無い `a` は文字だけ）・`ul` / `ol` / `li` → `- ` / `1. `（入れ子は 2 空白の字下げ・番号は 1 からの連番）・`blockquote` → 各行に `> `（引用の中の空行は `>`）・`code` → `` `…` ``・`pre` → ``` の塊（`pre > code` の `class="language-xxx"` は ``` の後ろに・中は空白を畳まず強調とリンクの記法を出さない・開きタグの直後の改行 1 つは数えない）・`hr` → `---`・`img` → `!` と角括弧の alt の直後に丸括弧の src（`![alt]` `(src)`・src の無い `img` は何も出さない）・`table` → 行（`tr`）ごとにセル（`td` / `th`）の文字を空白で区切る・`script` / `style` の中身は捨てる・`div` と節の要素（`section` `article` `header` `footer` `nav` `main` `aside` `figure` `figcaption` `address` `dl` `dt` `dd` `caption`）は記法を出さず前後で改行するだけ・`span` と知らない要素は中身の文字だけ・中身を持たない要素（`meta` `input` など）は何も出さない。中身の無いまま閉じた要素は記号を残さない（`<b></b>` は空）。空白の並びは 1 つに畳み、行頭と終わりの空白は落とす。`&nbsp;` は畳まない空白 1 つ。出力の改行は LF で、文書の改行の形に揃えるのは貼る道（決定 8）。リンクと画像の行き先は、改行を落とし、空白・括弧・`<` `>` を含むなら CommonMark の `<…>` で包み（中の `\` `<` `>` を `\` で逃がす）、ほかは `\` だけを逃がす。空の href は空の行き先（`[文字]()`）。画像の alt は `]` と `\` を `\` で逃がす（レビュー R4）。見出しの中の `br` は空白 1 つ（見出しを 2 行に割らない）。引用符の無い値の最後の `/`（`href=http://x/`）は値の一部で、自閉じにしない。
7. **壊れた HTML。** 閉じない要素は fragment の終わりで閉じたとみなす（`<b>x` は `**x**`）。閉じタグは開いた要素の内でいちばん内側の同じ要素まで閉じ、間の開いたままの要素も閉じる。開いていない閉じタグは捨てる。前の `li` が閉じないまま次の `li` が来たら前を閉じる。終わらない印（`<b`・`a</b`・`<!-- c`・`<!DOCTYPE html`・閉じない引用符）と正しくない UTF-8 は `malformed`。変換に失敗したら（`malformed`・`too_large`）文字だけ貼って「HTML を変換できないので文字だけを貼りました」を 1 行知らせる（鳴らさない）。HTML の暗黙の終わりとして、閉じないままの `li` は次の `li` で、`td` / `th` は次のセルか行で、`tr` は次の行で、`p` は次の塊の要素（段落・見出し・リスト・引用・`pre`・表・`div`）で閉じる（閉じないままだと入れ子の深さが上限に届く）。
8. **貼る道は 1 本。** 変換した本文も fragment も文字も、controller の `insert_pasted` の 1 本（`clipboard_line_feeds` → `with_document_newlines` → `replace(…, EditBoundary::separate)`）に流す。undo は 1 単位で、Vim INSERT では今の Ctrl+V と同じ道を通る。新しい挿入の経路を作らない。変換の結果か fragment が空（`<script>` だけ・src の無い `img` だけなど）なら、黙って文字の貼り付けに戻す（知らせは出さない・空を貼って選択を消さない・レビュー R3）。
9. **静かな道。** 貼り付け以外で `html()` を呼ばない。Ctrl+V でも、HTML があっても貼り方が変わらない言語の文書（`paste_kind_of(language, true) == PasteKind::text`）では `html()` を呼ばない。Ctrl+Shift+V も呼ばない。HTML が無い（値なし）ときは今までどおり `read()`。契約 `verify_quiet_paths`（`--history`）に、起動・開く・切り替え・保存・打鍵の道で替え玉の `html_reads() == 0` と `reads() == 0` を足す。
10. **限界。** リッチコピー（Nib から HTML で出す・M-8）・RTF・表の記法（`|` の表）・`ol start` と入れ子の番号の継続・画像の取り込み・Vim の `"+p` の変換（ADR 0051 のまま文字だけ）・設定は後続。

## 1 打鍵の道の費用

打鍵の道は変えていない。Ctrl+V の道に足したのは `paste_kind_of` の `switch` 1 回で、`html()` を読むのは Markdown と HTML の文書だけ。変換は fragment の長さに比例する 1 回の走査（字句器と書き手）で、frame には何も足さない。

## 振る舞いの細目と限界（分かっていて今回は採らない）

- 本文の Markdown の記号（`*` `_` `#` `[` など）を `\` で逃がさない（逃がすのはリンクと画像の行き先と alt だけ・決定 6）。HTML の文字に記号があれば、そのまま記法として読まれることがある。
- `ol` の入れ子の字下げは 2 空白なので、CommonMark では番号つきの項目の下の入れ子が別のリストとして読まれることがある（決定 6 の 2 空白に合わせた）。
- 段落の中の `<b><p>…</p></b>` のように塊をまたぐ強調は、閉じる記号が段落の後ろに付く。
- `li` の直下に `ul` / `ol` だけが来ると、外側の項目の記号は捨てられる（中身の無いまま改行するため・レビュー I6）。
- リンクの文字（`a` の中身）は逃がさない。`]` を含む文字はリンクの記法を割ることがある。
- `div` は記法を出さないが改行は入れる（`<div>a</div><div>b</div>` を `a` と `b` の 2 行にする。Gmail などの貼り付けで行が 1 つにつながらないように）。
- 「HTML のまま貼り付け」に鍵が無い（決定 3）。
- 見出しが読めない "HTML Format" は HTML が無いのと同じ扱いで、`paste_html` では「HTML が無い」と知らせる。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 外部の HTML parser（gumbo など）を `third_party` に入れる | 貼り付けの fragment は小さく、必要な要素は限られる。CNF-013 の照合と別 target を増やさず、純関数の表で守る |
| md4c で逆向きに変換する | md4c は Markdown → HTML の parser で、HTML を読まない |
| 変換を adapters で行う | 変換は OS に触れない純関数で、core の単体（`--html-markdown`）で 88 件の表と深さ・上限を確かめられる。adapters は見出しの切り出しだけ |
| Ctrl+V の判定を ui に書く | 文書の言語は application の値。ui は意図を送るだけ（ADR 0078 の決定 7） |
| どの文書でも HTML を読んでから判定する | 文字の文書で毎回 HTML の写しを作る。表の `paste_kind_of(language, true)` で読むかを先に決める（決定 9） |
| `paste_html` に Ctrl+Alt+V を割り当てる | `KeyChord` に Alt が無く、表に列を足すのは ADR 0078 の変更。鍵なしで一覧からにした（M-9） |
