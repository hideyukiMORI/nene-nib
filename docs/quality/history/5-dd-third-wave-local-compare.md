# 5-dd — 保存・断片収集・検索列挙の第三陣局所比較（#346〜#349）

出典: docs/quality/gate-proofs.md の節 5-dd（2026-10-16 に D50 で移動・本文は移動時のまま）

hideの継続実験と今回の分担指定に従い、設計席が入力/区間/採否を決め、別席が実装と対象試験を行った。規則ARC-001/003/007/009、CPP-002/003/005/012/016、QLT-001/008/012/013/014、GIT-003/004。ADR0037の列挙共有とADR0082の固定harnessを使用。waiver none。公開API、設定schema、速度基準/許容、fixture期待値、抑制/allowlistは不変。

### 対象と正しさ

- #346：ADR0082 stage4をf09289dで先行決定し、a93e0e6で固定17処理を追加。既存24処理は37項目の静的一致で保持を確認。Debug/Releaseの追加17smokeは各17成功、Pythonの新定義1試験成功。準備・期待生成・全結果照合・破棄は区間外、実呼出しと結果保持だけがTimingPortの2marks内。保存のFilePort内部copyは計測へ含む。実機I/O時間の測定ではない。
- #347：5ee45bdでprivate encodedがtext()の生成stringを所有として受け、UTF8は移管、BOMは同じ値の先頭へ追加。CP932の同期借用と外部raw防御copyは不変。`nib_tests --file-save`38 / `--ex-document`206 / `--ex-write-path`90、計334checks成功。
- #348 B1：812c835でcollectを既存visit_text_rangeへ集め、空範囲と終端で停止。reserveや集計融合は含めない。`nib_tests --buffer-range`235checks成功。head/tail/断片・改行交差/逆順/本文外/全削除と旧snapshotを直接確認。
- #349：設計fdce98cの後de51fb9で、行頭からの非重複一致列挙をprivateな停止可能処理へ集めた。first/lastと全件強調が同じ列を使い、caretから直接照合しない。`--vim-search`1374 / `--vim-search-highlight`76 / `--vim-search-incremental`141、計1591checks成功。
- 各製品差分の通常Debug/tidy/ASan/UBSan、関連source規約/File API/symbolsは成功。独立read-only reviewは製品3件とharnessを別作者の席で実施し、受理を止める所見なし。自己実装を独立認定した扱いにしない。

### 固定比較と結果

`compare-wave3.py batch-347-348b1-349 --execute`が既存`eng/compare-probes.py`へ固定の各20反復×ABBA3block（12process、120対応組）を渡した。warmup各1は区間外。全席のbuild/test/GUI/重いcopyを止め、前後環境を記録。13条件すべてobserved、欠測/外側retryなし。全sample・metadata・marks・完全checksum・exe SHAを保持する。beforeは同じ41処理harnessのa93e0e6、afterは各製品へ同じharnessを取り込んだbd507e9/a2d534d/d20602d。比は対応組after/beforeの中央値で、列の時間は全sampleの中央値なので両者の商とは限らない。

| 固定処理 | before µs | after µs | 対応比中央値 | 短縮した組/120 |
| --- | ---: | ---: | ---: | ---: |
| UTF8保存16.8MB | 8354.5 | 5197.5 | 0.625208 | 118 |
| UTF8 BOM保存16.8MB | 9298.5 | 6150.0 | 0.657926 | 116 |
| 8193断片・先頭erase16回 | 2639.5 | 2659.5 | 1.001508 | 58 |
| 同・中央erase16回 | 2892.5 | 2700.0 | 0.954840 | 86 |
| 同・末尾erase16回 | 2678.5 | 2786.0 | 1.052850 | 38 |
| 同・全文erase16回 | 224.0 | 2.0 | 0.008316 | 120 |
| 1断片・中央erase16回 | 4.0 | 4.0 | 1.000000 | 同値主体 |
| 前方検索・先頭 | 6432.0 | 56.0 | 0.008857 | 120 |
| 前方検索・中央 | 7297.0 | 3288.5 | 0.454514 | 120 |
| 前方検索・末尾 | 13948.0 | 6202.5 | 0.445879 | 120 |
| 後方検索・先頭 | 12975.0 | 5243.0 | 0.407485 | 120 |
| 後方検索・中央 | 7272.0 | 3317.5 | 0.458164 | 120 |
| 後方検索・末尾 | 8152.0 | 6595.5 | 0.806687 | 120 |

保存2条件と検索6条件で短縮を観測。B1は全文削除の不要走査を除く一方、末尾条件は約5%遅い（16回の中央値差107.5µs、1回あたり約6.7µs）。この代償を消さず採否へ含める。先頭と1断片に改善は主張しない。全文削除のafter1〜2µsは分解能に近く、桁数の多い改善率を保証しない。全値の分布・範囲はsummary.jsonと各comparison JSONが正本。BOMの再確保可能性、piece列生成/集計、行所有copyは残り、RSS/ゼロcopy/全入力の速度を保証しない。

### 証拠と残る受理

原記録は `D:/NeNeNib/outputs/20261010-speed-stability/`、harness `outputs/20261010-probes/`、実装 `outputs/20261010-save-path/`・`outputs/20261010-buffer-paths/`・`outputs/20261010-search-enumeration/`、独立レビュー `outputs/20261010-review-wave3/`。恒久先は `D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/` へ元path/SHA対応付きで収載する。収載前の追加作業木は保持する。#348Aのoffsetと#350照合器は別の入力/commit/計測/受理であり、この13条件の成功に混ぜない。
