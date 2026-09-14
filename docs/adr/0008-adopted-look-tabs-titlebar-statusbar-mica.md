# ADR 0008 — 採用した見た目: タブのタイトルバー・モードトグルのステータスバー・Mica・茄子色と橙

- 状態: 受理
- 日付: 2026-09-15
- Issue: #5
- 影響する規則: ARC-004 / ARC-005 / ARC-011 / CPP-002 / CPP-012 / CPP-017 / QLT-013

## 文脈

施主の優先順位は速さとモダンな見た目である（SPECIFICATION 第 1 節）。見た目の判断は文章より画で決めるという施主指示（2026-09-15）に従い、
Claude の `/design` で 3 枚（ダーク・ライト・Ctrl+P）を起こし、施主が承認した（[採用案](../design/2026-09-15-look.md)）。
決めてほしいと書いた 3 点（アクセントは Ubuntu 橙 #E95420 で良いか・角丸タブに橙の下線で良いか・トグルの見せ方）は、描いたとおりで確定した。

Issue #3 の窓は `WS_POPUP` で、Aero Snap・影・最小化のアニメーションが無く、起動直後に (0,0) で一瞬見える。Mica（`DWMWA_SYSTEMBACKDROP_TYPE`）は
DWM が扱う top-level 窓の背景なので、枠を自分で消した `WS_OVERLAPPEDWINDOW` の窓の方が素直に載る。

## 決定

1. **見た目の正本は [docs/design/2026-09-15-look.md](../design/2026-09-15-look.md) の寸法と配色トークン。** `core::Palette` はトークンをそのまま持ち、UI はそれを写す
2. **窓は `WS_OVERLAPPEDWINDOW`（`WS_THICKFRAME` を含む）で作り、`WM_NCCALCSIZE` に 0 を返して OS の枠を消す。** Snap・影・最小化と最大化のアニメーションは OS のまま。
   最大化時のはみ出しは `WM_NCCALCSIZE` で縁の分だけ内側へ寄せる。`WM_NCACTIVATE` は自分で答え、非アクティブ化で OS に枠を描かせない（Folio ADR 0014 の教訓）
3. **自前のタイトルバー（40 DIP）は Mica を透かす。** `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE, DWMSBT_MAINWINDOW)` を掛け、Direct2D はタイトルバー領域をアルファ 0 で描く
   （DirectComposition の premultiplied swap chain なので透明が通る）。本文とステータスバーは不透明。`WM_NCHITTEST` は自前: タブと「＋」と窓の操作は `HTCLIENT` / `HTMINBUTTON` / `HTMAXBUTTON` / `HTCLOSE`、
   残りのタイトルバーは `HTCAPTION`、縁 8 DIP は `HTLEFT` 等
4. **編集モード（通常 / Vim）は application の状態**（`EditMode`・ARC-004）で、トグルの意図は `select_ordinary_mode` / `select_vim_mode` の 2 つ（窓は controller の状態を読んで判断しない。
   選択中の側を押しても状態は変わらない）。鍵によるトグル（FR-004）は後の縦切りで `toggle_edit_mode` を足す。UI は `EditorFrame` の値（トグルの選択側・モードの文字列）を写すだけ。
   Vim エンジンが無い間、Vim 側のモード表示は `NORMAL` 固定
5. **ヒットテストとレイアウトは純関数**（`TitleBarLayout` / `StatusBarLayout`）にして単体テストで測る。窓手続きは座標を渡して結果を写すだけ（CPP-017）。
   置き場は **`src/core`**（OS 非依存の幾何。`unit_tests` の依存は core / application だけで、カバレッジの測定ビルドもそこしか見ない）。DIP → 物理画素は `(dip × dpi + 48) / 96` の整数演算
   （`std::lround` は libm のシンボルを core の外へ出し、ARC-003 が落とす）
6. **フォント**: UI は `Segoe UI Variable Text` → `Segoe UI`、本文は `Cascadia Code` → `Consolas`。無い場合の順序は DirectWrite のフォント集合で確かめる
7. 窓は配置してから `ShowWindow` する。生成時に (0,0) で見せない
9. **Mica を透かすのに要った 2 つの属性（Issue #5 で実測）。** `WS_EX_NOREDIRECTIONBITMAP` が無いと HWND の再描画面が残ってアルファ 0 のタイトルバーに Mica が透けない。
   `DWMWA_USE_IMMERSIVE_DARK_MODE` を外観に合わせて渡さないと、ダークの本文に対して Mica が明るいまま（タイトルバー画素 (244,244,244)）になる。渡すと (42,42,42)。
   そのため `EditorFrame` は `appearance` を持ち、窓は外観が変わるたびに DWM へ伝える
8. **テーマは拡張できる形にしておく（施主の問い 2026-09-15）。** `Palette` は「テーマ 1 つ分のトークンの値型」で、組み込みテーマ（`ubuntu_aubergine` / `neutral_light`）は
   `constexpr` の表から `Palette` を返す。ui/win32 は `Palette` 以外の色の定数を持たない。利用者のテーマファイルは**まだ作らない**（設定の保存形式と版を決める
   縦切りで、adapters が版付きのファイルを読んで同じ `Palette` を作る・ARC-009）。今の段階でファイル形式を決めないのは、設定全体（テーマ・フォント・鍵）の形を 1 回で決めるため

## 強制

- ARC-004 / ARC-011（モードの所有者は application・UI は写すだけ）: **planned**（レビュー事項。`EditorController::apply` 以外に状態遷移が無いことは CNF の字句検査で見る予定）
- CPP-012: レイアウトの純関数は 60 行・引数 4 に収める。窓手続きの `switch` は OS メッセージなので `default` を許す（CPP-017）
- QLT-013: Mica の見え・Snap・DPI・タイトルバーのヒットテストは `eng/verify-window.py` と gate-proofs 第 5 節で記録する。CI は WARP で描くだけ

## 結果

- 得られるもの: 施主が承認した見た目そのもの。Snap と影と Mica が OS のまま。モードトグルという最初の「本物の状態遷移」
- 失うもの: `WM_NCCALCSIZE` / `WM_NCHITTEST` / `WM_NCACTIVATE` を自分で扱う。最大化の縁・Snap Layouts（最大化ボタンのホバー）への対応は次の縦切り
- 正直に記録しておくこと: タブは 1 本で閉じられない（複数タブは別の縦切り）。Mica は Windows 11 22H2 以降で、非対応の環境では本文の地の色で塗る（fallback の経路は実機で踏んでいない）。ハイライトの 3 色はまだ使わない。Ctrl+P は描いただけで実装しない。
  `Palette` は採用案の 12 トークンに `on_accent`（アクセントの面の上の文字）を足して 14 になった。この機は Segoe UI Variable Text と Cascadia Code を両方持つので、フォントの fallback は実行されていない。96 DPI と `WM_DPICHANGED` は単体テストでしか通っていない

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `WS_POPUP` のまま（Issue #3） | Snap・影・アニメーションが無く、Mica の載り方も素直でない。起動直後の (0,0) も同根 |
| Mica をやめて茄子色で塗る | 施主の優先順位「モダンな見た目」に反する。非対応環境の fallback としてだけ残す |
| アクセントを茄子色の明るい系統（#77216F）にする | 施主が橙を承認した。茄子色の地に橙は Ubuntu の系統として一貫する |
| タブを本文の上の別の帯にする | 縦幅を節約する施主決定 D6（タイトルバーに横並び）に反する |
| DWM の標準の窓の操作（`DwmExtendFrameIntoClientArea` でキャプションボタンだけ OS） | タブとボタンの帯を 1 つの Mica の面として描く方が見た目が揃う。ボタンの自前描画は 3 つの記号だけ |
| UI 部品を Win32 の共通コントロール（タブコントロール・ステータスバー）で作る | Direct2D と Mica の面に載らない。ADR 0002 |

## 関連

- [ADR 0002](0002-plain-win32-with-direct2d.md)・[ADR 0007](0007-first-slice-frameless-window-direct2d-line.md)
- 施主決定 D6（タブはタイトルバー）・D10（トグル）・D11（茄子色）・SPECIFICATION FR-004 / FR-005 / FR-011
