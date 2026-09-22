# ADR 0040 — 描画用の行は core の純関数が作り、制御文字は `^X`、書式用文字は `<xxxx>` に置き換えて桁の対応表を持つ

- 状態: 受理（設計席 2026-09-23・Issue #117）
- 日付: 2026-09-23
- Issue: #117
- 影響する規則: FR-003 / ARC-001 / ARC-004 / CPP-002 / CPP-011 / CPP-012 / QLT-001 / QLT-012
- 前提: [ADR 0034](0034-vim-virtual-column-one-table.md)（表示幅の表 1 本）・[ADR 0036](0036-line-ending-owned-by-text-buffer.md)（LF 文書の `\r` は文字）・[ADR 0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md) 決定 8（色は `Palette` のトークン）・[ADR 0014](0014-ime-imm32-composition-outside-the-buffer.md)（`tint_runs` の文字色の経路）

## 文脈

`\r`（LF 文書）・`^A` などの制御文字と U+200B などの書式用文字は、engine では ADR 0034 の表で桁 2（`wide`）と桁 6（`unprintable`）に数えるが、描画は `LineView.text`（本文の生の文字列）をそのまま DirectWrite に渡していて（`Direct2DRenderer::text_layout`）、何も出ないか欠けた字形になる。桁 ↔ x 座標（`column_at` / `caret_x`）も本文の文字列で引くので、engine の桁と画面の位置がずれる。Vim は `^M` `^A` を 2 桁、`<200b>` を 6 桁で描く（`:help 'display'`・実測 `strdisplaywidth`: `\x01` = 2・U+200B = 6・U+FEFF = 6・DEL = 1）。

## 決定

**描画用の行は core の純関数 `display_line` が本文の行から作る。制御文字（表で `wide` かつ U+0020 未満）は `^X` の 2 文字、書式用文字（表で `unprintable`）は `<xxxx>` の 6 文字に置き換え、本文の各 code point が描画の何文字目から始まるかの対応表を持つ。renderer は描画用の文字列で layout を作り、桁の変換は対応表 1 本で行う。**

1. **型と関数**（`src/core/DisplayLine.hpp` / `.cpp`・1 ファイル 1 型）

   ```cpp
   struct DisplayLine {
       std::string text;                  // 置き換え済み（UTF-8）
       std::vector<std::size_t> starts;   // starts[i] = 本文の i 番目の code point が始まる描画の code point 位置。size = 本文の code point 数 + 1
   };
   [[nodiscard]] DisplayLine display_line(std::string_view text);
   [[nodiscard]] std::size_t display_position(const DisplayLine&, std::size_t column);   // 本文の桁 → 描画の位置（column が範囲外なら末尾）
   [[nodiscard]] std::size_t source_column(const DisplayLine&, std::size_t position);   // 描画の位置 → 本文の桁（置き換えの途中は、その文字の桁）
   [[nodiscard]] bool is_replaced(const DisplayLine&, std::size_t column);              // 本文の桁 column が置き換えられた文字か
   ```

   置き換えの規則は ADR 0034 の表（`display_width_of`）だけを見る: `wide` かつ code point < 0x20 → `^` + (cp + 0x40)（`\r` → `^M`・`\x01` → `^A`・`\x1f` → `^_`）。`unprintable` → `<` + 小文字 16 進 4 桁 + `>`（U+200B → `<200b>`・U+FEFF → `<feff>`）。それ以外（Tab・全角・結合文字・DEL）はそのまま 1 文字。`\n` は行に含まれない。置き換えた文字数は ADR 0034 の `cells_of` と一致する（`wide` の制御文字 = 2・`unprintable` = 6）。
2. **`LineView` に `core::DisplayLine display` を足す**。`text` は残す（engine と選択の桁は本文の桁のまま）。`EditorController::line_view` が `display_line(text)` で埋める。
3. **renderer は `display.text` で layout を作る**（`layout_of` / `text_layout`）。桁 → x は `display_position` を通してから `HitTestTextPosition`、x → 桁は `HitTestPoint` の `textPosition` を `source_column` で戻す。選択・検索の当たり・IME の差し込み位置も同じ変換を通す（変換は renderer の 1 か所の補助関数で、`SelectionSpan` の両端を写す）。
4. **色**: 置き換えた文字は `Palette::muted` で `tint_runs`（ADR 0014 の IME の経路と同じ）。新しいトークンは足さない（Vim の `SpecialKey` に相当。テーマの `.v1.theme` の互換を保つ）。
5. **Tab は変えない**（範囲外。ADR 0034 の桁 8 と DirectWrite の tab stop の不一致は別 Issue）。
6. **既知の差**: C1 制御文字（U+0080〜U+009F）は Vim が `<85>` の 4 桁で描くが、ADR 0034 の表に無い（`single`）ので置き換えない。表の訂正は保護対象の変更なので別 Issue（設計席が起票）。DEL（U+007F）は Vim も 1 桁なのでそのまま。

## 強制

- `display_line` の置き換え文字数 = `cells_of`（制御・書式用）の一致: **active**（`nib_tests` の scope `--display-line`。`\r` `\x01` `\x1f` U+200B U+FEFF・混在行・空行・全角と Tab を含む行で `starts` の差が `cells_of` と一致、`display_position` / `source_column` が互いの逆で、範囲外は末尾に畳む）
- renderer が `LineView.text` を layout に渡さないこと: **planned**（レビュー事項。`layout_of` の引数が `display.text` であることを見る）
- 実機の見た目: hide の手元（`verify-window.py --capture --keys` で `\r` を含む行を撮って設計席が Read で見る。鍵の記法に `<C-m>` 相当は無いので、`--keys` に生の `\r` を渡すか、`\r` を含むファイルを開く）

## 結果

得られるもの: `^M` `^A` `<200b>` が Vim と同じ形と桁で見え、キャレット・選択・クリック・検索の当たりが engine の桁と一致する。置き換えは core の純関数 1 本で、renderer は文字列の中身を知らない。
失うもの・残る穴: `LineView` が 1 行につき文字列を 2 本持つ（見えている行だけなので量は小さい）。C1 制御文字と Tab の画素幅は残る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| renderer で置き換える | 桁の対応表が ui/win32 に閉じ、application の unit で守れない（ARC-004） |
| `LineView.text` を置き換え済みにする | 選択・検索の桁が本文の桁なので、対応表無しではずれる。2 本持つほうが正直 |
| 専用の色トークンを足す | テーマ 9 本と `.v1.theme` の検証に波及する。`muted` で Vim の `SpecialKey` に足りる |
| DirectWrite の字形置換（font fallback）に任せる | 制御文字は字形を持たないので何も出ない。桁 2 / 6 も得られない |
