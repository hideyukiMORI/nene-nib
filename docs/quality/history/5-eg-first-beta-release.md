# 5-eg — 初回ベータの公開と再取得（Issue #410 / #413）

出典: docs/quality/gate-proofs.md の節 5-eg（2026-10-16 に D50 で移動・本文は移動時のまま）

PR412/main9afca7f、PR414/main6e1043fを必須CI成功後に正規merge入口で統合。2026-10-11 23:58:23 JST、clean main6e1043fのannotated tag `v0.1.0-beta.1`をGitHub pre-releaseへ公開した。規則D44/FR-019、ARC-001/002、QLT-001/011/012/013/014、GIT-001〜004、ADR0114/0115、waiver none。

package-main-01: 正規`eng/package-release.ps1 -Beta 1`、345.268秒、exit 0。x64/埋め込み版/12宣言DLL/ZIP直下3点/展開bytes成功。release-smoke-01: 展開exe、隔離profile、日本語長行、DPI120のPNG目視、正常終了/本文不変、1.191秒、exit 0。release-download-01: ZIP/SHA256SUMSの再取得bytes・asset digest・ZIP全項目・remote tag commit照合、1.162秒、exit 0。ZIP SHA `5428ba6669c1fdaf6b1dec2a10c61fee0a3a0e55b33ec2137f78ae8bb4c4ca06`。

実コマンドと原記録・配布物の所在・成功再利用・限界は[公開記録](../beta-release-2026-10-11.md)。製品/保存schema/基準に追加変更なし。記録文書だけの差分は正典document_checks、公開値照合、diff --checkで確認し、製品テストを追加しない。恒久収載/監査整理の実結果はD:/NeNeNib/evidence/wrap-beta-20261011/closeout.json、branch/commit/公開ZIP保持。
