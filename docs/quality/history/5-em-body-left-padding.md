# 5-em — 本文と行番号欄の間に左余白（Issue #434）

出典: docs/quality/gate-proofs.md の節 5-em（2026-10-16 に D50 で移動・本文は移動時のまま）

本文開始位置へ既定8 DIPの余白を加え、字体サイズ/DPIと有効な本文幅を正典body_layoutで決める。現在行の背景は余白も覆う。保存schema変更なし、Waivers none。
`nib_tests --body-layout`13、`--wrapped-view`35、`nib_window_tests --text-layout`961 checks、Debug/tidy/ASan/UBSan、対象6ファイルの整形が成功。[コマンド・根拠・証跡](../body-padding-2026-10-12.md)。
hide了承の16時ごろ、16:00:48〜55 JSTに隔離profileで前後を撮影。MonaLisa、Cascadia Code/13.5pt、120 DPI/1000×680。余白10px、余白/本文クリック後の保存内容一致、正常終了を確認した。製品入力が不変なら結果を再利用する。
