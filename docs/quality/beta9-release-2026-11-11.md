# beta.9 の公開（2026-11-11）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（この文書 PR の merge の後の commit）。beta.8（2026-11-04・`e556b0e2`）からの製品の変更は、2026-11-10 のまとめ枝 5〜14 と 2026-11-11 のまとめ枝 15 で、main の commit は 44 本（文書を除いて 38 本）。**公開は設計席が行う**（tag と pre-release も・施主の指示 2026-11-10 夜「リリースは区切りがあれば設計席が出す」）。

## 根拠（再利用する検証）

根拠を書いた時点の main は `7fe00360`（まとめ枝 15 の最後の commit・製品の最終）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644・変えていない）。2026-11-11 朝、ほかの席とビルドの無い機械で `eng/measure-speed.py --check --repetitions 5` を 1 回ずつ: main `04a45f4c`（まとめ枝 14 まで）の Release は 8 本 **0 regression**（起動 43.5・窓が見えるまで 33.5・単打 0.628・200 打鍵 2.821・16 MiB を開く 50.8・16 MiB で 200 打鍵 3.044・面 1.028・長い行 1.096 ms・負荷 2%。この 1 回は 07:10〜07:13 で、施主の入力が計測の窓に混ざった可能性がある）。まとめ枝 15 の木（`b9e8cb01`・main `7fe00360` と同じ中身）の Release は 8 本 **0 regression**（起動 40.9・窓 31.1・単打 0.616・200 打鍵 2.813・16 MiB を開く 49.0・16 MiB で 200 打鍵 3.050・面 1.032・長い行 1.100 ms・負荷 0%・07:46〜07:49）。2026-11-10 は `d8f33693`（まとめ枝 10）まで 0 regression（[quality-day-2026-11-10.md](quality-day-2026-11-10.md)）。
- (b) 独立レビュー（読むだけの席）: 各製品 PR に 1 本ずつ（[quality-day-2026-11-10.md](quality-day-2026-11-10.md) の表・`D:/NeNeNib/outputs/20261110/review-*/`）。まとめ枝は積んだ木の Debug 全 target・ctest・conformance・symbols・format を 1 回ずつ。
- (c) CI: まとめ枝 5〜14 の run（[quality-day-2026-11-10.md](quality-day-2026-11-10.md) に run の番号）と、まとめ枝 15 の run [42927203000](https://github.com/hideyukiMORI/nene-nib/actions/runs/42927203000)（4 commit → `7fe00360`）がすべて success。
- (d) hide の試用（D41・2026-11-10 夜）: 保存の箱・閉じるとすぐ消える・復元を聞かれない・Ctrl+Shift+M の巡回・エクスプローラーの幅のドラッグが OK。**試用前**: エクスプローラーのクリック 1 回で開く（D80）・端の行の ↑ ↓（D82）・Vim の `Ctrl-e` `Ctrl-y` `{ } ( )` Del ほか（oracle の fixture で守る）。試用前のものは公開の文に書く。
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
- **道具**: fuzz の生成器 6（#1353）・fixture を窓の道でも再生する試験（#1326）・まとめ枝の merge の後の待ち直し（#1345）。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 9 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.9 -Verification docs/quality/beta9-release-2026-11-11.md`（作業木 `D:/NeNeNib/worktrees/beta9`・HEAD は origin/main と一致・未追跡 0・出力先のフォルダは道具が作る）。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta9-release/release-smoke.py`）と `python eng/measure-speed.py --check --repetitions 5 --executable <展開した exe>`（負荷を先に読む）。
3. 同じ commit に annotated tag `v0.1.0-beta.9` と GitHub pre-release（zip と SHA256SUMS・本文は `D:/NeNeNib/evidence/beta9-release/release-notes.md`）。`publish.ps1` を設計席が実行する。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

（公開の後に埋める）
