# ファイルドロップ対応beta.2の公開と照合（#416）

2026-10-12 02:16:59 JST、[v0.1.0-beta.2](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.2)をGitHub pre-releaseとして公開した。
PR417を必須CI成功後に正規eng/merge-pr.pyでsquash統合したmain `8da81e36209b4700aa38559390245b34d6fa4690`が公開元である。
規則: FR-020、ADR 0115/0116、ARC-001/002/003、CPP-002/004/005/006/014/016、QLT-001/002/004/011/012/013、GIT-001〜004。Waivers: none。

## 成果と対象検証

FileDropとEditorWindowでWS_EX_ACCEPTFILES / WM_DROPFILESを受け、既存to_utf8 / FilePath / OpenDocumentへ渡す。
複数ファイル、通常/Vim、日本語パス、未保存タブの保持、既存タブの再選択、エラー後の継続に対応する。
ui_win32のShell32依存をCMake/architecture.jsonへ宣言し、仕様/ADR/配置規約/README/配布案内を更新した。保存schema変更なし。

新しい対象テスト15 checks、selectorの3反例、Debug/tidy/ASan/UBSan、conformanceと整形は成功。
実窓へposted WM_DROPFILESを送り、本文/タブ/通知を目視し、session.v2の一意な4パスと元の4 fixtureのbytesを照合した。
最初の未保存タブを含む計5タブと、元の文字列UNSAVED DROP TESTの保持も確認した。
Explorerからの物理ドラッグは、操作ツールが窓をまたぐ終点を拒んだため未測。受け取り側の実窓検証と区別する。
初回のselector静的解析失敗、同期SendMessageの未到達、Vim命令を貼付けにした刺激ミスも[対象検証](file-drop-2026-10-12.md)に残した。

## 配布物の検証

OUT=`D:/NeNeNib/outputs/20261012-file-drop`。各commands/labelにcommand/cwd/時刻/終了コード/stdout/stderrを保存した。
配布物とpackage.jsonは`D:/NeNeNib/releases/v0.1.0-beta.2/`へ保持する。

| 対象・退行理由 | 実行と結果 | 原記録 |
|---|---|---|
| main出典・版/依存/同梱漏れ | cleanなD側detached mainで`pwsh -NoProfile -File eng/package-release.ps1 -Beta 2 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.2 -Verification docs/quality/file-drop-2026-10-12.md`。355.693秒、exit 0。内部Release126 steps、340.780秒 | package-beta2、build.log、package.json、pe-inspection.txt |
| 展開された配布exeの起動と描画 | `python OUT/release-smoke.py`。正典window_driver、隔離profile、日本語長行、DPI120/900×640のPNG目視、正常終了0・本文不変。1.207秒、exit 0。hideが追加了承した02:15〜02:20の時間帯内に実施 | release-smoke-01、同名フォルダのlaunch.png/result.json |
| tag/public asset/ダウンロードbytesの取違え | annotated tagを指定commitへ作り、`gh release create --verify-tag --prerelease`。`gh release download v0.1.0-beta.2`と`python OUT/verify-publication.py`がローカルbytes・GitHub digest・ZIP全項目・remote peeled commitを照合。1.773秒/1.190秒、各exit 0 | release-download / publication-verification、公開先のgithub-release.json/remote-tag.txt/publication-verification.json |

- ZIP: `NeNeNib-0.1.0-beta.2-windows-x64.zip`、608,411 bytes。直下はNeNeNib.exe・README.txt・LICENSEのみ。
- ZIP SHA-256: `55fef1fc41823ccd068fa9078d3b892c4860d2250e08ee6e2d2289f5c8a69e0c`
- exe SHA-256: `d8741eb3658253712826df20c6ffb6fad1622d91a43976abaeac9b984fbdbdcd`、1,489,408 bytes。
- PE: x64、ProductVersion 0.1.0 / FileVersion 0.1.0.0、宣言済みWindows DLL 12個と一致、NotSigned。

package.jsonのlaunched/published=falseは生成時点の事実として維持し、その後の実施を別記録で証明する。
GitHubのdraft=false/prerelease=true、ZIPとSHA256SUMSの2 assets、再取得bytes一致を確認済み。

## 再利用・限界・整理

core/application/adaptersと既存unit testsはbase9efd71cから不変で、前回のtabs291 checksを再利用した。
機能の受理版からmainへのsquashとclean配布ビルドでは関連入力が不変なので、製品テストや性能を繰り返さない。
梱包ツール自体も変更しておらず[前回の9 tests](beta-package-2026-10-11.md)を再利用する。
今回の公開記録文書には正典conformance.document_checks、公開値照合、diff --checkだけを使う。

64 MiB超、split、Markdown、Vimの範囲ドラッグ/gj/gk/g0/g$、署名・インストーラ・自動更新は従来の別範囲。
既存#411/#365/#373と旧split判定holdも変更しない。ロードマップ#408は自動開始しない。

恒久先は`D:/NeNeNib/evidence/file-drop-20261012/`。commands/失敗/PNG/入力fixture/配布記録、検証exeとビルドメタデータをmanifestのbytes/SHAで照合し、source.bundleにtaskのbranch/commit/tagを保持する。
この文書PRのmain統合と収載後、416-file-drop/416-release-mainの2作業木と一時出力20261012-file-dropだけを監査整理する。
絶対path・取込状態・未保存/未追跡/無視ファイル・唯一の成果物・稼働参照・リンクを確認し、再生成できるobjects/CMake状態/bytecodeは整理する。
実施結果は同恒久先closeout.jsonで判断する。公開ZIP、branch/commit、既存の本体と通常試遊を保持する。
