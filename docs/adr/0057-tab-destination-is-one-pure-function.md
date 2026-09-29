# ADR 0057 — タブの行き先は 1 つの純関数が決め、Vim の `gt` `gT` と Ex と一覧が共用する

- 状態: 受理（設計席 2026-09-29・Issue #240）
- 日付: 2026-09-29
- Issue: #240
- 影響する規則: FR-003 / FR-005 / FR-006 / ARC-001 / ARC-004 / CPP-002 / CPP-003 / CPP-006 / CPP-012 / QLT-001 / QLT-012
- 前提: [ADR 0056](0056-tabs-park-inactive-documents-behind-the-active-one.md)（複数タブ・決定 11 が本 ADR の入口・施主決定 D20〜D22）・[ADR 0022](0022-ex-command-line-and-settings-evaluation.md)（Ex の評価は純関数・写し先は controller の 1 か所）・[ADR 0023](0023-command-palette-and-shared-input-session.md)（Ctrl+P の面は Ex と候補・評価を共用する）・[ADR 0012](0012-vim-engine-first-slice-and-oracle-fixtures.md)（engine は純関数・効果は閉じた和型）・[ADR 0046](0046-macros-record-keys-and-replay-through-dot-path.md)（失敗で再生を打ち切る）

## 文脈

ADR 0056 の縦切り 4/4。帯の「∨」は描かれているが押しても何も起きない。Vim の使い手の鍵 `gt` `gT` と `:tabnext` も無い。
ADR 0056 の決定 11 は「Ctrl+P の面を開いているタブの候補で開く・実行は Ex の `tabnext {N}`・`gt` は `g` の表に足す」までを決めた。形（行き先の数え方・失敗・型）は決めていない。

oracle の fixture はタブを観測できない（1 本の `:normal!` の結果の本文とカーソルを比べる）ので、Vim の振る舞いは probe で実測した（`out/probes/probe-vimtabs-2026-09-29.md`・Vim 9.1 patch 1-4・`-u NONE -i NONE -N -n -es`・約 230 件）。

### 実測（本数 3 本・今のタブ 1 / 2 / 3）

| 命令 | 結果 |
| --- | --- |
| `gt`・`:tabnext` | 次のタブ。末尾からは先頭へ折り返す |
| `gT`・`:tabprevious`・`:tabNext` | 前のタブ。先頭からは末尾へ折り返す |
| `{N}gt` | **絶対の番号**（1 始まり）。`1gt` `2gt` `3gt` は N 番目へ。範囲の外の `4gt` `9gt` は失敗（タブは動かず、後ろの鍵は打ち切られる・例外は出ない） |
| `{N}gT` | **N 個ぶん戻る**（相対）。折り返す（3 本で `4gT` は `1gT` と同じ・`3gT` `9gT` は同じ位置） |
| `:tabnext N` | 絶対の番号。`:tabnext 4` `:tabnext 0` は `E475: Invalid argument: 4` |
| `:tabprevious N` | N 個ぶん戻る（折り返す）。`:tabprevious 0` は E475 |
| `:tabnext +1` `-1` `$`・`:4tabnext`（範囲の形） | 相対は折り返さず端で E475・範囲の形の外は `E16: Invalid range` |
| `0gt` | 回数 0 ではなく `0`（行頭）の後の `gt` |
| `:tabnew` | 今のタブの右隣にできて、そこへ移る |
| `:tabclose` | 閉じた後は右隣（無ければ左隣）。最後の 1 本は `E784: Cannot close last tab page`（`!` でも）。未保存は `E37: No write since last change (add ! to override)` |
| `:tabs` | 「Tab page N」の見出しと、窓ごとの 1 行（今の窓に `>`・未保存に `+`） |
| 省略形 | `:tabn` `:tabne` は `tabnext`・`:tabp` は `tabprevious`・`:tabN` は `tabNext`・`:tabc` は `tabclose`・`tabnew` は完全一致だけ（`:tabe` は `tabedit`） |
| `dgt` `ygt` `cgt` `d2gt` | オペレータは打ち消され、タブは動かず、後ろの鍵は打ち切られる |
| `vgt` | タブは動き、VISUAL は終わる |
| `gt` の後の `.` | 何もしない（`gt` は繰り返しの対象ではない） |
| マクロに録った `gt` | 動く |

### 実測（本数 1 本）

| 命令 | 結果 |
| --- | --- |
| `gt` `gT` `1gt` `1gT` `2gT` `3gT` | 成功（位置は 1 のまま・後ろの鍵は実行される） |
| `2gt` | 失敗（後ろの鍵は打ち切られる） |
| `:tabnext` `:tabprevious` `:tabnext 1` `:tabprevious 1` `:tabprevious 2` | 成功（位置は 1 のまま） |
| `:tabnext 2` | `E475: Invalid argument: 2` |
| `dgt` | オペレータは打ち消され、後ろの鍵は打ち切られる |

本数 3 本で、`3gT`（本数と同じ数だけ戻る）は元の位置のまま成功し、`1gt` を 1 本目から打つのも成功する（今いるタブへの移動は失敗ではない）。
本数 1 本の結果は、3 本の規則（次と前は折り返す・`{N}gt` は絶対で範囲の外は失敗・`{N}gT` は折り返す）をそのまま当てた値と一致する。

## 決定

**「どのタブへ行くか」は core の純関数 `tab_destination` の 1 本が決める。Vim の `gt` `gT`・Ex の `tabnext` `tabprevious`・一覧の候補の実行は、どれもこの関数を通って同じ `SwitchTab` に着く。engine はタブの本数と今の位置を `VimEditorView` から借用して、失敗を自分で決める。**

1. **行き先（core）**: `TabJump { TabJumpDirection direction; std::optional<std::size_t> count; }`（`direction` は `forward` / `backward` の閉じた enum・1 型 1 ファイル）。`tab_destination(const TabJump &, std::size_t active, std::size_t tab_count) -> std::optional<std::size_t>`（帯の位置・0 始まり。値なしは失敗）。
   - `forward`・回数なし: 次（末尾から先頭へ折り返す）。
   - `forward`・回数 N: N 番目（1 始まり）。N が 0 か本数より大きければ失敗。
   - `backward`・回数なし: 前（先頭から末尾へ折り返す）。
   - `backward`・回数 N: N 個ぶん戻る（折り返す）。N が 0 なら失敗。
   - 本数 1 本でも同じ規則（特別な枝を作らない。実測と一致する）。
2. **Vim の鍵（engine）**: `g` の接頭辞の表に `t` `T` を足す。`VimEditorView` に `VimTabs tabs`（`active` と `count`・1 型 1 ファイル）を足し、engine は `tab_destination` で行き先を決める。効果は `VimSwitchTab { std::size_t index }`（`VimEffect` に 1 つ足す）。
   - 行き先が値なしなら失敗（`VimRepeatFailure`・効果なし・再生の中なら controller が残りの鍵を捨てる）。
   - オペレータ待ちの `gt` `gT` はオペレータを打ち消して失敗（タブは動かない）。
   - VISUAL の `gt` `gT` は動く。切り替えが VISUAL を終える（ADR 0056 の決定 4 の `vim_switched_document`）。
   - `.` の記録には入れない。マクロの録画は打った鍵を録るので、今のまま録れて再生できる。
   - 今いるタブへの移動（`1gt` を 1 本目から）は成功で、効果は同じ位置の `VimSwitchTab`（controller の `SwitchTab` が同じ位置なら何もしない）。
   - `g<Tab>`（直前のタブ）と `<C-PageDown>` `<C-PageUp>` は後続。
3. **controller の写し**: `VimSwitchTab` は `SwitchTab` と同じ経路（`accept(SwitchTab)` の中身の 1 本）へ写す。再生の途中で切り替わったときは、出ていく文書の undo の単位をそこで閉じ、残りの鍵は入った文書へ流す（undo の単位は文書ごと）。controller は鍵ごとに `VimEditorView` を作り直すので、`tabs` は常に今の値。
4. **Ex（core の `evaluate_ex`）**: `ExResult` に `std::optional<ExTabRequest> tab` を足す。`ExTabRequest { ExTabVerb verb; std::optional<std::size_t> number; }`・`ExTabVerb` は `next` `previous` `open` `close` `list` の閉じた enum。
   - 名前: `tabn[ext]`・`tabp[revious]` と `tabN[ext]`・`tabnew`（完全一致）・`tabc[lose]`・`tabs`（完全一致）。省略は実測のとおり「最短の形から完全な形までの前方一致」。表で書く（CPP-012）。
   - 引数: `tabnext` `tabprevious` は無しか 10 進の数 1 つ。`tabnew` `tabclose` `tabs` は引数なしだけ。
   - それ以外の形（`+1` `-1` `$`・範囲の形 `:4tabnext`・`!`・`tabnew <file>`・`tabclose N`）は今回は受けない。閉じた失敗 `ExFailure::unsupported_argument`（文言は `E474: Invalid argument` ではなく Nib の言葉「Not supported: <入力>」。Vim の E 番号を、Vim が成功する入力に付けない）。
   - 数が範囲の外（`tabnext 0`・`tabnext 4`・`tabprevious 0`）は、本数を知っている controller が `tab_destination` の値なしを受けて `E475: Invalid argument: <N>` を `command_message` に出す（Vim の実測の文言）。`evaluate_ex` は本数を知らないままの純関数で変えない。
5. **Ex の写し（controller の Ex の経路 1 か所）**: `next` / `previous` → `tab_destination` → `SwitchTab`。`open` → `NewTab`。`list` → 一覧を開く（決定 7）。`close` → 決定 6。
6. **`:tabclose` は Nib の閉じる流れを通す（Vim と違える）**: Vim は最後の 1 本で E784・未保存で E37 だが、Nib は × ・中ボタン・Ctrl+F4 と同じ 1 本の流れ（未保存なら「保存しますか」・最後の 1 本なら窓を閉じる・施主決定 D22）を通す。閉じ方が入口で変わらないことを優先する。確認は ui の仕事なので、controller は `EditorFrame.close_request`（`std::optional<std::size_t>`・閉じたいタブの位置・`closing` と同じく 1 意図だけ立つ）を載せ、ui は意図を送った結果を受ける 1 か所でそれを見て `close_tab(位置)` を呼ぶ。
7. **一覧（「∨」と `:tabs` と Ctrl+P の候補 `tabs`）**: 新しい面は作らない。`CommandPalette` に候補の出どころ `CommandPaletteSource`（`commands` / `tabs` の閉じた enum）を足し、`CommandPalette::opened_tabs(std::vector<CommandChoice> tabs, std::size_t active, ThemeCatalog)` で開く。
   - 候補は帯の順に 1 タブ 1 行。`label` は題名（未保存の印「● 」つき・帯と同じ）、`command` は `tabnext {N}`（N は 1 始まり）、`kind` は `execute`。`CommandChoice` に `std::optional<DisplayText> detail` を足し、場所（ファイルのあるフォルダ。無題は無し）を入れる。ui は `detail` を `muted` で題名の後ろに描く。既存の Ex の候補の `detail` は無し。
   - 開いた直後の入力は空で、選ばれているのはアクティブなタブの行。
   - 絞り込みは題名に対する今の `match_score`（大文字と小文字を区別しない部分列）。入力が空なら帯の順のまま、入力があれば点数の順（同点は帯の順）。
   - 実行は今の `submit_palette` → `evaluate_command("tabnext N")`（決定 4・5 を通る 1 本）。
   - 意図は `OpenTabList {}`（`EditorIntent` に 1 つ足す）。「∨」のクリック（押した要素と離した要素が同じとき・ADR 0056 の決定 9）と、Ex の `list` が同じ所へ着く。一覧が開いている間の `OpenTabList` と Ctrl+P は今の Ctrl+P と同じく閉じる。一覧が開いている間に「∨」を押したときも閉じる（面の外のクリックで閉じる今の規則がそのまま効き、開き直さない。開閉のボタンとして振る舞う・工程 2 の実装席の確認を設計席が受理）。
   - Ctrl+P の Ex の候補（`ex_command_candidates`）に `tabs` `tabnew` `tabnext` `tabprevious` `tabclose` を足す。
8. **縦切りの工程**: (1) core と application（決定 1〜7 の ui 以外）と契約 → (2) ui（「∨」のクリック・`close_request`・`detail` の描画）→ (3) 検証の記録と PR。
9. **範囲の外（後続）**: `g<Tab>`・`:tabfirst` `:tablast` `:tabrewind`・`:tabmove`・`:tabonly`・`:tabnext +N` `-N` `$`・範囲の形・`:tabclose N` と `!`・`:tabnew <file>` `:tabedit`・「∨」の真下に出る小さい一覧の面・一覧からタブを閉じる。

## 強制

- 契約（`--tabs` と Vim の engine の scope）: **planned**（Issue #240 の工程 1 で active にする）。`tab_destination` は本数 1 / 3 × 今の位置 × 回数の表・Vim の鍵は実測の表のとおり（`gt` `gT` `{N}gt` `{N}gT`・範囲外の失敗と打ち切り・`dgt`・`vgt`・`.`・マクロ）・Ex の 5 つと省略形と失敗の文言・一覧の候補と絞り込みと実行・`close_request`。
- fixture: **不能**（oracle はタブを観測できない）。既存の fixture 1853 件は不変（`eng/protected-diff.py`）。
- 網羅性: `VimEffect` `EditorIntent` `ExTabVerb` `CommandPaletteSource` `TabJumpDirection` は閉じた型で、写し漏れはコンパイルが落とす（CPP-002・active）。
- 実機: 設計席が PNG で確かめる（「∨」のクリック → 一覧・選ぶ → 切り替わる）。

## 結果

得られるもの: 「∨」から開いているタブを選べる。Vim の使い手は `gt` `gT` `{N}gt` `:tabnext` `:tabnew` `:tabclose` が使える。行き先の数え方が 1 か所にあり、鍵と Ex と一覧で食い違わない。
失うもの・残る穴: `:tabclose` は Vim と違う（E784 / E37 を出さず、確認して閉じる・最後の 1 本は窓を閉じる）。Ex の相対の引数と範囲の形は受けない。一覧は「∨」の真下ではなく Ctrl+P の面の位置に出る。engine が借用する値が 1 つ増える（`VimEditorView.tabs`）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| engine は「行きたい」だけを効果で返し、controller が行き先と失敗を決める | 失敗で再生を打ち切るのは engine の結果（`VimStep.failure`）を読む 1 本の経路。controller が失敗を後から作ると、オペレータの打ち消しと失敗の決め手が 2 か所に分かれる（ARC-004） |
| タブの本数を `VimState` に写しとして置く（ADR 0051 のクリップボードと同じ形） | クリップボードは読むのが高価で失敗もするので写しを置いた。本数と位置は controller がいつでも安く読めるので、借用（`VimEditorView`）で足りる。状態の欄を増やさない |
| `:tabclose` を Vim のとおりにする（E784・E37） | 同じ「タブを閉じる」が入口で違う結果になる。未保存の確認は × と同じ流れのほうが失うものが無い。`:tabclose!` は後続 |
| 一覧のために新しい面を作る | ADR 0056 の決定 11 で却下済み。候補・入力・絞り込み・実行が Ctrl+P と 2 本になる |
| 一覧の実行を `SwitchTab` へ直接写す | Ex の `tabnext N` と 2 本になる。候補の実行は Ex の評価の 1 本（ADR 0023） |
