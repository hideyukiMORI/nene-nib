# 1 行 16 MiB の基準値の採り直し（問いの待ちを除いた定義・#704）— 2026-10-20

設計席 Fable。#689（PR #693・main `cc2d484`）で `open-giant-line-16mib` の値を `body_presented − (question_answered − question_shown)`（問いのダイアログを出してから答えが返るまでの待ちを除く）にしたので、基準値の「はい」2 本（D52・ADR 0138 決定 4・判定の 8 本には入れない）を新しい定義で採り直した。規則: QLT-014（許容 25%・下限 2 ms は不変・判定の 8 本の基準値は不変）・ADR 0138。Waivers: none。

## 条件

- 対象: main `cc2d484` の Release（`eng/build-release.ps1 -Ref HEAD`・exe SHA-256 `6FF7014E428B3CDEA9D1EE60EC6E8561A0B8BDBEA4CFA9A161310572D1912C89`）。
- 機械: hide の実機（指紋 `bc8a356f37c68491`）。負荷 3%・ほかの席とビルド無し・前面の窓の取り合い無し。
- `python eng/measure-speed.py --record --bench <name> --repetitions 10 --executable build/release-cc2d484/NeNeNib.exe`（「いいえ」は `--grid-answer no`）。採用は `--adopt --bench <name> --values <記録>`。

## 結果（ms・10 試行）

| ベンチ | 答え | 中央値 | 最小 | 最大 | 問いの待ちの中央値（値に入れない） | 前の基準値（待ちを含む） |
|---|---|---:|---:|---:|---:|---:|
| `open-giant-line-16mib` | はい（格子） | **82.369** | 66.571 | 96.373 | 45.6 | 129.641 |
| `open-giant-line-16mib` | いいえ（text） | 81.982 | 64.805 | 89.604 | 44.9 | 161.7（記録だけ） |
| `key-to-frame-single-giant-line-16mib` | はい | **1.351** | 1.035 | 1.585 | — | 1.232 |

- 「はい」と「いいえ」の開く時間は待ちを除くと同じ（前の差 32 ms は問いの待ちの揺れだった）。
- 10 試行の分布は 66〜96 で、2 つの山（≈110 / ≈150）は消えた。残る幅（約 30 ms）は 16 MiB の読み（`document_opened` 26.5 ms）と格子の最初の frame の揺れで、次に判定に載せるなら試行数 10 の中央値で見る。
- 1 打鍵の 1.351 は前の 1.232 より 10% 大きいが、同じ exe の 10 試行の中央値（前は 5 試行）で、許容の内。新しい中央値をそのまま基準値にする（合格まで測り直さない・QLT-010）。
- 記録: `D:/NeNeNib/outputs/20261020/giant-adopt/`（`yes-10.json` `no-10.json` `key-yes-10.json` と log）。

## 変更

`eng/perf-reference.json` の `bc8a356f37c68491` の 2 本（`open-giant-line-16mib` 129.641 → 82.369・`key-to-frame-single-giant-line-16mib` 1.232 → 1.351）。判定の 8 本・許容・CI の指紋は不変。
