# 5-cj — splitの下準備と限定した描画費用（Issue #290・ADR 0068）

出典: docs/quality/gate-proofs.md の節 5-cj（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-03。FR-003 / FR-005 / D6 / D12 / D22 / D40 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-009 / ARC-011 / CPP-002 / CPP-012 / QLT-001 / QLT-012 / QLT-013 / QLT-014。hideの「その流れで進めて」「推しでいいと思う」「外出するので進めておいて」を受け、単体で下準備と描画実験を実施。文書タブ維持・最大2枠・本文/undoと枠の位置を分ける案、閉じる意味、未検証の境界を[準備資料](../../design/2026-10-03-split-preparation.md)へ記録した。製品採用と大きな所有移行は実測後に判断する範囲で、T3の初版範囲を実装済みに変えていない。

### 比較点と試作の追跡

PR #289を統合したclean mainは`088481b0c79436e9616445559aa4268225bd2ac0`。`git tag -a checkpoint/pre-split-20261003 088481b0c79436e9616445559aa4268225bd2ac0 -F D:/NeNeNib/briefs/tag-pre-split-20261003.txt`と`git push origin refs/tags/checkpoint/pre-split-20261003`が成功した。`git cat-file -t refs/tags/checkpoint/pre-split-20261003`はtag、`git rev-parse 'checkpoint/pre-split-20261003^{}'`と`git ls-remote origin 'refs/tags/checkpoint/pre-split-20261003*'`の展開先はともに088481b。注釈オブジェクトは`6eae6f7657a9903d3a2559d74ba160fb4f7ff049`。Releaseの生成元・SHA-256・5-ch / 5-ciの再利用を注釈に記録した。

測定条件は`7e14f8d`で試作前にコミットした。`test/290-split-render-probe`はDirect2DRenderer.cpp / .hppだけを変える実験用ブランチ。本文の矩形を分割し、同じframeを既存のdraw_body / draw_lineで描く。1窓・1device・1swap chain・1回のPresentを維持する。二つ目の文書やframe生成、独立した位置・操作・IME・閉じる挙動は実装しない。このブランチを製品へ統合しない。

| 版 | 生成元 | 実行ファイル | Release生成の結果 |
| --- | --- | --- | --- |
| A 元版 | 591b99f（製品は088481bと同一） | `build/release-591b99f/NeNeNib.exe` | 5-chの成功を再利用。SHA-256を再照合 |
| S 1枠の試作 | 5fc65010482e38de8ce7be223f5b562b4b02cb5c | `build/release-5fc6501/NeNeNib.exe` | `pwsh -NoProfile -File eng/build-release.ps1 -Ref 5fc65010482e38de8ce7be223f5b562b4b02cb5c` 終了0、147.233秒 |
| H 上下2枠 | cb1992bd1c28ed65c840cb7bc1393680081d4190 | `build/release-cb1992b/NeNeNib.exe` | 同コマンドのRefをcb1992bd1c28ed65c840cb7bc1393680081d4190として実行、終了0、147.490秒 |
| V 左右2枠 | 7a4a6ac4078c994349823fd5648528b4a67de5a4 | `build/release-7a4a6ac/NeNeNib.exe` | 同コマンドのRefを7a4a6ac4078c994349823fd5648528b4a67de5a4として実行、終了0、154.384秒 |

ビルドの目的は、試作のC++/型/描画呼出しと静的解析を既存の警告設定のまま確認し、測定元を固定すること。ログは`out/290-build-single.log` / `290-build-stacked.log` / `290-build-side-by-side.log`、各版の正式メタデータは`out/release/<短いSHA>.json`。全exeのSHA-256・ツール版は[実測JSON](../split-render-2026-10-03.json)にも保存した。新しいゲート・例外・描画器・製品用feature flagは作らない。

試作3コミットと準備文書の親は`D:/NeNeNib/evidence/290-split/source/probe.bundle`へ保存した。`git bundle verify`が成功し、必要な親は088481b。bundleのSHA-256は`CD71A2F2D7E4AC191D72F55CA83A1E32BC4243A115DF9E8F1DFC191737A366F4`。同じディレクトリに各版のsrc差分も保存した。追跡用bundleから`git fetch <bundleのパス> refs/heads/test/290-split-render-probe`で実験の履歴を復元できる。

### 画と測定器

外部スクリプト`D:/NeNeNib/scripts/290-split-render.py`は、既存`eng/measure-speed.py.keys_trial`と`eng/window_driver.py`を共用する。起動時だけ窓を(80,80)・1280×800へ固定し、専用profile、前面/覆われていないこと、DPI120を確認する。計測器の入力/TimingPort読取の実装は複製しない。新しい集計は6組の対応差と欠測の分離だけで、構文検査と6組/欠測1本の短い合成例を確認した。どちらも終了0。実測中はビルドやアプリのテストを走らせない。

`python D:/NeNeNib/scripts/290-split-render.py --manifest D:/NeNeNib/briefs/290-split-manifest.json --capture-only`は終了0（`out/290-preflight.log`）。4版の画像を目視し、単画面・上下・左右の矩形とクリップを確認。`python eng/compare-frames.py out/split-290/captures/preflight-A.png out/split-290/captures/preflight-S.png`はdifferentPixels=0 / bounds=null / 終了0。本文の行数はA/Sで19、Hで9×2、Vで19×2。同じframeを再使用するためキャレットも各枠に出る試作であり、操作中の枠のUXを受理した証拠ではない。

続いて22:38〜22:47 JSTに`python D:/NeNeNib/scripts/290-split-render.py --manifest D:/NeNeNib/briefs/290-split-manifest.json`を1回実行、終了0（`out/290-paired.log`）。3本文×4版×6組の72試行、欠測0、再試行0。奇数組A/S/H/V、偶数組V/H/S/A。全数値と環境・ファイルのhashは[実測JSON](../split-render-2026-10-03.json)、元の集計は`out/split-290/paired.json`、生のmarksと画像はその下位ディレクトリ。空文書と長い日本語行の8枚も目視済み。16MiBの測定時4枚は、目視した事前4枚とPNGの全bytesが一致した（計12枚目視＋4枚一致確認）。

`python D:/NeNeNib/scripts/290-split-audit.py`は終了0（`out/290-audit.log` / `out/split-290/audit.json`）。全72本で202入力、暖機の提示→1文字→その提示→200文字の順を検証し、raw marksから1文字/200文字の時間を再計算して記録値と一致した。窓寸法・DPIも全72本で一致。集計器の初版は本文内の片方の指標だけを揺れで保留していたが、事前文書の「本文条件全体を保留」に合わせて判定記録を修正した。元の72試行と測定器は変更せず、再計測もしない。

### 結果と次の境界

1文字の中央値A/S/H/V（ms）は空文書0.9375 / 1.007 / 1.213 / 0.9255、16MiB短行1.9225 / 1.8195 / 1.6915 / 2.5325、長い日本語行326.2945 / 326.1265 / 314.106 / 657.696。200文字は空3.4275 / 3.676 / 4.0745 / 3.448、短行10.423 / 9.6725 / 9.7105 / 10.3015、長行487.3225 / 486.2145 / 471.791 / 811.316。6本しかないのでp95とは呼ばず、全数値・中央値・最小/最大と対応差を残した。

空文書は事前条件内。16MiBは元版自身の前半/後半の1文字の中央値差0.208msが許容0.19225msを越え、本文条件全体を保留。左右の1文字の増分0.610ms・総時間2.5325msも条件外だが、この条件で確定した退行とは主張しない。長い日本語行は6217 bytes/2077 code pointsの行を1000行。左右は全6組で元版より遅く、1文字の増分331.4015ms、200文字323.9935msで条件外。上下も1文字の総時間2msを満たさない。元版自身の長い行の課題は#291へ分離した。原因の内訳はまだ未計測で、今回修正を始めない。

**splitの製品採用と文書/枠の所有移行は保留。** 既存ゲートの合格を速度維持の証明にせず、測定後に条件を緩めない。同じframeを二度描く下限寄りの試作であり、独立した文書・frame生成・位置補正・undo・保存・IME・hit test・sessionの正しさや性能は未検証。次に進むなら#291で単画面の支配的な費用を特定してから再判断する。

全110ファイル（元の時刻・画像・ソースbundle/差分・測定スクリプト・manifest・ビルド記録）を`D:/NeNeNib/evidence/290-split/split-render-evidence.zip`へ保存し、ZIPのCRC検査も成功した。1166821 bytes、SHA-256 `cc8ae74fb956b24bcfb1cfe6b2d00c03e207015824003671afebfbb4462e1b57`。exe自体は含めず、既存Releaseと各生成元・hashで追跡する。

### 統合する差分と再利用

統合するのはSPECIFICATION・ADR/索引・設計・日報/current・本節・実測JSONだけ。`git diff --exit-code checkpoint/pre-split-20261003 -- src tests eng CMakeLists.txt`は終了0。`python eng/protected-diff.py --base 088481b --head 7e14f8d`も終了0（`out/290-protected.log` / `out/protected/7e14f8d.json`）で、fixtures 1853→1853、metadata/deleted/changed/addedはいずれも0、files changed none、scopes 33 / same 0 / 未測33。exeを渡しておらず、全scopeの件数を測った証明ではない。

文書の規則ID/リンク/状態・空白の不整合だけを対象に、追記後の`conformance.document_checks(root, inventory(root), rules)`は違反0（`out/290-doc-conformance.log`）。`git diff --check`も終了0。製品の実装・試験・関連依存は区切りのタグと同じなので、5-chの製品検証とReleaseを再利用し、文書の統合を理由に製品テストや72試行を再実行しない。全件テスト・新しいVim oracle・IME/クリップボードの試験は選んでいない。製品schema変更なし、実測記録JSONを追加。Waivers: none。
