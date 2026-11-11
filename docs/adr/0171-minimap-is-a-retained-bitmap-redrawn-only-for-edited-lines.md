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
- 8 本の速さ（QLT-014 の正式なゲート）: **active**（2026-11-11 07:13〜07:20・静かな機械・枝 → 基点 `d8f33693` → 枝の 3 回とも 8 本 0 regression）。+1% の線（オフの費用）: **窓の計測では判定できない**（この機械は run ごとに 2 つの山に分かれ、+1% は分解能の外・下の記録）。根拠は独立レビューの読み（オフの 1 打鍵の道に足された仕事は、先頭で返る関数呼び出し 6 回と空の `std::optional` 1 つで、確保・写し・D2D の呼び出しは 0・見積もり 20〜60 ns・`D:/NeNeNib/outputs/20261111/review-1366-off-path.md`）で、機械の強制ではない（active と書かない・ADR 0001）。

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
- 速さの +1% の線（空の文書の 1 打鍵・200 打鍵・長い行の 1 打鍵の、オフの A/B）は設計席が静かな機械で測る。測るまで、上の「強制」の 2 行目は **未測定** と書く（測っていない強制を active と書かない・ADR 0001）。**無効にした測定の記録**（2026-11-10 20:24〜20:32・Release・枝 `b203c502` → 基点 `d8f33693` → 枝・`eng/measure-speed.py --check --repetitions 5`・ログ `D:/NeNeNib/outputs/20261110/gate-minimap-*.log`）: 測る直前の負荷は 6〜9%・ビルドの process は 0 で、3 回とも 0 regression だったが、施主が 20:00 ごろから同じ機械でほかの席を動かしていたと測った後に分かった（CLAUDE.md §2「速さを測る間は、同じ機械のほかの席とビルドを止める」の前提を満たしていない）。値は受理の根拠にしない。参考として、中央値（ms）は 枝 / 基点 / 枝 の順に 1 打鍵 0.581 / 0.625 / 0.652・200 打鍵 2.213 / 2.821 / 2.148・長い行の 1 打鍵 1.101 / 1.100 / 1.075 で、200 打鍵は試行ごとに約 2.1 ms と約 2.8 ms の2 つの山に分かれていた。静かな機械で 1 回だけ測り直す。
- **2026-11-11 の A/B の記録**（Release・枝 `b203c502` の製品と基点 `d8f33693`・ほかの席とビルドなし・測る前の負荷 0〜5%。07:10〜07:33 のどこかで施主の入力が計測の窓に混ざった可能性がある）。(1) 8 本ずつ 枝 / 基点 / 枝（`--check --repetitions 5`・ログ `D:/NeNeNib/outputs/20261111/gate-minimap-*.log`）: 3 回とも 0 regression。中央値（ms）は 起動 41.76 / 41.88 / 40.46・窓が見えるまで 31.45 / 31.51 / 31.22・1 打鍵 0.621 / 0.586 / 0.629・200 打鍵 2.790 / 2.769 / 2.793・16 MiB を開く 48.72 / 49.40 / 50.01・16 MiB で 200 打鍵 2.999 / 3.073 / 3.039・5000 件の面 1.035 / 1.052 / 1.041・長い行の 1 打鍵 1.101 / 1.106 / 1.116。(2) 打鍵の 3 本だけを 枝 / 基点 の交互に 4 周・各 20 本（`D:/NeNeNib/outputs/20261111/minimap-fine/`）: どのベンチも run ごとに 2〜3 つの山（1 打鍵 0.47〜0.52 と 0.59〜0.63・200 打鍵 2.1 / 2.8 / 3.3・長い行 0.86 と 1.10）に分かれ、20 本の中央値の差（1 打鍵 −0.2%・200 打鍵 −7.4%・長い行 +13.4%）は「どの山に何 run 落ちたか」で決まる。同じ山どうしでは 1 打鍵の低い山が 枝 0.504 / 0.514 対 基点 0.468 / 0.484、高い山が 0.608 / 0.609 対 0.617、長い行は 0.877 対 0.872 / 0.860 と 1.102 / 1.106 対 1.093、200 打鍵の低い山は 2.17 対 2.17。向きのそろった差は無く、+1%（1 打鍵で約 6 µs）はこの計測の分解能の外。合格まで測り直すことはしない（CLAUDE.md §2）。オンの費用（`minimap-single-key` `minimap-burst-200`）は基準値が無く、試用の後に採る。

## 追補（2026-11-11・#1449）: ミニマップの描画の失敗で窓を閉じない

施主が公開版 beta.9 でミニマップを出して操作していて「落ちた」（外の機械・ログなし・再現の手順は未確認）。オンの道を総当たりで読み（候補の表は `D:/NeNeNib/outputs/20261111/1449/candidates.md`）、ASan・UBSan の Debug で性質の試験と WIC の描画先の試験を流したが、**再現はしなかった**。読みで見つけた「落ちる」道は 1 つで、ミニマップの命令（オフスクリーンの bitmap への `SetTarget`・`Clear`・`FillRectangle`・`Flush`・`CopyFromBitmap`・`DrawBitmap`）が実機の GPU で失敗すると、その失敗は frame 全体の `EndDraw` の失敗（`RenderFailure::direct2d`）になり、`EditorWindow::present` は device lost 以外を作り直さずに `abandon()`（「描画できなくなったので閉じます。保存しますか。」）へ回すこと。決めたこと:

- ミニマップの命令には `minimap_tag`（`MinimapPicture.hpp`）を付け、描く先の tag は戻す。`Direct2DRenderer::draw` は `EndDraw(&tag1, &tag2)` で失敗した命令の tag を読み、失敗の読みは `minimap_flush_of(HRESULT, tag)` の 1 本（`MinimapFlush`: 成功 `kept`・device lost は tag に依らず `device_lost`・それ以外はミニマップの tag なら `minimap`・ほかは `frame`）。device lost と本文の失敗は今までどおり（作り直し・`abandon()`）。HRESULT → `RenderFailure` は `RenderFailureOf.hpp` の `render_failure_of` の 1 本にした（renderer の `classify` もこれを呼ぶ）。
- `minimap` の失敗は窓を閉じない。`render` が同じ frame を描き直し、1 度目は bitmap の道を捨てて（`MinimapPicture::abandon_bitmap`・以後その renderer は直接描く）、それでもミニマップの失敗なら面を出さない（`minimap_hidden_`・本文の枠の幅は frame のまま・面の所は描かない）。renderer を作り直したら（device lost・WARP からの差し替え）元に戻る。
- 行をずらす前の `Flush` は描く先の失敗の状態を戻すので、その `Flush` の失敗も tag で読み、ミニマップのものなら bitmap を捨てて直接描き、本文のものなら今までどおり描画の失敗として返す（本文の失敗を飲み込まない）。毎 frame の `Flush` は足していない（同じ行の数の打鍵は今までどおり流さない・レビュー L1）。
- bitmap は描く先の `GetMaximumBitmapSize` に収まるときだけ作る（`minimap_bitmap_fits`）。5,000 行の窓は 10,240 画素で、FL 10.x（8,192）や FL 9.x（2,048〜4,096）の GPU では作らずに直接描く（毎 frame 作ろうとしない）。
- 契約: `nib_tests --minimap-property`（ミニマップを出したまま打鍵・削除・切り取りと貼り付け・全選択・undo / redo・行への移動・ホイール・窓の寸法（0 × 0・縦か横だけ 0・面より狭い）・`set minimap` / `nominimap`・`syntax on|off`・タブの開く / 切り替え / 閉じる / 新規・2 枠・全面プレビューを 2 seed × 2,000 歩。frame ごとに窓が文書の中・塊の行の数・最上段が窓の中・塊の桁が今の本文から作った塊と同じ・renderer と同じ規則で積んだ写しが今の塊と同じ。わざと帯の行の数を壊すと落ちることを確かめた（4 seed の版で 4 本とも））・`nib_window_tests --minimap` の `check_random_frames`（`MinimapCache` の乱数の編集と面の寸法（幅 0・高さ 0・1 画素・狭い）とテーマの切り替えで、bitmap を経た画素が直接の画素と同じで EndDraw が成功する）・`check_failure_reading`（tag の読みと bitmap の上限）・`check_abandoned_bitmap`。
- 限界: 実機の GPU での失敗は WIC の描画先では起こせないので、描き直しの道（`redraw_without_minimap_bitmap`）は試験が通っていない（読みだけ）。落ちた原因がミニマップの外（窓の寸法の変更の `ResizeBuffers` など）なら、この直しでは止まらない。施主に聞くこと: 落ちたときに「描画できなくなったので閉じます」の箱が出たか・窓がすぐ消えたか／落ちる直前の操作（ホイール・窓の大きさ・`:set minimap`・タブ）／文書の大きさと GPU（内蔵か）。

## 追補（2026-11-11・#1472・#1371 の段 2 の 1 本目）: 面のクリックとドラッグ

施主が面をドラッグしようとした（段 1 は面の上の押しが何もせず、押して × の上で離すと窓が閉じた・#1464 で直した）。段 2 のうち決定 5 の「クリック・ドラッグ」だけを入れる。ホイール（決定 5 の 3 つ目）は段 1 から本文と同じ（面の上のホイールも `ScrollLines`）。帯の描き足し・窓の外の色の tick・枠ごとの面・`EditorFeature` の表は段 2 の残り（#1371）。

9. **押した所へ跳び、ドラッグで追従する（VS Code と同じ）。** 押した点の段（`core::minimap_pointed_row`・面の上端から 1 段 2 物理画素・面の外は負か段の数以上）を、見えている範囲の面の中央に来る「見えている最初の行」に写すのは `core::minimap_pointed_first(row, visible, total, rows)` の 1 本。面が送られる文書では、見えている範囲の面の上端は `minimap_top` の比例の送りで `first × (rows − visible) ÷ (total − visible)` 段目に来るので、その逆で上端 `row − visible ÷ 2`（`[0, rows − visible]` に切り詰め）から first を四捨五入で求める。結果は今のスクロールに依らない（同じ点を 2 回押しても動かない）・段について単調・`[1, total − visible + 1]` の中。文書が面に収まるときは押した段の行を中央に、面が見えている行より短いときは段の割合を文書に写す。
10. **意図は 1 つ・送りはホイールと同じ道。** `PointMinimap{MinimapPointing(pressed | dragged), row}`（閉じた enum）。application は `minimap_shown()` が偽なら何もせず、面の段の数は frame と同じ `core::minimap_layout`、見えている行の数は最後の frame の値（`add_minimap` が置く）。押したときにその値を覚え、ドラッグの間は押したときの値で送る（折り返す行で見えている論理行の数が変わってもドラッグが単調）。送りは `ScrollLines` と同じく `ScrollState` を置いて `clamp_scroll` で収める（第 2 の送りの道を作らない・ARC-001）。キャレット・選択・本文・undo・`.`・Vim の録画は動かさず、Vim のモードでもキャレットを寄せない（ホイールと同じ）。使う人の操作の側で、`keeps_tab_walk`・`begin_intent`・`keeps_wanted_x` のどれにも書かない（ホイールと同じ側）。全面プレビューの間は面が無く、届いても本文のマウスと同じく何もしない（`pointer_route` の ignore）。
11. **窓は押したら捕まえる。** 面の上の押し（`click_client` の中・エクスプローラーの境と面の行の後・状態バーの前）は `SetCapture` し、捕まえたときだけ覚え（`MinimapGrab`: 押したときの面の上端と最後に送った段）を持つ。覚えの次の値と送る意図は `minimap_step(message, y, panel, held)` の 1 本（`MinimapPress.hpp`・窓の試験もこれに列を流す）: 押しは `pressed`、`WM_MOUSEMOVE` は段が変わったときだけ `dragged`、`WM_LBUTTONUP` は段が変わっていれば `dragged` を送って終わり、`WM_CAPTURECHANGED`・`WM_MOUSELEAVE`・Esc（`WM_KEYDOWN`）はその位置のまま終わる。Esc はその 1 回と押し続けた繰り返しを食べる（`take_explorer_escape` と同じ形）。ドラッグの間の `WM_MOUSEMOVE` は面の送りだけで（帯の hover・境のカーソル・選択のドラッグへ流さない）、`controller_.frame()` を呼ばない。離しは `release_left` で窓の操作と境の次に引くので、× の上で離しても閉じない。押している間に面が消えたら（`:set nominimap`・全面プレビュー）`follow_minimap` がドラッグを終える。
12. **配置は 1 本。** 押しの点が面の上かは、描画と本文の枠と同じ `core::minimap_layout(explorer_frame().body, …)`（エクスプローラーの面の右の client）で引く。面は client の右端に 1 つなので、2 枠（操作中の枠の文書だけが載る）・折り返し・格子表示でも同じ配置で、送るのは操作中の枠。オフのときの費用は今のまま（`minimap_layout` と `add_minimap` は最初の 1 行で返り、押しの道は `minimap_shown()` の比べ 1 つ・打鍵の道には何も足さない）。

強制（この追補で足したもの）:

- 段 → 行の純関数は 1 本で、端・丸め・性質（文書の中・単調・同じ段で同じ行・送った後の面が押した段の中央から 1 段以内・固定 seed 4,000 件）と application の契約（押した段の行・同じ段の 2 回目・ドラッグの単調と端・キャレットと本文が不変・undo と `.` と録画・オフと全面プレビューでは何もしない）: **active**（`nib_tests --minimap-pointer`・既定実行の契約にも載せた）。`--minimap-property` の意図の列にも押しとドラッグを足した。
- 押しは捕まえ、離し・捕まえを失う・窓の外・Esc で終わり、× の上の離しで窓の操作が動かない: **active**（`nib_window_tests --minimap-press`・`minimap_step` と `caption_step` にメッセージの列を流す）。窓そのもの（`SetCapture` の成否・本物のマウス）は実物の確かめ 1 回（記録は PR）。
- 限界: 押した後に窓の大きさが変わっても、ドラッグの間は押したときの面の上端で段を数える（面の上端は帯の高さで決まり、窓の大きさでは変わらない）。DPI の切り替えはドラッグの間に起きない前提で、確かめていない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| 毎 frame 縮図を描く | 打鍵が 0.1〜0.5 ms 増える（D67） |
| 本文の全部を bitmap に | 1 GB で 2 GB の bitmap。窓で足りる |
| 字形を小さく描く | 読めないうえに `DrawGlyphRun` の数が本文と同じになる |
