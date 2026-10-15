# 5-ep — 宣言サイズまで読み込んでから成功を返す（Issue #463・ADR0124）

出典: docs/quality/gate-proofs.md の節 5-ep（2026-10-16 に D50 で移動・本文は移動時のまま）

既存FilePortの同期readで、短い成功は残りへ進み、0進捗/失敗/過大返却は `unreadable`。反復をadapter内の一つのhelperに集約し、所有stringは全量成功だけ返す。空0回・通常完全read1回、64MiB上限・文字コード・保存schemaを維持する。

固定clang-cl 19.1.5のDebug/tidy/ASan/UBSanで対象build成功、`nib_adapter_tests.exe --files` は追加40を含む139 checks/0 failures、変更C++4ファイルformat、FileAPIを含むconformance 0違反。初回nesting4>3と次のWindows min/max衝突はコードで修正し、原ログを保持した。独立レビューも残る指摘なし。[実行コマンド・失敗と修正・範囲と再利用](../complete-file-read-2026-10-13.md)。

速度改善を主張せず、GUI・性能・2GiB・無関係な全件を実行しない。#452の性能待ちから独立して受理する。ARC/CPP詳細はADR0124、QLT-001/002/004/008/012/013、Waivers none。統合/収載/監査整理は `D:/NeNeNib/evidence/complete-file-read-463/closeout.json`。
