# 5-eo — 分割UTF-8は機能確認後、性能未受理で保持（Issue #452）

出典: docs/quality/gate-proofs.md の節 5-eo（2026-10-16 に D50 で移動・本文は移動時のまま）

最新head `6c2c13f8b4d721d2f56a4fb3f1fdb08752b05c87`は3案目。`cmake --build build --target nib_tests NeNeNib --parallel 4`がDebug/clang-cl19.1.5/tidy/ASan/UBSanで成功。`build/nib_tests.exe --utf8`13,921 + `--file-text`307 = 14,228 checks成功。`python eng/symbols.py --build-dir build --require core application`は2 libraries/0違反、`python eng/conformance.py --build-dir build`は0違反。変更Utf8.cppのformatと差分空白も成功。分割/EOF/不正列/所有と既存UTF-8/文字コード/読込の直接境界だけを選択し、全件は実行しない。
既存`compare-probes.py`、before e5f8e70、ABBA×3/各process20標本/全240値で、初回abb38e0のASCII中央値1095.0→1109.0µs・対応比1.015476、日本語5552.0→7038.5µs・1.268325。2案目27cc383はASCII1112.5→1225.0µs・1.102459、日本語5485.0→10541.5µs・1.923092。2案とも不採用。正式GUI速度ゲートとは別の局所比較だが悪化を無視して受理しない。
3案目のRelease性能/GUI openは未実行。hideの01:36終了指示に従いPR453をdraftのまま保持し、CI/mergeを実行しない。[詳細記録](https://github.com/hideyukiMORI/nene-nib/blob/6c2c13f8b4d721d2f56a4fb3f1fdb08752b05c87/docs/quality/streaming-utf8-2026-10-12.md)。110ファイル87,749,691 bytesの原ログ/標本/exe/configをD:/NeNeNib/evidence/streaming-utf8-452/checkpoint-20261013へ収載・SHA照合した。
初回の不要なASAN_OPTIONS指定下の結果は採用せず、overrideなしで確認し直した。最新検証も通常環境。最新ソース変更後の成功契約は関連入力が不変なら次回へ再利用し、過去候補の性能は流用しない。対象規則FR-008/013・ARC/CPPはADR0122、QLT-001/008/012/013/014。保存schema不変、Waivers none。

2026-10-13夜の更新: PR453を `7d8cf09` へ更新。製品は3案目のまま、scalar拒否時点18契約を追加してUTF-8 13,939 checks成功。file-text 307と既存境界検査は再利用。Release probeを生成し固定planへSHA/条件を収載したが、性能/GUIは未測定。準備証拠は `D:/NeNeNib/evidence/streaming-utf8-452/checkpoint-evening-20261013`。
