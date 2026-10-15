# 5-cy — 固定した処理を窓なしで前後比較する道具（Issue #329・ADR 0082）

出典: docs/quality/gate-proofs.md の節 5-cy（2026-10-16 に D50 で移動・本文は移動時のまま）

hideの「閉じた小さな部分を対応前後で比べる」指示に従い、開発target `nib_perf_probes` と `eng/compare-probes.py` を追加。八つの固定workloadを同じharnessで実行し、入力生成・ポート準備・正しさの照合は測定の外へ置く。時計とファイル出力は既存Win32TimingAdapterだけを使う。製品には未使用の二つのMilestoneと名前の網羅分岐を追加しただけで、製品の入力・描画経路は不変。

**確認する退行。** 違う本文・未実行の処理・欠測を改善と誤認すること、新しいtargetの依存違反、結果の選別と上書き。固定ABBA・既定3block/各20sampleの全値、対応比、exe/入力hash、build metadataを記録し、欠測・timeout・checksum不一致は非0。性能の合否閾値は設けない。

| 対象 | 実行したコマンド・結果 |
| --- | --- |
| 比較器 | `python -X utf8 -m unittest discover -s tests/conformance -p test_compare_probes.py -v` — 8テスト成功。壊れたmarks、片側失敗、timeout、metadata、checksum差、上書き拒否を含む |
| Debug/Release target | 固定toolchain、各CMake構成で `cmake --build build/probes-<debug/release> --target nib_perf_probes --parallel 2` — 成功、警告・clang-tidy・DebugのASan/UBSanを維持 |
| 八場面とCLI | 両構成で各warmup1+sample1、本文/行数/count/undo/redo/marksを照合して全終了0。未知名・反復0/2049は終了2 |
| 比較器の実行 | 同じRelease exeでdisplay-line-long、ABBA1block/各1sample — observed、4全値/2対応比を保存。同一exeの比0.815705は性能差の根拠に使わない |
| 規約・依存・整形 | 変更源のconformanceとCMake File API依存検査0違反。`python eng/symbols.py --build-dir build/probes-debug --require core application` — 2library/0違反。変更C++の固定clang-formatとgit diff --check成功 |
| レビュー | 実装席と別の設計席がC++の測定境界、正しさ確認、Pythonの全値保存/失敗処理、CMake/層の変更を読んで確認。統合を止める所見なし |

初回Debugで新しい入力生成関数のnesting4がclang-tidyに拒否された。private関数へ分割して4a42aa0で修正し、対象TUを再buildした。初回ログも保存。製品runtime・全件回帰は実行していない。対象は比較器そのものであり、正式QLT-014や製品性能の受理を代替しない。

**証拠と再利用。** 実装commit `4a42aa09c160d441283a23b214de725082c19a52`。cleanで明示configure後、build中のソース変更なし。Release probe SHA256 `d602d19400243299a55bc33663d2919c087ec8117bdad004802171d33714e928`。`D:/NeNeNib/evidence/329-scoped-probes/` に実装報告・全対象ログ・最小実行JSON・probe exeを収載した。以後はこの記録と進捗文書だけで、src/tests/eng/CMakeListsの一致をもって成功結果を再利用する。configure時のcommit/dirtyは自動追跡されないので、比較版はclean commit→明示configure→buildで作る。

規則: ARC-001/002/003/007、CPP-002/005/007/012/016、QLT-001/012/013/014、GIT-003/004。ADR 0082、PROJECT_LAYOUT、利用文書を追加。製品設定schema・perf-referenceは不変。waiver: none。
