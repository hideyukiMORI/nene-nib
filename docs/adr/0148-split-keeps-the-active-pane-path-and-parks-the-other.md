# ADR 0148 — split は操作中の枠を今の経路のまま持ち、もう 1 つの枠を受け身の表示として脇に置く

- 状態: 提案（設計席 2026-10-19・hide 不在の自走の日。決定 1・4・5・6 は hide の決定が要る仮置き。merge は独立レビュー・ゲート（ADR 0120 の条件）・CI・hide の試用の後・D41）
- 日付: 2026-10-19
- Issue: #680（親）・#681（計測器）・#682（状態と意図）・#683（ui と鍵）・#684（Vim の `Ctrl-w`）
- 規則: FR-003 / D6 / D12 / D22 / D40 / D45、ARC-001 / ARC-003 / ARC-004 / ARC-011 / CPP-002 / CPP-012 / QLT-014、ADR 0056 / 0059 / 0068 / 0078 / 0114 / 0120 / 0129 / 0133 / 0144
- 調査: `D:/NeNeNib/outputs/20261019/survey-split.md`（状態の欄の分類・本文の矩形の 5 か所・`BodyTextLayouts` の鍵・鍵の空き・Ex の `:q` 系・旧試作の衝突）

## 文脈

hide の指示（2026-10-19）「高速化の残を対応して、終わったら split の実装を試みて」。速さの壁は消えた（2026-10-18 の基準値: 単打 0.64 ms・長い行 1.57 ms）。採用条件は D45 / ADR 0120（単画面 S と 2 枠 P の中央値の差 ≤ max(1 ms, 0.25 S)・P の p95 ≤ 16.7 ms・固定 100 打鍵・同じ文書の 2 表示と別文書を別群で）。準備資料（`docs/design/2026-10-03-split-preparation.md`・D40）の案: 最大 2 枠・上下または左右の等分・文書タブは窓全体で 1 列を維持・選んだ文書を操作中の枠に表示・同じ文書を両枠に出せる・本文と undo は文書に 1 つ。

今の状態（調査 §1・§11）: `EditorState` はアクティブな文書の欄（本文・履歴・パス・保存位置）と表示の欄（選択・スクロール・欲しい列）を同じ層に持ち、切り替えでまとめて束 `DocumentState` に畳んで脇に置く（ADR 0056）。編集の経路が触るのはアクティブな文書だけ（CLAUDE.md §2）。controller にも表示ごとの欄（`view_geometry_` `pending_follow_` `pending_clamp_` `wanted_x_`）がある。本文の矩形は `core::body_layout` の 1 本で 5 か所から呼ばれる。`BodyTextLayouts` は幅を書式の鍵に持ち、幅が変わると保持を全部捨てる。旧試作（`test/404-split-*`）は同じ frame を 2 つの矩形に描くだけで、今の main には当たらない。

## 決定

1. **枠は最大 2 つ。左右（`vertical`）と上下（`horizontal`）の等分で、境界は動かせない。両枠の本文の幅は常に等しい。** 配置は閉じた `enum class SplitLayout { none, vertical, horizontal }`。左右の等分は `(band.width − divider) / 2` を切り捨てて両枠に同じ幅を与え、余りの 1 物理画素は境界（divider・1 DIP 以上）に足す。上下は高さを同じ規則で分け、幅は同じ。幅が等しいので `BodyTextLayouts` の書式の鍵（ADR 0114）は変えない（決定 8）。**hide の決定が要る点（仮置き）**: 枠の数 2・分け方は両方・等分固定。
2. **操作中の枠（active pane）は今の `EditorState` の経路そのもの。もう 1 つの枠（other pane）は受け身の表示 `PaneView` として `EditorState` の脇に 1 つだけ置く。** `PaneView{ tab: AutosaveId（帯の順でもパスでもない通し番号・ADR 0137 の `autosave_ids()` と同じ名指し）, caret: Offset, affinity, first_visible, first_row, horizontal, wanted_column }`。`SplitState{ layout, active_side: PaneSide { first, second }, other: std::optional<PaneView> }` を `EditorState` の窓の欄に足す（`active_side` は操作中の枠が画面のどちら側か。`first` は左または上。`with_split` は `first`、`with_pane_swapped` が反転する・2026-10-19 の実装席の指摘）（`layout == none` なら `other` は無し・両方が揃うのを不変条件にして `with_split` の 1 か所で置く）。1 枠は `SplitState{none}` で今と同じ経路を通る（旧経路や隠しフラグを残さない・ADR 0068 決定 6）。編集・Vim・IME・検索・面・保存・自動退避は今までどおりアクティブな文書だけに触れる。other の本文は読むだけ: `other.tab == active` なら `text()`、違えば脇の束 `DocumentState::text`（脇の束は不変なので読んで安全・`UnloadedDocument` は `reach_tab` の規則で分ける前に読む）。
3. **枠を移るのは `EditorState::with_pane_swapped` の 1 か所。** other の tab がアクティブと違えば、タブの切り替え（`reach_tab` → `leave_document` → `with_switched` → `enter_document`）と同じ道を通した上で、キャレットとスクロールを束の値でなく `PaneView` の値に置き替え、元のアクティブの表示を新しい other の `PaneView` に写す。同じ tab なら文書は動かさず表示の欄（選択はキャレットに畳む・`caret_affinity_`・`scroll_`・`vim_.wanted_column`）だけを入れ替える（`vim_switched_document` と同じ口）。使った順（`TabRecency`）は文書の切り替えのときだけ直す（同じ tab の枠の移動では動かさない）。controller の表示ごとの欄（`pending_follow_` `pending_clamp_` `wanted_x_`）は `leave_document` と同じく移る前に解決か破棄する。
4. **文書の選択は操作中の枠へ（D40）。** `NewTab` `OpenDocument` `SwitchTab` `WalkRecentTab` 帯のクリック・Ctrl+P・`gt` は今までどおりアクティブ（＝操作中の枠）を替え、other は不変。other が同じ tab を見ていても構わない（同じ文書の 2 表示）。**hide の決定が要る点（仮置き）**。
5. **閉じるのは「枠を閉じる」と「文書を閉じる」の 2 つの意図。** 文書を閉じる（×・Ctrl+F4・通常の Ctrl+W・`:tabclose`・最後の 1 本で窓を閉じる D22）は今のまま。閉じた文書を other が見ていれば、other はアクティブが向かう文書（アクティブを閉じたときは `close_active_tab` の隣・アクティブ以外を閉じたときはアクティブはそのまま）と同じ文書を見る（`PaneView` の表示は文書の先頭）。アクティブの文書が丸ごと置き替わるとき（無題の未変更タブへ開く `with_opened`）も、同じ tab を見る other は文書の先頭に戻す。枠を閉じる `ClosePane{which}` は文書に触れず `SplitState{none}` に戻すだけで、保存の確認は要らない。2 枠のとき Ex の `:q` `:q!` は操作中の枠を閉じ、`:wq` `:x` は保存に成功してから枠を閉じる（1 枠のときは今のとおり文書）。`:only` と Vim の `Ctrl-w o` は other を閉じる。**hide の決定が要る点（仮置き）**: `:q` が 2 枠では枠を閉じること。
6. **鍵。** 通常モードと Vim の両方で効く操作を `EditorOperation` に 4 値足す（ADR 0078・表の行・一覧にも載る）: `split_vertical`（Ctrl+\）・`split_horizontal`（Ctrl+Shift+\）・`next_pane`（F6・Shift+F6 も同じ・2 枠なので「隣」は 1 つ）・`close_pane`（Ctrl+Shift+F4）。`OperationKey` に `backslash` と `f6` を足し、`\` は JIS 配列の 2 つの物理キー（VK_OEM_5 の ¥ と VK_OEM_102 の ろ）を同じ `backslash` に写す（実機で確かめる・調査 §8）。Vim の `Ctrl-w` は engine が受ける（決定 7）。既に 2 枠のときの `split_*` は 1 行知らせて何もしない（同じ向きでも違う向きでも）。**hide の決定が要る点（仮置き）**: 鍵の割り当て。
7. **Vim の `Ctrl-w {s v w W p h j k l c q o}` は engine の 2 打鍵。** `VimSpecialKey::control_w` を足し、NORMAL / VISUAL で `input_wait` に「窓の命令の 2 打鍵目を待つ」を 1 値足す。2 打鍵目の表は `VimWindowCommand`（閉じた enum: `split_horizontal`(s, S)・`split_vertical`(v, Ctrl-v)・`next`(w, W, p, Ctrl-w)・`left`(h)・`down`(j)・`up`(k)・`right`(l)・`close`(c, q)・`only`(o)）で、engine は `VimEffect` に `VimWindowCommand` を 1 値返すだけ（`VimSwitchTab` と同じ形）。回数は読み捨てる（2 枠なので意味が無い・限界に書く）。表に無い 2 打鍵目は失敗（鍵の列を打ち切る）。`h j k l` は配置に合わないときも「隣の枠」へ移る（2 枠では隣は 1 つ・Vim の実機では動かないが、Nib は動かす・限界に書く）。controller は効果を決定 6 と同じ意図に写す。ui の `press_tab_key` が Vim で Ctrl+W を飲み込む扱いをやめ、engine へ流す。oracle の fixture は窓を観測できないので書かず、契約（`--vim-window`）で守る。
8. **frame と描画。** `EditorFrame` に `std::optional<PaneFrame> other`（`lines`・`caret`（キャレットは受け身の印）・`first_visible`・`total_lines`・`horizontal`・`display`・`grid`・`document: DocumentView`）と `SplitLayout layout` を足す。本文の配置は `core::body_layout` を `core::pane_layouts(client: LayoutRect, dpi, font_size, layout) -> PaneLayouts{ first: BodyLayout, second: optional<BodyLayout>, divider: optional<LayoutRect> }`（位置の順・引数は tidy の上限 4 つに収める）の 1 本に置き換え、どちらが操作中かは frame の `active_side` で引く（renderer は `active_side == first ? first : second` に操作中の本文を描く）、5 か所（renderer・EditorWindow 2・EditorPointer 2・`verify-window.py` の写し）を全部これにする。renderer は `draw_body(frame の active, active の BodyLayout)` を 2 回呼ぶだけで（other は `PaneFrame` から同じ `draw_line` 群を通す・IME の差し込みと `caret_rectangle_` は操作中の枠だけ）、境界を `panel_border` の色で 1 本描く。other の文字組みは同じ `TextLayoutPort` と `BodyTextLayouts` を通り、幅が等しいので書式の鍵は 1 つ。entry は同じ frame の `current_` に両枠ぶん並ぶ（同じ文書の同じ行は 1 つを共有）。格子の other は `GridCells` を other の索引から作る（索引は文書が持つので共有）。
9. **非操作枠のキャレットとスクロールの補正は core の純関数 1 本。** `core::offset_after_edit(Offset, const Edit&) -> Offset`（編集より前なら不変・後なら削除と挿入の差だけずらす・削除の範囲の中なら `at` へ畳む・ちょうど `at` の挿入は後ろへずらす）と `line_after_edit(LineNumber, …)`。controller の `replace` / `undo_edit` / `redo_edit` の 3 か所で、`other.tab == active` のときだけ `PaneView` に当てる。`first_visible` は「その行の行頭の offset に `offset_after_edit` を当てて行番号に戻す」の 1 つの規則で動かし（先頭行の行頭への改行の挿入でも中身に追従する）、`first_row` は保ち、frame の `clamp` は other にも効かせる。`Edit.restore`（ADR 0052）は操作中の枠だけ。
10. **操作中の入力は窓に 1 つで、枠を移るときは `leave_document` と同じ扱い。** IME の変換は捨て（`CancelComposition` と同じ）、入力行（Ex・検索・面）は閉じ、Vim の保留（count・pending・`input_wait`）は捨てる（`Ctrl-w` 自体が Vim の命令なので保留は無い）。INSERT は NORMAL に戻さない（Vim の `Ctrl-w` は INSERT では効かないので、通常モードの F6 だけがこの道を通る・INSERT のまま移る）。IME の候補窓の位置 `caret_rectangle_` を書くのは操作中の枠だけ。
11. **マウスは点の下の枠へ。** クリックは other の矩形なら先に `SwitchPane` を送り（決定 3）、その後で `PointText`。ホイールは点の下の枠を送る（other なら意図 `ScrollPane{lines}`・枠を移らない）。ドラッグの選択は操作中の枠だけ（other を押した時点で移るので自然に満たす）。`WM_NCHITTEST` は変えない。
12. **session には保存しない。** `session.v2` のまま。起動は常に 1 枠（準備資料の案）。配置の復元は hide の決定の後に `session.v3` で。
13. **ステータスバーとタブの帯は 1 つ。** 表示は操作中の枠の文書。other の文書名は枠の中に描かない（見た目の採用案で決める・/design・D12）。
14. **速さの口は鍵。** ADR 0120 の 2 枠 P は、製品の exe に Ctrl+\（左右）か Ctrl+Shift+\（上下）を送って作る。測定用の起動引数は足さない。計測器は `eng/measure-speed.py` の `OPTIONAL_BENCHES` に `split-single-key-100`（S）と `split-pane-key-100-{same,other}-{vertical,horizontal}`（P）を足し、1 打鍵ずつ次の提示を待って 100 回（ADR 0120 決定 5）、中央値（50/51 番目の平均）と p95（nearest rank）と 16.7 ms 超の件数を出し、S と P の群別の判定を `--check` でなく専用の `--split-verdict` で出す（基準値 `perf-reference.json` には載せない・ADR 0138 の形）。

## 1 打鍵の道の費用

2 枠の 1 打鍵で増えるのは: other の `PaneFrame` の行の切り出し（見えている行だけ・同じ文書なら同じ `LineView` の作り直し）、other の文字組み（同じ文書の同じ行なら保持を共有・別文書なら別の entry）、`draw_body` 1 回分、`offset_after_edit` 1 回。減らさないもの: 保持の鍵・1 回の `Present`。目標は ADR 0120 の条件で、ゲートは #681 の計測器。

## 限界（分かっていて今回は採らない）

- 3 枠以上・入れ子・境界のドラッグ・枠ごとのタブ列・配置の永続化・同時 2 つの IME（準備資料のまま）。
- Vim の `Ctrl-w` の回数と `Ctrl-w {N}`・`Ctrl-w Ctrl-s`（`VimSpecialKey` に Ctrl-s が無い・#687 のレビュー R1）・INSERT の `Ctrl-w`（語の削除）・`Ctrl-w =` `+` `-` `<` `>`・`Ctrl-w n`（新しい枠に空の文書）・`Ctrl-w t` `b` `x` `r`。`h j k l` は配置を見ず「隣」。
- other の枠に文書名を出さない（見た目の案の後）。
- 削除の範囲の中にあった other のキャレットは `at` へ畳まれ、その削除を undo しても元の位置には戻らない（戻した本文の後ろ）。
- 同じ文書の 2 枠で枠を移るときも `leave_document` / `enter_document` を丸ごと通す（undo の単位も閉じる）。
- 検索の `n` `N` と incsearch は操作中の枠だけ。`hlsearch` の面は両枠（`lines` の一致は同じ関数で作る）。
- other が `UnloadedDocument` を指すことは無い（分けるときと文書を閉じた後の隣は `reach_tab` を通す）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 文書の表（ID）と表示の列へ全面移行（準備資料の案） | 編集・Vim・IME・保存・自動退避の全経路が「アクティブな文書」を前提に書かれており（ADR 0056）、2 枠のために全部を書き換えるのは限定試作の範囲を越える。受け身の other で同じ振る舞いを得られ、1 枠は今の経路のまま |
| `EditorController` を 2 つ持つ | 本文・undo・レジスタ・保存位置の二重化（ADR 0068 の却下） |
| `BodyTextLayouts` の書式の鍵から幅を外す | 幅を等しくすれば要らない（決定 1）。ADR 0114 を変えない |
| 測定用の起動引数で 2 枠にする | 製品に測定だけの口が増える。鍵で足りる |
| Vim の `Ctrl-w` を ui で受ける | 2 打鍵目の待ちは engine の `input_wait` の形が既にある（`f` `r` `q` `"`）。ui に Vim の状態機械を作らない |
| 枠を移るときに INSERT を NORMAL に戻す | Vim の `Ctrl-w` は INSERT で効かず、通常モードの F6 はモードに触れない方が素直 |

## 検証

- core: `pane_layouts` の等分と境界（奇数幅の余りは境界へ・両枠の幅が等しい・上下は高さ）・`offset_after_edit` の表（前・後・中・ちょうど `at`・undo で元に戻る）・`VimWindowCommand` の表と `input_wait`。
- application: 契約 `--split`（分ける → other が同じ tab・枠を移る → 選択とスクロールが入れ替わる・別の tab を開く → active だけ替わる・同じ文書を編集 → other のキャレットが補正される・undo でも・文書を閉じる → other が隣へ・枠を閉じる → none・`UnloadedDocument` を指さない）・`--vim-window`（`Ctrl-w` の 2 打鍵と失敗）・`verify_quiet_paths`（分ける・移るの道でファイルに触れない）。
- ui: `verify-window.py --split`（分けた直後の 2 枠・左で打つ → 右にも反映・F6 でキャレットの印が移る・クリックで移る・閉じて 1 枠・PNG を `compare-frames.py` の regions `body0` `body1` で）。
- 速さ: #681 の計測器で S / P の群別（ADR 0120）・既存の 8 本の `--check` で退行なし（静かな機械）。
