# ADR 0177 — 印刷は port 1 本で、本文と同じ DirectWrite の layout を XPS の print control に描く

- 状態: 受理（施主決定 D71・2026-10-28 夜。実装は 1〜2 席・受理は D41 の形）
- 日付: 2026-10-29
- Issue: #1047
- 規則: FR-031、ARC-001 / ARC-003 / ARC-007 / ARC-010、CPP-002 / CPP-005 / CPP-011 / CPP-017、QLT-014、ADR 0008（色のトークン）/ 0017（テーマ）/ 0078（操作の表）/ 0114（`TextLayoutPort` と renderer は同じ layout・`BodyTextLayouts`）/ 0116（ui の許可されたライブラリ）/ 0130（描画 DLL の遅延読み込み）/ 0140（格子の行も `BodyTextPainter`）/ 0156（構文の色）

## 文脈

印刷は仕様にも計画にも無かった。D71 でメモ帳の役の 1 つとして実装予定になった。Nib の本文の文字組みは `BodyTextLayouts`（DirectWrite）と `BodyTextPainter`（Direct2D）の 1 本で、構文の色は `LineView.colors`。Direct2D は `ID2D1PrintControl` で同じ描画命令を XPS の文書パッケージに流せる（`IPrintDocumentPackageTarget`）。プリンタの選択は OS の `PrintDlgExW`。

## 決定

1. **操作は 1 つ、鍵は無し。** `EditorOperation::print`（「印刷」・F1 の一覧から・両モード・鍵の行は置かない。Ctrl+P は面に使う・D71 の「代替」）。Vim は `:hardcopy`（`:ha`）が同じ意図 `PrintDocument` を送る（Ex の request）。実行は `run_operation` の 1 本。

2. **port は 1 本、application は意図 1 つ。** `application::PrintPort{ [[nodiscard]] virtual std::expected<PrintOutcome, PrintFailure> print(PrintJob) = 0; }`。`PrintJob{ std::shared_ptr<const std::string> text（本文の連続 1 本・要求のときだけ写す）; core::LineFeed line_feed; core::DocumentTitle title; core::Language language; core::FontFamily family; core::FontSize size; bool gutter; }`。`PrintOutcome`（閉じた enum: `printed` / `cancelled`）・`PrintFailure`（閉じた enum: `no_printer` / `dialog_failed` / `rendering_failed` / `too_large`）。controller は `ports_.print.print(job)` をこの意図のときだけ呼び、`cancelled` は黙り、失敗は 1 行の知らせ（「印刷できませんでした: <理由>」）。port は同期（印刷ダイアログはモーダル・スプールは OS）。

3. **実装は `src/ui/win32` の `Win32Printer`。** DirectWrite の書式と `BodyTextPainter` を使うので renderer の隣（`TextLayoutPort` を ui が実装する ADR 0114 と同じ置き方）。手順: `PrintDlgExW`（`PD_RETURNDC` 無し・`PD_NOPAGENUMS` 無し・ページの範囲は受ける）→ `IPrintDocumentPackageTargetFactory::CreateDocumentPackageTargetForPrintJob` → `ID2D1Device::CreatePrintControl` → ページごとに `ID2D1CommandList` に描いて `AddPage` → `Close`。字体・字幅・折り返しは本文と同じ DirectWrite の書式（`BodyTextStyle`・紙の幅で折り返す `WordWrap`）。字形の収集は画面の保持（`BodyTextLayouts`）を使わず、印刷のために作った layout を使い捨てる（保持を汚さない）。

4. **配色は印刷用の 1 つ。** 白地・黒字・構文の色は `light` の既定テーマの値（`Palette` のトークンから作る `print_palette()`・core の純関数・新しいトークンは足さない）。選択・キャレット・検索の面・IME の差し込みは描かない。

5. **紙の形。** 余白は上下左右 20 mm（固定）。ヘッダは左にファイル名（無題は「無題」）・右に「n / N」のページ番号（本文と同じ字体・`muted` の色）。行番号は gutter の設定に従う（`gutter == true` なら本文と同じ幅の規則）。Tab は本文と同じ 8 升。制御文字の `^M` `<200b>` も本文と同じ（`display_line`）。1 ページの行数は紙の高さ − 余白 − ヘッダを行の高さで割る（整数演算・DPI は印刷の DC の論理 DPI）。

6. **DLL は呼ぶときだけ。** `comdlg32`（`PrintDlgExW`）・`prntvpt` / `xpsprint` / `documenttarget`（`IPrintDocumentPackageTargetFactory` は `CoCreateInstance`）は遅延読み込み（ADR 0130 の形・起動の道に載せない）。ADR 0116 の ui の許可の表に `comdlg32` を足す（追補）。

7. **大きさ。** 本文が `core::print_byte_limit`（16 MiB・`preview_byte_limit` と同じ所の定数 1 つ）を超えるときは頼まず `too_large`（「印刷は 16 MiB までです」）。ページの上限は置かない（OS のスプールに任せる）。

8. **契約。** application `--print`（意図 → port の呼び出し 1 回・`PrintJob` の中身（本文・題名・言語・字体・gutter）・`cancelled` は黙る・失敗 4 種の 1 行・`too_large`・`verify_quiet_paths` に `PrintPort` の欄）。ui: `nib_window_tests` に「1 ページの命令を WIC の bitmap に描いた画素」（`ID2D1PrintControl` はプリンタが要るので、ページを描く関数 `draw_print_page(ID2D1DeviceContext &, PrintPageInput)` を print control と bitmap の両方から呼べる形にし、試験は bitmap に向ける・ヘッダの位置・行番号・折り返し・構文の色）。速さ: 打鍵と起動の道に載らない（8 本に退行なし・DLL は遅延）。

9. **限界（後続）。** ページの設定（用紙・向き・余白の変更）・印刷プレビュー・選択の範囲だけの印刷・ヘッダとフッタの書式・折り返しの無い印刷（`nowrap` の文書の横の切れ）・テーマの色で印刷・`:hardcopy > file.ps`（PostScript）は後続。

## 強制

- 印刷の道が port 1 本・描画が本文と同じ書式: **planned**（契約 `--print` と `nib_window_tests`・この PR で active）。
- 起動の道に DLL を載せない: 遅延読み込みは `NeNeNib` の `/DELAYLOAD:` の 1 か所で、確かめは `llvm-readobj --coff-imports` の手の記録（Debug・**planned**・機械の検査の道具は無い）。8 本の速さに退行なし: **active**（QLT-014・起動の速さの間接の検出）。（2026-10-30 の追補 10 の後で書き分けた・#1160 のレビュー R6-1・ADR 0001）

## 結果

得られるもの: 画面と同じ字体・字幅・色分けで印刷できる。メモ帳の役の最後の穴が埋まる。
失うもの・残る穴: 配色は 1 つ・余白は固定・プレビューは無い。印刷の DLL の読み込みは最初の印刷で 1 回かかる。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| GDI の `TextOut` で印刷 | 字体選択・字幅・結合文字・絵文字が画面と違う。文字組みは DirectWrite の 1 本（ADR 0114） |
| `adapters/win32` に実装 | DirectWrite / Direct2D は ui の許可（ADR 0116）。`TextLayoutPort` と同じく ui が実装する |
| 印刷プレビューの面を初版に | 見た目の判断（/design と D 番号）が要る。先にダイアログ → 印刷の 1 本 |
| Ctrl+P を印刷に | 面の鍵。D71 の「代替」（一覧と Ex） |

## 追補（2026-10-30・#1047 の N6b の実装の結果）

1. **`print_byte_limit` は印刷の header。** 決定 7 の「`preview_byte_limit` と同じ所」を改め、定数は `src/core/PrintPages.hpp`（`print_margin_millimetres` の隣）に置く。印刷の controller と試験が Markdown の解析の header を読まない（PR #1157 の独立レビュー R1-2）。知らせの「16 MiB」の文字は定数と別に書いてある（プレビューと同じ形・揃えるなら別 Issue）。
2. **紙面の座標は DIP。** 印刷の DC は作らない（GDI を結ばない・CPP-017）ので、決定 5 の「印刷の DC の論理 DPI」は Direct2D の DIP（dpi 96・`core::reference_dpi`）で、print control が紙の解像度へ写す。紙の大きさはダイアログの DEVMODE の幅と長さ、無ければ用紙の番号の表（`PrintPaperSizes.cpp`・A3 / A4 / A5 / B4 / B5 / Letter / Legal / Tabloid / Executive）、どちらも無ければ A4。
3. **頁割りの単位（R5-1）。** 1 ページの行数は `print_rows_per_page` に「上の余白から本文の最初の行まで」をヘッダの高さとして渡す。本文の行の高さは `body_layout_in` の整数で、印刷の layout も同じ整数の `DWRITE_LINE_SPACING_METHOD_UNIFORM`（`BodyTextLayouts::set_spacing`）なので、切り上げは要らず割り算が合う。行番号の欄の幅は本文と同じ `body_layout_in`、番号の字の大きさと余白は画面と同じ `GutterMetrics.hpp` の定数（renderer と共用）。
4. **文字組みは使い捨ての `BodyTextLayouts`。** 印刷の 1 回ごとに `BodyTextLayouts` を 1 つ作り（画面の合成ルートのものとは別の値）、本文と同じ書式・字体選択・Tab の 8 升で、紙の幅で文字単位に折り返す（`wrapping = wrap` に固定・決定 9 の nowrap は後続）。ヘッダの題名と「n / N」も同じ書式で、右寄せは字の幅を測って置く（書式は LEADING）。
5. **配色の型。** `core::print_palette()` の戻りは `core::PrintPalette{ Palette ui; SyntaxPalette body; }`。`BodyTextPainter` は区間の色を `ColoredRun`（`syntax_color(body, role)`）で、区間の外を `ui.text` で受ける。
6. **置き場。** `Win32Printer`（WARP の device → 紙面 → job の受け口 → 描く・ダイアログは追補 10 で ui へ）、`choose_printer`（`PrintDlgExW` と DEVMODE → print ticket の `PTConvertDevModeToPrintTicket`・写せなければプリンタの既定）、`render_print_job`（print control・ページの範囲・失敗で `Cancel`）、`PrintComposition`（紙面）、`draw_print_page`（1 ページ）は `src/ui/win32`。ダイアログの親は本窓（追補 10 で `GetActiveWindow()` から改めた）。
7. **ライブラリと遅延読み込み。** `eng/architecture.json` の `ui_win32` に `comdlg32` と `prntvpt` を足し（ADR 0116 の追補）、`NeNeNib` の `/DELAYLOAD:` に `comdlg32.dll` と `prntvpt.dll` を足す。`IPrintDocumentPackageTargetFactory` と WIC と XPS の object model は `CoCreateInstance`（ole32）。`CLSID_WICImagingFactory2` は `Win32Printer.cpp` の `INITGUID` で定義を置く（windowscodecs を結ばない）。
8. **試験の形。** `nib_window_tests --print` が、1 ページを WIC の bitmap に描いた画素（ヘッダの左右・行番号の有無・構文の色・折り返し）、1 ページの行数が下の余白に収まること、ページの数と 2 ページ目の続き、紙の大きさの純関数、print control からメモリの XPS（`MemoryPackageTarget` と `MemoryXpsTarget`・ファイルも実機のプリンタも使わない）へ書いたページの数と範囲の選択を確かめる。`Win32Printer` は ui の型なので `nib_adapter_tests`（ui を結べない・ARC-002）ではなく `nib_window_tests` に置く。ダイアログを出す窓の節（`eng/verify-window.py --print`）は作らない。
9. **強制の段。** 「印刷の道が port 1 本・描画が本文と同じ書式」は契約 `--print`（N6a）と `nib_window_tests --print`（この追補）で **active**。実機のプリンタでの印刷・ダイアログの操作は施主の確認。
10. **ダイアログは ui、port は描くだけ（設計席の判断 A・#1160 のレビュー R1-3）。** `PrintDlgExW` の modal loop を controller の `apply` の中で回さない（modal loop が配る `WM_TIMER` の意図が外側の `apply` に再入するため）。開く・保存と同じ形で、ui の `run_document_operation(print)` が先に `choose_printer`（本窓が親）でダイアログを出し、選んだ設定 `application::PrintSettings{ printer（UTF-8）; paper（DIP）; ranges; ticket（print ticket の XML のバイト列） }` か、ダイアログの失敗（`no_printer` / `dialog_failed`）を意図 `PrintDocument{ std::expected<PrintSettings, PrintFailure> settings; }` で送る。取り消しは送らない。controller は失敗なら 1 行で知らせ、そうでなければ 16 MiB を確かめて `ports_.print.print(job, settings)` を呼ぶだけ。`PrintPort` の署名は `print(const PrintJob &, const PrintSettings &)`。Vim の `:hardcopy` は controller が `operation_request(print)` を立て（一覧から選んだときと同じ形・ADR 0078）、ui が `deliver` の終わりで読んでダイアログ → 意図の道を通る。`PrintPaper` と `PrintRange` は application の型（ui の `PrintSetup` と `PrintChoice` は無くした）。`Win32Printer::print` は WARP の device → 紙面（compose）→ job の受け口の順で作り、受け口を作った後の失敗は `render_print_job` の 1 か所で `Cancel` する（R1-1）。1 ページで描く行は `print_page_lines` の二分探索で、ページと交わる行だけを見る（R4-1）。16 MiB を超える本文はダイアログを出した後に知らせる（ui は本文の大きさを持たない・限界）。
