# ADR 0019 — 表示領域は engine への入力で、画面移動は選択と先頭行を一緒に返す

- 状態: 受理
- 日付: 2026-09-19
- Issue: #58
- 影響する規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-011 / CPP-001 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / CPP-017 / QLT-008 / QLT-009 / QLT-013 / QLT-014 / CNF-010

## 文脈

ADR 0018 までの `vim_step(state, text, selection, key)` は引数が上限の 4 つで、画面の先頭行も高さも知らない。画面の状態は application の `EditorState::scroll()`、選択は `EditorState::selection()` が所有する。`H M L`、`Ctrl-d/u/f/b`、PgUp / PgDn を再現するために engine が画面の値を読む必要があるが、VimState に複製してはならない。

既存の `move_caret_to` と `settle_vim_caret` は `follow_caret` を呼ぶ。スクロールの上限は「最後の画面を本文で埋める」位置であり、Vim が末尾の空白を許す先頭行を単独で設定しても直後か次の鍵で戻ってしまう。

Vim 9.1 の既存 oracle（`-es`）では `set lines=10/20` を変えても `winheight(0)` が 24 のままであった。`:resize` は高さを変えたが、`winsaveview().topline` と `line('w0')` / `line('w$')` が矛盾し、画面の位置を正しく測れない。画面に依存する fixture は、この実行条件から生成してはならない。

## 決定

**engine は借用する本文・選択と、application から作った表示領域を入力として受ける。画面移動は `VimNavigate` という 1 つの効果で選択と先頭行を返し、既存の状態の所有者へ反映する。**

1. core に `VimViewport{LineNumber first_visible, size_t visible_lines}` と `VimEditorView{const TextBuffer& text, const Selection& selection, VimViewport viewport}` を置く。後者は 1 回の鍵処理の間だけ使う読み取り専用の借用であり、状態を保存しない。唯一の入口を `vim_step(const VimState&, const VimEditorView&, VimKey)` に置き換え、旧シグネチャを残さない（CPP-012）。
2. controller は `ScrollState` と `Selection` から view を都度作る。UI は既存の `VisibleLines` とキーの意図を送り、画面移動の計算を持たない（ARC-004 / ARC-011）。
3. `VimNavigate{Selection selection, LineNumber first_visible}` を `VimEffect` に追加する。engine が NORMAL では選択を畳み、VISUAL では anchor を保つ。移動後の caret は本文内かつ表示領域内である。controller は選択とスクロールを一緒に置き換える。単独のスクロール効果の列や、UI 側の状態補正は足さない。
4. `first_visible_within` の第 4 引数に閉じた `ScrollExtent`（`filled_viewport` / `last_line`）を追加する。通常編集は従来どおり末尾の画面を埋め、Vim は最終行までを先頭にできる。モードから extent を選ぶのは controller の 1 か所だけで、カーソル追従・ホイール・resize が同じ境界を使う。通常編集へのトグル時にも同じ境界へ寄せる。`settle_vim_caret` を効果別に迂回せず、画面内の caret なら先頭行を保つ。
5. `H M L` は既存の motion の閉じた集合に追加し、NORMAL・VISUAL・保留中の `d c y` が同じ移動先を使う。`H` / `L` は画面の上端／下端からの回数、`M` は画面内の本文の中央へ移る。既定の `startofline` に従って最初の非空白へ寄せる。operator が受ける範囲は行単位である。
6. `Ctrl-d/u` は半画面、`Ctrl-f/b` と PgDown / PgUp は 1 画面の移動として扱う。半画面の明示回数は以後の `Ctrl-d/u` にも残る window-local な値であり、現在の先頭行とは別の意味である。`VimState` に省略可能な行数として持ち、未指定時は表示行数の半分（最小 1）から導く。鍵の完了・Esc・モード切替・ファイルを開く操作では明示値を失わず、表示行数が実際に変わったときだけ未指定へ戻す。同じ高さの通知では消さない。
7. 画面移動の Ctrl キーは NORMAL / VISUAL が対象。INSERT の Ctrl-d/u は別の編集操作なのでスクロールへ流さない。PgUp / PgDn は INSERT でも画面を移動し、既存の INSERT 内移動と同じく undo の区切りを閉じる。通常編集のキー割り当ては変えない。
8. 新しい鍵・motion・効果は閉じた型と表で扱い、`default` を足さない。新しい画面移動の回数計算は、本文と画面の範囲に制限してから加減算し、巨大な回数でループ・桁あふれを起こさない。
9. 本文・レジスタ・undo の変更は既存の操作経路だけで行う。純粋な画面移動は本文とレジスタを変えない。矩形選択・`virtcol`・ドラッグ選択・折り返し・横スクロール・設定永続化はこの Issue の範囲に含めない。

### 境界の実測による補足

`Ctrl-d/u` の明示回数は表示高までに制限して `'scroll'` へ記憶する。高さ 10 で `999<C-d>` は 10 行の移動であり、本文末尾への移動ではない。ただし、すでに `Ctrl-d` で最終行、`Ctrl-u` で先頭行にいる場合は何も変えず、非空白への桁補正も `'scroll'` の更新もしない。

ページ移動のカーソルを「旧カーソルを新しい画面内へ寄せる」だけで求めない。高さ 10・先頭 6 行の画面では、下端 15 行からの `Ctrl-f` は先頭とカーソルが 14 行、上端 6 行からの `Ctrl-b` は先頭 1 行・カーソル 7 行になる。最終行 30・先頭 21 行からの `Ctrl-f` は、最終行 30 を画面先頭にして末尾の空白を残す。画面先頭がすでに 1 行の `Ctrl-b`、最終行の `Ctrl-f` はカーソルの行・桁も動かさない。

回数つきのページ移動は、境界へ達した後の no-op を含む繰り返しと同じ結果を、回数に比例するループなしで求める。高さ 10・先頭 21 行からの `999<C-b>` は画面先頭が 21 → 13 → 5 → 1 と変わり、最後の有効な移動でカーソルが 6 行になる。新しい画面の末尾 10 行へ一律に置く動作ではない。小さい表示高を含む各鍵の結果は独立した oracle fixture で再生する。

### 自動追従と明示したスクロールの区別

最終状態だけを比較すると、VISUAL のページ移動が誤っていても、その後の yank で正しい画面へ戻ったように見える。高さ 10・先頭 6 行・カーソル 10 行では、`v<C-f><Esc>` と `V<C-f><Esc>` はともに先頭・カーソルが 14 行になるが、`v<C-f>y<Esc>` はカーソル 10・先頭 6 行になる。ページ移動を抑制して後者だけへ合わせてはならない。中間の画面位置と選択両端も独立したテストにする。

画面外へ戻るカーソルの追従は yank に特有ではなく、回数つきの `j/k` でも同じである。既存の `first_visible_for_caret` に閉じた `ScrollFollow{minimal, vim}` を第 4 引数として足し、通常編集は従来の minimal、Vim の自動追従は vim を controller の 1 か所で選ぶ。yank の効果経路は増やさない。半画面・ページ移動の明示した先頭行へ caret を収める計算は minimal を使い、画面内にいる限り自動追従が先頭行を上書きしない。

固定した Vim 9.1・折り返し無し・`scrolloff=0` の実測では、高さを `h >= 1`、上側の閾値を `u = max(h - 2, 0) / 2`（整数除算）とすると、画面の上へ `u` 行以上外れる移動は上側中央（カーソルより `(h - 1) / 2` 行上）へ、下へ `h - u` 行以上外れる移動は下側中央（カーソルより `h / 2` 行上）へ画面を置く。閾値未満は必要な行数だけ動く。先頭行は 1 以上とし、最終的な本文の境界は `ScrollExtent` が適用する。

確認した境界は高さ 9 / 10 / 11 で、上へ 3 / 4 / 4 行、下へ 6 / 6 / 7 行。閾値の直前と一致をテストし、回数に比例するループも、`first + height` の桁あふれも使わず差分から求める。

自動追従だけでは本文末尾に新たな空白を作らない。30 行・高さ 10・先頭 6 行・カーソル 15 行からの `99j` はカーソル 30・先頭 21 行、`14j` はカーソル 29・先頭 21 行となる。一方、明示したページ移動で先頭 30 行となった後の `k` はカーソル・先頭ともに 29 行で、空白を保つ。controller の `follow_caret` は、現在の先頭が最後の埋まった画面以内なら自動追従結果を `filled_viewport` へ収め、すでに末尾空白があるときはモードが選ぶ既存の extent を保つ。明示した `VimNavigate` は先頭と選択を先に適用するので、この同じ経路で空白が残る。

## 検証と強制

- 新しい画面移動を含む単体テスト、329 件の oracle の再生と生成の再現性、実機キー経路を確認した。条件・実数・未確認の環境は `docs/quality/gate-proofs.md` の 5-j に記録する。最終 HEAD のフルゲートと CI は PR の検証結果を正とする。
- 既存の 256 fixture と生成物の一致（CNF-010）、引数・複雑度の上限、分岐カバレッジ 90%、性能の基準値と閾値は維持する。
- `-es` で画面が測れないことを、Vim の仕様の根拠に置き換えない。正常な画面状態を持つ端末実行で測れる場合は同じ oracle 生成器内で扱う。環境上測れない条件は help による決定的テストと未確認の記録に分け、差分検証済みとは書かない（QLT-013）。

### oracle の表示領域入力（実測で確定した方法）

`-es` を外した Vim 9.1 の通常端末モードでは、Python の標準 `subprocess` のパイプだけで画面高・先頭行・末尾行が整合し、反復結果も一致した。PTY や新しいライブラリ、利用者のキーボード操作は必要ない。通常端末モードには `--not-a-term` を付け、pipe が意図した入出力先であることを Vim へ伝える。これは同梱 help `starting.txt` にある、警告と 2 秒の待機を省くオプションであり、初期・最終の整合検査は省かない。追加前後で通常・VISUAL のページ移動と yank 後の値が一致した。

同じ `eng/vim-oracle.py` と `tests/vim/fixtures.json` に任意の `viewport` 入力を足す。フィールドは `visible_lines` / `first_visible` / `line` / `column`（column は UTF-8 のバイトで 1 始まり）。入力の無い既存 fixture は従来の `-es` を保つ。入力があるときだけ通常端末モードで `resize` と `winrestview` を使い、本文が折り返さないこの縦切りの画面を `nowrap` で明示する。設定を既存の全 fixture へ追加しない。

初期の画面高・カーソル・表示先頭が指定値と等しいことと、最終の `winsaveview().topline` / `line('w0')` / `line('w$')` が整合することを確認してから期待値にする。最終の表示先頭と `'scroll'` の値も保存する。生成物の `VimFixture` には `optional<VimViewportFixture>` を加え、旧 fixture は空、新 fixture は入力 4 値と `expected_first_visible` / `expected_scroll_lines` を持つ。生成物の SHA-256 と本数は既存の CNF-010 が引き続き検査する。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| `VimState` に表示中の先頭行と高さを保存する | `ScrollState` と同じ事実の所有者が 2 つになる |
| `vim_step` に第 5 引数を足す | CPP-012 に反する。現在の編集対象の view にまとめる |
| UI が PgUp / PgDn を通常編集の `MoveCaret` に写す | Vim の回数・選択・画面の重なりの判断が engine の外へ分かれる |
| スクロールだけを変更して既存の境界を使う | 末尾の空白を通常編集の上限が消してしまう |
| `VimNavigate` だけ `settle_vim_caret` を飛ばす | 次の通常の移動で同じ上限に戻る。境界の意味を明示して直す |
| `set lines=N` の成功を画面高が変わった証拠とする | 実測で `&lines` と `winheight(0)` が一致しなかった |

## 関連

ADR 0005（Vim を oracle にする）・ADR 0009（選択とスクロールの所有）・ADR 0012（純粋な engine）・ADR 0015（operator の範囲）・ADR 0018（VISUAL）。Vim の参照は固定した 9.1 の同梱 help の `scroll.txt` / `motion.txt` / `options.txt` と、同じ実行ファイルの実測。

## 追補（2026-10-20・#411）: `yH` `yM` のキャレットと回数つきの 1 画面の端

上へ動いた行単位の `y` は `gg` `G` と同じく `H` `M` `L` でも着地（最初の非空白）へキャレットを置き（Vim の `op_yank` は範囲の始まりへ置く）、回数つきの Ctrl-f / Ctrl-b が途中の周で端に当たったときは Vim の `onepage` が FAIL を返して `beginline` を飛ばすので、キャレットは画面へ入る行の同じバイトの桁を保つ（回数を 1 つ減らしても先頭の行が同じなら端に当たった・`page_count_reached_the_end`。fixture `viewport-yank-H-*` `viewport-counted-page-*-past-*` ほか 8 件）。開始が端でなく回数の途中の周で初めて端に届く場合も、前向きは最後の周も画面の先頭を動かして端に当たらず最初の非空白へ、後ろ向きは最後の周が FAIL で同じバイトの桁を保つことを oracle で確かめた（fixture `viewport-counted-page-*-reaches-*-mid-count-indented`）。
この列の保持は折り返さない画面の経路だけに効く。折り返し・格子の画面行の単位は `page_step` の先頭で `view.navigation` から `displayed_navigation` へ戻るので不変で、そこでの回数つきの端は未検証。

## 追補（2026-10-29・#1041）: `scrolloff` の余白は追従の 1 本

1. **余白の計算は core の `first_visible_with_margin(先頭, キャレット, ViewportRows{行数, 余白}, follow)` の 1 本。** 余白の内側（先頭 + 余白から、行数 − 2 × 余白の高さ）を見える範囲とみなして今の寄せ（ADR 0009 の決定 6・Vim の中央寄せの閾値）を当て、結果を余白の分だけ戻す。余白は画面の半分（(行数 − 1) / 2）で切り詰める（Vim と同じ・`so=999` は常に中央）。余白 0 は `first_visible_for_caret` と同じ値（`first_visible_for_caret` は余白 0 の呼び出し）。
2. **呼ぶのは application の追従の 1 か所。** `VisualTextView::followed_first` が設定の `scrolloff` を渡す（画面行の単位なので、折り返し・格子・巨大な行の窓でも同じ）。仮の行の座標は余白の分だけ下げ、上へ寄せる余白が仮の 1 行目で切れないようにする（余白 0 なら今までと同じ）。文書の先頭と末尾では余白は効かない（先頭は 1 行目で止まり、末尾は `filled_viewport` の収めで切れる）。
3. **engine が画面を決める鍵（`VimNavigate`）の後も、同じ追従の計算を縦だけに 1 回当てる**（`EditorController::perform(VimNavigate)`・余白が 0 なら何もしない）。`zt` `z<CR>` は余白の分だけ上を残し、`zb` `z-` は下を残し（Vim の scroll_cursor_top / bot と同じ値）、`zz` `z.` は変わらない。Ctrl-d / Ctrl-u は窓とキャレットを同じ量だけ動かすので、余白の外のキャレットは動かさない。
4. **限界（後続）。** `H` `M` `L` と Ctrl-f / Ctrl-b は Vim では余白の分だけ**キャレットを**動かす（cursor_correct）が、Nib は engine が置いたキャレットを保って**窓を**余白の分だけ送る。キャレットの行が Vim と違い得る（`so=3` の `H` は Vim では先頭 + 3 行目・Nib は先頭の行で窓が 3 行上がる）。oracle は画面を測れないので fixture にはならない。engine に余白を渡して cursor_correct を足すかは設計席が決める。→ **#1099 で解いた**（次の追補）。
5. **両方のモードで効く。** `scrolloff` は窓の設定で、`wrap` と同じくモードを見ない（通常モードの打鍵・クリック・ドラッグの追従も余白を残す・Vim の `scrolloff` もマウスで同じ性質・既定 0 では変わらない）。中央へ寄せる閾値は余白の内側の行数で計る（Vim との一致は oracle で測れず未確認）。
6. **契約。** `nib_tests --vim-view`（既定実行にも載る）が `j` `k` の余白・文書の先頭・`zt` `zb` `zz`・`so=999` の中央・折り返した行の画面行の単位・通常モードのクリックを controller で、既定実行の `verify_scroll_margin`（`CoreTests.cpp`）が純関数の値を確かめる。
7. **追補（2026-10-30・#1100）: 仮の先頭は `limit + 1 + 行数`。** 上へ遠く跳ぶと、仮の先頭が `limit + 1 + 余白` では中央へ寄せる `(行数 − 1) / 2` 行が仮の 1 行目で切れ、キャレットが最上段（余白があれば余白の縁）に来ていた（距離が打ち切りの `2 × 行数 + 1` に近いときも・余白 0 でも・#1098 の独立レビュー R4-1）。仮の先頭を画面 1 つ分下げて中央と余白の両方を覆う。契約 `--vim-view`: 10 行の窓・60 行で `G` の後の `30G`（打ち切り）は先頭 26・`33G`（18 行上）は先頭 29・`48G`（閾値より近い 3 行上）は寄せずに先頭 48・`so=2` の `30G` は先頭 26。
   **Vim の `scroll_cursor_halfway` との比較（測ったのは Nib の契約だけで、Vim の画面は oracle で測れず未測定）。** ソースの読み（`move.c`）では、`update_topline` は上へ外れた量が `halfheight` 以上なら `scroll_cursor_halfway` を呼び、行の高さ 1 ならキャレットの上に `(行数 − 1) / 2` 行を置く。行の高さ 1・行数 6 以上ではこの値と閾値 `(行数 − 2) / 2` が Nib と同じ見込み。**既知の差:** Vim の `halfheight` は `行数 / 2 − 1` で下限 2、Nib の閾値 `(行数 − 2) / 2` には下限が無い（行数 5 以下の窓で寄せる閾値が Vim より小さい・追従の規則の変更なので本件では直さない・Issue にせず記録だけ）。折り返した行を挟む跳びは、Vim は行の単位（`plines`）で釣り合わせて先頭は行の頭、Nib は画面行で `(行数 − 1) / 2` 上（行の途中から始まり得る）で、差があり得る（未測定）。

## 追補（2026-10-30・#1099）: `H` `M` `L` と Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u のキャレットは Vim の `cursor_correct`

前の追補の決定 4 の「既知の差」を解く。

1. **engine は余白を `VimEditorView.scroll_margin` で 1 つ借りる。** 値は controller が `measured_vim_step` で設定の `scrolloff` から写す 1 か所（engine は設定を知らない・既定 0）。
2. **寄せ方は core の `vim_cursor_corrected(VimCursorWindow{先頭, 窓の下の行, 画面行の数, 余白}, キャレットの行, 行数)` の 1 本**（`VimCursorCorrect.cpp`・Vim 9.1 の `move.c` の `cursor_correct` の早期の戻り・欲しい行の切り詰め・詰め方・最後の寄せを折り畳みなしでなぞる）。**詰めるときは論理行で数える**（1 行詰めるごとに 1 を足す）。Vim は詰める行の画面行の数（`plines`）を足すので、折り返して高さの違う行がある窓では 1 行ほど違い得る（例: 窓 11 行・`so=2`・窓の先頭 T が 5 画面行の `H` は Vim が T+1・Nib が T+2）。画面行で数える直しは後続の Issue。窓の先頭が 1 行目なら上の余白は 0 で下は `画面 / 2` まで、本文の末尾まで見えていれば（窓の下の行 = 行数 + 1・Vim の `w_botline`）下の余白は 0 で上は `(画面 − 1) / 2` まで。足りていれば動かさず、足りなければ窓の両端から 1 行ずつ詰めた範囲へ寄せる。窓は動かさない（窓は今まで通り追従の 1 本・前の追補の決定 3）。
3. **呼ぶ所。** `H` `M` `L` はオペレータの無いとき（NORMAL と VISUAL）だけ寄せ、`dH` `dL` `yH` などの範囲は寄せない行のまま（Vim の `nv_scroll` は `op_type == OP_NOP` のときだけ `cursor_correct` を呼ぶ・実測で `so=3` の `dL` は L の行 = 窓の下端まで消す）。文字組みの道は `VimDisplayNavigation.settled` に寄せた行を置き（`caret` は範囲のための寄せない行）、無い道は `settled_screen_line` が今の窓で寄せる。Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u は新しい窓の先頭で寄せる（Vim の `onepage` と `halfpage` の最後の `cursor_correct`・文字組みの道は `VimScreenGeometry` の `corrected`、無い道は `VimStep.cpp` の `corrected_in`）。1 画面も送れなかった周（端の Ctrl-f / Ctrl-b）はキャレットが余白の内側にいるので寄せない。格子の画面行の単位の半画面と 1 画面（`row_target`）は変えていない（未検証）→ **#1142 で決めた**（決定 7）。
4. **前へ 1 画面送れない条件も余白を見る。** Vim の `onepage` は末尾まで見えていて窓の先頭が `行数 − scrolloff` 以上なら FAIL する（`so=999` で末尾の近くの Ctrl-f は窓もキャレットも動かない）。`forward_page_fails` が両方の道で同じ条件を見て、回数つきは周ごとに確かめる（文字組みの無い道は `forward_pages_held` が末尾の見える窓に着いた周を算術で求める）。余白 0 では先頭が最終行のときだけで、今までと同じ。
5. **実測（正本）。** Vim 9.1.0004 を `vim -u NONE -i NONE -N -n --not-a-term -S probe.vim` で `:set lines=12`（窓 11 行）・100 行の本文・`winrestview` で窓を置いて測った 560 件（余白 0 / 3 / 5 / 6 / 999 × 窓 8 通り × 鍵 14 通り・`D:/NeNeNib/scripts/1099/gen.py`）。キャレットの行は 560 件で一致した（文字組みの無い道と、文字組みの道（幅 400）の両方）。ただし本文は `line1`〜`line100` の短い行で、幅 400 でも 1 行も折り返していない。決定 2 の論理行と画面行の差はこの実測では測れていない。窓の先頭が違うのは 2 種類だけで、どちらも追従（`first_visible_with_margin`）の側の差: (a) 余白が `(画面 − 1) / 2` を越えるときの末尾の近くの Ctrl-f（Vim の `check_top_offset` と `update_topline` は切り詰めない余白で窓を上げる・Nib は切り詰める）と、回数つきの Ctrl-f が末尾の FAIL で終わったときの窓（Vim は FAIL の周で `check_top_offset` を飛ばす）の 11 件、(b) `dH` `dL` `vHd` で行を消した後の窓の 22 件。
6. **契約。** `nib_tests --vim-view` の `verify_scrolloff_cursor_correct` が実測の 28 行（`so=3` の `H` `2H` `5H` `M` `L` `2L` `Ctrl-f` `Ctrl-b` `Ctrl-d` `Ctrl-u`・先頭と末尾の端・`so=5` と `so=999`・余白が画面の半分を越えて範囲が潰れる `so=6` / `999` の `H` `L`・先頭が 1 行目の下の余白の切り詰め・回数つきの Ctrl-f が途中の周で末尾の FAIL に当たる `so=3` の `2<C-f>`）を窓の先頭とキャレットの行で、窓の先頭が追従の差に当たる `so=6` の `2<C-f>` をキャレットの行だけで、`dL` と `vHd` の消した行の数を、文字組みの無い道（幅 0）と折り返しの道（幅 400）の両方で確かめる。余白 0 の挙動は変えない（既存の fixture 全件と既存の契約）。
7. **格子の画面行の単位（#1142）。** 格子（`VimScrollUnit::row`）の Ctrl-f / Ctrl-b / Ctrl-d / Ctrl-u も、送った後の画面でキャレットの画面行を同じ `vim_cursor_corrected` の 1 本で余白の内側へ寄せる（`VimScreenRows.cpp` の `target_row`・第 2 の寄せを作らない）。Vim に格子は無いので実測ではなく、論理行の道の規則を**画面行を行に読み替えて**固定する: 窓の先頭は最上段の画面行（文書の最初の画面行なら「1 行目」で上の余白は 0）、窓の下は画面にある画面行の次（最下段が文書の最後の画面行なら「行数 + 1」で下の余白は 0）、余白と画面の高さは画面行で数える。先頭もキャレットも動かない周（端の Ctrl-b / PgUp と末尾の Ctrl-f）は寄せない（決定 3 と同じ）。前へ送れない条件（決定 4）と窓の先頭は変えない（格子の 1 画面は今まで通り最後の画面行が先頭になるまで送る・窓は追従の 1 本）。`H` `M` `L` は格子でも論理行のまま（ADR 0144 の限界・不変）。契約は `nib_tests --vim-view` の `verify_grid_rows_cursor_correct`（8 升 × 見えている 10 画面行・200 字の 1 行 = 25 画面行・`so=3` の `<C-f>` `<C-b>` `2<C-f>` `<C-d>` `<C-u>` と `so=0` の `<C-f>`・最上段の画面行とキャレットの桁）。余白 0 では寄せは何もしない。
