# 5-ei — 配色95種と行番号の背景（Issue #419・ADR 0117）

出典: docs/quality/gate-proofs.md の節 5-ei（2026-10-16 に D50 で移動・本文は移動時のまま）

既存Theme/ThemeCatalogを9→95へ拡充し、Vim標準28、IDE50、MonaLisa、映画着想7種を追加した。行番号背景を独立したPaletteトークンとし、現在行の帯を本文へ限定する。構文16役割は全件で保持し、Python/TypeScriptの解析は未実装と区別する。FR-016、ARC-001/002/003/004/007/009/011、CPP-002/003/004/005/011/012/014/016、QLT-001/002/004/008/009/011/012/013、GIT-001〜004、Waivers none。

themes4235、ex-settings483、command-palette583、user-theme-values21、user-theme-selection97、theme codec514 checksと梱包9 tests成功。通常Debug/tidy/ASan/UBSan、Release、変更整形、規約0違反、core/application symbols0違反を確認。新しい短名redの同点順、増加した補完期待、梱包fixtureの初回失敗は直し、関係scopeだけ再実行した。コマンド・依存範囲・結果・原ログと再利用根拠の正本は[詳細](../colorschemes-2026-10-12.md)。

了承された03:20〜03:25内に、暗い/明るい映画テーマとMonaLisa、折り返し/空文書/縮小/設定復元を実画面で確認。7枚すべての行番号背景330画素とテーマ値が一致した。確認用の2プロセスは03:23:27までに終了。HTML見本は95件のC++表から抽出し、製品の言語解析の証拠には使わない。

clean製品commitから正規preview ZIPを作り、x64/版/DLL/4エントリ/展開一致を確認。原値は詳細とD:/NeNeNib/releases/419-colorschemes-preview/package.json。公開beta.2は不変。関連入力不変のテストは文書追記/push/レビュー/mergeで再利用し、全件や性能へ広げない。恒久先D:/NeNeNib/evidence/colorschemes-20261012、統合/収載/整理の実結果はcloseout.json。branch/commitと確認用ZIPを保持する。
