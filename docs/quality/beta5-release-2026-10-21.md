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

## 公開の結果（2026-10-21 朝・hide 不在）

2026-10-21 08:01:58 JST（`published_at` 2026-10-20T23:01:58Z・release id 418965274）、[v0.1.0-beta.5](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.5) を GitHub pre-release として公開した。
公開元は main `4329e78309ef73d3ad1596527c3ef691bcbc1a37`（#692 の merge `e383fe0` → 夜の文書 PR #797 → 梱包の道具の直し PR #799 の squash）。annotated tag `v0.1.0-beta.5` は同じ commit。hide の指示（2026-10-21 夜「区切りがついたらタグ打って β リリースしておいて」「公開は明日の朝から」）に従い、設計席が公開した。

- 梱包の道具の直し: 最初の梱包は `CMake must contain one canonical product VERSION` で止まった（#764 が `project()` を `LANGUAGES C CXX` にしたのに `eng/package-release.py` の正規表現が旧のまま）。#798 / PR #799 で正規表現と試験の fixture を直してから（CI の `check` と `build-and-test` 成功・run 39509509437）梱包した。
- 出典・依存・同梱漏れ: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 5 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.5 -Verification docs/quality/beta5-release-2026-10-21.md` が `D:/NeNeNib/worktrees/beta5`（detached・origin/main `4329e78`）で成功（ビルド 172.5 秒・clang-cl 19.1.5）。x64・版 0.1.0・宣言済み DLL 12 本・4 項目（exe・README.txt・LICENSE・THIRD_PARTY_NOTICES.txt 72,930 B・md4c の MIT 入り）・展開 bytes を道具が照合。記録 `D:/NeNeNib/releases/v0.1.0-beta.5/package.json`・`build.log`・`pe-inspection.txt`。
- 起動・描画: `python D:/NeNeNib/evidence/beta5-release/release-smoke.py` が成功。隔離 profile の日本語本文を 900×640・DPI 120 で表示し、正常終了 0・本文不変。`release-smoke-01/launch.png` を設計席が目視（帯・本文の折り返し・ステータスバーの案内と右側 3 項目）。
- 速さ: 梱包した exe で `python eng/measure-speed.py --check --repetitions 5 --executable D:/NeNeNib/releases/v0.1.0-beta.5/extracted/NeNeNib.exe` → **8 benches checked, 0 regression(s), 0 unmeasurable**（負荷 0.7〜2.6%・ほかの席とビルド無し・hide 不在）。中央値 ms: 起動 40.96・窓 31.87・単打 0.648・連打 2.586・16 MiB open 48.89・16 MiB 連打 2.70・面 1.66・長い行 1.918。記録 `D:/NeNeNib/evidence/beta5-release/gate-beta5-2026-10-20T23-01-34Z.json`・ログ `gate-beta5.log`。
  - 連打 2.586（基準 2.16・+20%）・16 MiB 連打 2.70（基準 2.34・+15%）・長い行 1.918（基準 1.57・+22%・beta.4 も 1.91）は許容（25% と下限）の内だが、3 本とも基準より遅い側にある。split の本測定（同じ系統の Release `2958d6c`）では単画面 S が 0.625 / 1.889 / 0.724 で基準の内だったので機械の揺れの可能性が高いが、次の計測（#792 の merge の前）で同じ向きなら調べる。
- 公開物の取違え: `gh release download v0.1.0-beta.5 --repo hideyukiMORI/nene-nib --dir D:/NeNeNib/releases/v0.1.0-beta.5/downloaded` と `python D:/NeNeNib/evidence/beta5-release/verify-publication.py` が成功。公開状態（prerelease・非 draft）、annotated tag の commit、ZIP / SHA256SUMS の再取得 bytes と GitHub digest、ZIP の全項目が一致。記録 `D:/NeNeNib/releases/v0.1.0-beta.5/publication-verification.json`・`github-release.json`。
- ZIP: `NeNeNib-0.1.0-beta.5-windows-x64.zip`、813,303 bytes。SHA-256: `c1202448a20183819ca6f9b224eb3fd3b0e6a0fb19ba51af2431109111a80842`。
- `SHA256SUMS`: 104 bytes。SHA-256: `28fccb87995361318cb408ce3d42b3a78cbe904d448c9d11061aebfbcd752e78`。
- exe の SHA-256: `09c8b6a016ddfce995773578f0d3dac668962ca1fe525be19c3ba4a5c1bcdf1d`（1,946,112 bytes）。
- hide の試用（D41）: 2026-10-21 夜の 19 手順（根拠 (d)）。公開後の試用は beta.5 の exe で。
- 配布物・package.json・build.log・publication-verification.json は `D:/NeNeNib/releases/v0.1.0-beta.5/` に保持する。
