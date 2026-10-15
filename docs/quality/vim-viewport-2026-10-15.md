# Vim の鍵は viewport を必要なときだけ求める — Issue #476（ADR 0129）

## 変更と確認する退行

`VimEditorView.viewport` を `std::optional<VimViewport>` にし、`first_row` を `VimLayoutView` から `VimViewport` へ移す。viewport を読む鍵（`H M L`・Ctrl-d/u/f/b・NORMAL/VISUAL/INSERT の PgUp/PgDn）は、無ければ `VimStep.viewport_request` を立てて効果なし・state そのままで返り、controller が `settled_scroll()` を読んで同じ鍵を流し直す（`layout_request` と同じ往復・`answered_vim_request` の 1 本）。読む口は `vim_viewport_of` の 1 本。viewport を読まない鍵（INSERT の文字・`h j k l`・`w b`・`x`…）は追従を解決しない。

確認する退行: 画面行の移動の結果（fixture 1853 件）、要求の 1 回目が回数・オペレータ・録画・`.` の記録・レジスタを消費しないこと、折り返しの画面行の計算、再生（`@a` `.`）の列、連打以外の速さ。

## 対象検証（worktree `D:/NeNeNib/worktrees/476-vim-viewport`・実装 bcf4e30・契約の追加 d0a4a61）

| 対象 | 退行 | コマンド・結果 |
|---|---|---|
| core / application の Debug | 型・複雑度・範囲外・未定義動作 | `cmake --build build --target nib_tests NeNeNib nib_perf_probes --parallel 8`（clang-cl 19.1.5・tidy・ASan/UBSan）成功（bcf4e30・d0a4a61 の両方） |
| Vim の全 fixture と engine | 画面行の移動・要求の往復・消費しないこと | `build/nib_tests.exe --application` 16,299 checks、`--vim` 1,174,044 checks（d0a4a61。bcf4e30 では 16,029 / 1,173,831）成功 |
| 折り返し・検索・画面移動・タブ・一覧 | 画面行の位置解決と周辺 | `--vim-wrapped --vim-search --vim-search-incremental --vim-line-jumps --vim-open-lines --vim-dot --vim-macro --vim-visual-block --vim-virtual-column --vim-text-objects --vim-character-search --vim-characters --vim-replace --wrapped-view --tabs` すべて exit 0（bcf4e30。d0a4a61 はテストだけの変更で再利用） |
| 境界・規約・整形 | 外部依存・規約逸脱・空白 | `eng/symbols.py --require core application` 0 違反、`eng/conformance.py` 0 違反、`clang-format --dry-run --Werror` 0、`git diff --check` 0 |

契約（d0a4a61 で広げた形）:

- application `verify_vim_insert_follow_settles_at_frame`: `i` の後に 200 文字を打っても要求は増えず、frame を 1 回読むと `first_visible` と `screen_row` が 1 鍵ごとに読んだ場合と同じ。
- application `verify_vim_viewport_settles_before_screen_keys`: frame を読まずに 12 行打ってから `<Esc>` `H`。`H` の後のキャレット行が、解決済みの先頭行と 1 鍵ごとに読んだ場合の両方に一致。
- engine `verify_vim_viewport_request`: 読む鍵 26（NORMAL の PgUp、VISUAL の Ctrl-d、`V` `<C-v>` の後の `H M L`、録画・`.` の記録・名前つきレジスタの選択の途中を含む）・読まない鍵 11。要求の step が state を消費しないことは `same_vim_memory` が欄ごとに比べる（mode・count・pending・レジスタの選択・insert_repeat・input_wait・wanted_column・scroll_lines・recording・last_change・macro・無名・小削除・クリップボード・名前つき 26・数字 10）。
- engine `verify_vim_wrapped_viewport_request`: 折り返し有効の 7 鍵で、各 step が 1 種類だけを求め、viewport の要求が先に 1 回、行の画面行数の要求はその後だけ。最後は最初から全部を渡した step と一致。

独立レビュー（`out/reports/review-476.md`）: 対応が必要な指摘は F1（再生とまとめて届いた鍵の追従の時期）の 1 件で、ADR 0129 の限界に書いて受けた（6e2cc75）。F2（契約の広さ）は差し戻し 1 命題で上の契約を足した（d0a4a61・製品コードは変えていない）。F3（文書のずれ）は 6e2cc75 で直した。

## Vim INSERT の連打（正式ゲートの外・`D:/NeNeNib/scripts/vim-burst.py`・各 5 試行）

同じ driver で「Vim に切り替え → `i` → 暖機 → 単打 → 200 の `b` をまとめて post → 最後の frame」。

| exe | 中央値 | 全値 |
|---|---:|---|
| main 89c582d | 12.825 ms | 12.679, 15.114, 13.913, 12.751, 12.825 |
| #474 9bff5a3 | 12.585 ms | 12.585, 12.481, 11.424, 12.872, 13.027 |
| #476 bcf4e30 | **3.956 ms** | 3.988, 3.908, 4.000, 3.555, 3.956 |

#474 は Vim の鍵を毎回解決するので変わらず（ADR 0128 決定 5 のとおり）、#476 で通常モードの連打（2.1 ms）に近づいた。残る差は engine の 1 鍵ぶんの費用。JSON は `D:/NeNeNib/outputs/perf-day-20261015/vim-burst-*/vim-burst.json`。

## 正式な速さのゲート（QLT-014・bcf4e30 の Release・負荷 5〜12% の後）

`python eng/measure-speed.py --check --executable build/release-bcf4e30/NeNeNib.exe --repetitions 5` を 1 回。8 本とも基準内・退行 0・欠測 0。

| bench | #474 9bff5a3 | #476 bcf4e30 | 基準（上限） |
|---|---:|---:|---:|
| startup-first-frame | 189.735 | 191.072 | 191.488（239.4） |
| startup-window-shown | 30.252 | 31.311 | 34.933（43.7） |
| key-to-frame-single | 0.637 | 0.631 | 0.906（2.906） |
| key-to-frame-burst-200 | 2.090 | 2.098 | 2.695（3.369） |
| open-large-file-16mib | 202.457 | 204.021 | 249.783（312.2） |
| key-to-frame-burst-200-16mib | 2.227 | 2.184 | 7.411（9.264） |
| key-to-frame-palette-5000 | 1.956 | 1.878 | 2.630（4.630） |
| key-to-frame-single-long-line | 6.183 | 6.226 | 5.431（7.431） |

JSON は `D:/NeNeNib/outputs/perf-day-20261015/gate-476-bcf4e30-speed.json`。基準値・許容は変えない。d0a4a61 以後は製品コードが同一なので、この Release と結果を再利用する。

## 再利用と限界

追従の時期は ADR 0129 の限界のとおり（再生とまとめて届いた鍵）。Ctrl-e/y・`zt zz zb` は engine に無い。成功後は実装・テスト・関連依存が同一なら再実行しない。独立レビューと CI は PR #478（base を main へ変えた後）、施主の試用は D41 のとおり merge の前。Waivers: none。
