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
- 起動の道に DLL を載せない・8 本の速さに退行なし: **active**（QLT-014・ADR 0130 の遅延読み込みの検査）。

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
