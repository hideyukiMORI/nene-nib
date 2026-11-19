# ADR 0151 — md4c を `third_party/md4c` の別 target として取り込み、core の `parse_markdown` が平らなブロックの列に写す

- 状態: 提案（設計席 2026-10-21・hide の判断 M-1〜M-6 は全部「はい」・merge は独立レビュー・CI の後）
- 日付: 2026-10-21
- Issue: #763（親・Markdown プレビュー）・#764（段 1: 取り込みと CNF-013）・#765（段 2: `parse_markdown`）。計画は `docs/plans/2026-10-21-markdown-preview.md`
- 規則: D7 / FR-007、ADR 0003（外部コードは別 target に `/W4 /WX /utf-8`・tag と SHA-256 で固定・ソースを書き換えない・ADR・許可リスト・規約の表の 3 点）、ARC-002（モジュールの依存）/ ARC-003（core に OS 無し）/ ARC-010（期待される失敗は値）、CPP-002 / CPP-005 / CPP-011、CNF-005（ゲートを弱めない）、`docs/PROJECT_LAYOUT.md`（`third_party/<name>` は独立したモジュール・そこにだけ緩い警告集合）

## 文脈

D7 は md4c（MIT・C）と Direct2D の自前描画を決め、ADR 0003 は取り込みの形（別 target・`/W4 /WX /utf-8`・C17・tag `release-0.5.2` / commit `729e6b8b320caa96328968ab27d7db2235e4fb47`・3 ファイルの SHA-256）を予約した。2026-10-21 の probe（`docs/quality/md4c-probe-2026-10-21.md`）で、今の clang-cl 19.1.5 でも `/W4 /WX /utf-8 /std:c17` の警告は 0 件、16 MiB の `md_parse` は 63.8 ms、`MD_TEXT_NORMAL` / `MD_TEXT_ENTITY` の pointer は入力を指す、SHA-256 は改行の形で値が割れる（ADR 0003 の値は `core.autocrlf=true` の作業ツリー＝CRLF の値）ことが分かった。
md4c は SAX 型（`enter_block / leave_block / enter_span / leave_span / text` のコールバック）で、ブロックの元の位置を渡さず、実体参照を解決しない。Nib の frame は見えている行だけを作り直し、描画は 1 行 1 layout（ADR 0114）だが、プレビューは行高が一定でない。

## 決定

1. **取り込みは vendoring。** `third_party/md4c/{md4c.c,md4c.h,LICENSE.md}` に上流のバイト列をそのまま置く。`.gitattributes` に `third_party/md4c/** -text` を書き、改行の変換を止める。照合は **LF（上流の blob）の SHA-256**（`md4c.c` `52e39843b96b64d0281bf795128bcc88c9c4dc52d7436737e9f4306648dc4590`・`md4c.h` `4efd19bf7ec270691d5b4189f496886e421768a814b5e817eb945aa85e859f18`・`LICENSE.md` `d30937367d5413e7eaa218b1640b8946ff76fd34d97152f6979fd96169d5d0fc`）。ADR 0003 の値（CRLF の作業ツリーの値）は追記で改める。FetchContent は採らない（`conformance.py` は git の木を読むので取得物を照合できない・オフラインで構成できない）。
2. **規約検査が照合する（CNF-013・新規）。** `eng/conformance-rules.json` に `thirdPartyDigests`（path → SHA-256）を置き、`eng/conformance.py` が git の blob（`git show HEAD:<path>` 相当・作業ツリーでない）のバイト列を照合する。無い・違う、のいずれも CNF-013。`tests/conformance` に正例・反例。`docs/QUALITY_GATES.md` に行を足し、`active` にするのは証明の行を足したとき（ADR 0001）。
3. **別 target と C の有効化。** `CMakeLists.txt` を `LANGUAGES C CXX` にし、`eng/toolchain.ps1` は `CC=clang-cl` も固定する。`eng/targets.cmake` に `nenenib_third_party_target(<target> <module> <sources>)` を 1 本: `C_STANDARD 17`・`/W4 /WX /utf-8`・`NENENIB_CXX_OPTIONS` は付けない・Debug のサニタイザ（ASan / UBSan）は**付ける**（ADR 0003 の「厳格集合とサニタイザ以外は当てず」は「厳格集合は当てず・サニタイザは当てる」と読む: 信用できない入力を読むパーサこそ検査の対象で、ソースを書き換えずに当てられ、Debug の exe は ASan の runtime を全体で結ぶ）。モジュールの検査（`architecture.json` に無いモジュールは FATAL）は共有する。clang-tidy は `CMAKE_CXX_CLANG_TIDY` だけで C の target に掛からない（`CMAKE_C_CLANG_TIDY` は置かない）。
4. **モジュールと依存。** `eng/architecture.json` にモジュール `third_party/md4c`（依存なし・`runtimeDependencies` 空＝静的に結ぶ）を足し、`core` の依存に 1 本足す。`docs/PROJECT_LAYOUT.md` の「現在は無い」を直し、ARC-002 の表に行を足す。`eng/symbol-allowlist.json` に `third_party/md4c` の欄（`malloc` `realloc` `free` `qsort` `bsearch` `memcpy` `memcmp` `memset` `strchr` `strlen` …・probe で見た md4c の未定義）。core の許可は変えない。
5. **core が `md4c.h` を読むのは 1 ファイル。** `src/core/MarkdownParse.cpp` だけが `md4c.h` を include し、`target_include_directories(... SYSTEM ...)` で受けて `HeaderFilterRegex '.*'` の tidy から外す。公開ヘッダ `MarkdownParse.hpp` は md4c の型を出さない。
6. **`parse_markdown` は純関数で値を返す。** `[[nodiscard]] std::expected<MarkdownDocument, MarkdownFailure> parse_markdown(std::string_view text, const MarkdownStop& stop)`。`MarkdownFailure` は閉じた enum `{parser_rejected, too_large, stopped}`（`MD_SIZE` は `unsigned`・4 GiB 未満）。`stop` は「止めるか」を問う型のある述語（core は時計も印も持たない。adapters の worker が自分の印を読む lambda を渡し、md4c のコールバックが非 0 を返して止める）。flags は `MD_DIALECT_COMMONMARK`（0）。表・打ち消し・タスクリストの flag は立てない（M-2）。
7. **出力は平らな列。** `MarkdownDocument{std::vector<PreviewBlock> blocks}`。`PreviewBlock{PreviewBlockKind kind; std::uint8_t level; std::uint8_t quote_depth; std::uint8_t list_depth; std::string marker; Offset source_begin; std::string text; std::vector<PreviewSpan> spans}`。`PreviewBlockKind` は閉じた enum `{paragraph, heading, code, html, rule, list_item}`、`PreviewSpan{Offset begin; Offset end; PreviewStyle style}`（`text` の中のバイト位置）、`PreviewStyle` は閉じた enum `{emphasis, strong, code, link}`（リンクの URL は `PreviewSpan` に持たない・開かないので・M-5）。追記（#784）: `PreviewStyle` に `image` を足す。builder は `MD_SPAN_IMG` の中の alt を「`[画像: alt]`」の枠で囲んで `text` に書き、枠を含む全体に `image` の span を掛ける（画像は読まない・描画は `comment` の斜体）。enter / leave の積み（深さ・リストの番号）は parse の中だけに持ち、leaf ブロック（P・H・CODE・HTML・HR と LI の直下の文字）ごとに 1 要素を出す。実体参照（`MD_TEXT_ENTITY`）は core の表で解決して `text` に書く（名前つき実体は HTML5 の表のうち CommonMark の例に出る範囲・解決できないものは綴りのまま）。
8. **`source_begin` は最初の text の pointer から引く。** NORMAL / ENTITY の最初の pointer − 入力の先頭（probe で入力を指すことを確認済み）。BR・SOFTBR・NULLCHAR と CODE / HTML の行末・字下げは md4c の固定の文字列なので位置に使わない。文字の無い水平線は直前のブロックの `source_begin` 以降で `---` 等を探さず、直前のブロックの終わりの次の行頭を補う。 追記（#782 のレビュー Q1）: 補う位置は「直前の要素の終わりの次の行から**空行を飛ばした最初の行の行頭**」に改める（`para\n\n---` で `---` の行頭）。前に位置が無ければ本文の先頭から空行を飛ばす。
9. **入力は連続 1 本。** 呼び出し側（application）が `TextBuffer::segments()` から連続の `std::string` を 1 回作って `std::shared_ptr<const std::string>` で渡す。worker が piece を直接読まない（`AddChunk` は末尾の chunk を伸ばす）。
10. **型は 1 ファイル 1 型。** `MarkdownDocument` `PreviewBlock` `PreviewSpan` `PreviewBlockKind` `PreviewStyle` `MarkdownFailure` `MarkdownStop` の 7 ファイル（CPP-011）。比較は非メンバー（CPP-003）。
11. **`THIRD_PARTY_NOTICES.txt`** に md4c の節（名前・tag・commit・URL・MIT の全文・Copyright Martin Mitáš）。`eng/package-release.py` の `ENTRIES` で ZIP に入る。`eng/test-package-release.py` が中身を照合していれば期待値を足す。
12. **検証**: 単体テスト（CommonMark の例を 8 構文の範囲で数十・`source_begin`・実体参照・打ち切り・`too_large` は境界だけ）・`tests/performance` に 16 MiB の parse の probe（組み立て込み・値を記録）・CNF-013 の正例・反例・`/W4 /WX` 0 件（CI の check job）・`check.ps1 -Full`（外部コードと C の有効化は限定した検証で覆えない: 理由を書いて 1 回）。
13. **外部コードは whitespace の検査の対象外。** `.gitattributes` を `third_party/md4c/** -text -whitespace` にし、`git diff --check`（pre-commit フックと `eng/validate-git.ps1` の GIT-004）が上流の行末空白（`md4c.h:167`）で落ちないようにする。バイト列は CNF-013 が守る（設計席 2026-10-21・#764 の実装で発見）。
14. **File API の target の依存は「宣言した依存の閉包」と比べる。** CMake の File API の `dependencies` は推移的（静的ライブラリの PRIVATE link が `LINK_ONLY` で下流に乗る）なので、core が `third_party_md4c` を結ぶと core を使う全 target に `forbidden target dependency third_party_md4c` が出た（15 件）。`eng/conformance.py --build-dir` の照合は `eng/symbols.py` の `dependency_closure` と同じ意味の閉包と比べる。直の link は `eng/targets.cmake` の `nenenib_link` が configure で守り、相対 include の検査は直の依存のまま。下流のモジュールの依存には `third_party_md4c` を足さない。`tests/conformance` に正例（閉包の中の推移的な依存は通る）と反例（閉包の外の target に結ぶと落ちる）（設計席 2026-10-21）。
15. **外部コードは clang-format の対象外（書き換えないため）。** `eng/check.ps1` の QLT-004 の列挙から `:(exclude)third_party/**` で外す。Nib のソースの整形の検査は変えない（設計席 2026-10-21）。

## 段 1 の実装で分かった事実（#764・2026-10-21）

決定は変えず、実装で確かめた値と名前だけを書く。

- モジュールの名前は `third_party_md4c`（`eng/architecture.json` の既存の名前の付け方＝ path の区切りを `_` にしたもの・path は `third_party/md4c`）。target は `nenenib_md4c`、作るのは `eng/targets.cmake` の `nenenib_third_party_target`（モジュールの検査は `nenenib_target` と共有する `nenenib_module_path` の 1 本）。
- index に add した blob の SHA-256 は決定 1 の 3 値と一致した（`git show :third_party/md4c/md4c.c | sha256sum` → `52e39843…c4590`）。写しは probe の clone（同じ commit）の `git show <commit>:<path>` のバイト列。
- clang-cl 19.1.5 の Debug（`/std:c17 /W4 /WX /utf-8` ＋ ASan / UBSan）で md4c.c の警告は 0 件。CMake は C の target に `-imsvc`（SYSTEM の include）と `/std:c17` を渡し、`NENENIB_CXX_OPTIONS` は渡らない。
- `eng/symbols.py` は `symbol-allowlist.json` の `modules` に無いモジュールを見ないので、`third_party_md4c` の欄を足して照合の対象にした。Debug の `nenenib_md4c.lib` が外へ要求するのは `malloc` `realloc` `free` `qsort` `bsearch` `memcpy` `memmove` `memset` `memcmp` `strchr` `strlen` と、`__chkstk` `__security_*` と ASan / UBSan の runtime と `__stdio_common_vsprintf`。最後のものは UCRT の `<stdio.h>` が inline で定義する `sprintf` 系の包みが `/Od` で obj に残った参照で、md4c が `sprintf` を呼ぶのは `#if 0` の中だけ。時刻・OS・スレッドに触れるものは無い。
- `md4c.h` は ARC-007 の字句検査（`src/adapters/win32/` 以外の `.h`）と CNF-003 / CNF-008 で指摘 0（`.c` は `cppExtensions` に無いので字句検査の対象外）。

## 段 2 の実装で分かった事実（#765・2026-10-21）

決定は変えず、実装で確かめた値と名前だけを書く。

- 打ち切りの値は **負**（`-2`）。md4c の内側の関数は `MD_CHECK` で負の値だけを上へ返し、正の値は捨てて読み続ける（text のコールバックの非 0 が `md_parse` まで届かなかった・単体テストで実測）。md4c 自身の負の値は `-1` だけ。打ち切りは builder が覚え（一度 true を聞いたら以後は問わない）、`md_parse` の戻り値でなく覚えた値で `stopped` を返す。
- 止める述語 `MarkdownStop` は関数ポインタと借りた文脈の対（core は `<functional>` の `std::function` を使っていない）。`MarkdownStop::asking(呼び出し可能な値)` と `MarkdownStop::never()`。
- md4c の enter / leave を畳む型は `MarkdownBuilder`（md4c の型を知らない・1 ファイル 1 型）。実体参照の表は `MarkdownEntity`（名前つきは CommonMark の例の 15 個・数値は 0・サロゲート・U+10FFFF 超を U+FFFD）。`marker` は決定 7 のまま `std::string`（番号つきは区切りが `)` でも `"N."`・箇条書きは `"•"`）。記号だけの項目（`- - a` の外側・`-` だけの行）は空の `list_item` を 1 つ出す。
- `source_begin` は NORMAL / ENTITY と、本文の中を指す CODE / HTML の text から引く（コードブロックの本体の行は本文の中・行末と字下げは外）。中かどうかは `std::less_equal` の全順序で比べる。文字の無い要素の補いは「直前の位置つきの text の終わりの次の行から空行を飛ばした最初の行の行頭」・前に何も無ければ本文の先頭から空行を飛ばす（決定 8 の追記）。行の探索は位置を持たない要素のときだけ走る。行の終わりは md4c と同じく CR・LF・CRLF のどれでもよい（CRLF は 1 つ・2026-10-27・#887。LF だけを行の終わりとみなしていて、CR だけの改行の後ろの空の項目の `source_begin` が本文の末尾を指していた）。
- `eng/symbols.py` は core の未定義 `md_parse` を閉包の `nenenib_md4c.lib` の定義で解決し、`eng/symbol-allowlist.json` は変えずに 0 violation（#764 の時点の記録。#772 の名指しの import は不要だった。その後 #772 で、third_party の定義は `exports` に名指しした API だけが解決する形にした・下の追補）。
- 16 MiB の parse（組み立て込み）の probe は `markdown-parse-mixed-16mib`（入力は probe の (i) と同じ 16,777,183 バイト・209,263 要素）。Release（`77f391d`・負荷 1〜5%）の 10 回で中央値 143.2 ms・最小 141.4 ms（md4c だけの 63.8 ms に、組み立てが約 80 ms 足される）。正式な速さのゲートではない。

## 決定 16〜18（2026-10-21 追記・#784）: 描画の保持と painter の形

段 3（描画）の形。置き場（split の other）と更新（worker）は段 4・5 の ADR B で書く。座標は本文と同じく描画先の DPI を `core::reference_dpi` にした物理画素で、DIP の寸法は `PreviewPlacement`（`src/ui/win32`）の 1 か所の表から写す。

16. **1 要素 1 layout の保持は `PreviewTextLayouts`（ui/win32）。** `BodyTextLayouts`（ADR 0114）と `BodyTextPainter`・字形の収集（ADR 0073）には入れない。鍵は `{text, spans, kind, level, 幅（物理画素）, PreviewTextStyle{字体名, pt, DPI}}` で全部が一致するときだけ使う。書式は本文の字体（可変幅・`DWRITE_WORD_WRAPPING_WRAP`）と等幅の字体（コードと HTML ブロック・`NO_WRAP`・設定の字体が等幅ならそれ、でなければ Cascadia Mono → Consolas・ADR 0140 の決定 6 / D58）の 2 つ。見出しは layout 全体に倍率ぶんの `SetFontSize` と `SetFontWeight(BOLD)`。行高は本文と同じ比（pt → DIP × 1.6・`BodyLayout` と同じ丸め）の `SetLineSpacing(UNIFORM)` で、字は行の縦の中央（`BodyTextLayouts` の set_spacing と同じ式・空白 1 つの layout で大きさごとに 1 回測る）。装飾の範囲は要素の `text` のバイト位置を core の `utf16_length` の 1 本で UTF-16 の位置に写す（`preview_units`・新しい変換器を書かない）: 強調 `SetFontStyle(ITALIC)`・太字 `SetFontWeight(BOLD)`・行内コード `SetFontFamilyName(等幅)`・リンク `SetUnderline`。字体に斜体が無ければ DirectWrite の疑似斜体に任せる。保持は最近使った順に 256 件までで、超えたら最も古いものから捨てる。高さは `GetMetrics().height`。高さの部分和は `PreviewBlockTops`（要素ごとの上端と下端の `std::vector<float>`・見えている最初の要素は下端の `upper_bound`）で、測るときは `PreviewTextLayouts::measure` が layout を保持しない（見えている要素だけを `get` が作って保持する）。文書・幅・字体が変われば作り直す（今は呼び手が毎回作る・段 5 で差し替えのたび）。
17. **描くのは `PreviewPainter<Target>`（`BodyTextPainter` と同じく描く先を template で受ける）。** 入力は `PreviewScene{MarkdownDocument の借用, PreviewBlockTops の借用, PreviewTextStyle, 枠の矩形, 上の送り}` と、借りた `PreviewTextLayouts`・`PreviewBrushes`。枠を `SyntaxPalette.background` で塗って枠で切り、見えている要素だけを `DrawTextLayout`。面と線は `FillRectangle` / `FillRoundedRectangle`（替え玉の描画先 `CountingTarget` の口と同じ）。色は役割 `PreviewInk{background, foreground, keyword, string, function, comment, operators, panel}` → `PreviewBrushes` の `switch` の 1 か所（`default` 無し・ui/win32 に色のリテラルを書かない）。brush は役割ごとに 1 つ（行の中の装飾は layout の範囲に drawing effect として置くため）で、描画先とテーマの名前が変われば作り直す。drawing effect は描くたびに置き直す（保持の鍵にテーマを入れない）。採用案 D62 の表のとおり: 見出し `keyword`・コードブロックは `string` と `panel` の面（角丸 5・内側の余白 12・折らず面の外は切る）・行内コードは `string` と `panel` の面（`HitTestTextRange` の画面行ごとの矩形・左右 4・角丸 3・行高の上下を 1 割ずつ空ける）・リンクは `function` と下線・引用の中の段落と項目は `comment` と深さごとの左の線 2 DIP（同じ深さの前の要素との間もつなぐ）・HTML ブロックは等幅の `comment`・水平線は `comment` 1 DIP。字下げは左の余白 16＋引用 12 × 深さ＋箇条書き 24 × 深さ、右の余白 16・上の余白 12。記号は深さ 1 が `•`・2 が `◦`・3 以上が `▪`（番号はそのまま）で `operators`、文字の左端から 8 DIP 空けて右に揃える。記号の layout は折り返さない幅で作り（保持に入る）、記号の列の幅は同じリストの同じ深さで最大の記号の幅＋8 DIP と 24 DIP の大きい方で、項目の字下げもその列で揃える（`PreviewBlockTops` が要素ごとの字下げを持つ・#788 のレビュー R1）。リストの切れ目は続く要素の深さと記号の種類（箇条書き / 番号）の切り替わりで判定する。要素の間は前の要素の後（段落・見出し・コード・HTML・項目は行高の半分・水平線は行高）と次の要素の前（見出しと水平線は行高）の大きいほうで、同じリストの続く項目の間は 0、種類の違うリストの境は行高の半分（採用案 §5）。drawing effect の失敗は他の DirectWrite の呼び出しと同じく `RenderFailure::directwrite` で止める。
18. **確かめは offscreen の PNG。** `tests/preview`（モジュール `preview_tests`・依存は core / application / ui_win32・OS のライブラリは `tests/ui` と同じ ole32 d2d1 dwrite windowscodecs）の exe `nib_preview_capture` が、WIC の bitmap（`GUID_WICPixelFormat32bppPBGRA`）に `ID2D1Factory::CreateWicBitmapRenderTarget` のソフトウェアの描画先で製品と同じ型を通して描き、`IWICBitmapEncoder`（PNG）で保存する。引数は `--in --out --width --height（DIP）--dpi --theme --font-size（pt）--scroll（DIP）--font`。テーマは core の組み込みの表から名前で引く。製品の旗は足さず、CTest に登録しない（合否を判定しない・設計席が Read で見る）。`eng/symbols.py --require` の対象にしない。保持の契約（再利用・幅で別・高さ・範囲の写し・測るだけでは保持しない・256 件・見えている要素だけ）は本物の DirectWrite の試験として `nib_window_tests` の組 `--preview`（`tests/ui/PreviewLayoutTests.cpp`）に置く（`nib_tests` は ui_win32 に結べないので・ADR 0077 の形）。

### 段 3 の実装で分かった事実（#784・2026-10-21）

- 画像は段 2 の型で区別できなかった（`MD_SPAN_IMG` は装飾を持たず alt は地の文字）。設計席の判断で `PreviewStyle::image` を足し（決定 7 の追記）、painter は `comment` の drawing effect と斜体で描く。
- `eng/symbols.py --build-dir build`（Debug）で core の未定義 `memchr` が 1 件出た。`MarkdownBuilder.cpp` の `std::string_view::find` / `find_first_not_of`（#782 のレビュー Q1 で足した行の探索）が Debug の `/Od` で `memchr` の呼び出しとして残ったもの。1 バイトずつの走査（`line_end` / `blank_between`）に替えた（allowlist は変えない・SIMD は書かない）。
- 色の写し `to_color`（`RgbColor` → `D2D1_COLOR_F`）と等幅の字体の候補（Cascadia Mono・Consolas）は `Direct2DRenderer.cpp` / `BodyTextLayouts.cpp` の無名名前空間にあり、この段はそこを変えない約束なので `PreviewBrushes` / `PreviewTextLayouts` に同じ式を置いた。1 本へ寄せるのは段 4 で renderer に接続するとき。
- 見本（`tests/preview/sample.md`・27 要素）の 720 × 900 DIP の 1 枚で、作った layout は高さの測定と描画（記号を含む）を合わせて 49、描いた後の保持は 23。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| FetchContent（tag＋SHA） | 木に無いファイルは規約検査が照合できない。構成のたびにネットワーク。リポジトリに前例が無い |
| `eol=crlf` で ADR 0003 の値を保つ | 上流と一致しないバイト列を正本にすることになる。値を改めるほうが小さい |
| ブロックの木（再帰の `MarkdownNode`） | 描くたびに平らにする。1 要素 = 1 layout・二分探索・高さの部分和は平らな列がそのまま |
| `LineView` に装飾の区間を足す | プレビューの要素は本文の行と 1 対 1 でなく、選択・検索・IME の欄が意味を持たない（計画 §2.2） |
| md4c に厳格集合を当てる | 64 件（sign-conversion 44 ほか・probe §2）。ソースを書き換えない約束（ADR 0003）と両立しない |
| サニタイザも外す | 信用できない入力を読む C のパーサから検査を外す理由が無い。ソースの変更なしに当てられる |

## 限界

- ブロックの元の位置は md4c が渡さないので、`source_begin` は最初の text の位置（ブロックの記号の位置ではない）。スクロールの同期は段落の単位で足りる。
- 実体参照の表は CommonMark の例の範囲。表の外は綴りのまま。
- builder の確保の失敗（と止める述語が投げた `bad_alloc`）は、md4c に渡すコールバックの中で受けて打ち切りの値を返し、`parse_markdown` は `MarkdownFailure::insufficient_memory` で返す（#817 の追補）。例外は md4c の C のフレームを通らず、md4c は自分の確保を解放して戻る（`/EHsc` は `extern "C"` の `md_parse` を投げないものとして扱うので、`md_parse` の外では受けない）。プレビューの入力は上限（計画 M-3・16 MiB）で抑える。
- 表・脚注・数式・画像・HTML の解釈はしない（初版の範囲案）。
- （#772 で解いた）`eng/symbols.py` は core の未定義を閉包の定義の全部で解決していたので、Debug の `nenenib_md4c.lib` が定義する UCRT の inline の `sprintf` 系を core が呼んでも ARC-003 にならなかった。third_party の定義で解決するのを名指しの API（`md_parse`）だけに限った（下の追補）。

## 追補（#772・2026-10-30）: third_party の定義で解決するのは名指しの API だけ

- `eng/architecture.json` の third_party のモジュール（path が `third_party/` で始まる）は `exports` に公開シンボルを完全な名前で持つ（md4c は `md_parse` の 1 つ）。`eng/symbols.py` の `unresolved` は、依存先が third_party のときその定義のうち `exports` に載るものだけを `provided` に入れる（関数 `offered`）。`exports` の無い third_party は何も解決しない。
- third_party 自身の未定義は、これまでどおり自分の定義の全部で解決する（md4c の中の `sprintf` 系の包みは md4c の中で閉じる）。
- これで core が `sprintf` などを呼ぶと、Debug の `nenenib_md4c.lib` が同じ名前を定義していても `ARC-003: core: undeclared external symbol sprintf` で落ちる。正例・反例は `tests/conformance/test_symbols.py`、実ライブラリでの反例の記録は PR の本文。

## 強制

- CNF-013（`thirdPartyDigests` の照合・planned → 正例・反例を足して active）。
- `eng/targets.cmake` の `nenenib_third_party_target` 以外に C の target を作れない（モジュールの検査）。
- ARC-003（active）: `eng/symbols.py` の `offered` が、third_party の定義のうち `eng/architecture.json` の `exports`（md4c は `md_parse`）に載るものだけで依存元の未定義を解決する。正例・反例は `tests/conformance/test_symbols.py`（#772）。
- `md4c.h` の include は `src/core/MarkdownParse.cpp` の 1 か所。CNF-009 と同じ字句検査（`eng/conformance.py` の `concurrency_and_simd`）が `src/` の中のほかのファイルの include を ARC-002 で拒否する。ヘッダ名と許す path は `eng/conformance-rules.json` の `markdownParserHeader` / `markdownParserAllowedPath`、正例・反例は `tests/conformance/test_conformance.py`（#765）。

追補（2026-11-11・#1477）: 決定 16 の「`PreviewTextLayouts::measure` は layout を保持しない」と、決定 17 の「drawing effect は描くたびに置き直す」は、ADR 0155 の追補 69〜72 で改めた（保持した layout は描くときに書式を変えず位置も問わない・色と面の矩形は entry が 1 回だけ置いて持つ・`measure` も保持する）。

## 追補（2026-11-18・#1851・設計席）: 打ち消し線・裸の URL・実体参照の全表・front matter・表・alert（#1839・施主決定 D103）

PR #1846（#1840）と PR #1852（#1848）の実装と独立レビュー（`D:/NeNeNib/outputs/20261118/review-1846.md` `review-1852.md`）で決めた形。md4c のソースは 1 バイトも変えていない（flag を足しただけ）。

1. **打ち消し線は `~~` だけ。** md4c の `MD_FLAG_STRIKETHROUGH` は `~` 1 つでも打ち消しにするが、日本語の文の「10時~12時、14時~16時」が打ち消しになるので、開く記号が `~` 1 つのときは開く `~` と閉じる `~` を文字として元の位置に戻す。中に文字の無い `~` 1 つの形（`~` の間が画像だけ）は打ち消しのまま残る（限界）。
2. **裸の URL** は md4c の permissive の autolink（URL・`www.`・メール）で受ける。
3. **実体参照は HTML5 の全表**（`;` で終わる名前の全部・生成物）。表は名前の順に並べて二分探索で引き、整列は `static_assert` で確かめる。表は道具が作り、手で書かない（CNF-010）。生成物に Python の版を書かない。
4. **front matter** は文書の先頭の `---` の行から次の `---` の行までで、md4c には後ろだけを渡し、位置の対応は front matter の長さの分だけずらす。閉じる行が無い間は今までどおり（front matter にしない）。開く行と閉じる行は後ろの空白と Tab を許す。中の NUL は U+FFFD。
5. **表は 1 行 = 要素 1 つ。** 文字は升を Tab でつないだ 1 本で、位置の対応・スクロールの錨・選択とコピー（升の間が Tab・行の間が改行）は今までの「要素の列」の作りに載る。描くのは升ごとの文字組みで、本文とは別の保持に入れる。列の幅は表ごとに 1 回だけ測り、表の前を打っても測り直さない。見た目は横の罫だけ（D103）。升の中の画像は文字のまま・枠より広い表は右を切るだけ（後続）。
6. **alert**（`[!NOTE]` `[!TIP]` `[!IMPORTANT]` `[!WARNING]` `[!CAUTION]`）は、いちばん外の引用で最初の段落の 1 行目が印だけのとき。左の線と種類の名前を種類の色にし、本文は foreground（D103）。色は `PreviewInk` に足した値だけで、テーマのトークンは足さない。種類が違う隣の引用とは線をつながない。
7. **限界。** エスケープした印（`> \[!NOTE]`）は alert にしない。印の後ろが Tab の行と、リストの中の引用（`- > [!NOTE]`）は alert にしない。md4c の限界として、段落の直後（空行なし）の表は表にならず、升の中のコードの `\|` は `\` が残る。見えている升が保持の上限を超える表は毎 frame 作り直す（正しく描くが遅い）。

## 追補（2026-11-18・#1862・設計席）: 囲みのコードを言語ごとの構文の色で描く（#1858）

PR #1860 の実装と独立レビュー（`D:/NeNeNib/outputs/20261118/review-1860.md` `review-1860b.md`）で決めた形。

1. **言語は info string の最初の語で選ぶ。** ASCII の大文字小文字を畳み、`,` より後ろは読まない（`rust,ignore` は rust）。引くのは core の言語の表の 1 つ（`language_of_label`: 言語の名前 → 拡張子 → 別名）で、プレビューのための 2 本目の表は無い（ARC-012）。別名は表に無かった 8 つ（`c++` `shell` `powershell` `pwsh` `golang` `csharp` `c#` `docker`）。`js` `ts` `py` `sh` `yml` などは拡張子の行で当たる。
2. **別名と拡張子で引くのはプレビューの囲みだけ。** `:set filetype=` と設定の `lsp.<language>.command` は今までどおり言語の名前だけを受ける（ADR 0154 の決定 4 のまま。`:set ft=c++` と `:set filetype=py` は E474）。
3. **1 色のままの囲み。** 知らない言語・info string の無い囲み・字下げのコード・`markdown` `md`（入れ子を読まない）・字句器を持たない plain。front matter も 1 色。
4. **字句解析は core で、要素を作るときの 1 回。** 本文と同じ `lex_line` を行ごとに当て、行をまたぐ状態（複数行のコメント・生文字列・三重引用符）は本文と同じ形で引き継ぐ。始まりの状態は `lex_start_state(language, false)` で、`php` の囲みは `<?php` が無くても色が付く。色の区間（位置・長さ・構文の役）は要素に載り、ui は文字組みを作るときに 1 回だけ置く（描き直しと送りでは置かない・決定 16〜18 と ADR 0155 の保持の形のまま）。色は本文の構文の色のトークンで、新しい色は無い。
5. **上限。** 1 つの囲みが 5,000 行か 512 KiB を超えるとき、または色の区間（隣り合う同じ役は 1 つに畳んだ後）が 2,500 を超えるときは 1 色。DirectWrite の `SetDrawingEffect` は区間の数の 2 乗で伸びる（実測: 2,500 区間で約 8 ms ＋ `GetMetrics` 約 3 ms・5,000 で 31〜37 ms・10,000 で 126 ms・50,000 で 3.7 秒）ので、窓のスレッドの 1 回を 10 ms 前後に収める数にした。
6. **限界。** 区間の上限は普通のコード（1 行に 5〜10 区間）で 300〜500 行あたりに当たり、それより長い囲みは 1 色になる（行とバイトの上限より先に効く・後続 #1863）。`:syntax off` はプレビューに効かない（施主決定 D105）。不正な UTF-8 を含む行は色の位置がずれることがある。info string の実体参照は解かない。md4c は囲みの行頭の Tab を空白 4 つにして渡すので、色はその文字の位置に立つ。プレビューを開いている間の再解析は、変わっていない囲みも字句解析し直す（worker の中・窓のスレッドではない）。行内のコード（`` `x` ``）は色分けしない。

## 追補（2026-11-19・#1869・設計席）: リンクの宛先を持ち、種類を core の 1 本で決める（施主決定 D105 の段 L1）

決定 7 の「リンクの URL は `PreviewSpan` に持たない・開かないので」を改める。D105 でリンクは確かめてから開くことになったので、宛先を要素に持つ。確かめの見た目（どこに何を出すか）は段 L2 で、/design のキャンバス（https://claude.ai/artifact/Y1Gk5UsPmj1PucmjVAtEzR ）の答えの後に書く。

1. **宛先は要素ごとの別の列。** `PreviewSpan` には足さない（全部の装飾に文字列を持たせない）。`PreviewBlock` に `std::vector<PreviewLink> links`（`PreviewLink{Offset begin; Offset end; std::string target}`・`text` の中のバイト位置は `link` の span と同じ）を足す。`target` は md4c の `MD_SPAN_A_DETAIL.href` を、実体参照と `\` のエスケープを解いた綴り（`%` の符号は解かない）。裸の URL（決定 2 の autolink）も同じ列に載る（`www.` は `http://` を前に付けた形、メールは `mailto:` を前に付けた形）。画像（`MD_SPAN_IMG`）の宛先は載せない（画像は読まない・開かない）。
2. **種類を決めるのは core の純関数 `link_destination` の 1 本。** 入力は `target` と文書の経路（`FilePath`・無題なら無し・画像と同じ）、出力は `LinkDestination{LinkKind kind; std::string text}`。`LinkKind` は閉じた enum `{web, mail, document, heading, refused}`。
   - 空・前後の空白・制御文字（U+0000〜U+001F・U+007F）・不正な UTF-8 を含む宛先は `refused`。先頭の scheme（`[A-Za-z][A-Za-z0-9+.-]*:`・判定は画像の参照と共用の `uri_scheme_length` の 1 本）を ASCII の大小を畳んで見る。`http` `https` → `web`（`//` の後ろに空でない ASCII の host を求める・無ければ `refused`。authority（`//` の後ろから最初の `/` `?` `#` まで）に `%` か `@` があれば `refused`。`%` で書いた host はブラウザが解いて開くので見せる綴りと開く先がずれ、userinfo は見かけの host を偽れる）。`mailto` → `mail`（宛先の後ろの引数は `subject` `body` `cc` `bcc` だけを受け、名前は `%` を解いて比べる。ほか（`attach` など）か、値の中の 2 つ目の生の `?` があれば `refused`）。`file` → ローカルの参照（下）。それ以外の scheme（`javascript` `ms-*` `data` など）→ `refused`。1 文字の scheme（`C:`）はドライブの文字として扱い、scheme と見なさない。
   - `#` で始まる → `heading`（`text` は `#` の後ろ・段 L3 で使う）。
   - scheme の無い宛先と `file:` の URL はローカルの参照。**パスの解き方は画像と同じ `resolve_local_reference` の 1 本**（ADR 0155 の追補 48 の規則・`resolve_image_reference` を画像とリンクの共用の名前に改め、拒否の型も `LocalReferenceRefusal` にした。規則と振る舞いは変えない）。解決器の拒否（network・no_base・malformed など。`\\?\` と `\\.\` も network）→ `refused`。解いた経路が確かめの綴りを偽れる字（双方向の制御 U+200E U+200F U+202A〜U+202E U+2066〜U+2069・C1 制御 U+0080〜U+009F・幅 0 の字 U+200B〜U+200D U+2060 U+FEFF・行と段落の区切り U+2028 U+2029・ARABIC LETTER MARK U+061C・U+180E・ハングルの埋め字 U+115F U+1160 U+3164 U+FFA0・タグ文字 U+E0000〜U+E007F）を含めば `refused`。最後の段の名前の最初の `.` か `:` より前（末尾の空白を落とす・Windows は最後の段の末尾の `:` と空白を落として装置に写す）が装置の予約名（`CON` `PRN` `AUX` `NUL` `CONIN$` `CONOUT$` `COM0`〜`COM9` `LPT0`〜`LPT9`・上付きの `¹` `²` `³` の形も・ASCII の大小は畳む）なら `refused`（core に予約名を扱う関数は無かった。#1805 の `NativePath` は adapters で OS の答えを見る形で一覧を持たないので、core の一覧は `LinkDestination.cpp` の 1 つ）。名前が `.` か空白で終わる（`x.exe.`）なら `refused`。解いた経路の名前の拡張子が `unlisted_extensions`（D34 の「テキストでない拡張子」の表・`folder_lists` と同じ口）にあれば `refused`、それ以外は `document`（`text` は解いた経路・存在とテキストかどうかは開くときに `open_document` が決める）。文書へのリンクの `#見出し`（`other.md#sec`）は解決器の規則どおり後ろを捨てる（見出しへ跳ぶのは段 L3 で、要ればそのとき別の欄を足す）。
   - `web` と `mail` の `text` は OS に渡す ASCII の URI。非 ASCII の字は path・query・fragment で UTF-8 の `%XX`（大文字の 16 進）に符号化する（日本語の URL を通す）。host に非 ASCII があれば `refused`。ASCII のうち RFC 3986 の unreserved・reserved・`%` 以外（空白・`"` `<` `>` `\` `^` `` ` `` `{` `|` `}`）を含むか、`%` の後ろが 16 進 2 桁でなければ `refused`（符号化しない）。`heading` は `#` の後ろ、`document` は解いた経路、`refused` は元の綴り。
   - **不変条件。** `document` は OS へ渡さず、Nib の `open_document` の 1 本でだけ開く。ShellExecute などで OS に渡すのは `web` と `mail` の `text` だけ（`unlisted_extensions` は「テキストでない」の表で、実行できる物の表ではない）。
3. **段 L1 は開かない。** OS で開く port・確かめの状態・クリックは段 L2。段 L1 で変わる振る舞いは無い（描画も今のまま）。
4. **試験。** `nib_tests` の Markdown の scope に、宛先の列（インラインの形・参照の形 `[a][1]`・autolink 3 種・エスケープと実体参照・画像は載らない・表の升と見出しの中のリンク）と、`link_destination` の表（各種類・大小・ドライブの文字・UNC・`..` を含む相対・無題・`%` の解き・不正な `%`・制御文字・テキストでない拡張子）。

**限界（段 L1）。** 宛先が実在するか・テキストかは見ない（開くときに分かる）。国際化ドメイン名（IDN）は `refused`（見た目の似た字での偽装を避ける・punycode で書いた host は通る）。`file:` の URL は画像と同じく host が空か `localhost` のものだけを受ける。参照の形の定義の行そのものは要素に出ない（md4c の作り）。表の升の要素（`preview_table_cell`）は links を写さない（升の中のリンクをクリックで開くなら段 L2 で写す・申し送り）。画像の alt の中に書いたリンク（画像の角括弧の中のリンクの記法）は links に載る。

### 段 L1 の実装で分かった事実

- md4c は href の `\` のエスケープを解いて渡す（autolink は解かない・`<https://x/a\_b>` は綴りのまま）。実体参照は ENTITY の部分列で来るので、画像の属性と同じ `attribute_text` で解く。
- `www.` の autolink には md4c が `http://` を、裸のメールと `<a@b.c>` には `mailto:` を前に付けて渡す（builder で足さない）。参照の形 `[a][1]` は定義の宛先が href に来る。
- 宛先の範囲は link の装飾の範囲から要素を閉じるときに作るので、`~` 1 つの打ち消しを戻したずれと alert の印の置き換え（`markdown_alert_titled` が装飾と同じだけずらす）がそのまま入る。

## 追補（2026-11-19・#1863・設計席）: 長い囲みのコードを 128 行ごとの切れに分けて色を保つ

2026-11-18 の追補（#1862）の決定 5 は、色の区間が 2,500 を超える囲みを 1 色にした。DirectWrite の `SetDrawingEffect` が 1 つの layout の中の区間の数の 2 乗で伸びるからで（`D:/NeNeNib/outputs/20261118/review-1860.md` `review-1860b.md` の実測）、普通のコードで 300〜500 行より長い囲みが 1 色になった。費用は「1 つの layout の区間の数」で決まるので、layout を小さく分ければ区間の総数に対して線形になる。

1. **囲みのコードは 128 行ごとの切れ（要素）で出す。** `parse_markdown` は囲みのコード（字下げのコードを含む）が 128 行（`preview_code_piece_lines`）を超えるとき、続く要素の列として出す。各切れは `PreviewBlockKind::code` のまま、新しい欄 `PreviewCodePiece piece`（閉じた enum `{only, first, middle, last}`）を持つ。128 行以下の囲みは `only` で、今までと同じ 1 要素（見た目も保持も変わらない）。front matter は分けない（1 色のまま・長いものは稀）。
2. **字句解析は囲み全体で 1 回、色は切れごとに切り分ける。** 行をまたぐ状態（複数行のコメント・生文字列）は切れの境を越えて引き継ぐ。区間は切れの `text` の中の位置に直し、切れの境をまたぐ区間は境で 2 つに割る。区間の上限 2,500 は**切れごと**に当てる（超えた切れだけ 1 色・ほかの切れは色を保つ）。囲み全体の行とバイトの上限（5,000 行・512 KiB）は今のまま。
3. **1 行だけの巨大な囲みで区間を溜めない**（review-1860b の L1）。切れの区間が上限を超えた時点で、その切れの区間を捨てて数えるだけにする（行をまたぐ状態は引き継ぐ）。
4. **描き方。** 面（`panel`）は切れの列で 1 枚に見える: `first` は上の余白と上の角、`last` は下の余白と下の角、`middle` は余白と角なし、`only` は今のまま。続く切れの間の空きは 0。文字組みの保持（決定 16）と見えている要素だけを作る形（`PreviewBlockTops`）はそのまま効くので、見えていない切れの layout は作られない。
5. **選択・写し・位置の対応。** 切れの列の写しは元の囲みと同じ文字列（切れの間は改行 1 つ）。各切れの `source_begin` はその切れの最初の行の元の位置で、本文 ↔ プレビューの位置の対応と送りの錨は切れの単位で効く。
6. **限界。** 再解析のたびに囲みを字句解析し直す（review-1860 の R4）は今のまま（後続）。1 つの切れの中に区間が 2,500 を超える行（1 行に 2,500 区間）はその切れが 1 色。

### 実装で分かった事実（#1863）

- 字句器の区間は 1 行の中で閉じ（`lex_line` は改行を含まない行を読む）、切れは行の境で切るので、切れの境をまたぐ区間は起きない（決定 2 の「境で 2 つに割る」は通らない）。区間の位置は切れの text の中の位置として作る（`preview_code_piece_colors`・切れの始まりは `preview_code_piece_starts` の 1 本）。
- 決定 3 は `append_line` の中で畳んだ後の区間が上限を超えた時点で止め、その切れの残りの行は区間の vector を確保しない `lex_state` で状態だけを進める。上限を超えた 1 行そのものの区間は `lex_line` の答えとして一時に作られる（字句器の口は行ごと・変えていない）。
- md4c は囲みの行末を位置の無い固定の `"\n"` で渡すので、切れの最初の行の位置は builder がその行の最初の位置つきの文字（引用の `>` の後ろ・字下げの後ろ）から取る。最初の行が空の行なら、最後に位置を持った行の終わりから本文の行を数えた行頭。囲みの頭からそこまで空の行だけなら前の切れの位置（要素の位置は昇順のまま）。
- 2 つ目からの切れは項目の記号とチェックボックスを持たない（項目の最初の要素だけが持つ規則のまま）。引用の深さ・リストの深さ・alert は全部の切れが持つ。
- 写しは `PreviewSelectionText` を変えずに元の囲みと同じ文字列になる（切れの text は各行が `\n` で終わり、見えている文字は末尾の改行を 1 つ落とし、要素の境に改行 1 つを挟む）。Shift+上下と Home / End は `preview_point_moved` が切れの列（first から last まで）を 1 つの囲みとして扱い、前と同じく囲み全体の頭と終わりへ動く（#1877 のレビュー R2）。左右の 1 文字は切れの端から次の切れの 0 へ 1 打鍵で移る。
- 限界: 空の行だけの囲みや、頭から 128 行より多く空の行が続く囲みでは、いくつかの切れの `source_begin` が同じ値になり、本文の位置からの追従（`preview_anchor_block` の upper_bound − 1）はその中の最後の切れへ行く（#1877 のレビュー L1・稀なので直さない）。
- 面の角は `FillRoundedRectangle` の上に、続く側の半分を角の無い矩形で重ねて塗る（middle は矩形だけ）。前の切れに続く側（middle・last）の矩形は上へ 1 物理画素伸ばして前の切れと重ねる。境は丸めない float の y なので、端数の y で antialias の 2 回の塗りが地の色を混ぜた線を出すのを避ける（#1877 のレビュー R1）。重ねた 1 画素は前の切れの最後の行の下端（行高の行間の中）に掛かる。文字の上の余白は `preview_panel_top_inset`、下の余白は `preview_panel_bottom_inset` の 1 か所で、高さ（`preview_extent`）・描く原点・hit test が同じ関数を読む。

## 追補（2026-11-19・#1869・段 L2）: リンクをクリックすると札で確かめて開く（施主決定 D106 = 案 B）

見た目は採用案（`D:\NeNeNib\design\link-20261119` の Card と Kinds・キャンバス https://claude.ai/artifact/Y1Gk5UsPmj1PucmjVAtEzR ）。

1. **クリック。** プレビュー（側・全面）のリンクの上の左の押しと離しが同じ要素の同じリンクに当たれば札を開く（ADR 0056 の決定 9 の形）。ui は押した点と離した点の文字（`PreviewPane::character_at`・`point_at` と同じ layout の HitTestPoint で文字の上だけ）を意図 `ClickPreviewLink{pressed, released}` で送るだけで、どの文字がリンクか（`core::preview_link_at` で `PreviewBlock.links` を引く）と種類（`link_destination` の 1 本）は application が決める。`heading` は段 L3 まで札を出さない。表の升のリンクも開く（`preview_table_cell` が升に収まる links を写す・段 L1 の限界を外す）。全面プレビューではリンクの上の押しも今までどおり選択の始まり（空の選択）を送り、リンクの中でドラッグして同じリンクの中で離しても札が開く。
2. **札の状態は application。** `EditorState::link_card()`（`LinkCardView{block, link, destination, tab}`・`tab` は札を開いたアクティブなタブの `autosave_ids()` の通し番号）に置き、frame の `EditorFrame.link_card` には、アクティブなタブがその番号で、見せている結果の同じ要素に同じリンクがある間だけ載せる（hover の札と同じ形・ADR 0190 の決定 6）。札の意図は `ActOnLinkCard{LinkCardAction}`（open = Enter・close = Esc と札の外の押し・copy = Ctrl+C・primary / secondary = 右と左のボタン）。札は札の意図と `not_user_operation`（合図・窓の寸法・帯の hover・窓の非アクティブ化）のほかの全部の意図の入口（`close_link_card_before`）で閉じ、ほかの鍵は閉じた後にふつうに効く。hover の札と違い、窓が非アクティブになっても閉じない（OS の道へ渡した後の戻りで確かめが消えていない）。アクティブなタブが札のタブでなくなったら、合図の道（裏の読みの `collect_reads` で替わったとき）でも札を消す（`drop_stale_link_card`・前の文書のフォルダで解いた宛先を別のタブで開かない・#1876 のレビュー R1）。札で受けた Enter は、離されるまでの押し続けの反復を窓が捨てる（2 回目から本文に改行を入れない・`link_card_enter_repeat`）。全面プレビューの `passes_full_preview` に 2 つの意図を足す（札の Esc は全面を抜けない）。
3. **写す。** Ctrl+C と、開けない宛先の右のボタン（写す）は `clipboard_written` の 1 本で宛先の綴りを写し、写せたら札を閉じて「宛先を写しました」を 1 行出す。写せなければ ADR 0134 の 1 行で札は開いたまま。
4. **開く。** `web` と `mail` は新しい port `LinkOpenPort`（`open(uri)` と `collect()`・失敗は `LinkOpenFailure{refused, failed}` の 1 行）に `text` の綴りのまま渡す。本物の `Win32LinkOpenAdapter` はワーカーの仕事の中で STA を作って `ShellExecuteExW`（verb は `open`）を呼び、入口で守りの 2 重として「scheme が http / https / mailto」「綴りが ASCII の URI の字だけ」の 2 つを確かめる（core の判定の写しは書かない）。`document` は OS へ渡さず `open_listed` → `open_document` の 1 本で Nib のタブに開く（段 L1 の不変条件）。開く道の読みはディスクのファイルだけを読む（`Win32FileAdapter::read` と `Win32DocumentReader::request` の `GetFileType == FILE_TYPE_DISK`・装置と名前付きパイプは `unreadable`・Ctrl+O と起動引数にも効く）。`LinkOpenPort` は #1790 の `RevealPort`（エクスプローラーで場所を見せる）と同じ形の別の port のまま（開くと場所を見せるは別の事柄）。起動・開く・切り替え・保存・打鍵の道は呼ばない（契約 `verify_quiet_paths`）。
5. **文。** 札に描く宛先の綴りは、制御文字と `link_deceptive_code_point`（`link_destination` の見えない字の判定と同じ 1 本）の字を U+FFFD に置き換える（`link_card_shown_text`・写すのは元の綴りのまま）。札の題・開き方・ボタン・鍵の案内と 2 つの知らせを `UiMessage` の末尾に 17 値足す（鍵の名前は訳さない・中国語の案内は全角のコロンで区切る）。色は `panel` / `panel_border` / `text`・構文の `comment` / `error` / `function` のトークンだけ。

**限界（段 L2）。** 札の位置はクリックした frame で描いたリンクの矩形から決め、測った範囲の外なら領域の左上に置く。ワーカーは 1 本なので、`ShellExecuteExW`（`SEE_MASK_NOASYNC`）が戻るまで（冷えたブラウザ・DDE のハンドラで数百 ms〜数秒）同じワーカーのほかの裏の仕事（Markdown の解析・フォルダ・画像・プレビューの窓）が待つ（#1790 の `RevealPort` と同じ形・専用のスレッドにするかは `RevealPort` とまとめるかと一緒に後で決める・#1876 のレビュー R2）。マウスを載せたときの見た目（指の形のカーソル）は入れていない。見出しへ跳ぶのは段 L3。

### 段 L3（2026-11-19・#1869）: プレビューの `#見出し` のリンクで、その見出しへ跳ぶ（確かめない）

1. **名札（slug）は core の純関数 `heading_slugs` の 1 本**（`HeadingSlug.hpp`）。見出しの要素（`PreviewBlockKind::heading`）の表示の文字（`PreviewBlock.text`・装飾の記号を除いた後）から、前後の ASCII の空白を除き、ASCII の大文字を小文字に畳み、ASCII の英数字と `-` `_` と非 ASCII の字（日本語など）を残し、空白を 1 つずつ `-` にし（続く空白をまとめない）、ほかの ASCII の字（記号・Tab・改行）を消す。非 ASCII のうち、日本語の文書で効く CJK と全角の句読点・記号（U+2010〜U+2027・U+3000〜U+303F・U+FF01〜U+FF0F・U+FF1A〜U+FF20・U+FF3B〜U+FF40・U+FF5B〜U+FF65）と絵文字（U+1F000〜U+1FAFF・U+2600〜U+27BF・U+FE0F・U+200D）は消す（範囲は `HeadingSlug.cpp` の名前の付いた constexpr の表 `removed_code_points` の 1 つ・設計席の決め 2026-11-19 の追記）。全角の英数字（U+FF10〜U+FF19・U+FF21〜U+FF3A・U+FF41〜U+FF5A）は残す。非 ASCII の大文字は畳まない。同じ名札が 2 つ目から現れたら `-1` `-2` … を付け、付けた形が既にあればさらに次の数へ進む（GitHub の公開の説明「Basic writing and formatting syntax」の Section links の規則と例・数え方は github-slugger と同じ・`# a` `# a` `# a-1` は `a` `a-1` `a-1-1`）。列は `parse_markdown` が解析の終わりに 1 回だけ作って `MarkdownDocument.headings`（`MarkdownHeading{block, slug}` の本文の順の列）に置き、frame ごとに作り直さない。確保の失敗は `insufficient_memory`。
2. **リンクの `#…` を名札の列で引く**（`heading_block_of`）。`LinkDestination.text`（`#` の後ろ）を `percent_decoded` の 1 本で解いてから、ASCII の大小を畳んで名札と比べ、最初の 1 つ。見つからなければ 1 行の知らせ（`UiMessage::notice_heading_not_found`・3 列の末尾に 1 行ずつ）。札は出さない。
3. **跳ぶ先。** 見出しの要素の `source_begin`（見せている結果が古い本文のものなら本文の長さに寄せる）の行の頭（桁 0）へ、本文のクリックと同じ `PlaceCaret` の道でキャレットを置く（通常・Vim の両方。Vim の VISUAL と INSERT の扱いもクリックと同じ）。横のプレビューは今の追従のまま動く（本文が送られれば追従に戻る）。全面のプレビューは送りの錨をその行の頭に置き（追従をやめる・`scroll_preview` と同じく本文の先頭行を覚え直す）、本文のキャレットも同じく動かす。
4. **ジャンプの印。** Vim の `:help jump-motions` は「新しいファイルを開く命令」と `:tag` を跳ぶ命令に数え、文書の中のリンクで別の所へ移るのは help のタグへ跳ぶ（`Ctrl-]`）と同じ形なので、跳ぶ前の位置を `''` とジャンプの列に置く。道は全文検索の着地（`land_project_hit`）と同じ `vim_mark_set(…, '\'', …)`（`m'` の 1 本）で、モードを問わず置く（通常モードでは読まれないだけ）。
5. **入口は段 L2 の札の道と同じ。** `ClickPreviewLink` で押しと離しが同じリンクに当たり、種類が `heading` なら札を開かずに `jump_to_heading` へ。表の升の中のリンクも同じ道。

**限界（段 L3）。** 他の文書の `other.md#sec` は文書を開くだけで、見出しへは跳ばない（段 L1 のとおり解決器が `#` の後ろを捨てる）。GitHub（github-slugger）は Unicode の大文字も小文字に畳み（`Θ` → `θ`・全角の `Ａ` → `ａ`）、Unicode の句読点と記号の全部を消すが、Nib は ASCII だけを畳み、非 ASCII で消すのは上の表の範囲だけ（一般カテゴリの表は持たない）。そのため GitHub の名札をそのまま写した `#…` が、非 ASCII の大文字か、表の外の句読点・記号（ギリシャ文字の句読点・数学の記号など）を含む見出しに当たらないことがある（その形の見出しは Nib の名札で書けば当たる）。日本語の文書の例（「設定（Ctrl+,）」→ `設定ctrl`・「手順 1. 準備」→ `手順-1-準備`・「🚀 Launch」→ `-launch`）は GitHub と同じ名札になる（試験の表）。空の `#` は空の見出しがあればそれに当たる（GitHub の「頁の頭」の意味は持たない）。本文（プレビューでない側）の `#見出し` から跳ぶ道と見出しの一覧（目次）は無い。