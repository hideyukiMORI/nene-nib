# beta.5 の公開（#796）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（#788・#790・#692 の merge の後の squash）。hide の指示（2026-10-21 夜）「区切りがついたらタグ打って β リリースしておいて」。

## 根拠（再利用する検証）

根拠を書いた時点の main は `cc3b1a64d6b4ab0dd426fa0b2ad5a7892df9044a`（`cc3b1a6`・#790 まで）。公開元はこの上に #692 を merge した main `e383fe0e52378dd4e2a7487c9b127b693f5de696`（`e383fe0`・2026-10-21 07:30 の merge・この文書 PR を除く製品の最終）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644）。公開元の exe で `--check` 8 本 0 regression（公開の手順 2 で測る）。
- (b) split の本測定（ADR 0120・12 対・公開元と同じ系統の Release・[split-speed-2026-10-21.md](split-speed-2026-10-21.md)）。
- (c) 最後の製品 merge の CI run（`check` と `build-and-test`）が成功。
- (d) hide の試用（D41・2026-10-21）: split の 3 本・#701・Ctrl+Shift+Space・Vim の `J` `>` `gU`・colorscheme のプレビュー・UTF-16・長いパス・面の鍵・hc light・`:w! file`・`it` `ip`・`:` の履歴がすべて OK。見つかった小さな直し（#785 #786 #787 #789 #795）は beta.5 の後。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md)。

## 主な変更（beta.4 `5987ebd` から・公開の文に使う）

- **split（分割ウィンドウ）**: Ctrl+\ / Ctrl+Shift+\ で左右・上下 2 枠、F6 で隣へ、Ctrl+Shift+F4 で枠を閉じる。Vim は `Ctrl-w s v w W p h j k l c q o`。同じ文書の 2 表示でも別の文書でも、本文・undo・保存は 1 つ（#682 #684 #683・ADR 0148・D40 / D45・本測定 12 対 pass）。
- **通常 ↔ Vim の切り替えの鍵 Ctrl+Shift+Space**（#771・D61）。
- **Vim**: `J` `gJ`・`~` `g~` `gu` `gU`（ASCII）・`>` `<` `>>` `<<`（#766）、`%` とマーク `m` `'` `` ` `` `''` `'.`（#767・ADR 0152）、テキストオブジェクト `ip` `ap` `is` `as`（#729）・`it` `at`（#745）、`g_` `g^`（#721）、`yH` `yM` と回数つき Ctrl-f / Ctrl-b の端の列（#411）、VISUAL の `j` `k` の端（#730）、文の探索の高速化（巨大な 1 行の `is` 652 → 0.4 ms・#751）。
- **Ex**: `:w! file` `:saveas! file` の上書き（#738）、`:` と検索の行の履歴 Up / Down（#725）、`:noh` の省略と `:wq!`（#501）、補完の名前順と大文字小文字（#696 #712）。
- **面と補完の操作感**: 端を越えるときだけ送る・Tab の巡回・colorscheme の候補を焦点で一時的に見せる（#707・ADR 0150・D59）、面の鍵 PgUp / PgDn / Ctrl+N / Ctrl+P / Ctrl+Home / End（#706）、`vscode-high-contrast-light` の修正（#713）。
- **文字コードとパス**: UTF-16 LE / BE（BOM つき）の読み書き（#497・ADR 0149）、UTF-32 は開かず断る（#734）、260 文字を超えるパス（#498）。
- **ホイール**: 高分解能ホイール・タッチパッドの小さな送りを累積し、1 刻みの行数は OS の設定から（#499）。
- **終了の堅牢化**: OS の終了の強行で未保存を同期に退避してから終わる（#735・ADR 0137 決定 9）。
- **Markdown プレビューの土台**（製品の振る舞いはまだ変わらない）: md4c の取り込み（#764・ADR 0151・CNF-013）、`parse_markdown`（#765）、描画の保持と painter（#784・D62）。枠に出すのは次の版。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 5 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.5 -Verification docs/quality/beta5-release-2026-10-21.md`。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta4-release-678/release-smoke.py` を beta.5 の path で）と `python eng/measure-speed.py --check --repetitions 5 --executable <展開した exe>`（静かな機械）。
3. 同じ commit に annotated tag `v0.1.0-beta.5` と GitHub pre-release（zip と SHA256SUMS・本文は上の「主な変更」）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果

（公開の後に埋める）
