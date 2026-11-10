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
- 8 本の速さと +1% の線（オフの費用は早い return の分岐 1 つ・決定 1 の追補）: **未測定**（QLT-014・オフの A/B は設計席が静かな機械で測る。測るまで active と書かない・ADR 0001）。

## 結果

得られるもの: 文書の形と今の位置が右端で分かり、クリックで跳べる。巨大な文書でも費用は窓の大きさ。
失うもの・残る穴: 本文の 80 DIP が狭くなる（オフが既定）。窓の外の色は tick で埋まるまで `muted`。

## 追補（2026-11-10・#1017 の段 1）: 入れた範囲・段 2 に残した範囲・オフの道の形

段 1 で入れたもの:

- 設定 `minimap` は `features` の名前（`core::minimap_feature`）。`:set minimap` / `nominimap` / `minimap?` は `ExResult.cpp` の `minimap_option`（`relativenumber` と同じ `switch_request`）で、補完の候補の表に `set minimap` `set nominimap` を足した（probe の写しも同じ並び）。`EditorFeature` の表（ADR 0170 の決定 4）はまだ無いので、名前は core の 1 定数で持ち、表ができたら表の 1 行に移す。
- core: 1 行の塊 `minimap_row`・面の最上段 `minimap_top`（VS Code と同じ比例の送り）・窓 `minimap_window`（最上段の周りの前後 2,000 行・含んでいれば前の窓のまま・編集で 5,000 行を超えたら作り直す）・帯の合わせ `minimap_band_joined`・面の配置 `minimap_layout`（決定 2 の値のまま: 80 DIP・1 行 2 物理画素・100 桁）。
- application: `MinimapCache`（窓の行ごとの塊と行頭の字句の状態）。編集は replace・undo・redo の 1 か所から受け、変わった行と、行頭の状態が前と揃うまでの下の行だけを読み直して帯を積む。frame の `minimap` は窓の塊の借り（`shared_ptr<const MinimapRows>`）と、前の frame からの帯（`base` と `band`）だけで、本文の全部は写さない。窓を作り直すときは窓の手前を `syntax_backfill_lines` だけ遡って状態を求め、窓の 4,000 行を一度に字句器へ通す（決定 4 の「見えている範囲の外の色は tick で埋める」は段 2）。4 KiB を超える行は先頭だけを読み、色を付けない（巨大な行の窓と同じ限界）。
- application の帯（レビュー V-1366 の L1）: 行の増減の無い編集（いちばん多い打鍵）は、変わった行の要素だけをその場で書き換える（窓の 4,000 要素を動かさない）。行の増減のある編集だけが列を入れ替える。
- 見えている範囲（レビュー L2）: 面の大きさと最上段の送りに使う「見えている行の数」は、frame の行（格子なら画面行）の最後の論理行から数えた論理行の数。折り返す行があっても面が下へ伸びない。
- 全面プレビュー（ADR 0155 改訂 15・レビュー L6）: 全面の間は本文が隠れているので面を出さない。`EditorController::minimap_shown()` は「設定がオン かつ 全面でない」で、frame の `minimap` も同じ条件（`add_minimap` は `add_other_pane` の後に呼ばれ `full_preview` を見る）。窓は出入りで寸法を送り直す（`follow_minimap`）。
- ui: `MinimapPicture`（bitmap は面の幅 × 512 行刻みの高さ）。frame ごとに面の地・`DrawBitmap` 1 回・見えている範囲の面（`accent` を brush の不透明度 10% で）。帯は同じ行の数なら上書き（命令を流す `Flush` も呼ばない）、行の増減は `Flush` の後に下の行を `CopyFromBitmap`（scratch の bitmap を経る）でずらし、減った後の下の残りを消す。行の塊・テーマ・面の幅が変われば窓ごと描く。
- 構文の色（レビュー M1）: 塊の色は塊ごとに `core::syntax_color(body, role)`（`default` の無い `switch` の 1 本）で引き、役割を色の表の位置に写さない。`SyntaxRole` に値を足せば `syntax_color` でコンパイルが落ちる（CPP-002）。UI の色の表は地・面・`muted` の 3 つだけ。テーマの変わり目は UI の 3 色と `SyntaxPalette` の値の一致で見る。
- 契約: `nib_tests --minimap`（core と application。塊の結合文字・制御文字 `^M` の 2 桁・面の上の点がどの本文の枠の band にも入らないこと・ブロックコメントの途中の編集で下の行を読み直すこと・undo / redo の帯・4 KiB を超える行・折り返す行の見えている範囲・全面プレビューの間は出さないこと）・`nib_window_tests --minimap`（WIC の描画先で bitmap を経た画素と直接の画素の一致・2 frame 目の使い回しと `DrawBitmap` 1 回・帯の描き直し（同じ行の数・増える・減る）と `Flush` の回数・base の違う frame は窓ごと・テーマと面の幅で作り直す・どの `SyntaxRole` の値の塊も `syntax_color` の色で描くこと）。任意のベンチ `minimap-single-key` と `minimap-burst-200`（C++ の文書・自分の profile に `features=minimap`）を足した（測るのは設計席）。
- 面の上の押下（レビュー L4）: 窓の `click_body` は操作中の本文の枠の band の中だけで `place_caret` し、面は band の外なので、**面の上の押下は何もしない**（キャレットも選択も動かない・`EditorWindowPointer.cpp`）。契約は `--minimap` の「面の上の点はどの本文の枠の band にも入らない」（1 枠・左右・上下の 2 枠のどちらの側でも）。本文の上で押してから面へドラッグしたときは、本文の点として行末に寄る見込み（実機で確かめていない）。

段 2 に残したもの（Issue「ミニマップ 段 2」・親 #1017）: クリック・ドラッグ・ホイール（決定 5・段 1 の面の上の押下は何もしない）、スクロールで窓を出たときの帯の描き足し（段 1 は窓ごと作り直す）、窓の外の構文の色の静かな tick、2 枠の枠ごとのミニマップと文書ごとの bitmap（段 1 は client の右端に 1 つ・アクティブな文書だけ）、`EditorFeature` の表（ADR 0170 の決定 4）ができたら `minimap_feature` を表の 1 行に移すこと、DPI とテーマの切り替えの窓の試験（`nib_window_tests` は面の幅とテーマで作り直すことを WIC で確かめた）。

オフの道の形（決定 1 の読み替え・設計席の判断・レビュー V-1366）:

決定 1 の「オフで打鍵の道に分岐を足さない」は、**オフの費用を早い return の分岐 1 つ（予測が当たる）に留める**、と読み替える。間接呼び出し（メンバー関数ポインタ・描く関数の表）で字面の「分岐なし」を守る形は採らない。字面を守る代わりにオフでも面の配置（`minimap_layout` の中の `body_layout`）を毎 frame 計算することになり、趣旨（オフの費用を 0 に近づける）と逆向きで、機能のオフの形がこのリポジトリで 2 本になる（ほかの機能は `explorer_view`・`follow_explorer`・`forget_syntax_before`・`language_server_edit` の早い return）。

- application: 編集の 3 か所（`replace`・`undo_edit`・`redo_edit`）は `note_minimap_edit` を、frame の 1 か所（`finish_frame`）は `add_minimap` を呼び、どちらもオフなら最初の 1 行（`if (!minimap_shown_) return;`）で返る。`minimap_shown_` を置くのは設定が変わったとき（`persist_settings`）と起動の 1 か所の `sync_minimap` だけ。オフの frame の `minimap` は既定の値なし（割り当て・写し・行の走査は 0）。
- renderer: `minimap_layout` は `shown` が偽なら最初の 1 行で返り（本文の配置を計算しない）、`draw_minimap` も `frame.minimap` が無ければ最初の 1 行で返る（`draw_explorer` と同じ形）。
- 窓: 本文の枠の幅は同じ `minimap_layout` の 1 本。出した・消した（全面プレビューの出入りも）ときに寸法を送り直すのは `follow_minimap`（`follow_explorer` と同じ形・前の値との比べ 1 つ）で、字の比べ（`FontKey`）には混ぜない。
- 速さの +1% の線（空の文書の 1 打鍵・200 打鍵・長い行の 1 打鍵の、オフの A/B）は設計席が静かな機械で測る。測るまで、上の「強制」の 2 行目は **未測定** と書く（測っていない強制を active と書かない・ADR 0001）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 毎 frame 縮図を描く | 打鍵が 0.1〜0.5 ms 増える（D67） |
| 本文の全部を bitmap に | 1 GB で 2 GB の bitmap。窓で足りる |
| 字形を小さく描く | 読めないうえに `DrawGlyphRun` の数が本文と同じになる |
