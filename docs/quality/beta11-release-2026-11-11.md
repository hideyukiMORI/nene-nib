# beta.11 の公開（2026-11-11）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（この文書 PR の merge の後の commit）。beta.10（2026-11-11・tag `v0.1.0-beta.10` = `f43537e6`）からの製品の変更は、2026-11-11 夜のまとめ枝 20（PR #1470・1 本・`a28225d3`）と 21（PR #1490・6 本・`17f0d960` まで）の 7 本（試験の 1 本 #1431 を含む）。**公開は設計席が行う**（tag と pre-release も・施主の指示 2026-11-10 夜「リリースは区切りがあれば設計席が出す」）。

**施主が 2026-11-11 夜の実機の試用で踏んだ固まりを直した（#1477）。** Markdown プレビューを出したまま送ると（往復・エクスプローラーでの切り替えを含む）、UI のスレッドが数十秒止まっていた。原因は ui の `PreviewPainter::paint_spans` が span ごとに「行内コードの面の位置を問う `HitTestTextRange`」と「色を置く `SetDrawingEffect`」を交互に呼び、保持した layout を汚しては要素の全文を組み直していたこと（1 要素を描くたびに span の数 × 全文の組み直しの 2 乗）。#784（2026-10-20）から同じ形で、beta.8・beta.9・beta.10 でも再現する。切り替えは原因ではない（application の要求の数は送りと同じ）。調査では CLAUDE.md の段落（36 KB・span 424）で交互の 424 回が Debug で 364,930 ms（`HitTestTextRange` だけなら 3,351 ms・約 109 倍）。直しは、保持の entry を作るときに行内コードの面の矩形と画像の箱の位置を 1 回だけ引き、色も 1 回だけ置き、描くときは書式を変えず位置も問わない形（ADR 0155 追補 69〜72）。描き直し 20 回の間の `SetDrawingEffect` と `HitTestTextRange` は 0 回になった。記録 `D:/NeNeNib/worktrees/1477-preview-hang/out/reports/done-1477-investigation.md` と `done-1477-impl.md`。**残る限界**: 長い段落を初めて表示する 1 回は、entry を作る費用（span の数 × `HitTestTextRange` と最初の組み直し 1 回）が残り、Release で約 377 ms（424 span・負荷 23〜60% の 1 回）。後続 #1485。

**もう 1 つの柱は、長く編集した文書ほど 1 打鍵が遅くなる穴の直し（#1473・ADR 0180）。** 状態の写し（1 鍵に 3〜12 回）が undo の履歴と piece の列を毎回複製していた。写しで複製しない形にし、振る舞いは不変（undo / redo・まとめた打鍵・改訂番号）。probe `vim-lh-after-{1000,4000,16000}-edits` の `l h`（200 鍵）は、N = 1,000 / 4,000 / 16,000 回の編集の後で 21,522 / 50,902 / 155,787 → 13,682 / 13,878 / 13,663 µs（Release・混んだ機械）で、編集の回数に依らなくなった。piece の列の作り直しは編集 1 回につき O(piece の数) のまま（#1474）。

## 根拠（再利用する検証）

根拠を書いた時点の main は `17f0d960`（まとめ枝 21 の最後の commit・製品の最終）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644・変えていない）。**まとめ枝 21 の木（`47ab0141`・main `17f0d960` と同じ tree）の Release の 1 回は 8 本 0 regression**（起動 42.278・窓が見えるまで 30.173・単打 0.600・200 打鍵 2.852・16 MiB を開く 49.658・16 MiB で 200 打鍵 3.082・面 1.043・長い行 1.141 ms・施主の了承の後・席を止めた機械・ログ `D:/NeNeNib/outputs/20261111/gate-integration21-47ab0141.log`）。**梱包の exe の速さはこの 1 回を再利用する**（QLT-012・製品の入力が同じ木の別のビルド・beta.9 / beta.10 と同じ形）。
- (b) 独立レビュー（読むだけの席）: 各製品 PR に 1 本ずつ（PR #1460 #1471 #1476 #1481 #1484 #1486 #1478・記録はリポジトリの外の `D:/NeNeNib/outputs/20261111/review-1460/` `review-1471/` `review-1476/` `review-1481/` `review-1484/` `review-1486/` `review-1478/`）。まとめ枝は積んだ木の Debug 全 target・ctest・conformance・symbols・format を 1 回ずつ。
- (c) CI: まとめ枝 20 は PR #1470（run [43003061137](https://github.com/hideyukiMORI/nene-nib/actions/runs/43003061137) → `a28225d3`）、まとめ枝 21 は PR #1490（run [43032806338](https://github.com/hideyukiMORI/nene-nib/actions/runs/43032806338) → `17f0d960`）がどちらも success。
- (d) hide の試用（D41・2026-11-11 夜・試用の exe `NeNeNib-trial21-4e63eead.exe` と `NeNeNib-trial22-27ac6510.exe`）: 起動画面は「いい感じ」（無題のタブにも一覧を出すこと・上下の中央寄せは施主の決定）。プレビューの固まりは「よくなった」。ミニマップのクリックとドラッグは「いい感じ」。**試用前**: 矩形の VISUAL の `p` `P`（#1461）・長い編集の直し（#1473）。試用前のものは公開の文に書く。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md) と #798・公開の照合 `verify-publication.py`（beta.10 と同じ・版の文字列だけ直した写しを `D:/NeNeNib/evidence/beta11-release/` に置いた）。

## 主な変更（beta.10 `f43537e6` から・公開の文に使う）

使う人に見える変化:

- **Markdown プレビューの送りで固まらない**（#1477・PR #1484）: 上の冒頭の節。見た目は変えない（画素の試験が同じ）。
- **ミニマップのクリックとドラッグ**（#1472・PR #1476・段 2 の 1 本目・ADR 0171 決定 5）: 面を押すとその行が見えている範囲の中央に来るように窓を送り、押したままのドラッグで追従する。キャレットは動かさない。離す・`WM_CAPTURECHANGED`・Esc で終わり、× の上で離しても閉じない。オフのときの押しの道は比べ 1 つだけ。
- **起動画面**（#1480・PR #1486・施主の試用の案 B）: 出るたびに（前回のタブで起動した後の Ctrl+T の無題のタブでも）最近使ったファイルとブックマークの一覧を裏で読み直す。届くまで前の一覧を見せ、同じ一覧なら選びを保つ。ブロックを本文の面の上下の中央に置く（上の余白は max(36 DIP, (欄 − ブロック) / 2)）。「新しいファイル」の行の右端に Ctrl+T の鍵。
- **長く編集した文書の 1 打鍵**（#1473・PR #1481・ADR 0180）: 上の冒頭の節。
- **Vim の VISUAL の `p` `P`**: 文字単位と行単位は選択をレジスタの本文で置き換える（#1418・PR #1460・`p` は消した本文を無名・`"1`・`"-` へ、`P` はレジスタを変えない・`.` `gv` undo 1 単位）。矩形は矩形を消してからレジスタの本文を貼る（#1461・PR #1478・行単位の本文は終わりの角の行の下 / 上・1 行の文字単位は各行の同じ桁）。`:set clipboard=unnamed` でもクリップボードを読む。

内部の直し:

- **折り返す窓の fixture の provenance**（#1431・PR #1471）: 生成物 `tests/vim/VimWrapFixtures.hpp` の入力 3 つ（`wrap-fixtures.json`・`eng/vim-wrap-oracle.py`・`eng/vim-oracle.py`）の SHA を CNF-010 で照合する（`eng/conformance-rules.json` の `wrapFixtureProvenance`）。製品の振る舞いは不変。
- fixture は 4,224 → 4,486 件（`tests/vim/fixtures.json`）、折り返す窓の fixture は 335 件（不変）。

## 既知の限界（各 PR の「残るリスク」から）

- プレビュー（#1477）: 長い段落を初めて表示する 1 回は Release で約 377 ms（#1485）。
- ミニマップ（#1472）: 見えている範囲の色が薄い（次の版で直す・#1487）。窓の手続きの順（client → 帯 → ミニマップ）は試験が通らない（読みと純関数の試験だけ）。10 万行の文書の端から端のドラッグの費用は測っていない。折り返しで見えている数が変わる文書では、押しの間にほかの道で送ると同じ点でも前と違う行へ行く。DPI の切り替えがドラッグの間に起きる場合は確かめていない。
- 窓の角に小さな点が見えることがある（調査中・#1488）。
- 起動画面（#1480）: 「フォルダを開く」は操作の表に鍵が無い（施主の確認待ち）。ほかの窓が書いた履歴は、この窓が次に書くか起動し直すまで反映されない。一覧が届いて行が増えるとブロックが上下に動く（施主は受けると回答済み・2 回目以降の同じ一覧では動かない）。
- 長い編集（#1473）: piece の列の作り直しは編集 1 回につき O(piece の数) のまま（#1474）。正式な速さのゲートは根拠 (a) の 1 回だけで、この直しを狙ったベンチは probe だけ。
- VISUAL の `p` `P`（#1418 #1461）: `'.` の位置が Vim と違う形がある（#793・#1475）。巨大な回数（`v999999999p`）は消す前に断る（#1341 の方針）。全角の途中への矩形の貼付（#1482）・`zb` の後の失敗する `<C-b>` `<C-y>`（#1483）は後続。
- provenance（#1431）: `eng/vim-oracle.py` を変える PR は折り返す窓の fixture も Vim のある機械で測り直す必要がある。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 11 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.11 -Verification docs/quality/beta11-release-2026-11-11.md`（作業木 `D:/NeNeNib/worktrees/beta11`・HEAD は origin/main と一致・未追跡 0・出力先のフォルダは道具が作る）。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta11-release/release-smoke.py`）。速さは根拠 (a) のまとめ枝 21 の木の 1 回を再利用する（梱包の exe では測らない）。
3. 同じ commit に annotated tag `v0.1.0-beta.11` と GitHub pre-release（zip と SHA256SUMS・本文は `D:/NeNeNib/evidence/beta11-release/release-notes.md`）。`publish.ps1` を設計席が実行する（`$commit` と本文の配布元の `@COMMIT@` を公開元の SHA に埋めてから・打つ前に構文だけ先に確かめる）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

2026-11-11 23:34（JST）に公開した。手順 1〜4 はすべて設計席が行った（tag と pre-release は `D:/NeNeNib/evidence/beta11-release/publish.ps1`・1 回で成功）。

- URL: https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.11（pre-release・release id 441810419・published 2026-11-11T14:34:24Z）
- tag `v0.1.0-beta.11`（annotated）→ `0398365468f22bb3dfb0a4934d53194cf52a8757`（main・梱包の commit と同じ・この記録の文書 PR #1494 の merge）
- 梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 11 ...` が `D:/NeNeNib/worktrees/beta11`（detached・origin/main と一致）で成功。記録 `D:/NeNeNib/releases/v0.1.0-beta.11/package.json`（recordedAt 2026-11-11T14-27-38Z）。exe の SHA-256 `b77cf0a92238173d66a6030fa486a83a9ca177587afb4c7844db2f8a27315194`（3,319,808 bytes）。
- 起動・描画: `python D:/NeNeNib/evidence/beta11-release/release-smoke.py` が pass（release-smoke-01・隔離 profile・DPI 120・正常終了 0・本文不変）。施主が机にいたので、窓を出す smoke は施主の了承の後に回した。
- 速さ: 梱包の exe では測っていない。同じ中身の木（まとめ枝 21 の `47ab0141`）の Release の 1 回（8 本 0 regression）を再利用した（QLT-012）。記録 `D:/NeNeNib/outputs/20261111/gate-integration21-47ab0141.log`。main `17f0d960` と `47ab0141` は `git diff` が空（同じ tree）。公開元は `17f0d960` の上の文書だけの commit（`git diff 17f0d960 03983654` は README.md とこの記録の 2 ファイル）。
- 添付: `NeNeNib-0.1.0-beta.11-windows-x64.zip` 1,382,247 bytes・SHA-256 `52b017461af27d0a731d3d713a18af03e9bda1876079e9e6304f1c161b5c5a01`、`SHA256SUMS` 105 bytes
- 照合（`verify-publication.py`・`gh release download` で再取得）: bytes 一致・zip の中身一致・tag の commit 一致・展開した exe の起動 pass（release-smoke-01）→ `result: pass`。写しは `D:/NeNeNib/releases/v0.1.0-beta.11/publication-verification.json` と `github-release.json`。
- hide の試用: 公開の前（2026-11-11 夜）に起動画面・プレビューの固まり・ミニマップのクリックとドラッグ。試用前のもの（矩形の VISUAL の `p` `P`・長い編集の直し）は公開の文に書いた。公開の後は、まとめ枝 22 の試用の exe で本文の面の角・ミニマップの色・Mica を外した窓のリサイズを見た（[quality-day-2026-11-11.md](quality-day-2026-11-11.md) の実機の試用の章）。
