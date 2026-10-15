# 5-ef — ベータZIPの正規梱包経路（Issue #413・ADR 0115）

出典: docs/quality/gate-proofs.md の節 5-ef（2026-10-16 に D50 で移動・本文は移動時のまま）

正規入口`pwsh -NoProfile -File eng/package-release.ps1 -Beta 1 -OutputDirectory D:/NeNeNib/releases/<task-id> -Verification docs/quality/wrap-beta-2026-10-11.md`は、cleanなremote-main一致の作業木から既存build-release.ps1だけでReleaseを作り、版/x64/production宣言DLL/3エントリ/展開bytes/SHAを検査する。D側detached mainも許し、previewは公開用にしない。既存出力/既存build/親リンクは拒否。規則ARC-001/002、QLT-001/011/012/013、GIT-001〜004、waiver none。製品/保存schema/性能基準は不変。

`python -B eng/test-package-release.py --work-dir D:/NeNeNib/outputs/20261011-beta-package/tool-tests-04`: 9 tests成功、2.691秒。初回のjunction見落としをlstatで修正し、同じ範囲の正例/反例を確認。外部build/PE境界だけfixture化した全梱包の正例ではZIP/README版/展開3点/SHA256SUMS/package.jsonを照合。実PEはpackage-inspection-01で49f3393 Releaseを読取り、x64/0.1.0/12 Windows DLL/NotSignedを確認（6.114秒）。実dirty呼出しとPowerShell入口は出力/build前に拒否。全原失敗・commandsと再利用は[詳細](../beta-package-2026-10-11.md)。ソースcommit変更だけで製品テストを再実行しない。

main統合後の実ZIP生成・起動・公開/再取得はこれから行い、結果はD:/NeNeNib/evidence/wrap-beta-20261011のrelease/closeoutへ収載する。署名・インストーラ・自動更新は未対応。ベータの利用者向けREADMEはeng/release/README.txt.inが正本で、通常/Vim/IME/折り返し/保存場所と制限を同梱する。
