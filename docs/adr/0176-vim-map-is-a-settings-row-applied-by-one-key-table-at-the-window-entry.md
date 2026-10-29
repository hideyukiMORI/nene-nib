# ADR 0176 — Vim の `map` は設定 v4 の行で、置き換えは窓の入口の鍵の表 1 つが行い、engine は知らない

- 状態: 受理（施主決定 D72・2026-10-28 夜「全部推し」。実装は 1〜2 席・受理は D41 の形）
- 日付: 2026-10-29
- Issue: #1058
- 規則: FR-032、ARC-001 / ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-011 / CPP-012 / CPP-013、QLT-014、ADR 0030（`.` の記録は鍵の列）/ 0046（マクロは打った鍵を録る）/ 0048 / 0051 追補（Ex の request の形）/ 0054（鍵の記法の表は `eng/vim-oracle.py` の `KEY_TABLE` の 1 つ・C++ の表は生成物）/ 0114 / 0170 決定 3（設定 v4 の鍵の集合）/ 0137（タイマーの意図は使う人の操作ではない）

## 文脈

仕様 T2 の「簡単な `map`」が無い。`inoremap jj <Esc>` と `<leader>` は Vim の人がほぼ必ず置く。D72 で決まったこと: 置き場は `settings.v4` の行（別の `.nibrc` は読まない）・置き換えは窓の入口の表（engine は知らず、`.` とマクロには替えた後の鍵が載る）・左辺 1〜3 打鍵・右辺は Nib が知っている鍵だけ・`<expr>` `<Cmd>` は入れない・2 打鍵の待ちは 1 秒のタイマーの意図。

## 決定

1. **設定の行。** `settings.v4` に `map.<modes>.<lhs>=<rhs>` の行を 0 行以上と `mapleader=<key>` を 1 行まで。`<modes>` は `n` `i` `v` `x` `o` の 1 文字以上（順不同・重複は拒否・`v` と `x` は Nib では同じ VISUAL）。`<lhs>` と `<rhs>` は fixture と同じ鍵の記法（`<Esc>` `<Space>` `<C-s>` `<leader>` `<CR>`・ADR 0054 の表・`<leader>` は `mapleader` の鍵で、既定は `\`）。`noremap` は `map.<modes>.<lhs>=<rhs>` の行の値の先頭に `!` を置く（`map.i.jj=!<Esc>`）。行の形は `lsp.<language>.command` と同じ接頭辞の族として codec が受ける（ADR 0170 決定 3 の追補: 鍵の集合に `map.*` と `mapleader` を足す。v4 は 2026-10-28 に定義されどの公開版も書いていないので版は上げない）。16 KiB の上限は同じ。壊れた行（記法に無い鍵・左辺が 4 打鍵以上・右辺が空）は設定全体の拒否（ADR 0114 の厳密な読み・保存を拒む）。

2. **表は core の純粋な値。** `core::KeyMap`（`KeyMapping{VimModeSet modes; std::vector<VimKey> lhs; std::vector<VimKey> rhs; bool remap;}` の列・設定から `key_map_of(settings)` で作る・`<leader>` は作るときに展開）。引く口は 1 本 `key_map_step(const KeyMap &, VimMode, std::span<const VimKey> pending, VimKey key) -> KeyMapStep`（閉じた和型: `Emit{std::vector<VimKey> keys}`（確定・右辺の鍵の列、または不一致で流す左辺の分と今の鍵）/ `Pending{}`（左辺の途中）/ `Pass{}`（表に無い・鍵をそのまま））。左辺の最大 3 打鍵なので表は線形で足りる（行は数十）。鍵の記法 ↔ `VimKey` の写しは `tests/vim/VimKeyNames.hpp` の生成物を **core へ移して**（`src/core/VimKeyNames.hpp`・生成は同じ `python eng/vim-oracle.py --key-names`・CNF-010 が守る）、tests は core のものを読む（表は 1 つ）。

3. **置き換えは application の入口 1 か所。** `EditorController::press_vim_key`（使う人が打った鍵の口）の頭で `key_map_step` を引く。`Pass` はそのまま engine へ。`Pending` は `EditorState.key_map_pending`（`std::vector<VimKey>`・窓に 1 つ）に積んで engine に渡さず、`EditorDelivery.map_timer = start` を ui に頼む。`Emit` は鍵の列を順に engine へ流す（`map` の右辺は 1 段だけ表で引き直す・`noremap` は引かない・入れ子の上限は 1 で無限の連鎖を作らない）。再生の口 `press_vim_keys`（`.`・マクロ・fixture の harness）は表を**引かない**（Vim の `.` はマップ後の鍵を再生する）。engine（`vim_step`）は表を知らない（ARC-012: 鍵の意味は engine の 1 本・置き換えは入口の 1 本）。

4. **待ちはタイマーの意図。** 左辺が 2 打鍵以上の途中は 1 秒まで待つ。時計は adapters だけなので、ui が `SetTimer`（1 秒・1 回）で意図 `MapTimeout` を送り、controller は `key_map_pending` の鍵をそのまま engine へ流して空にする（`AutosaveTick` と同じく `keeps_tab_walk` と `begin_intent` に「使う人の操作ではない」と書く）。不一致の鍵が来たら、積んだ鍵を順に engine へ流してから、その鍵を改めて表で引く（新しい待ちを始めうる）。待ちの間に使う人の操作（IME の確定・マウス・モードを変える意図・ほかの鍵）が来たら、先に積んだ鍵を流してから受ける（積んだ鍵を失わない）。使う人の操作でない意図（`WorkCompleted` `AutosaveTick` `PreviewTick` `LanguageServerTick`・窓の寸法・外観・帯の hover）は待ちを保つ（CLAUDE.md §2「`WorkCompleted` は Vim の待ちを動かさない」・判定は「使う人の操作ではない意図」の一覧 `not_user_operation` の 1 か所で `begin_intent` と共用・2026-10-29 に字面を直した: 最初は `WorkCompleted` を流す側に書いていた）。`EndSession`（窓を閉じる）は積んだ鍵を**捨てる**（流すと保存済みの文書に 1 打目が入って未保存になる・PR #1096 のレビュー）。INSERT の待ちの間はその文字を本文に入れない（Vim と同じ・`jj` の 1 打目は見えない）。待ちの鍵はステータスバーに出さない（限界）。

5. **モード。** `n` は NORMAL（OPERATOR-PENDING は `o`）、`i` は INSERT、`v` `x` は VISUAL（文字・行・矩形の全部）、`o` は OPERATOR-PENDING。今のモードは `VimState` の値から `vim_mode_of` で読む。通常モード（「通常 | Vim」の通常）には効かない。入力行（`:` と検索）の中にも効かない（`cmap` は入れない）。

6. **Ex。** `:nmap` `:imap` `:vmap` `:xmap` `:omap` `:map`（全部）と `noremap` 系（`:nnoremap` …）、`:unmap` `:nunmap` … と `:let mapleader=` は `ExMapRequest`（`ExClipboardRequest` と同じ形: add / remove / list・モードの集合・左辺・右辺・`remap`）の request として controller に届き、設定を書き換えて `persist_settings`（v4）。引数なしの `:nmap` は一覧を Ctrl+P の面（`:tabs` と同じ読むだけの出どころ）に出す。Ctrl+P の `:` の候補に載せる。

7. **記録。** `.`・マクロ・`q` の録画には engine に届いた鍵（替えた後）が載る（Vim と同じ・ADR 0030 / 0046 のまま・何も足さない）。

8. **契約。** application `--key-map`（設定の行 → 表・`inoremap jj <Esc>` で 2 打目に抜ける・`j` の後に `k` で `j` `k` が本文に入る（不一致の流し）・`MapTimeout` で `j` が入る・`noremap` の右辺は引き直さない・`map` の右辺は 1 段だけ・`<leader>` の展開と `mapleader` の変更・`nmap` と `imap` の分離・OPERATOR-PENDING・VISUAL・通常モードでは効かない・`.` に替えた後の鍵・`press_vim_keys` は引かない・待ちの間の IME の確定と `WorkCompleted` で積んだ鍵を失わない・`:nmap` の追加と `persist_settings`・壊れた行の拒否）。adapters: `settings.v4` の `map.*` の往復と拒否。core: `key_map_step` の性質（固定 seed のランダムな表と鍵の列で、流れた鍵の列は「替えた後の列を順に並べたもの」と一致し、`Pending` のまま終わらない）。
   速さ: 表が空なら `press_vim_key` の頭の分岐 1 つ（`KeyMap` が空かの `bool`）。表があるときは 1 打鍵に線形の照合 1 回。8 本のゲートは表が空の既定で不変・PR 本文に A/B（空の表・行 10 の表の単打）を記録する。

9. **限界（後続）。** `<expr>` `<Cmd>` `<Plug>` `<SID>`・`cmap` `abbreviate`・`timeoutlen` の変更・待ちの鍵の表示（showcmd）・`<buffer>` `<silent>` `<nowait>` の引数・通常モードの鍵の置き換え・設定モーダルの「鍵の割り当て」の節（#1016 の後）。

## 強制

- 置き換えが入口の 1 か所で engine が表を知らない: **planned**（契約 `--key-map`・`press_vim_keys` が引かないこと・この PR で active）。
- 時計が adapters だけ: **active**（`eng/symbols.py`）。
- 8 本の速さに退行なし: **active**（QLT-014）。

## 結果

得られるもの: `inoremap jj <Esc>` と `<leader>` が設定で効き、`:nmap` で足せる。engine と fixture は今のまま。
失うもの・残る穴: 左辺 3 打鍵まで・1 秒固定・待ちの鍵が見えない・通常モードには効かない。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| engine の鍵の表（`VimKeyTable`）に動的な行を足す | 鍵の意味の表が 2 種（固定と設定）になり、fixture と oracle の前提（Nib の map を知らない）が崩れる。D72「engine は知らない」 |
| `.nibrc` を読む | 設定の正本が 2 つになる（ADR 0114 / 0170）。D72 で `settings.v4` の行 |
| 待ちを `std::chrono` の deadline で | application は時計を読めない（ARC-007）。タイマーの意図（ADR 0137 の形） |
| 入れ子の `map` を Vim と同じ 1000 段まで | 無限の連鎖の検出が要る。1 段で `map` と `noremap` の違いは表せる |
| `settings.v5` に上げる | v4 は未公開（beta.7 は v3）。接頭辞の族として受け、版は保つ |

## 追補（2026-10-29・PR #1082 の M1a の席が決定の字面から踏み出した点・設計席が受けた）

10. `KeyMapStep` の `Emit` は `keys` に加えて `waiting`（不一致で流した後、今の鍵から始まる新しい待ちの左辺の分）を持つ。決定 4 の「不一致の鍵が来たら積んだ鍵を流してからその鍵を改めて表で引く」を 1 回の `key_map_step` で表す。
11. `map` の右辺の 1 段の引き直しは `key_map_step` の中で済ませて返す（入口ではもう引かない）。右辺が左辺で始まる分は引き直さない（Vim と同じ・無限の連鎖を作らない）。引き直しは同じモードのまま。
12. `key_map_flush`（タイムアウトで積んだ鍵を流す口）は、積んだ鍵に一致する最も長い行があれば右辺に替えて流す（`map.i.j` と `map.i.jj` が両方あるときの Vim の動き）。一致が無ければそのまま流す。
13. `vim_map_mode_of` は 1 鍵の引数を待つ間（`f` `r` `m` `"` `g` `z` の次など・`input_wait` があるとき）は値なしを返し、表を引かない（Vim の `no_mapping`）。
14. 同じ鍵（同じモードの綴りと左辺）の 2 行は設定全体の拒否（`KeyValueFields` の重複の規則・ADR 0114 の厳密な読み）。綴りが違う行（`map.v.x` と `map.x.x`）はファイルの順で後が勝つ。
15. 書いて読み戻せない行（左辺の `=`・端の空白・改行・右辺の先頭の `!`）は `map_row_of` が `unwritable` で断る。`:nmap` 系（M1b）も同じ関数を通す。
16. 鍵の記法の表の生成物は `src/core/VimKeyNames.hpp`（`tests/vim/VimKeyNames.hpp` は消えた・CNF-010 の照合先を直した・ADR 0054 の追補）。
17. 受ける鍵の記法は `KEY_TABLE` の名前（綴りは表どおりで大小を区別する）と `<leader>` / `<Leader>` だけ。`<C-s>` `<cr>` `<Tab>`（表に無い綴り）は設定全体の拒否。足すときは `KEY_TABLE` に行を足す（ADR 0054 の道・CNF-010）。`<C-x>` の形を一般に受けないのは、窓の操作の鍵（Ctrl+S 等）が engine に届かないことと、`<C-r>` のように表で特殊鍵になっている綴りと 2 つの意味を持たせないため。空白の `mapleader` は `<Space>` と書く（端に空白のある値は拒否）。保存で同じ鍵になる 2 行（`map.vn.x` と `map.nv.x`）は読みの時点で拒否する（正準の鍵で判定・追補 14 の形）。
18. 引数なしの `:nmap` 等の一覧（決定 6 の「Ctrl+P の面の読むだけの出どころ」）は、面の行頭の記号を 1 つ足す施主の判断（D29 の系）が要るので planned のまま。それまで `Not supported: listing mappings` の 1 行で断る（PR #1096）。
