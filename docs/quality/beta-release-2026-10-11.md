# 初回ベータの公開と照合（#410 / #413）

2026-10-11 23:58:23 JST、[v0.1.0-beta.1](https://github.com/hideyukiMORI/nene-nib/releases/tag/v0.1.0-beta.1)をGitHub pre-releaseとして公開した。公開元は`6e1043f2e6b0d018de23576b04ed03e7ba74e116`。PR412で機能、PR414で梱包ツールを、必須CI成功後に正規`eng/merge-pr.py`経由でmainへ統合した。

規則: D44、FR-002/003/012/014/019、ARC-001/002/003/007/010/012、QLT-001/011/012/013/014、GIT-001〜004、ADR 0114/0115。Waivers: none。

## 配布物と実行結果

原コマンド記録は`D:/NeNeNib/outputs/20261011-wrap-beta/commands/<label>/`。配布物と検査結果は`D:/NeNeNib/releases/v0.1.0-beta.1/`へ保持する。

| 対象・退行理由 | 実行と結果 | 原記録 |
|---|---|---|
| mainと配布exeの出典、版/依存/同梱漏れ | `pwsh -NoProfile -File eng/package-release.ps1 -Beta 1 -OutputDirectory D:/NeNeNib/releases/v0.1.0-beta.1 -Verification docs/quality/wrap-beta-2026-10-11.md`。D側のclean detached mainで実行、345.268秒、exit 0。内部Releaseは125 steps/330.922秒 | package-main-01、build.log、package.json、pe-inspection.txt |
| 展開物が起動して本文を描画できる | D側`release-smoke.py`から正典window_driverを使用。展開exeのSHAを照合し、隔離profileと日本語長行を開き、900×640/DPI120のPNGを目視。正常終了0、本文bytes不変、1.191秒 | release-smoke-01、launch.png、result.json |
| タグのcommitと公開先のbytesの取り違え | annotated tagのremote peeled commitが上記mainと一致。`gh release create --verify-tag --prerelease`後、`gh release download`でZIP/SHA256SUMSを再取得。`verify-publication.py`が両ファイルとGitHub asset digest、ZIP内全項目のbytesを照合、1.162秒、exit 0 | release-download-01、github-release.json、remote-tag.txt、publication-verification.json |

ZIP名は`NeNeNib-0.1.0-beta.1-windows-x64.zip`、605,903 bytes。直下はNeNeNib.exe・README.txt・LICENSEの3点だけ。

- ZIP SHA-256: `5428ba6669c1fdaf6b1dec2a10c61fee0a3a0e55b33ec2137f78ae8bb4c4ca06`
- exe SHA-256: `99aa39d950791ef102b7dfc7273c4c549ed353dec2ebc2ce68298fe21d62480f`（1,483,776 bytes）
- PE: Windows x64、ProductVersion 0.1.0 / FileVersion 0.1.0.0。DLLはproduction依存グラフで宣言したWindows DLL 12個と完全一致。NotSigned。

package.jsonの`launched: false` / `published: false`は生成時の事実であり改変しない。その後の起動と公開は独立した上記記録で証明する。GitHubのdraft=false、prerelease=true、公開assetがZIPとSHA256SUMSの2個であることも読取確認した。

## 製品検証の再利用と限界

[機能の対象検証](wrap-beta-2026-10-11.md)と[梱包ツールの検証](beta-package-2026-10-11.md)を再利用した。実装、対象テスト、関連依存は受理版と同じで、squashや配布用clean buildだけを理由に製品テスト/性能を繰り返していない。公開用の起動は展開exeの接続確認だけ。CIはGit規約・検証記録・空白の検査であり、製品テストの実行証拠ではない。

既定wrap、通常ドラッグ、カーソル/選択/IME/スクロールは対象契約と実機で確認済み。settings.v3/session.v2へ移行し、旧版不在時の読込と旧ファイル保持を含む。nowrapの初回長行退行は可視字形の収集範囲を修正し、修正後はnowrap/wrapとも既存QLT-014内。nowrapの旧版比+0.549 ms（中央値、約10.8%）は残る費用として記録する。

64 MiB超、split、Markdown、Vimドラッグとgj/gk/g0/g$は未対応。既存の列位置差#411、#365/#373、複数monitor/DPI実機移動未測を残す。署名、インストーラ、自動更新も今回の範囲外。正式版のロードマップ#408は別の開始指示を待つ。

## 恒久収載と終了

恒久先は`D:/NeNeNib/evidence/wrap-beta-20261011/`。`files/`以下にD:/NeNeNibからの相対構造を保ち、全コマンド、初回失敗、GUI画像、性能生データ、作業木の無視ファイルを含む証拠を収載する。source.bundleへtaskのbranch/commitとタグを保持し、manifest.jsonの全bytes/SHAを照合する。公開ZIPは`D:/NeNeNib/releases/v0.1.0-beta.1/`にも残す。

この文書PRのmain統合後、追加作業木410-wrap-beta/413-beta-package/413-release-main/413-beta-publishedと一時出力20261011-wrap-beta/20261011-beta-packageだけを監査整理する。絶対path、取込状態、未保存/未追跡/無視ファイル、唯一成果、稼働参照、リンクを確認し、branch/commitと既存の本体・通常試遊を保持する。整理の実結果は恒久先`closeout.json`で判断し、予定を実施済みとは扱わない。

今回の記録変更だけには、正典conformance.document_checksで変更文書の規則ID/リンク、公開値との照合、`git diff --check`を選ぶ。製品実装・保存schema・性能基準を変更しないため、アプリのテストと実機測定は追加しない。成功した検査結果はpush/レビュー/mergeで再利用する。
