# beta.7 の公開（2026-10-27）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（#859 の merge の後の squash）。hide の指示（2026-10-23 夜）「よさそうならベータ版リリースしておいて」→ #865 と #859 の merge の後に出す進め方に「いいよ」（[引き継ぎ 2026-10-23](../handoffs/2026-10-23.md) の手順 2）。hide は 2026-10-27 終日不在なので、D41 の試用は「hide の指示で公開の後に回す」。

## 根拠（再利用する検証）

根拠を書いた時点の main は `215162b8`（#859 まで・公開元・この文書 PR を除く製品の最終）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644）。今日のゲートは #846 の Release（`ec49778`・main `605f21f` の上）で `--check` 8 本 0 regression（`D:/NeNeNib/outputs/20261027/gate-846.log`・中央値 ms: 起動 47.5・窓 35.7・単打 0.540・連打 2.503・16 MiB open 51.8・16 MiB 連打 2.842・面 1.816・長い行 1.915）。公開元の exe で `--check` 8 本を公開の手順 2 で測る。
- (b) 独立レビュー（Sonnet・読むだけ）: #856 #858 #861 #863（2026-10-23・`D:/NeNeNib/outputs/20261022/review-*.md`）・#846 と #874（2026-10-27・`D:/NeNeNib/outputs/20261027/review-846.md` `review-874.md`・ブロッカ無し・R は試験の直しで #874 は差し戻し 1 回で直した）。#865 は差分が小さく設計席が読んだ。
- (c) 各 PR の merge の CI run（`check` と `build-and-test`）が成功（`merge-pr.py` の記録・#865 は run 40411679328・#846 は 40414471993・#874 は `D:/NeNeNib/outputs/20261027/merge-874.log`）。
- (d) hide の試用（D41）: beta.6 の 14 項目は 2026-10-22 夜に OK。beta.6 以降の変更（構文の色分けの 16 言語・拡張子・表引きの門・打鍵の節目）は hide の指示で**公開の後に試用する**（見本 15 ファイルは `D:\NeNeNib\evidence\20261023-trial\files\`・表は引き継ぎの手順 3）。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md) と #798。

## 主な変更（beta.6 `f7f0bf2` から・公開の文に使う）

- **構文の色分けが 12 → 28 言語**（ADR 0156 の決定 19〜22）: GDScript・dosini・Godot の設定（`.tscn` `.tres` `.godot`）・C#・Rust・Go・PHP・SQL・Lua・Dockerfile・PowerShell・shell・CMake（#855 #857 #860 #862）。GDScript の文字列は Godot 4 の tokenizer、Rust の入れ子の `/* */` と `r#"…"#`、PHP のヒアドキュメント、Lua の長い括弧、Dockerfile の `\` の続き、PowerShell のヒア文字列、shell のヒアドキュメント、CMake の括弧の引数。
- **言語の表の拡張子**: `.tsx` `.jsx`（TS / JS に畳む）・`.import`・`.pssc`・`.bashrc` など 6 つの名前・`*.cmake.in`。JS / TS で `</div>` を正規表現と読まない（#864）。
- **字句器の表引きの門**（#859・ADR 0156 追補）: 識別子ごとの表の二分探索を、表から `consteval` で作る門（先頭バイトの 256 ビットと長さ）で先に落とす。probe の ns/byte は Go 0.69〜0.74・C# 0.78・Rust 0.79〜0.84・Lua 0.78〜0.86 倍（分類は不変）。
- **計測の道具**（#844・ADR 0011 の節目に `frame_built` `frame_drawn`）: 単打のベンチが「frame を作る / 描く / 提示する」の段の内訳を出す。製品の振る舞いは不変。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 7 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.7 -Verification docs/quality/beta7-release-2026-10-27.md`。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta7-release/release-smoke.py`）と `python eng/measure-speed.py --check --repetitions 5 --executable <展開した exe>`（静かな機械・席のビルドを止めてから）。
3. 同じ commit に annotated tag `v0.1.0-beta.7` と GitHub pre-release（zip と SHA256SUMS・本文は上の「主な変更」）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

2026-10-27 20:19（JST）に公開した。手順 1〜2 は設計席、手順 3（tag と pre-release）は自動の権限（Create Public Surface）で止まったので hide が `!` で実行し（tag の注釈は折り返しで切れたので `v0.1.0-beta.7 (main c5e72857)` に作り直してから push・pre-release は `D:/NeNeNib/scripts/beta7-release-create.ps1`）、手順 4 の照合は設計席が行った。

- URL: https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.7（pre-release・release id 425358068・published 2026-10-27T11:19:17Z）
- tag `v0.1.0-beta.7` → `c5e72857a4abbf32ed27c16326f8bf302176656b`（main・梱包の commit と同じ）
- 添付: `NeNeNib-0.1.0-beta.7-windows-x64.zip` 972,437 bytes・SHA-256 `c55d797b6a402053676617c020c9e765733ce03789b62f33be36f7a500e393ed`、`SHA256SUMS` 104 bytes
- 照合（`verify-publication.py`・`gh release download` で再取得）: bytes 一致・zip の中身一致・展開した exe の起動 pass（release-smoke-01）→ `result: pass`。写しは `D:/NeNeNib/evidence/beta7-release/github-release.json`
- hide の確認（公開の前・2026-10-27 夜）: 展開した exe で `samples/07-save-check.txt` を開いて 1 行足して保存して閉じる・見た目が beta.6 と同じ → OK（試用の表 `D:/NeNeNib/evidence/20261027-trial/README.md` の手順 B）

教訓: `!` で長い命令を打つと画面の折り返しで改行が入り、引数が切れる（tag の注釈に改行・commit の指定が別の命令に）。hide に頼む命令は 1 行の短いものにし、長いものはスクリプトにして `pwsh -File` で呼ぶ。

- 梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 7 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.7 -Verification docs/quality/beta7-release-2026-10-27.md` が `D:/NeNeNib/worktrees/beta7`（detached・origin/main `c5e72857`）で成功。記録 `D:/NeNeNib/releases/v0.1.0-beta.7/package.json`。exe の SHA-256 `9cba0f717b028bc85389506e1cb63d8084100e500875ec5eeaf029e8a61f1cbb`（2,344,448 bytes）。
- 起動・描画: `python D:/NeNeNib/evidence/beta7-release/release-smoke.py` が成功（隔離 profile・DPI 120・正常終了 0・本文不変・PNG を設計席が見た）。
- 速さ: `python eng/measure-speed.py --check --repetitions 5 --executable D:/NeNeNib/releases/v0.1.0-beta.7/extracted/NeNeNib.exe` → **8 benches checked, 0 regression(s), 0 unmeasurable**（負荷 32/25/19%・中央値 ms: 起動 49.5・窓 35.4・単打 0.540・連打 2.302・16 MiB open 57.7・16 MiB 連打 2.122・面 1.919・長い行 1.558）。記録 `D:/NeNeNib/evidence/beta7-release/gate-beta7.log`。
- 公開物の取違え: 未（公開の後に `verify-publication.py`）。公開の文は `D:/NeNeNib/evidence/beta7-release/release-notes.md`。
