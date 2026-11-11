# beta.9 の公開（2026-11-11）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（この文書 PR の merge の後の commit）。beta.8（2026-11-04・`e556b0e2`）からの製品の変更は、2026-11-10 のまとめ枝 5〜14 と 2026-11-11 のまとめ枝 15・16 で、main の commit は 53 本（文書を除いて 46 本）。**公開は設計席が行う**（tag と pre-release も・施主の指示 2026-11-10 夜「リリースは区切りがあれば設計席が出す」）。

## 根拠（再利用する検証）

根拠を書いた時点の main は `786b4161`（まとめ枝 16 の最後の commit・製品の最終）。**計画の変更（2026-11-11 昼）**: 1 回目は main `7ceb2bdd`（まとめ枝 15 まで）で梱包と smoke まで済ませた（tag は打っていない・`D:/NeNeNib/releases/v0.1.0-beta.9-rc-7ceb2bdd/`）。その後の独立レビューが、幅のドラッグ（#1380）に「離した後、次のクリックまで左右矢印のカーソルが残る」穴を見つけたので、直し（#1413）とレビュー済みの 7 本を入れたまとめ枝 16 の後の main から出し直す。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644・変えていない）。2026-11-11 朝、ほかの席とビルドの無い機械で `eng/measure-speed.py --check --repetitions 5` を 1 回ずつ: main `04a45f4c`（まとめ枝 14 まで）の Release は 8 本 **0 regression**（起動 43.5・窓が見えるまで 33.5・単打 0.628・200 打鍵 2.821・16 MiB を開く 50.8・16 MiB で 200 打鍵 3.044・面 1.028・長い行 1.096 ms・負荷 2%。この 1 回は 07:10〜07:13 で、施主の入力が計測の窓に混ざった可能性がある）。まとめ枝 15 の木（`b9e8cb01`・main `7fe00360` と同じ中身）の Release は 8 本 **0 regression**（起動 40.9・窓 31.1・単打 0.616・200 打鍵 2.813・16 MiB を開く 49.0・16 MiB で 200 打鍵 3.050・面 1.032・長い行 1.100 ms・負荷 0%・07:46〜07:49）。**まとめ枝 16 の木（`7eb8d4cc`・main `786b4161` と同じ中身）の Release は 8 本 0 regression**（起動 42.4・窓 31.5・単打 0.607・200 打鍵 2.233・16 MiB を開く 48.8・16 MiB で 200 打鍵 3.071・面 1.104・長い行 1.068 ms・席のビルドと試験を止めた機械・12:48〜12:50）。**梱包の exe の速さはこの 1 回を再利用する**（QLT-012・製品の入力が同じ木の別のビルド。beta.8 までは梱包の exe でも測ったが、今日は席を何度も止めないため・設計席の決め）。2026-11-10 は `d8f33693`（まとめ枝 10）まで 0 regression（[quality-day-2026-11-10.md](quality-day-2026-11-10.md)）。
- (b) 独立レビュー（読むだけの席）: 各製品 PR に 1 本ずつ（[quality-day-2026-11-10.md](quality-day-2026-11-10.md) の表・`D:/NeNeNib/outputs/20261110/review-*/`）。まとめ枝は積んだ木の Debug 全 target・ctest・conformance・symbols・format を 1 回ずつ。
- (c) CI: まとめ枝 5〜14 の run（[quality-day-2026-11-10.md](quality-day-2026-11-10.md) に run の番号）と、まとめ枝 15 の run [42927203000](https://github.com/hideyukiMORI/nene-nib/actions/runs/42927203000)（4 commit → `7fe00360`）・まとめ枝 16 の run [42955390831](https://github.com/hideyukiMORI/nene-nib/actions/runs/42955390831)（8 commit → `786b4161`）がすべて success。
- (d) hide の試用（D41・2026-11-10 夜）: 保存の箱・閉じるとすぐ消える・復元を聞かれない・Ctrl+Shift+M の巡回・エクスプローラーの幅のドラッグが OK。**試用前**: エクスプローラーのクリック 1 回で開く（D80）・端の行の ↑ ↓（D82）・幅のドラッグ中の Esc とカーソルの直し（#1413・窓の状態機械の自動の試験は無い）・通常モードの Ctrl+N ほか（#1395）・ミニマップ（既定オフ）・Vim の `Ctrl-e` `Ctrl-y` `{ } ( )` Del ほか（oracle の fixture で守る）。試用前のものは公開の文に書く。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md) と #798・公開の照合 `verify-publication.py`（beta.8 と同じ）。

## 主な変更（beta.8 `e556b0e2` から・公開の文に使う）

- **エクスプローラー**: ファイルの行はクリック 1 回で開く・フォルダの行はクリックで開閉（D80・#1388）。面と本文の境のドラッグで幅を変え `settings.v4` の `explorer_width` に覚える（D78・#1380）。
- **「保存しますか」の箱**（#1282）: TaskDialog の自前のボタン（保存する / 保存しない / キャンセル）。
- **窓を閉じる**（#1340）: 先に窓を隠してから一覧と履歴を書く（567 → 2.6 ms）。
- **Markdown プレビュー**: Ctrl+Shift+M は「閉じている → 側の枠 → 全面 → 閉じている」と巡る（D77・#1354）。画像の段 1（D79・#1383・解析と参照の解決と port だけで、画像はまだ出ない）。
- **通常モードは Windows の標準へ**（D82・#1386）: 最後の画面行の ↓ は本文の末尾・最初の画面行の ↑ は本文の先頭。
- **Vim**: `Ctrl-e` `Ctrl-y`（#1232）・段落と文の移動 `{ } ( )`（#1316）・Delete キー（#1325）・レジスタの接頭辞の前後の回数（#1319）・レジスタつきの `.`（D76・#1337）・`map` の右辺のモード（#1276 #1266）と録画（#1312）・巨大な回数の `@` と `p`（#1341・文書の上限で断る）。
- **Vim の直し（差分 fuzz の生成器 5・6）**: `so` が窓より大きい跳び（#1214）・回数つきの `H` `L`（#1293）と `zt` の後の `y`（#1294）・先頭の行が窓より高い折り返す窓（#1307）・検索の移動で空になる範囲（#1344）・nroff のマクロの行と改ページの段落の境（#1359）・回数つきの 1 画面の送りの端（#1362）・矩形のオペレータの後の `gv` とマーク（#1361）・回数の続きの `0` と `map`（#1311）・巨大な 1 行の大きな回数（#1269）ほか。fixture は 3,906 件。
- **堅さ**: 窓（バイト列の一部）の端で UTF-8 を割らない 1 本（#1292）・合図は上下の移動の狙いの横位置を保つ（#1296）・Vim の engine を通らない意図の後もキャレットを 1 文字の境に寄せる（#1309）。性質の試験（UTF の往復・印刷の頁・テーマのファイル・shell の登録計画・混ぜた意図の列・自動退避の組・LSP の枠・置換の全件）と、名指しの scope を全部回す `nib_unit_scopes`（#1324）。
- **まとめ枝 16（2026-11-11）**: エクスプローラーの幅のドラッグ中の Esc は押す前の幅へ戻す・ドラッグの後に左右矢印のカーソルが残る穴の直し（#1413）／通常モードに Ctrl+N・Ctrl+Insert・Shift+Insert・Shift+Delete・Ctrl+Shift+Z は redo（#1395・D82・選択なしの Shift+Delete はメモ帳の engine の実測どおり何もしない）／ミニマップ 段 1（#1357・既定オフ・`:set minimap`・オフの費用は独立レビューの読みで 20〜60 ns）／置換の全件の結果が 1 GiB を超えるなら確保の前に断る（#1376）／Vim: `> < ~ u U r` の後はマークの行を動かさない（#1360）・1 桁目から 1 桁目までの `d<BS>` `c<BS>` はレジスタへ行単位で書く（#1364）・`q` `"` `@` の後の Esc は鳴らさずに打ち消す（#1369）／文とタグの 1 文字の窓の歩きの 1 本化（#1322・振る舞い不変）。fixture は 3,952 件。
- **道具**: fuzz の生成器 6（#1353）・fixture を窓の道でも再生する試験（#1326）・まとめ枝の merge の後の待ち直し（#1345）。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 9 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.9 -Verification docs/quality/beta9-release-2026-11-11.md`（作業木 `D:/NeNeNib/worktrees/beta9`・HEAD は origin/main と一致・未追跡 0・出力先のフォルダは道具が作る）。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta9-release/release-smoke.py`）。速さは根拠 (a) のまとめ枝 16 の木の 1 回を再利用する（梱包の exe では測らない）。
3. 同じ commit に annotated tag `v0.1.0-beta.9` と GitHub pre-release（zip と SHA256SUMS・本文は `D:/NeNeNib/evidence/beta9-release/release-notes.md`）。`publish.ps1` を設計席が実行する。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

2026-11-11 13:41（JST）に公開した。手順 1〜4 はすべて設計席が行った（tag と pre-release は `D:/NeNeNib/evidence/beta9-release/publish.ps1`・施主の指示 2026-11-10 夜）。

- URL: https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.9（pre-release・release id 441246953・published 2026-11-11T04:41:44Z）
- tag `v0.1.0-beta.9`（annotated）→ `5cc9ffade3a61dac7a19ebc99fc0d97ca6f992d3`（main・梱包の commit と同じ・この記録の 2 本目の文書 PR #1443 の merge）
- 梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 9 ...` が `D:/NeNeNib/worktrees/beta9`（detached・origin/main `5cc9ffad`・未追跡 0）で成功。記録 `D:/NeNeNib/releases/v0.1.0-beta.9/package.json`。exe の SHA-256 `f84a12a7629bc77fc66eb4253ed779b675187f5879abaef054df73cba873df42`（3,153,408 bytes）。
- 起動・描画: `python D:/NeNeNib/evidence/beta9-release/release-smoke.py` が pass（隔離 profile・DPI 120・正常終了 0・本文不変・PNG を設計席が見た）。
- 速さ: 梱包の exe では測っていない。同じ中身の木（まとめ枝 16 の `7eb8d4cc`）の Release の 1 回（12:48〜12:50・8 本 0 regression・席のビルドと試験を止めた機械）を再利用した（QLT-012）。記録 `D:/NeNeNib/outputs/20261111/gate-integration16-7eb8d4cc.log`。
- 添付: `NeNeNib-0.1.0-beta.9-windows-x64.zip` 1,313,735 bytes・SHA-256 `0f690aa71474e733279f2ec9f3522e65ee89146b9c400fd63a7bb5938d2f7d67`、`SHA256SUMS` 104 bytes
- 照合（`verify-publication.py`・`gh release download` で再取得）: bytes 一致・zip の中身一致・tag の commit 一致・展開した exe の起動 pass（release-smoke-01）→ `result: pass`。写しは `D:/NeNeNib/releases/v0.1.0-beta.9/publication-verification.json` と `github-release.json`。
- 1 回目の梱包（main `7ceb2bdd`・まとめ枝 15 まで・09:47）は tag を打たずに捨てた（`D:/NeNeNib/releases/v0.1.0-beta.9-rc-7ceb2bdd/`・証跡 `release-smoke-rc-7ceb2bdd/`）。独立レビュー V-1421 が幅のドラッグの後にカーソルが残る穴を読みで見つけ、直し（#1413）を入れてから出し直した。
- hide の試用（公開の前・2026-11-10 夜）: 保存の箱・閉じるとすぐ消える・復元を聞かれない・Ctrl+Shift+M の巡回・幅のドラッグ。公開の時点で試用前のもの（クリック 1 回で開く・端の行の ↑ ↓・幅のドラッグ中の Esc とカーソルの直し・Ctrl+N ほか・ミニマップ）は公開の文に書いた。

教訓: `publish.ps1` の 1 回目は、ancestor の確かめの行が PowerShell の構文として誤りで落ちた（tag を打つ前）。外へ出る命令のスクリプトは、打つ前に構文だけ先に確かめる。速さのゲートは席が走っていると静かにならないので、席に「今の 1 本が終わったら次を始めずに待って」と送ってから測る（待ちの上限で勝手に測る形にしない）。止めた背景のスクリプトは子が生き残ることがある（1 回目のゲートのスクリプトが生き残って測った）。
