# beta.8 の公開（2026-11-04）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（この文書 PR の merge の後の commit）。beta.7（2026-10-27・`c5e72857`）からの製品の変更は 10-27 夜の速さの 7 本・10-28〜10-30 の品質向上の日 3 日分・11-03 と 11-04 のまとめ枝 4 本（製品 PR 36 本と道具 2 本）で、製品の commit は 159 本。

## 根拠（再利用する検証）

根拠を書いた時点の main は `39caaa94`（まとめ枝 4 の `b40ea348` ＋ 文書 PR 4 本・この文書 PR を除く製品の最終は `b40ea348`）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644・変えていない）。今日のゲートはまとめ枝 4 本の木それぞれの Release で `--check` 8 本 **0 regression**（[quality-day-2026-11-04.md](quality-day-2026-11-04.md) §2・§6.1・§8・§10。最後のまとめ枝 4 の木 = main `b40ea348` の値: 起動 41.0・窓が見えるまで 30.6・単打 0.59・200 打鍵 3.15・16 MiB を開く 52.8・16 MiB で 200 打鍵 2.99・面 1.08・長い行 0.86 ms・負荷 5%）。10-27〜10-30 の各日の main のゲートも 0 regression（各日の記録）。
- (b) 独立レビュー（読むだけの席）: 各製品 PR に 1 本ずつ（10-28〜11-04 の記録の表・`D:/NeNeNib/outputs/<日付>/review-*.md`）。まとめ枝は積んだ木の Debug 全 target・ctest・conformance・symbols・format を 1 回ずつ。
- (c) CI: まとめ枝 1〜4 の run [41740575152](https://github.com/hideyukiMORI/nene-nib/actions/runs/41740575152)（16 commit → `a9e93eec`）・[41771475498](https://github.com/hideyukiMORI/nene-nib/actions/runs/41771475498)（10 → `c6e77916`）・[41790476108](https://github.com/hideyukiMORI/nene-nib/actions/runs/41790476108)（6 → `40cc1467`）・[41806832885](https://github.com/hideyukiMORI/nene-nib/actions/runs/41806832885)（4 → `b40ea348`）がすべて success。10-27〜11-03 の PR は 1 本ずつの CI（各日の `merge-pr.py` の記録）。
- (d) hide の試用（D41）: 10-29・10-30 の試用の表と 11-04 の表（`D:/NeNeNib/evidence/20261104-trial/README.md`・24 項目）。2026-11-04 夜に hide が `NeNeNib-main-b40ea348.exe` で LSP の診断（E の件数・波線・キャレットの行の文・打鍵の引っかかり無し）と普段の操作 6 つ（置換・全面プレビュー・印刷・ダブルクリック・matchparen・速さの印象）を確認して OK。項目 2 の「Ctrl+Shift+M を 2 回で全面」は表の書き方の誤りで、製品は ADR 0155 の決定 16（全面は F1 の一覧か `:preview full`）と決定 27（全面で Ctrl+Shift+M は側へ）のとおり。鍵を足すかは hide の判断（beta.9 以降）。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md) と #798・公開の照合 `verify-publication.py`（beta.7 と同じ）。

## 主な変更（beta.7 `c5e72857` から・公開の文に使う）

- **速さ**（10-27・ADR 0007 / 0091 / 0155 の追補）: 長い日本語の行の打鍵（字体選択の区切りの覚え・1.9 → 0.84〜1.18 ms）・描く前に待つ `Present(0, 0)`（画面に出るまで 37 → 14.5 ms）・面の行の layout の保持・プレビューの bitmap・面の照合の ASCII の速道・タブの題名の layout の保持。
- **言語サーバー（LSP）段 L0 / L1**（ADR 0160 / 0161・D60）: `settings.v4` に `lsp=on` と `lsp.<language>.command=` を書くと `.cpp` などで診断が届く。波線の下線・ステータスバーの右端に件数（`E2 W1`）・キャレットの行の診断の文・`:set lsp` で止め直し。既定はオフ。
- **メモ帳の役の操作**（D71・FR-031）: 通常モードの検索 Ctrl+F / F3・置換 Ctrl+H（Enter は 1 つ・Ctrl+Enter は全件）・行へ移動 Ctrl+G・印刷（F1 の「印刷」・`:hardcopy`・本文と同じ字形）・Ctrl+ホイールで字の大きさ・F5 で日時・保存の文字コードと改行の選択（ステータスバーのクリック・`:set ff=`）・エクスプローラーの右クリックへの登録（F1・HKCU・D73）。
- **エクスプローラー**（ADR 0162 / 0170・D66）: `explorer=on` と `root=` でフォルダの木を左に出す（鍵・マウス・IME の絞り込み・フォルダ選び）。
- **Markdown**: 全面プレビュー（F1 の「Markdown プレビュー（全面）」・`:preview full|side|off`・描いた文字の選択とコピー・ADR 0155 改訂）・タスクリストのチェックボックス（クリックで `[ ]` ↔ `[x]`・D74）。
- **Vim の定番と `map`**（D71 / D72・ADR 0174〜0177）: `gv` `gi`・INSERT の `Ctrl-w` `Ctrl-u` `Ctrl-r {reg}`・`Ctrl-a` `Ctrl-x` と VISUAL の `g Ctrl-a`・ジャンプの列 `Ctrl-o` `Ctrl-i` と `g;` `g,`・`:set ic scs rnu so=`・矩形 VISUAL の `I A c C > <` と `~ u U`・`gI`・移動 `_`・`:N`・`:set clipboard=unnamed`・`map` / `nmap` / `inoremap` など（`settings.v4` の `map.<mode>.<lhs>=` と `mapleader`）・matchparen（既定オン）・Vim の追従を `update_topline` に揃え（`scrolloff` と `so=999`）。
- **Vim の直し（差分 fuzz）**: 本物の Vim 9.1 と突き合わせる `eng/vim-fuzz.py`（生成器 3〜5・固定 seed）が見つけた違いを 40 本余り直した（`''` の起点・`*` の全角・`"_p`・非 ASCII の大小 `~ gu gU`・語の分類の表・結合文字の検索・画面移動と `z` の位置合わせ・矩形の端・INSERT の矢印と BS・回数つきの `Ctrl-f/b` `zz` ほか）。fixture は 2077 件。
- **ダブルクリック**（hide の要望・#1250）: 語をダブルクリック、行をトリプルクリックで選ぶ。Vim モードは VISUAL（`gv` で選び直せる）。
- **開く道**（ADR 0159）: 読みは worker の塊の仕事で、上限は 1 GiB（復号は 64 MiB まで・D63）。巨大な 1 行の矩形 `I A r p .`・INSERT の `Ctrl-w`・`99999999h` が索引の道で一瞬。
- **分割と門**（ADR 0158）: `EditorController.cpp` と `VimStep.cpp` と `EditorWindow.cpp` を翻訳単位に分け、3,000 行の門 CNF-014 を active に。性質の試験 20 本余り（固定 seed）。
- **道具**: まとめ枝の rebase merge（`merge-pr.py --integration`・ADR 0021 追補）・CI に clang-format の検査・probe の Ex の候補の表の conformance・`verify-window` の配色の面。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 8 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.8 -Verification docs/quality/beta8-release-2026-11-04.md`（作業木 `D:/NeNeNib/worktrees/beta8`・HEAD は origin/main と一致・未追跡 0）。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta8-release/release-smoke.py`）と `python eng/measure-speed.py --check --repetitions 5 --executable <展開した exe>`（静かな機械・負荷を先に読む）。
3. 同じ commit に annotated tag `v0.1.0-beta.8` と GitHub pre-release（zip と SHA256SUMS・本文は上の「主な変更」）。tag と pre-release は hide の手（1 行の命令・beta.7 の教訓）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

（公開の後に埋める）
