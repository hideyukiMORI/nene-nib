# ADR 0030 — `.` は直前の変更を鍵の列として記録し、同じ経路へ再生する

- 状態: 受理
- 日付: 2026-09-22
- Issue: #87
- 影響する規則: ARC-001 / ARC-004 / ARC-007 / ARC-009 / CPP-002 / CPP-004 / CPP-006 / CPP-011 / CPP-012 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / CNF-010

## 文脈

FR-003 の T2 にある「繰り返し `.`」を実装する。ADR 0012 は `.` の挿入再生を難所として残し、ADR 0028 は o/O の範囲で「反復のためのキー再配送・再帰的 controller 呼び出し・効果の汎用列」を却下した。その却下の根拠は「o/O の回数反復は既存の挿入効果と編集合成で表現できる」ことであり、`.` には当てはまらない。`.` が再生する対象は任意の変更命令（削除・置換・貼り付け・INSERT を伴う命令）であり、命令ごとの構造化した記録を持つと、後続の全 Vim 機能（テキストオブジェクト・検索 motion・VISUAL）がそれぞれ記録の形を増やすことになる。

いまの engine は次を既に持つ。1 鍵 1 効果の純関数 `vim_step`（ADR 0012）、回数の積（ADR 0015）、排他的な次キー待ち（ADR 0027 / 0029）、INSERT の出入りが undo の区切り（ADR 0012 の決定 6 / ADR 0015 の決定 5）、回数付き INSERT の入力記録 `VimInsertRepeat`（ADR 0028）。controller の `EditorController::accept(VimKeyPress)` が 1 鍵を `vim_step` へ流して効果を写す唯一の経路であり、IME 確定も打鍵としてここへ入る（ADR 0014）。

Vim 自身の `.` は打鍵の記録（redo buffer）を再実行する。fixture は固定 Vim 9.1 の oracle が生成し、Vim ソースは読まない。本 ADR の決定は help と実測を根拠にし、実測で食い違ったら決定を直す（fixture を都合よく変えない）。

### 実測（2026-09-22・Issue #87・固定 Vim 9.1）

実装の前に 201 ケースを測り（`out/issue87-oracle/probe.py` 172 件・`probe2.py` 29 件）、取消の扱いを確かめるために 49 ケースを測り足した（`probe3.py` 28 件・`probe4.py` 21 件は鍵を区切った形と 1 回にまとめた形の両方を測るので 42 起動）。採用した fixture は 99 件で総数 650。

- 決定 2 は一致した。`2d3w.` は 6 語で、`2d3w5.` と `6dw5.` は同じ 5 語、`d2w3.` は 3 語。回数は完了時の積 1 つとして記録され、移動側の桁は記録に残らない。
- 決定 4 は一致した。`ifoo<Home>bar<Esc>.` は `bar` だけを入れ直す。`<End>`・`<Left>`・`<Down>`・`o` から入った挿入でも同じで、移動のあとは `i` から取り直した記録になる。
- 決定 7 は一致した。`x3..`・`3x3..`・`cwfoo<Esc>3..`・`3rz..` のいずれも、後続の `.` が `3.` の回数を引き継ぐ。
- 決定 5 は Vim と異なるままにした。Vim は `vlld.` を「同じ大きさの範囲」で再生し、`xjvlld.` でも VISUAL の削除を繰り返す。本 ADR は何もしないほうを選ぶので、この 2 つは fixture に採らず probe の記録だけを残す。
- 決定 3 も一致した。最初 `xdk.` と `xGdj.` が「Vim は失敗したオペレータで記録を入れ替える」ように見えたが、これは測り方の誤りだった。Vim の `clearopbeep()` は `beep_flush()` → `flush_buffers(FLUSH_MINIMAL)` を呼び、`:normal!` が積んだ**残りの鍵を捨てる**。`xdkj` の最終カーソルが 1 行目のままで `j` すら効いていないことが証拠である。鍵を命令の切れ目で区切って `:normal!` を分けて測り直すと（`out/issue87-oracle/probe4.py`）、`x` → `dk` → `j` → `.` は `x` を繰り返す。範囲の作れない `dk` / `dj` / `2dd` / `2D`、回数の入らない `3r`、外れた `f` / `;`、motion でない鍵（`dq`）、`g` の続きが無い場合（`dgz`）、失敗した yank（`yk`）、Esc の取消のいずれも同じで、取消になった命令は自分の鍵を捨てるだけで直前の変更を変えない。
- このため「ビープする命令の後ろに鍵が続く列」は既存の 1 回 `:normal!` の生成器では fixture にできない（生成器は残りの鍵を捨てた答えを記録してしまう）。ビープしない Esc の取消（`xd<Esc>j.` と `xr<Esc>j.`）だけを fixture に採り、残りは `--vim-dot` の対象 unit が probe4 の実測と同じ規則を直接確かめる。生成器は変更しない。
- 実装の途中で ADR 0028 の範囲外の穴が見つかった。`N.` が挿入命令を繰り返すには `3ifoo<Esc>` が `foofoofoo` になる必要があるが、回数付きの `i a I A` は回数を捨てていた。ADR 0028 の `VimInsertRepeat` をそのまま `i a I A` の入りでも立てるようにし、`counted-insert-*` の 5 件で固定 Vim と一致を確かめた。

## 決定

**`.` は「直前の変更を完了させた鍵の列」を engine が記録し、controller が同じ `accept` 経路へ 1 鍵ずつ流して再生する。命令ごとの構造化した記録は持たない。**

1. `VimState` に `recording`（組み立て中の命令）と `last_change`（直前の変更）を持つ。どちらも `std::optional<VimRepeatRecord>` で、`VimRepeatRecord` は `std::optional<VimCount>` と回数の桁を除いた `VimKey` の列。空は「記録していない」を表す（VISUAL から入った挿入がこれ）。本文・履歴・レジスタは所有しない（ARC-004）。
2. NORMAL で受けた鍵は、回数の桁を除いて `recording` に足す。回数は完了時に engine が既に計算している積（オペレータ側 × 移動側）を 1 つの `VimCount` として記録に残す。`0` が motion になる場合は鍵として記録する。
3. 鍵が命令を完了させたとき（次キー待ち・保留オペレータ・回数が空で、モードが NORMAL のまま）、効果で分ける。`VimRemoveRange` / `VimRemoveLines` / `VimInsertString` / `VimNewLine` / `VimInsertAt` / `VimReplaceRange` は変更であり `recording` を直前の変更として確定する。`VimNoEffect` / `VimMoveTo` / `VimNavigate` / `VimSelect` / `VimUndo` / `VimRedo` / `VimOpenCommandLine` は変更ではなく `recording` を捨て、直前の変更は変えない。効果の種類が増えたらこの分岐がコンパイルで落ちる（CPP-002）。yank は `VimMoveTo` 系で返るので記録しない。**取消になった命令も同じ規則で扱う**。範囲が作れない（1 行目の `dk`・最終行の `dj` / `2dd` / `2D`）・回数が行に入らない（`3r`）・文字検索が外れた（`dfz` / `d;`）・次の鍵が motion でない（`dq`）・`g` の続きが無い（`dgz`）・失敗した yank（`yk`）・Esc の取消（`d<Esc>` / `r<Esc>`）は、どれも `VimNoEffect` なので自分の鍵を捨てるだけで、直前の変更は変えない。固定 Vim 9.1 も同じで、`x` のあとにこれらを打ってから `.` を押すと `x` が繰り返される（`out/issue87-oracle/probe4.py` で実測）。
4. 命令が INSERT へ入る場合（`i a I A o O c cc C s` 等）は、その命令の鍵に続けて INSERT で受けた鍵（文字・Enter・Backspace・Esc）を同じ `recording` に足し、Esc で NORMAL へ戻ったときに 1 つの変更として確定する。INSERT 中の Home / End / 矢印 / ページ移動は ADR 0028 の決定 3 と同じ境界で、記録を `i` から取り直す（Vim は移動のあとの入力を新しい挿入として扱う。Home / End で実測し、矢印は同じ境界として扱う）。IME の確定文字は打鍵として届くので、そのまま記録される。
5. VISUAL / 行単位 VISUAL への遷移と Ex（`:`）の開始は `recording` を捨てる。VISUAL で完了した変更は直前の変更を **消す**（`.` が古い NORMAL の変更を再生して意図しない編集になるより、何もしないほうが安全）。Vim は VISUAL の変更を範囲の大きさで再生するが、これは [Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91) で `VimRepeatRecord` に範囲の大きさを足して扱う。
6. `.` は NORMAL の action `repeat_change` であり、直前の変更が無ければ `VimNoEffect`。あれば新しい効果 `VimReplay`（回数と鍵の列）を返す。回数は `.` に付いた回数があればそれ、無ければ記録の回数。`VimReplay` は `VimEffect` の和型に足す 1 つの効果で、鍵の列は engine の中で回数の桁を先頭に展開した `VimKey` の列にする。VISUAL と INSERT の `.` は文字として扱わず `VimNoEffect`（VISUAL）／文字（INSERT）のままにする。
7. controller は `VimReplay` を受けたら、履歴を閉じてから鍵を順に `accept(VimKeyPress)` へ流し、終わったら履歴を閉じる。再生は同じ `vim_step` を通るので engine は通常どおり記録し直す。これにより `3.` の後の `.` は回数 3 を引き継ぐ（Vim の help `.` と同じ。実測で確認）。再生中の鍵は `.` を含まない（`.` は変更ではなく記録されない）ので再帰は深さ 1 で止まる。再生は UI・IME・描画を経由せず、`WM_PAINT` は既存のとおり 1 フレームに 1 回。
8. 再生の途中で命令が止まる場合（文書末で motion が失敗する等）は、鍵を普通に打ったときと同じ結果にする。特別な巻き戻しはしない。`.` 1 回の undo 単位は決定 7 の 2 つの区切りが作る。
9. UI のキー配送・IME・描画・保存形式は変更しない。

## 強制

- 効果の写し漏れ・記録の分岐漏れ: **active**（`std::visit` と `switch` の網羅性・CPP-002）
- 記録の確定・破棄・回数の置換・undo 単位・再生の非再帰: **active**（`--vim-dot` の対象 unit と oracle fixture・CNF-010 で生成物の一致）
- 再生が UI を経由しないこと: **active**（controller の unit が窓なしで `.` を通す）
- 期待値が本物の Vim の答えであること: **不能**（CI に Vim は無い。ADR 0012 のとおり Vim のある機械でしか測れない）
- 取消のあとの `.`: **active**（`--vim-dot` の対象 unit。oracle fixture にはできない。ビープが `:normal!` の残りの鍵を捨てるため、生成器が測れるのはビープしない Esc の取消だけである）

## 結果

得られるもの: 既存と今後の変更命令がすべて自動的に `.` の対象になる。回数付きの `i a I A` も ADR 0028 の入力記録に載り、Vim と同じになった（ADR 0028 の決定 3 にあった「一般の i/a の回数には広げない」は本 ADR が置換する）。記録の形が命令の種類に依存しないので、テキストオブジェクト・検索 motion を足しても `.` 側の変更は要らない。engine は純関数のまま。
失うもの・残る穴: VISUAL の変更の `.` はこの縦切りでは何もしない（決定 5・Vim は範囲の大きさで再生する。[Issue #91](https://github.com/hideyukiMORI/nene-nib/issues/91)）。`i a I A` が入力記録を持つようになったので、controller の「外からの割り込みで記録を捨てる」経路（クリック・Ctrl+Z・全選択）と「Vim の鍵による移動」を分け、後者は undo の単位を切るだけにした。前者は今も controller が `VimState` の入力記録を直接消す二重経路で、engine のポートへ寄せるのは [Issue #92](https://github.com/hideyukiMORI/nene-nib/issues/92)。`VimState` が鍵の列を持つので 1 鍵あたり小さな vector の複製が増える（1 打鍵 0.9 ms の予算に対して無視できる見込み。速さのゲートが見張る）。Vim の redo は `"` によるレジスタ指定や `&` も含むが、名前付きレジスタが無いいまは範囲外。controller の `accept` が自分自身を鍵ごとに呼ぶ形になるので、`command_input` が開いたときの早期 return を再生中も通す（`:` は記録しないので実際には起きない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 命令ごとの構造化した記録（オペレータ・motion・回数・挿入文字列） | 命令の種類が増えるたびに記録の形と再生の写しが増える。鍵の列なら engine の表がそのまま再生の意味を定める |
| `VimEffect` を列にして 1 鍵で複数の効果を返す | 効果の列は controller の写しと undo 境界の規則を全効果で見直すことになる。`.` に必要なのは「鍵の列を同じ経路へ流す」ことだけ |
| `VimInsertRepeat` の入力文字列を `.` の記録に流用する | あれは Esc 時の回数反復のための LF 文字列で、命令の鍵を持たない。`.` は命令ごと再生する必要があるので別物。記録そのものは分けたまま、回数付き `i a I A` が入力記録を持つ点だけを本 ADR が広げた |
| VISUAL の変更も直前の変更として残し、鍵をそのまま再生する | Vim は範囲の大きさで再生するので鍵の再生では motion の到達点が変わり、期待と違う範囲を編集しうる。何もしないほうが安全 |
| controller ではなく engine の中で鍵の列を畳んで 1 効果にする | 1 鍵 1 効果を保ちながら任意の命令を畳むには効果の合成器が要り、上の「効果を列にする」と同じ問題になる |
