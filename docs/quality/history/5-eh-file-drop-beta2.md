# 5-eh — ファイルのドロップとbeta.2（Issue #416・ADR 0116）

出典: docs/quality/gate-proofs.md の節 5-eh（2026-10-16 に D50 で移動・本文は移動時のまま）

UIのWS_EX_ACCEPTFILES / WM_DROPFILESを既存の変換/FilePath/OpenDocumentへ接続し、PR417/main8da81e3へ受理した。FR-020、ARC-001/002/003、CPP-002/004/005/006/014/016、QLT-001/002/004/011/012/013、GIT-001〜004。Waivers none、保存schema不変。

対象15 checks、selector3反例、通常Debug/tidy/ASan/UBSan、conformance source/build各0 violation、変更C++整形が成功。初回のselector複雑さ12を表で解消した。Release実窓へposted通知を送り、日本語・複数順序・通常/Vim・未保存保持・重複排除・フォルダの通知後の継続を確認。session.v2の一意な4パスと4 fixtureのbytes一致も成立。Explorerの物理ジェスチャは窓外終点の操作ツール制約で未測。同期SendMessageの未到達等を成功扱いしない。[対象検証](../file-drop-2026-10-12.md)。

2026-10-12 02:16:59 JSTにv0.1.0-beta.2を公開。clean mainから正規梱包355.693秒、配布exe起動/正常終了/本文不変1.207秒、再取得/公開照合1.773秒/1.190秒、各exit0。ZIP SHA55fef1fc41823ccd068fa9078d3b892c4860d2250e08ee6e2d2289f5c8a69e0c、608411 bytes。12宣言Windows DLL、ZIP直下3点、remote tag commitとGitHub digestまで一致。[公開記録](../beta2-release-2026-10-12.md)。

既存tabs291と梱包9 testsは関連入力不変の前回結果を再利用し、全件/無関係な性能は選ばない。文書の記録は正典document_checksと公開値/空白だけを照合する。恒久先D:/NeNeNib/evidence/file-drop-20261012、実整理結果はcloseout.json。branch/commit/tagと公開ZIPを保持する。
