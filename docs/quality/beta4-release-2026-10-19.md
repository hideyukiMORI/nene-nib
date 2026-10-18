# beta.4 の公開（#678）

公開の前の記録。公開元は clean な main `f6916d72d14e6e51480af94ecbe04450c4c6b554`（`f6916d7`・origin/main と一致）。規則: ADR 0115 / 0118、QLT-001 / 011 / 012 / 013、GIT-001〜004、CNF-005。Waivers: none。

## 根拠（再利用する検証）

- (a) 速さの基準値の採用 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644・main `b3e6f01`・既存 8 本 0 regression）。
- (b) 最後の製品 merge（PR #671・#665）の CI run 39137022515（2026-10-18T14:40Z・`Merge PR #671 at 1082cf0`）で `check` と `build-and-test`（ADR 0136 の Release 全 target と ctest）が成功。
- (c) hide の試用（D41）が各 PR の merge の条件だった。
- (d) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md)。

beta.3（`00ff3f6`）から公開元までの製品・梱包入力の commit は 43 本（`git log --oneline 00ff3f6..HEAD -- src CMakeLists.txt eng/package-release.ps1 eng/package-release.py eng/build-release.ps1 eng/release eng/tool-versions.json eng/toolchain.ps1 eng/architecture.json | wc -l`・本 PR の README を含まない数）。基準値の計測元 `b3e6f01` から公開元までの製品の差分（`git log --oneline b3e6f01..HEAD -- src CMakeLists.txt`）は次の 7 本で、個々の PR の CI と hide の試用で受理済みなので製品テストを繰り返さない。

- #639 縦の位置合わせ `zt` `zz` `zb` `z<CR>` `z.` `z-`
- #653 `gj` `gk` の最初に欲しい列を画面行で埋めた長さ − 1 に切る
- #654 縦の位置合わせ `z+` `z^`
- #660 `{N}|` で行の N 桁目（仮想桁）へ
- #664 画面行の答えを 1 つの layout の行の metrics から作る
- #661 `g0` `g$` `gm` `gM`
- #665 Vim の問いの往復の破れを `inconsistent` として鍵を拒否し 1 行知らせる

## 主な変更（公開の文に使う）

- 起動が約 190 ms → 40 ms（WARP の最初の frame #485・device の先行生成 #481・DLL の遅延読み込み #479・追従の遅延解決 #474・Vim の viewport は要求で #476）。
- 自動退避 30 秒（`autosave.v1`・#515・ADR 0137）。
- 巨大な 1 行（16,384 文字超）の「格子表示で開きますか」と `:set display=`（D53・#545〜#547・ADR 0139 / 0140）。
- Vim の `gj` `gk`（#596 / #653）・`g0` `g$` `gm` `gM`（#661）・`{N}|`（#660）・`zt` `zz` `zb` `z<CR>` `z.` `z-` `z+` `z^`（#639 / #654）・`zl` `zh` `zL` `zH` `zs` `ze`（#634）。
- 終了と I/O の堅牢化: ログオフ・シャットダウンは未保存があれば止める（#493）・一時ファイルの排他と回収（#494）・クリップボードの失敗を知らせる（#496）・描画を失っても保存を聞く（#492）・Ex と検索の行の IME の確定（#635）・Vim の問いの往復の破れは鍵を拒否して 1 行知らせる（#665）。

## 公開の手順（これから行う）

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 4 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.4 -Verification docs/quality/beta4-release-2026-10-19.md`。
2. 展開した exe の smoke（隔離 profile）。
3. 同じ commit に annotated tag `v0.1.0-beta.4` と GitHub pre-release（zip と SHA256SUMS）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合。

公開の結果（zip の bytes・SHA-256・公開時刻・照合）は公開後の PR でこの記録に足す。
