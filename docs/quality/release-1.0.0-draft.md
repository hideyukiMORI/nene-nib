# 1.0.0 の公開（雛形・設計席が本番で埋める）

> 雛形（#1596）。**1.0.0 はまだ公開も申請もしていない**（この雛形を置いた時点の公開版は `v0.1.0-beta.13`）。本番では `release-1.0.0-<日付>.md` に写して埋め、この雛形は残すか消すかを設計席が決める。
> 1.0.0 は Microsoft Store に出す最初の版（施主決定 D88）。GitHub は同じ exe の ZIP を tag `v1.0.0` の pre-release（題に「Beta」）で出す。
> 手順の正本は [DEVELOPMENT_WORKFLOW.md](../DEVELOPMENT_WORKFLOW.md) の公開の節、Store の欄は [submission.md](../store/submission.md)。
> 予行（作業枝・配る物ではない）の記録は `D:/NeNeNib/evidence/20261113-release-rehearsal/README.md`（beta.12 の後の main）と `D:/NeNeNib/evidence/20261114-release-rehearsal/README.md`（beta.13 の後の main）。

## 公開元

| 項目 | 値 |
| --- | --- |
| 公開元の commit（main・この記録の文書 PR の merge の後） | `<40 桁>` |
| 前の公開 | `v0.1.0-beta.13`（`3d3859e6`・2026-11-14 00:16・[記録](beta13-release-2026-11-13.md)） |
| 版（`project()`） | `1.0.0` |
| 梱包の作業木 | `D:/NeNeNib/worktrees/<名前>`（detached・HEAD = origin/main・未追跡 0） |

## 根拠（再利用する検証）

- (a) 速さのゲート（QLT-014）: `<木の SHA>` の Release で 8 本 `<0 regression>`（記録 `<所在>`）。
- (b) 独立レビュー: `<PR と所在>`。
- (c) CI: `<run の URL>` → `<commit>`。
- (d) 施主の試用（D41）: 試用の表 `<所在>`（3 言語の表示・`:set language=`・アイコン・長い `:s` の不具合 #1616 の直しは beta.13 の前の試用で OK・D89。1.0.0 で試すのは beta.13 の後に入った物と packaged の起動）。
- (e) 梱包の道具の試験: `eng/test-package-release.py`・`tests/conformance/test_package_msix.py`。

## 梱包

| 物 | 名前 | bytes | SHA-256 |
| --- | --- | --- | --- |
| exe（`build/release-<short>/NeNeNib.exe`） | `NeNeNib.exe` | | |
| ZIP | `NeNeNib-1.0.0-windows-x64.zip` | | |
| ZIP の `SHA256SUMS` | | | |
| MSIX（Store 用・署名なし） | `NeNeNib-1.0.0.0-windows-x64-store.msix` | | |
| MSIX の `SHA256SUMS` | | | |

照合:

- [ ] `package.json` の `productVersion` と `version` が `1.0.0`、`metadata` の `fileVersion` `1.0.0.0` と `productVersion` `1.0.0`
- [ ] `msix.json` の `executable.sha256` が ZIP の中の exe と同じ（`zip.executableMatches`）
- [ ] `msix.json` の `packageVersion` が `1.0.0.0`・`store` が true
- [ ] manifest の `Resources` が `en-US` `ja-JP` `zh-Hans` の順
- [ ] `msix.json` の `entries` が 32 個（中身 30 個 ＝ exe・`LICENSE`・`THIRD_PARTY_NOTICES.txt`・`AppxManifest.xml`・`resources.pri`・`Assets` 25 枚、に makeappx が足す `AppxBlockMap.xml` と `[Content_Types].xml`）
- [ ] 展開した exe の短い起動（隔離 profile）

## GitHub の公開

- tag: annotated `v1.0.0` → `<commit>`（tag object `<SHA>`）
- Release: 題 `NeNe Nib 1.0.0 Beta`・**pre-release**・URL `<URL>`・本文 `<所在>`
- 添付: ZIP と `SHA256SUMS`
- 再取得の照合: `<結果>`

## Microsoft Store の提出

| 順 | 誰 | 状態 | 日時・記録 |
| --- | --- | --- | --- |
| 自己署名の写しで入れて起動 | hide | | |
| packaged の実機の確かめ（submission.md の表） | hide | | |
| WACK | hide | | 報告 `D:/NeNeNib/releases/v1.0.0/wack/` |
| Partner Center に上げる（署名なしの `-store.msix`） | hide | | |
| 申請 | hide | | |
| 認定 | Microsoft | | |
| Store で公開 | Microsoft | | URL |

## 既知の限界

- `<各 PR の「残るリスク」から>`

## 公開の結果

（公開の後に埋める）
