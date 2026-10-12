# 本文左余白の確認（#434）

hideの指定に従い、行番号欄と本文の間に既定8 DIP（等幅文字の約0.75文字）の余白を置いた。
`core::body_layout`のcontent開始位置と幅が正本。フォントサイズ/DPIに追従し、小窓では幅0へ畳む。rendererの現在行背景は余白も覆う。
規則: FR-011/014/017/019、ARC-001/003/011、CPP-012/017、QLT-001/002/004/008/012/013、GIT-001〜004。ADR 0008/0114/0117。Waivers: none。

| 確認する退行 | 実行したコマンドと結果 |
|---|---|
| 文字サイズ/DPI/狭い窓の余白寸法 | `build/nib_tests.exe --body-layout`: 13 checks成功 |
| 本文幅変更後の折り返し、余白クリック、選択、IMEの位置対応 | `build/nib_tests.exe --wrapped-view`: 35 checks成功。本文矩形から幅とクリック原点を使う4 checksを追加 |
| DirectWriteの実際の文字組みと座標往復 | `build/nib_window_tests.exe --text-layout`: 961 checks、失敗0、未測0。窓/GPUなし |
| C++の型/警告/静的解析/メモリ安全と整形 | 固定toolchainで`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build --target nib_tests nib_window_tests`成功。ASan/UBSan有効。変更したC++6ファイルへ`clang-format --dry-run --Werror`成功 |
| 実画面での余白・現在行背景・クリック位置 | `pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD`と`python D:/NeNeNib/evidence/body-padding-20261012/verify-padding-window.py`成功。下記条件で目視と保存本文を確認 |

製品commit `031e332fca4c9d1af461f1925a85dcde780dbb51`。Releaseは171.675秒、1521664 bytes、SHA-256 `801d93e424368b23741263144fd03f4a8343d9425757827e3eb47c582b1b2c1a`。
hideが了承した16時ごろの約1分内、16:00:48〜16:00:55 JSTに実画面を確認した。MonaLisa、Cascadia Code/13.5pt、120 DPI、1000×680。文書とLOCALAPPDATAはD側で隔離。
公開beta.3と修正版のPNGを目視し、欄の右端70pxに対し本文左端80px、余白10pxを確認。余白クリック後の挿入は行頭、本文の次の文字位置へのクリック後の挿入は2桁目となり、保存した文書が期待値と一致。両テスト窓は正常終了0。

初回検証。以降は文書だけを追記し、実装/検証入力/環境が不変なのでpush・レビュー・mergeで上記結果を再利用する。全件編集/Vimテスト、性能測定は対象外。新設定・保存schema・テーマ色の変更なし。
採用寸法、ADR 0117の背景範囲、current/日報を更新。公開beta.3は変更せず、修正版exeと証跡を`D:/NeNeNib/evidence/body-padding-20261012/`に保持する。ログ、PNG、Release metadata、統合/監査整理の結果は同先の`closeout.json`を参照。
字体によって「1文字」の幅は異なる。実IME候補窓と全フォント/DPIの組合せは今回再測定していない。
