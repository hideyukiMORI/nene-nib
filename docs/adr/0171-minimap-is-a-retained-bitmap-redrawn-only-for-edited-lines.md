# ADR 0171 — ミニマップは文書ごとの bitmap に 1 回描いて持ち、編集した行の帯だけ描き直す

- 状態: 受理（施主決定 D70・FR-030・2026-10-28。既定オフ。実装は 1〜2 席・受理は D41 の形）
- 日付: 2026-10-28
- Issue: #1017・計画 `docs/plans/2026-10-28-editing-features-roadmap.md` §3.5
- 規則: FR-030、ARC-001 / ARC-003 / ARC-004 / ARC-011、CPP-002 / CPP-005 / CPP-011、QLT-014、ADR 0007（Direct2D の 1 frame 1 回）/ 0008（色のトークン）/ 0121（改行の索引）/ 0148（`pane_layouts`）/ 0155 の追補 #907（プレビューの bitmap の形）/ 0156（構文の区間）/ 0162（面の配置の縮め方）

## 文脈

ミニマップは本文の縮図を右端に置く機能で、素直に作ると打鍵ごとに数万の矩形を描いて 1 打鍵が 0.1〜0.5 ms 増える（D67 の線を超える）。Nib にはプレビューの枠をオフスクリーンの `ID2D1Bitmap1` に描いて `DrawBitmap` 1 回で貼る形（#907）がある。

## 決定

1. **設定で切る・既定オフ。** `minimap`（`features` の欄・ADR 0170）。`:set minimap` / `nominimap`。オフで打鍵の道に分岐を足さない。
2. **配置。** 本文の右端に幅 80 DIP（エクスプローラーと同じく `pane_layouts` の `client` を右から縮める・2 枠のときは右の枠の右）。1 行 = 2 px（物理）・1 文字 = 1 px の幅（最大 100 文字）。見えている範囲は `accent` の 10% の面で覆う（アルファは brush の opacity・色のトークンは足さない）。
3. **描く中身。** 字形は描かない。文字の種類ごとの塊: 空白は無し・構文の色の区間（`LineView.colors` と同じ役割 → `SyntaxPalette` の色）・区間の無い文字は `muted`。Tab は 8 升。全角は 2 px。1 行 1 本の `FillRectangle` の列（区間ごとに 1 つ）。
4. **bitmap を文書ごとに持ち、編集した行の帯だけ描き直す。** `MinimapPicture`（文書の `shared_ptr` の同一性・DPI・テーマ・幅・窓の先頭の行の鍵）。本文の全部を描くのではなく、**窓**（見えている最初の行の前後 2,000 行・8,000 px）を bitmap に持ち、スクロールで窓から出たら帯を描き足す（`CopyFromBitmap` でずらし、空いた帯だけ描く）。編集は `replace` の 1 か所から「変わった行の範囲と行の増減」を受け（ADR 0166 の決定 2 と同じ 1 本）、その帯だけ `SetTarget(bitmap)` で描き直す（行の増減は下の帯をずらす）。打鍵ごとの frame は `DrawBitmap` 1 回 ＋ 見えている範囲の面 1 つ。構文の色は見えている範囲の外では「遅延」（窓の中の行は構文の覚え `SyntaxStateCache` の遡りの規則で色を引く・覚えが無い行は `muted` で描き、frame の後の `PreviewTick` と同じ静かな tick で 200 行ずつ色を埋める・限界）。
5. **操作。** クリックでその行へ（`PointMinimap{y}` → 行番号 → `PlaceCaret` 相当の `ScrollLines`・キャレットは動かさない・VS Code と同じ）。ドラッグで連続して送る。ホイールは本文と同じ。
6. **費用。** 打鍵ごと: `DrawBitmap` 1 回 ＋ 編集した行の帯（1〜2 行 × 100 矩形・数 µs）。スクロール: 帯の描き足し（1 行 2 px・60 行で 120 px の帯）。巨大な文書（16 MiB・1 GB）も窓が 2,000 行なので同じ。任意のベンチ `minimap-single-key` と `minimap-burst-200`・空の文書の 1 打鍵の A/B（オンで「小」の見込み）。
7. **契約。** `nib_window_tests --minimap`（WIC の描画先で bitmap を経た画素と直接描いた画素の一致・編集の帯の描き直し・窓の移動・2 枠・DPI・テーマの切り替えで作り直す・`DrawBitmap` の回数 1）。application: `--minimap`（窓の行の範囲・編集の帯の範囲・クリックの行）。
8. **限界（後続）。** 字形の縮図（文字の形は描かない）・検索の当たりの印・変更の印（ADR 0166）の反映・幅の設定・窓の外の構文の色の即時。

## 強制

- 打鍵ごとに bitmap を作り直さない（`DrawBitmap` 1 回）: **active**（窓の試験の回数の照合）。
- 8 本の速さと +1% の線（オフで分岐なし）: **active**（QLT-014・PR 本文の A/B）。

## 結果

得られるもの: 文書の形と今の位置が右端で分かり、クリックで跳べる。巨大な文書でも費用は窓の大きさ。
失うもの・残る穴: 本文の 80 DIP が狭くなる（オフが既定）。窓の外の色は tick で埋まるまで `muted`。

## 追補（2026-11-10・#1017 の段 1）: 入れた範囲・段 2 に残した範囲・オフで分岐なしの示し方

段 1 で入れたもの:

- 設定 `minimap` は `features` の名前（`core::minimap_feature`）。`:set minimap` / `nominimap` / `minimap?` は `ExResult.cpp` の `minimap_option`（`relativenumber` と同じ `switch_request`）で、補完の候補の表に `set minimap` `set nominimap` を足した（probe の写しも同じ並び）。`EditorFeature` の表（ADR 0170 の決定 4）はまだ無いので、名前は core の 1 定数で持ち、表ができたら表の 1 行に移す。
- core: 1 行の塊 `minimap_row`・面の最上段 `minimap_top`（VS Code と同じ比例の送り）・窓 `minimap_window`（最上段の周りの前後 2,000 行・含んでいれば前の窓のまま・編集で 5,000 行を超えたら作り直す）・帯の合わせ `minimap_band_joined`・面の配置 `minimap_layout`（決定 2 の値のまま: 80 DIP・1 行 2 物理画素・100 桁）。
- application: `MinimapCache`（窓の行ごとの塊と行頭の字句の状態）。編集は replace・undo・redo の 1 か所から受け、変わった行と、行頭の状態が前と揃うまでの下の行だけを読み直して帯を積む。frame の `minimap` は窓の塊の借り（`shared_ptr<const MinimapRows>`）と、前の frame からの帯（`base` と `band`）だけで、本文の全部は写さない。窓を作り直すときは窓の手前を `syntax_backfill_lines` だけ遡って状態を求め、窓の 4,000 行を一度に字句器へ通す（決定 4 の「見えている範囲の外の色は tick で埋める」は段 2）。4 KiB を超える行は先頭だけを読み、色を付けない（巨大な行の窓と同じ限界）。
- ui: `MinimapPicture`（bitmap は面の幅 × 512 行刻みの高さ）。frame ごとに面の地・`DrawBitmap` 1 回・見えている範囲の面（`accent` を brush の不透明度 10% で）。帯は同じ行の数なら上書き、行の増減は下の行を `CopyFromBitmap`（scratch の bitmap を経る）でずらし、減った後の下の残りを消す。行の塊・テーマ（色の表）・面の幅が変われば窓ごと描く。
- 契約: `nib_tests --minimap`（core と application）・`nib_window_tests --minimap`（WIC の描画先で bitmap を経た画素と直接の画素の一致・2 frame 目の使い回しと `DrawBitmap` 1 回・帯の描き直し（同じ行の数・増える・減る）・base の違う frame は窓ごと・テーマと面の幅で作り直す）。任意のベンチ `minimap-single-key`（C++ の文書・自分の profile に `features=minimap`）を足した（測るのは設計席）。

段 2 に残したもの: クリック・ドラッグ・ホイール（決定 5・面の上の押下はまだ本文の点として扱われる）、スクロールで窓を出たときの帯の描き足し（段 1 は窓ごと作り直す）、窓の外の構文の色の静かな tick、2 枠の枠ごとのミニマップ（段 1 は client の右端に 1 つ・アクティブな文書だけ）、DPI とテーマの切り替えの窓の試験（`nib_window_tests` は面の幅とテーマで作り直すことを WIC で確かめた）。

オフで分岐なしの示し方（決定 1・強制の 2 行目）:

- application: 編集の 3 か所（`replace`・`undo_edit`・`redo_edit`）と frame の 1 か所（`finish_frame`）は、メンバー関数ポインタ `minimap_edited_` / `minimap_framed_` を 1 回呼ぶだけ。オフの間は何もしない関数（`skip_minimap_edit` / `skip_minimap_frame`）を指し、差し替えるのは設定が変わったとき（`persist_settings`）と起動の 1 か所の `sync_minimap` だけ。frame の `minimap` は既定の値なし。
- renderer: 面の幅は `minimap_layout` が `shown` を掛けて 0 にし、描く関数は `frame.minimap.has_value()` で引く 2 行の表（`skip_minimap` / `draw_minimap`）の間接呼び出し。
- 窓: 本文の枠の幅は同じく `minimap_layout` の掛け算で、出した・消したときに寸法を送り直す判定は既にある字の比べ（`FontKey`）の 1 欄にした（新しい比べは足していない。ただし tuple の比べが 1 欄ぶん長くなる）。
- 速さの A/B（空の文書の 1 打鍵）は設計席が静かな機械で測る（この席は測っていない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 毎 frame 縮図を描く | 打鍵が 0.1〜0.5 ms 増える（D67） |
| 本文の全部を bitmap に | 1 GB で 2 GB の bitmap。窓で足りる |
| 字形を小さく描く | 読めないうえに `DrawGlyphRun` の数が本文と同じになる |
