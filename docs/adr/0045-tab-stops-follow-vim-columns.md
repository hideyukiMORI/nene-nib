# ADR 0045 — 描画の Tab は空白 8 個ぶんの tab stop で、Vim の仮想桁と同じ位置に止まる

- 状態: 受理（設計席 2026-09-23・Issue #175。hide 未確認・引き継ぎの「次の順」の 3 番目）
- 日付: 2026-09-23
- Issue: #175
- 影響する規則: FR-003 / ARC-001 / QLT-001 / QLT-012
- 前提: [ADR 0034](0034-vim-virtual-column-one-table.md)（Tab は仮想桁で次の 8 の倍数へ・意味論専用の純関数）・[ADR 0040](0040-display-line-with-control-glyphs.md)（`display_line` は Tab を置き換えない・見た目は別 Issue と明記）・[ADR 0008](0008-adopted-look-tabs-titlebar-statusbar-mica.md)

## 文脈

本文の Tab は `display_line` を素通りして DirectWrite の layout に生の `\t` で渡り、tab stop は DirectWrite の既定（`SetIncrementalTabStop` の呼び出しは 0 件・Sonnet の probe `probe-tabwidth.md`）。キャレット・選択・検索の枠・矩形 VISUAL の x 座標はすべて layout の hit test 1 経路で、内部の食い違いは無いが、layout の tab stop と ADR 0034 の仮想桁（`tabstop=8` 固定）が別の規則なので、`j` `k` や `Ctrl-v` の矩形で「Vim の言う桁」と「見える位置」がずれる。Tab の見た目を守るテストは 0 件。

## 決定

**本文の `IDWriteTextFormat` に、空白 1 個の advance × 8 を `SetIncrementalTabStop` で設定する。Tab は行頭からの 8 桁ごとの位置に止まり、等幅フォントでは仮想桁と一致する。core の `display_line` と仮想桁は変えない。**

1. **設定の場所**: `Direct2DRenderer` が本文の `TextFormat` を作る 1 か所（フォント名・サイズ・DPI が変わって作り直すたび）で、空白 1 個の layout（`" "`）の `widthIncludingTrailingWhitespace` を測り、その 8 倍を `SetIncrementalTabStop` に渡す。値は DIP。
2. **等幅でない guifont**: 空白の advance × 8 で割り切る（Vim の GUI も `'guifont'` が等幅でないときは桁が揃わない）。
3. **`tabstop` の設定化はしない**: ADR 0034 の固定 8 のまま（`:set tabstop=` は後続。入れるなら core の仮想桁と renderer の両方が同じ値を読む 1 経路にする）。
4. **検証**: 実機の PNG（`verify-window --open <Tab を含むファイル> --capture`）で、`a\tb` の `b` が空白 8 個の `        b` の `b` と同じ x に描かれることを設計席が見る。等幅フォント（既定）で確かめる。unit は無い（renderer は ui/win32）。
5. **ARC-001**: tab stop の値は renderer の 1 か所。core は触れない。

## 強制

- PNG の受理: **planned**（設計席が Read で見る・#131 の形）。
- fixture・unit: 不変（renderer だけ）。

## 結果

得られるもの: Tab の見た目が Vim の桁と一致し、`Ctrl-v` の矩形と `j` `k` の着地が見たとおりになる。
失うもの・残る穴: 等幅でないフォントでは近似。`tabstop` は固定 8。`list` `expandtab` は無い。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `display_line` で Tab を空白に展開し `starts` で写す | `starts` は code point の数しか持たず仮想桁の計算が要る。仮想桁の計算が core に 2 本になる（ARC-001） |
| DirectWrite の既定のまま | 8 桁と一致せず、矩形と着地が見た目とずれる |
