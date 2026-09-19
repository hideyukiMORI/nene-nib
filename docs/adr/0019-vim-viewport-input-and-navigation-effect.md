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
