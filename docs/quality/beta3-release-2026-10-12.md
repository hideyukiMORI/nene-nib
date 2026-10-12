# 125テーマ入りbeta.3の公開（#432）

2026-10-12 13:46:21 JST、[v0.1.0-beta.3](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.3)をGitHub pre-releaseとして公開した。
公開元はcleanなmain `00ff3f698bb111d342b50d2925a1264223bf0729`。規則: ADR 0115/0117/0118、QLT-001/011/012/013、GIT-001〜004。Waivers: none。

- 出典・依存・同梱漏れ: `pwsh -NoProfile -File eng/package-release.ps1 -Beta 3 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.3 -Verification docs/quality/cinema30-2026-10-12.md`が成功。x64/版/宣言済みDLL12個/展開bytesを確認。exe・README.txt・LICENSE・THIRD_PARTY_NOTICES.txtの4項目を同梱。
- 起動・描画: `python D:/NeNeNib/evidence/beta3-release-432/release-smoke.py`が成功。隔離profileの日本語本文を900×640/DPI120で目視、正常終了0、本文不変。PNG/result.jsonは同恒久先の`release-smoke-01/`。
- 公開物の取違え: `gh release download v0.1.0-beta.3 --repo hideyukiMORI/nene-nib --dir D:/NeNeNib/releases/v0.1.0-beta.3/downloaded`と`python D:/NeNeNib/evidence/beta3-release-432/verify-publication.py`が成功。公開状態、annotated tagのcommit、ZIP/SHA256SUMSの再取得bytesとGitHub digest、ZIP全項目が一致。
- ZIP: `NeNeNib-0.1.0-beta.3-windows-x64.zip`、634904 bytes。SHA-256: `5b0292b8916fed2415a755b813f0427903ac9b41daf0a91884a9e4acb1d9fbcb`。
- 配布物・package.json・build.log・publication-verification.jsonは`D:/NeNeNib/releases/v0.1.0-beta.3/`に保持する。

[125テーマの対象検証](cinema30-2026-10-12.md)と[梱包ツールの9 tests](beta-package-2026-10-11.md)を再利用した。受理版`1aa702b`から公開元まで製品/梱包入力は不変（`git diff --name-only 1aa702b HEAD -- src CMakeLists.txt eng/package-release.ps1 eng/package-release.py eng/build-release.ps1 eng/release eng/tool-versions.json eng/toolchain.ps1 eng/architecture.json`は空）。製品テストを繰り返さない。
README・配色HTML・currentの公開版案内と本記録だけを更新し、文書差分と目視で確認する。CI不要。製品挙動・保存schema変更なし。64 MiB超・split・Markdown・署名など従来の制限を維持する。
統合と作業用コピーの監査整理結果は`D:/NeNeNib/evidence/beta3-release-432/closeout.json`に記録し、配布物・branch・commitを保持する。
