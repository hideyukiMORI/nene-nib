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
- `source_begin` は NORMAL / ENTITY と、本文の中を指す CODE / HTML の text から引く（コードブロックの本体の行は本文の中・行末と字下げは外）。中かどうかは `std::less_equal` の全順序で比べる。文字の無い要素の補いは「直前の位置つきの text の終わりの次の行から空行を飛ばした最初の行の行頭」・前に何も無ければ本文の先頭から空行を飛ばす（決定 8 の追記）。行の探索は位置を持たない要素のときだけ走る。
- `eng/symbols.py` は core の未定義 `md_parse` を閉包の `nenenib_md4c.lib` の定義で解決し、`eng/symbol-allowlist.json` は変えずに 0 violation（#772 の名指しの import は不要だった）。
- 16 MiB の parse（組み立て込み）の probe は `markdown-parse-mixed-16mib`（入力は probe の (i) と同じ 16,777,183 バイト・209,263 要素）。Release（`77f391d`・負荷 1〜5%）の 10 回で中央値 143.2 ms・最小 141.4 ms（md4c だけの 63.8 ms に、組み立てが約 80 ms 足される）。正式な速さのゲートではない。

## 決定 16〜18（2026-10-21 追記・#784）: 描画の保持と painter の形

段 3（描画）の形。置き場（split の other）と更新（worker）は段 4・5 の ADR B で書く。座標は本文と同じく描画先の DPI を `core::reference_dpi` にした物理画素で、DIP の寸法は `PreviewPlacement`（`src/ui/win32`）の 1 か所の表から写す。

16. **1 要素 1 layout の保持は `PreviewTextLayouts`（ui/win32）。** `BodyTextLayouts`（ADR 0114）と `BodyTextPainter`・字形の収集（ADR 0073）には入れない。鍵は `{text, spans, kind, level, 幅（物理画素）, PreviewTextStyle{字体名, pt, DPI}}` で全部が一致するときだけ使う。書式は本文の字体（可変幅・`DWRITE_WORD_WRAPPING_WRAP`）と等幅の字体（コードと HTML ブロック・`NO_WRAP`・設定の字体が等幅ならそれ、でなければ Cascadia Mono → Consolas・ADR 0140 の決定 6 / D58）の 2 つ。見出しは layout 全体に倍率ぶんの `SetFontSize` と `SetFontWeight(BOLD)`。行高は本文と同じ比（pt → DIP × 1.6・`BodyLayout` と同じ丸め）の `SetLineSpacing(UNIFORM)` で、字は行の縦の中央（`BodyTextLayouts` の set_spacing と同じ式・空白 1 つの layout で大きさごとに 1 回測る）。装飾の範囲は要素の `text` のバイト位置を core の `utf16_length` の 1 本で UTF-16 の位置に写す（`preview_units`・新しい変換器を書かない）: 強調 `SetFontStyle(ITALIC)`・太字 `SetFontWeight(BOLD)`・行内コード `SetFontFamilyName(等幅)`・リンク `SetUnderline`。字体に斜体が無ければ DirectWrite の疑似斜体に任せる。保持は最近使った順に 256 件までで、超えたら最も古いものから捨てる。高さは `GetMetrics().height`。高さの部分和は `PreviewBlockTops`（要素ごとの上端と下端の `std::vector<float>`・見えている最初の要素は下端の `upper_bound`）で、測るときは `PreviewTextLayouts::measure` が layout を保持しない（見えている要素だけを `get` が作って保持する）。文書・幅・字体が変われば作り直す（今は呼び手が毎回作る・段 5 で差し替えのたび）。
17. **描くのは `PreviewPainter<Target>`（`BodyTextPainter` と同じく描く先を template で受ける）。** 入力は `PreviewScene{MarkdownDocument の借用, PreviewBlockTops の借用, PreviewTextStyle, 枠の矩形, 上の送り}` と、借りた `PreviewTextLayouts`・`PreviewBrushes`。枠を `SyntaxPalette.background` で塗って枠で切り、見えている要素だけを `DrawTextLayout`。面と線は `FillRectangle` / `FillRoundedRectangle`（替え玉の描画先 `CountingTarget` の口と同じ）。色は役割 `PreviewInk{background, foreground, keyword, string, function, comment, operators, panel}` → `PreviewBrushes` の `switch` の 1 か所（`default` 無し・ui/win32 に色のリテラルを書かない）。brush は役割ごとに 1 つ（行の中の装飾は layout の範囲に drawing effect として置くため）で、描画先とテーマの名前が変われば作り直す。drawing effect は描くたびに置き直す（保持の鍵にテーマを入れない）。採用案 D62 の表のとおり: 見出し `keyword`・コードブロックは `string` と `panel` の面（角丸 5・内側の余白 12・折らず面の外は切る）・行内コードは `string` と `panel` の面（`HitTestTextRange` の画面行ごとの矩形・左右 4・角丸 3・行高の上下を 1 割ずつ空ける）・リンクは `function` と下線・引用の中の段落と項目は `comment` と深さごとの左の線 2 DIP（同じ深さの前の要素との間もつなぐ）・HTML ブロックは等幅の `comment`・水平線は `comment` 1 DIP。字下げは左の余白 16＋引用 12 × 深さ＋箇条書き 24 × 深さ、右の余白 16・上の余白 12。記号は深さ 1 が `•`・2 が `◦`・3 以上が `▪`（番号はそのまま）で `operators`、文字の左端から 8 DIP 空けて右に揃える。要素の間は前の要素の後（段落・見出し・コード・HTML・項目は行高の半分・水平線は行高）と次の要素の前（見出しと水平線は行高）の大きいほうで、続く項目の間は 0（採用案に書いていない所・設計席が PNG で見る）。
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
- builder の確保の失敗（`bad_alloc`）は捕まえない。md4c の C のフレームを抜けて落ちる（Nib の他の場所と同じ）。プレビューの入力は上限（計画 M-3・16 MiB）で抑える。
- 表・脚注・数式・画像・HTML の解釈はしない（初版の範囲案）。
- `eng/symbols.py` は core の未定義を閉包の定義で解決するので、Debug の `nenenib_md4c.lib` が定義する UCRT の inline の `sprintf` 系を core が呼んでも ARC-003 にならない。名指しの API（`md_parse`）だけに限るのは #772。

## 強制

- CNF-013（`thirdPartyDigests` の照合・planned → 正例・反例を足して active）。
- `eng/targets.cmake` の `nenenib_third_party_target` 以外に C の target を作れない（モジュールの検査）。
- `md4c.h` の include は `src/core/MarkdownParse.cpp` の 1 か所。CNF-009 と同じ字句検査（`eng/conformance.py` の `concurrency_and_simd`）が `src/` の中のほかのファイルの include を ARC-002 で拒否する。ヘッダ名と許す path は `eng/conformance-rules.json` の `markdownParserHeader` / `markdownParserAllowedPath`、正例・反例は `tests/conformance/test_conformance.py`（#765）。
