# beta.6 の公開（2026-10-22 夜）

公開の記録。前半は公開の前に書いた根拠、末尾の節が公開の結果（公開の後に埋める）。公開元は main の最新（#806 の merge の後の squash）。hide の指示（2026-10-22 夜・試用 14 項目 OK の後）「ベータ版をリリースできる？」→ 設計席が D41 の 4 つを確かめて公開する。

## 根拠（再利用する検証）

根拠を書いた時点の main は `1812e6bf`（#806 まで・公開元・この文書 PR を除く製品の最終）。

- (a) 速さの基準値 [speed-adoption-2026-10-18.md](speed-adoption-2026-10-18.md)（#644）。今日のゲートは #810 / #811 / #824 / #841 と main `dc06f5d` の 5 回とも 0 regression（記録 `D:/NeNeNib/outputs/20261022/gate-*.json`）。公開元の exe で `--check` 8 本 0 regression（公開の手順 2 で測る）。
- (b) 独立レビュー（Sonnet・読むだけ）11 本: #807 #810 #811（前半・後半）#812 #820 #821 #824 #835 #836 #841 #842（`D:/NeNeNib/outputs/20261022/review-*.md`）。
- (c) 各 PR の merge の CI run（`check` と `build-and-test`）が成功（merge-pr の記録）。
- (d) hide の試用（D41・2026-10-22 夜・`D:/NeNeNib/evidence/20261022-trial/README.md` の 14 項目）: 通常 ↔ Vim の切り替え・補完の面の余白と知らせの行・UTF-32 の文・断りの音（鳴る 3 / 鳴らない 3）・文書の言語と狭い窓・**Markdown プレビュー**（開く・差し替え・追従・F6 とクリック・閉じる・12 MiB で固まらない・16 MiB 超は断る）・改行の索引（`gg` `G` `5000gg`）・窓の角の縁（案 A で消えた）・リッチ貼り付け（Markdown に変換・文字だけ・`.js` は HTML のまま・一覧から HTML のまま）・深い HTML・構文の色（C/C++・JSON・Python・Markdown・JS/TS・HTML・YAML・格子・IME・`:syntax on|off`）がすべて OK。見つかった点は後続 Issue（#847 `:N`・#848 `clipboard=unnamed`・#849 Ctrl+O の絞り込み）で beta.6 の後。
- (e) 梱包ツールの tests [beta-package-2026-10-11.md](beta-package-2026-10-11.md) と #798（`LANGUAGES C CXX` の版の検出）。

## 主な変更（beta.5 `4329e78` から・公開の文に使う）

- **Markdown プレビュー**（FR-007・D7・D62）: Ctrl+Shift+M で右の枠にプレビュー。打つのを止めると 0.2 秒で差し替わり、本文のスクロールに追従。見出し・段落・太字斜体・リンク・箇条書き・引用・区切り線・コードの 8 つの構文。16 MiB までの文書（#800 #801 #813・ADR 0155）。
- **構文の色分け**（S-1〜S-4・ADR 0156）: C / C++・JSON・Python・Markdown・JavaScript / TypeScript・HTML / XML・CSS・YAML・TOML の 12 言語。見えている行だけを 200 行前から読む（1 打鍵 1.0〜1.2 ms）。格子表示と IME の変換中の行にも色。`:syntax on|off` は文書ごと（#815 #816 #829 #830 #833 #838 #839 #827）。
- **文書の言語**（ADR 0154）: 拡張子の表で決め、`:set filetype=` で上書き、ステータスバーの右端に表示。狭い窓では畳む（#776）。
- **リッチ貼り付け**（ADR 0157・M-7〜M-9）: Markdown の文書で Ctrl+V はクリップボードの HTML を記法に変換、Ctrl+Shift+V は常に文字だけ、HTML / JS / TS の文書は HTML のまま、「HTML のまま貼り付け」は F1 の一覧から（#774 #840）。
- **断りの音**（ADR 0153）: `:` と検索の確定の断り・ファイル操作の失敗で OS の警告音。鍵の失敗では鳴らない（#789）。
- **通常 ↔ Vim の切り替え**で画面を動かさず、キャレットを見えている端の行へ（#785）。補完の面と入力行の文字の余白 12 DIP（#786）。UTF-32 の断りの文（#787）。窓の縁の境界線を帯の色に（#795）。
- **改行の索引**を 64 KiB の塊ごとの疎 / 密の 2 表現に（英文で本文の 3.5%・全 LF でも 13%）。64 MiB の上限の正本を 1 か所に（#792・ADR 0121 追補・巨大ファイル 段 1）。
- **堅牢化**: md4c のコールバックの `bad_alloc` を受けて `insufficient_memory` で返す（#817）。HTML → Markdown の変換は入れ子の深さ 64 と出力 16 MiB で止める。
- **計測の道具**: プレビューの差し替えの費用の probe と任意ベンチ（#818・16 MiB で parse 143 ms / 高さ測り 6 ms・プレビューを開いた 1 打鍵 3.19 ms）。

## 公開の手順

1. main の detached 作業木で梱包: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 6 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.6 -Verification docs/quality/beta6-release-2026-10-22.md`。
2. 展開した exe の smoke（隔離 profile・`D:/NeNeNib/evidence/beta6-release/release-smoke.py`）と `python eng/measure-speed.py --check --repetitions 5 --executable <展開した exe>`（静かな機械・hide に打鍵を控えてもらう）。
3. 同じ commit に annotated tag `v0.1.0-beta.6` と GitHub pre-release（zip と SHA256SUMS・本文は上の「主な変更」）。
4. `gh release download` で再取得し、tag の commit・bytes・digest を照合（`verify-publication.py`）。

## 公開の結果（2026-10-22 夜・hide 在席）

2026-10-21 20:16:42 JST（`published_at` 2026-10-21T11:16:42Z・release id 419465676・記録の通し番号は 2026-10-22）、[v0.1.0-beta.6](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.6) を GitHub pre-release として公開した。
公開元は main `f7f0bf243d5675fb975e7a07820298562826cef5`（#806 の merge `1812e6b` → 記録の文書 PR #851 の squash）。annotated tag `v0.1.0-beta.6` は同じ commit。

- 出典・依存・同梱漏れ: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 6 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.6 -Verification docs/quality/beta6-release-2026-10-22.md` が `D:/NeNeNib/worktrees/beta6`（detached・origin/main `f7f0bf2`）で成功（ビルド込み）。4 項目・宣言済み DLL・展開 bytes を道具が照合。記録 `D:/NeNeNib/releases/v0.1.0-beta.6/package.json`。
- 起動・描画: `python D:/NeNeNib/evidence/beta6-release/release-smoke.py` が成功（隔離 profile・DPI 120・正常終了 0・本文不変）。
- 速さ: 梱包した exe で `python eng/measure-speed.py --check --repetitions 5 --executable D:/NeNeNib/releases/v0.1.0-beta.6/extracted/NeNeNib.exe` → **8 benches checked, 0 regression(s), 0 unmeasurable**（負荷 3〜5%・hide は打鍵を控えた）。中央値 ms: 起動 43.68・窓 33.24・単打 0.648・連打 2.530・16 MiB open 52.21・16 MiB 連打 2.759・面 1.733・長い行 1.932（標本に 1.548・2 山は調査中）。記録 `D:/NeNeNib/evidence/beta6-release/gate-beta6-2026-10-21T11-16-16Z.json`。
- 公開物の取違え: `gh release download` と `python D:/NeNeNib/evidence/beta6-release/verify-publication.py` が成功。公開状態（prerelease・非 draft）・annotated tag の commit・ZIP / SHA256SUMS の再取得 bytes と GitHub digest・ZIP の全項目が一致。記録 `D:/NeNeNib/releases/v0.1.0-beta.6/publication-verification.json`。
- ZIP: `NeNeNib-0.1.0-beta.6-windows-x64.zip`、933,857 bytes。SHA-256: `34c8f7b3d5b6c19675db8a8986aeb08a36b0bc27ea0eeee5e08c828a6980aeb7`。
- `SHA256SUMS`: 104 bytes。SHA-256: `07a6a2afe36df124de7084af089a0b87e240c3926ab3bc287fa7b046fb709047`。
- exe の SHA-256: `1356b83f42b76dfcb21c2e4df2316e8b125dc6369928c8484ef311cd82ca56f6`（2,226,688 bytes）。
- hide の試用（D41）: 公開の前に 14 項目（根拠 (d)）。
