# split の速さの本測定（ADR 0120 の条件・#680）— 2026-10-19

設計席 Fable。計画は測る前に固定した（`D:/NeNeNib/outputs/20261019/split-speed-plan.md`・ADR 0120 決定 6）。規則: ADR 0120 決定 2〜7・ADR 0148 決定 14・QLT-014（既存の 8 本の許容は不変）。Waivers: none。

## 条件

- 対象: `feat/683-split-ui` `1d1c882`（#688 `3de9248` + #687 `0853c47` の上に #692 とその差し戻し 1）の Release（`eng/build-release.ps1 -Ref HEAD`・exe SHA-256 `3c627df78167ef469ed7580175a66ac5e0cdeac74fac21fa5ad4b4576fada210`）。
- 機械: hide の実機（i9-10850K / RTX 3090 / 120 dpi・指紋 `bc8a356f37c68491`）。負荷 1〜4%・ほかの席とビルド無し・前面の窓の取り合い無し（`measuring.flag`）。
- 道具: `eng/measure-speed.py`（main `209add2`・#681）。bench は `split-single-key-100`（S）と `split-pane-key-100-{same,other}-{vertical,horizontal}`（P）。100 打鍵を 1 打鍵ずつ、前の打鍵の提示を観測してから送る（`await_quiet` 0.5 s）。各 1 試行。2 枠は製品の鍵（Ctrl+\ / Ctrl+Shift+\・`other` は F6 → Ctrl+T → F6）で作り、`panesDelivered: true` を記録で確かめた。分かれていることは `verify-window.py --split` の PNG で別に確認。
- 本文 3 つ: 空・16 MiB 200,000 行（既存）・長い日本語行 1000 行（既存の fixture）。
- 判定: `--split-verdict S P`（`P − S ≤ max(1 ms, 0.25 S)` かつ `p95(P) ≤ 16.7 ms`・中央値は 50/51 番目の平均・p95 は nearest rank）。

## 結果（中央値 ms・p95 ms・12 対すべて pass）

| 本文 | S | same-v | other-v | same-h | other-h |
|---|---:|---:|---:|---:|---:|
| 空 | 0.589 (p95 0.654) | 0.619 (0.669) | 0.649 (0.716) | 0.608 (0.666) | 0.650 (0.722) |
| 長い日本語行 | 1.837 (1.922) | 1.936 (2.012) | 1.912 (1.978) | 1.892 (1.952) | 1.883 (1.951) |
| 16 MiB | 0.713 (0.788) | 0.741 (0.817) | 0.753 (0.808) | 0.717 (0.780) | 0.739 (0.824) |

- 差（P − S）は最大 0.099 ms（長い行 same-v）。許容は 3 本文とも 1 ms の床（0.25 S は 0.15 / 0.46 / 0.18）。
- p95 は最大 2.012 ms（16.7 の 8 分の 1）。
- 16.7 ms 超: 空の other-v で 1 件（最大 34.1 ms・100 打鍵中 1 つ）。除外も再試行もしない（決定 7）。ほかの 14 試行は 0 件。
- 欠測 0・試行は計画どおり 15 本・順も計画どおり。
- 既存の 8 本（1 枠の道）は同じ系統の exe（`9997b0f`）で `--check` 0 regression（`split-speed/gate-split-9997b0f.log`）。`1d1c882` との差は ui の閉じる向きと表示名だけ。

## 試運転（参考・判定に使わない）

計画の前に `9997b0f` の Release で vertical だけ 6 対（空・長い行・16 MiB × same / other）を測り、全部 pass（差は最大 0.14 ms）。記録 `D:/NeNeNib/outputs/20261019/split-speed/`。

## 限界

- 1 試行ずつなので、試行間の揺れ（長い行の単打は環境で約 20% の 2 つの山がある・同日の診断）は見ていない。ADR 0120 は固定 100 打鍵の 1 試行を求めるので条件は満たす。
- 「同じ文書の 2 表示」の P は、編集が両枠へ反映された提示まで（決定 4）。`other` は空の新しいタブへ打つので、本文は S と違う（#681 のレビュー I-3）。
- 記録: `D:/NeNeNib/outputs/20261019/split-speed/formal/`（15 本の JSON と log・exe の SHA）。
