# ADR 0058 — Ctrl+Tab は最近使った順に歩き、Ctrl を離したときに確定する

- 状態: 受理（設計席 2026-09-30・Issue #248・**施主決定 D23**「Ctrl+Tab は最近使った順に直す」）
- 日付: 2026-09-30
- Issue: #248
- 影響する規則: FR-005 / ARC-001 / ARC-004 / CPP-002 / CPP-003 / CPP-011 / QLT-001 / QLT-012 / QLT-013
- 前提: [ADR 0056](0056-tabs-park-inactive-documents-behind-the-active-one.md)（複数タブ・決定 3・10 と却下の表の 1 行を本 ADR が置き換える）・[ADR 0057](0057-tab-destination-is-one-pure-function.md)（帯の位置の順の行き先・変えない）

## 文脈

仕様は最初から「Ctrl+Tab は最近使った順」と決めている（SPECIFICATION.md の D6「数が増えた弱点は Ctrl+P と Ctrl+Tab（最近使った順）で補う」・FR-005）。
ADR 0056 は Ctrl+Tab を「帯の位置の順・使った順にはしない」とし、却下の表に「Ctrl+Tab を使った順（MRU）にする」を置いた。**設計席が施主に確かめずに、仕様にある施主の決定を上書きした。** #239 はそのまま実装して main に入った（`d6537e1`）。
2026-09-30 に区切りの文書を準備していて食い違いに気づき、施主へ 2 つの動きの違いと良い点・悪い点を説明した。施主は「最近使った順に直す」と決めた（D23）。

帯の位置の順に動く鍵は、Vim の `gt` `gT` と Ex の `:tabnext` `:tabprevious` がある（ADR 0057）。Ctrl+Tab を同じ動きにすると、2 つの鍵が同じことしかできない。

「最近使った順」の普通の意味（VS Code・Visual Studio・OS の Alt+Tab）は、修飾鍵を押している間は順番を固定して歩き、離したときに着いた先を「いちばん最近」にすること。押すたびに順番を入れ替えると、Ctrl+Tab は 2 つのタブを行き来するだけになり、3 つ目へ届かない。

## 決定

**タブの使った順は application の状態が 1 本持つ。Ctrl+Tab は、Ctrl を押している間は順番を固定してその上を歩き、Ctrl を離したときに着いたタブを先頭へ動かす。帯の位置の順の鍵（`gt` `gT` `:tabnext` `:tabprevious`・一覧）は変えない。**

1. **使った順（core）**: `TabRecency`（帯の位置を「いちばん最近」から順に並べた列を持つ値・1 型 1 ファイル）。純関数:
   - `tab_recency_touched(recency, tab)`: そのタブを先頭へ。
   - `tab_recency_opened(recency, tab)`: 帯の位置 `tab` に新しいタブができた（それ以上の位置は 1 つずつ後ろへずれる）。新しいタブは先頭。
   - `tab_recency_closed(recency, tab)`: 帯の位置 `tab` のタブが閉じた（それより後ろの位置は 1 つずつ前へずれる）。
   - `tab_recency_walked(recency, from, step)`: 列の上で `from` の隣（`TabStep::next` は「より前に使った」ほう・`previous` は逆）の帯の位置。端は折り返す。
   - 列は常に「開いている全部のタブを 1 回ずつ」含む（不変条件）。1 本のときの歩きは同じ位置。
2. **状態（application）**: `EditorState` に `recency_`（`TabRecency`）と `tab_walk_`（歩いている間だけ値がある印・`bool` でよい）。
   - 切り替え（クリック・`gt` `gT`・Ex・一覧・開く・閉じた後の隣）: 歩きを終えて、着いたタブを先頭へ（`touched`）。
   - 新しいタブ・開く: `opened`。閉じる: `closed` のあと、アクティブになったタブを `touched`。閉じた後のアクティブの選び方は ADR 0056 の決定 6 のまま（右隣・無ければ左隣）。
   - 起動: 引数のファイルを順に開いた結果、最後に開けたものが先頭で、その前に開いたものほど後ろ。
3. **意図**: `StepTab` を `WalkRecentTab { core::TabStep step }` に置き換え、`SettleRecentTab {}` を足す（`EditorIntent`）。
   - `WalkRecentTab`: 歩きを始め（まだなら）、`tab_recency_walked` の行き先へ切り替える。**列は入れ替えない。**
   - `SettleRecentTab`: 歩いていれば、今のタブを先頭へ動かして歩きを終える。歩いていなければ何もしない。
   - 帯の 3 つの意図と同じく、Vim の報せは消さない（`SettleRecentTab`）。
4. **鍵（ui・ADR 0056 の決定 10 の表はそのまま）**: `tab_command_for` の `next` / `previous` の写し先を `WalkRecentTab` に変える。
   - Ctrl を離したとき（`WM_KEYUP` の `VK_CONTROL`）と窓がフォーカスを失ったとき（`WM_KILLFOCUS`）に、**歩いているときだけ** `SettleRecentTab` を送る。歩いているかは `EditorController` の読み取りの口（状態の 1 欄を読むだけ）で確かめる。歩いていないときの Ctrl の上げ下げ（Ctrl+C など）は意図を送らず、画面を描き直さない。
   - 入力行が開いている間はタブの鍵を引かない（今のまま）。歩いている間に入力行を開く鍵（`:` `/` Ctrl+P）が来たら、先に歩きを確定する。
   - 歩きを続ける意図は `WalkRecentTab` `SettleRecentTab` と、文書もアクティブのタブも動かさない窓の意図（`PointTitleBar` `ScrollTabs` `TitleBarWidth` `VisibleLines` `RefreshAppearance`）で、ほかの意図と Vim の鍵は写す前に確定する。判断は `EditorController` の入口の 1 か所（`apply` と `press_vim_key` / `press_vim_keys` が通る）に置き、ui には置かない（#248 の差し戻しで設計席が ui から移した）。
5. **変えないもの**: `gt` `gT` `{N}gt` `{N}gT`・`:tabnext` `:tabprevious`・一覧・Ctrl+T・Ctrl+F4・Ctrl+W・帯の並び。帯の並びは使った順では動かない。
6. **実機の検査**: `eng/verify-window.py --tabs` の Ctrl+Tab の期待値を使った順に直す（Ctrl+T × 2 の後の Ctrl+Tab は 2 本目・Ctrl を押したままの 2 回目は 1 本目）。Ctrl を押したまま Tab を 2 回送る手を `eng/window_driver.py` に足す。
7. **範囲の外（後続）**: 歩いている間に出す一覧の面・Ctrl+PageDown / Ctrl+PageUp（帯の位置の順）・Ctrl+1〜9・閉じた後のアクティブを使った順で選ぶこと・使った順の保存と復元。

## 強制

- 契約（`--tabs`）: **active**（Issue #248 の工程 1 `3ae7dca`・差し戻し `f140fcc`）。使った順の 4 つの純関数（`tests/unit/TabsTests.cpp` の `verify_tab_recency_functions` `verify_tab_recency_keeps_every_tab`）・歩き（押したままの 2 回目以降・折り返し・逆向き・`verify_walk_recent_tabs`）・確定（`verify_settle_without_walk`）・歩きの途中のクリックと `gt` `gT`（`verify_walk_interrupted`）・歩きを続けない意図と Vim の鍵の前の確定と、歩いていないときに順が変わらないこと（`verify_walk_settles_before_other_intents` `verify_order_kept_without_walk`）・新しいタブと閉じるの後の順（`verify_recent_after_open_and_close`）・起動の順（`verify_recent_after_startup`）・`gt` `:tabnext` が帯の位置の順のままであること（`verify_walk_interrupted` の `gt` `gT` と既存の `verify_vim_tab_keys` `verify_ex_tab_switch`）。入口は `build/nib_tests.exe --tabs`（scope の絞り込み）と、載る既定実行 `ctest -R nib_unit`。
- 網羅性: `EditorIntent` は閉じた和型で、`StepTab` の写し先はコンパイルが落とす（CPP-002・active）。
- 実機: `eng/verify-window.py --tabs`（本物のキー入力を送るので CI では回さない。施主に確かめてから設計席が回す）。
- fixture: **不能**（oracle はタブを観測できない）。既存の fixture は不変（`eng/protected-diff.py`）。

## 結果

得られるもの: 仕様（D6・FR-005）と実装が一致する。離れた 2 つのタブを Ctrl+Tab の 1 回で行き来でき、押したまま続ければ 3 つ目以降へも届く。帯の位置の順に動く鍵（`gt` `gT`）と役割が分かれる。
失うもの・残る穴: Ctrl+Tab の行き先は帯を見ただけでは分からない（歩いている間の一覧の面は後続）。ui が `WM_KEYUP` と `WM_KILLFOCUS` を見る所が 1 つ増える。#239 から本 Issue までの main は、仕様と違う順で Ctrl+Tab が動く。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 帯の位置の順のままにして、仕様を書き換える | 施主が「最近使った順に直す」と決めた（D23）。位置の順の鍵は `gt` `gT` がある |
| 押すたびに順番を入れ替える（歩きを持たない） | Ctrl+Tab が 2 つのタブを行き来するだけになり、3 つ目へ届かない。Ctrl+Shift+Tab が意味を持たない |
| 使った順を ui が持つ | 状態の所有者が増える（ARC-004）。切り替えの入口（クリック・`gt`・Ex・一覧）は application にあり、順の更新もそこで 1 本にできる |
| Ctrl を離すたびに `SettleRecentTab` を送る | Ctrl+C などのたびに意図が 1 つ走り、画面を描き直す。歩いているときだけ送る |

## 設計席の誤りの記録

ADR 0056 を書くとき、仕様の FR-005 と D6 を読み直さずに「位置の順のほうが画と一致する」と判断した。仕様にある施主の決定と違う形を選ぶときは、ADR を書く前に施主へ確かめる。ADR の「前提」に仕様の該当する決定（D 番号と FR 番号）を書き、却下の表に仕様の決定そのものが入っていたら、それは設計席が決められないものである。
