# split の速さの本測定（ADR 0120 の条件・#680・merge の前の最終版）— 2026-10-21

設計席 Fable。2026-10-19 の本測定（[split-speed-2026-10-19.md](split-speed-2026-10-19.md)・試作 `1d1c882`）の後、hide の試用（2026-10-21・全部 OK）を経て、3 本を main に追いつかせた最終版で測り直した（外部講評 2026-10-20 の求め: rebase 後の最終ビルドで再測定）。規則: ADR 0120 決定 2〜7・ADR 0148 決定 14・QLT-014。

## 条件

- 対象: `feat/683-split-ui` `2958d6c`（main `cc3b1a6`（#790 まで）の上に #692 の固有 4 commit・#794 の gutter の直し込み）の Release（`eng/build-release.ps1 -Ref 2958d6c`・exe SHA-256 `45740B8D5D2D00FEE4476FD3D2C90ABC435D3DF71756C72EEDAC521460D159B3`・1,946,112 B）。
- 機械: hide の実機（i9-10850K / RTX 3090 / 120 dpi・指紋 `bc8a356f37c68491`）。負荷 3 回読んで 10% 未満・ほかの席とビルド無し（hide 入浴中・`measuring.flag`）。
- 道具: `eng/measure-speed.py`（main）・台本 `D:\NeNeNib\scripts\20261021-split-formal.py`（15 record → 12 verdict → 8 本の `--check`）。
- 判定: `--split-verdict S P`（`P − S ≤ max(1 ms, 0.25 S)` かつ `p95(P) ≤ 16.7 ms`）。

## 結果（中央値 ms・p95 ms・12 対すべて pass）

| 本文 | S | same-v | other-v | same-h | other-h |
|---|---:|---:|---:|---:|---:|
| 空 | 0.625 (p95 0.698) | 0.647 (0.871) | 0.637 (0.724) | 0.604 (0.768) | 0.538 (0.679) |
| 長い日本語行 | 1.889 (2.043) | 1.670 (2.064) | 1.891 (2.049) | 1.690 (2.018) | 1.845 (2.029) |
| 16 MiB | 0.724 (0.869) | 0.627 (0.822) | 0.723 (0.852) | 0.707 (0.871) | 0.590 (0.812) |

（S と P の対応は `outputs/20261021/split-speed/formal/*.json` の bench 名と本文で引く。上の表の列は record の順。）

- 16.7 ms 超: 15 試行とも 0。欠測 0。
- 8 本の `--check`（同じ exe・5 runs）: 0 regression・0 unmeasurable（`split-speed/gate.log`）。
- 記録: `D:/NeNeNib/outputs/20261021/split-speed/`（`formal/`・`verdicts.json`・`gate.log`・`summary.md`）。

## 無効にした 1 回目

同日の 1 回目（`cc3e7aa`・#790 の前の rebase）は 11 pass・1 fail（16 MiB other-horizontal・trial が欠測）。測定の途中で hide が打鍵していた（「ごめん、打ち込んでた」）ので、無効として `D:/NeNeNib/outputs/20261021/split-speed-invalid-1/` に残し、条件を満たしてから最終版で 1 回だけ測り直した（合格まで繰り返していない・CLAUDE.md §2）。8 本の `--check` は 1 回目も 0 regression。

## 判断

ADR 0120 の条件を最終版でも満たす。hide の試用（D41）は 2026-10-21 に完了。#692 を merge し、beta.5 の公開元とする。
