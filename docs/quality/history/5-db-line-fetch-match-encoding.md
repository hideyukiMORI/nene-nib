# 5-db — 行取得・候補照合・文字コード変換の固定比較（Issue #337〜#341）

出典: docs/quality/gate-proofs.md の節 5-db（2026-10-16 に D50 で移動・本文は移動時のまま）

同一harness `77730f7a927dc89965ffad948880e68dbed137f7` をmain eef5aad/#338/#339/#340 A/B/Cへ適用。#337の追加12workloadは準備・入力hash・完全出力検証・結果破棄を2marksの外へ置き、旧12の本文/入力を維持した。Debug/Releaseの新12各warmup1/sample1、Python14例、conformance/File API、symbols2librariesが成功。性能試料へ短いsmokeの値を使わない。

**選んだ機能検証。** #338はpiece境界・CRLF/LFの行端・任意byte offsetの位置計数、#339は順位/包含/本文不変のcaret/失効/候補到着/原alias変更、#340は所有移管・全UTF8出力・CP932の不正列とNUL/空/長短の独立性が直接の退行リスク。固定toolchainのDebug target buildと次の最小scopeを実行した。

| 候補 | コマンドと実結果 |
| --- | --- |
| #338 | `nib_tests.exe --buffer-range` 227、`--application` 15989、`--vim-line-jumps` 999、全失敗0 |
| #339 | 所有境界修正後に `nib_tests.exe --command-palette` 480、`--operations` 516、`--background-work` 89、全失敗0 |
| #340 A/C | `nib_adapter_tests.exe --code-pages` 各22/失敗0。未知/余剰引数は終了1、file resetなし |
| #340 B | `nib_tests.exe --utf16` 71、`nib_adapter_tests.exe --code-pages` 22、全失敗0。Cはcore/tests不変なのでBのUTF16/symbolsを再利用 |
| 共通 | `python -B eng/conformance.py --build-dir <対象build>` 0、`python -B eng/symbols.py --build-dir <対象build> --require core application` 2libraries/0、固定clang-format/whitespace成功。`python -B eng/protected-diff.py --base eef5aad --head <対象HEAD>` はfixture1853・metadata/内容/増減・保護file不変。全scope runtime件数は未測 |

#339の独立レビューはraw vectorのmove前aliasが内部snapshotを書き換える反例を発見。ADR0091へ公開const&入口で一度copyする決定を先行追記し、opened/extendedと反例契約を修正した。旧471checks/f576製品は最終成功の代用にしない。修正後buildの新test optional guard4箇所をtidyが拒否した記録、正規File API query前のKeyError、修正成功を全て保存。規約や抑制を緩めていない。#337/#338/#339/#340の独立レビューは最終固定sourceでP0/P1/P2なし。

**固定比較。** `D:/NeNeNib/outputs/scoped-probes-batch4-20261009/predeclared-cases.md` を実行前に作成し、`plan.json`（SHA256 `0d8fdd9335cf148967bded7b575e91433f6945d1628aec6f77a79ff4093c8e84`）へ全6exe/fullcommit/SHA/入力SHA/比較器SHAを固定した。`python -B -X utf8 D:/NeNeNib/scripts/run-scoped-batch-20261009.py <plan.json>`。ABBA3blocks、warmup1は区間外、paletteは各側120samples、他は各側18samplesの計画。開始23:56:24 JSTにtaskのbuild/static/GUI/copy停止を確認、clang/ninja/cmake/lld/製品/probeのprocess inventoryは空。外部案件の活動は管理していない。

14組中13組はobserved。caretのみ分解能未満でnot-observedとなり、batchの終了値は2。欠測を成功とせず、全raw・試行順・stdout/stderr・失敗を保持し、再実行や除外を行っていない。13組の全exe SHAは終了後も不変。以下は局所処理の中央値usで、正式QLT-014や各機能全体の改善率ではない。

| 固定case | before → after us | 対応after/before比中央値 |
| --- | --- | --- |
| 338-crlf-lines | 557.5 → 7 | 0.011812 |
| 338-lf-lines | 291 → 6 | 0.017734 |
| 338-position-single | 6171.5 → 3144 | 0.506766 |
| 338-position-scattered | 9374.5 → 7831.5 | 0.800401 |
| 339-listed-name | 600.5 → 388 | 0.653393 |
| 339-listed-location | 1953.5 → 1440.5 | 0.722244 |
| 339-narrowing | 610.5 → 39 | 0.063794 |
| 339-caret | 計測分解能未満・not-observed | 算出しない |
| 340-transfer | 73656 → 72070.5 | 0.986353 |
| 340-reserve-japanese | 44585 → 35300 | 0.791671 |
| 340-reserve-ascii | 15955 → 17097 | 0.997582 |
| 340-reserve-supplementary | 31690 → 26212.5 | 0.825481 |
| 340-reserve-codepage | 70932.5 → 64380 | 0.918135 |
| 340-one-pass-codepage | 58699.5 → 47816.5 | 0.827020 |

caretはbefore1process20試料535〜1216us、after1process20試料0〜3us（0が10個）。比較器は非正の区間を拒否して停止した。全120試料が成功したとはしない。0を正数へ補正せず、paired ratioを作らない。この機能は本文不変時の結果共有という単一経路と契約試験を根拠に採用する。

#338はCRLF/LFの先頭30行抽出と57,344bytesの位置換算を採用。fragmented positionは範囲が重なるため、特定の20%を任意の入力へ一般化しない。#339は名前/場所のコピー削減と末尾絞り込みを採用し、場所＋名前の連結は残す。opened/extendedの防御copyは区間外なので、一覧を開く/到着する全体の改善を主張しない。

#340 Aは内部owned成功値の移管として採用、単独速度は分布重複で利益未確認。Bは日本語/補助平面の再確保を減らす利益を観測して採用。ASCIIは対応比0.997582・分布重複で利益未確認、要求capacityは最大3bytes/unit（従来の3倍）。CはCP932の前計数/zero fillを省き、検査済み上限へ一回書込。2byte日本語のwide要求capacityは実使用の最大2倍。この代償は明示し、RSSを測った倍率とはしない。A/B/Cの値は別の比較で、改善率を足さない。

統合source `2993bb482e7cf117aefe269226eac606d0e72b1c` は#337 finaldocs7e9155a、#338 3844e677、#339 7ebed77、#340 5f93135の合成。手動解決はADR索引の0091/0092の両行保持だけ。`out/341-component-reuse-proof.json` は各componentのsrc/tests/eng/CMake変更ファイルと統合版の完全一致、application sourceのbase一致を実証する。独立レビューでも新しい結合退行の具体的根拠が無ければ、componentの成功を工程やHEADの変更だけで再実行しない。統合の最終Release・実機・正式速度は5-dcへ分ける。

全26候補の採否と未実験の理由は[有限台帳](../../design/2026-10-10-speed-candidate-disposition.md)。規則 ARC-001/002/003/004/005/007/012、CPP-003/005/007/008/012/016、QLT-001/012/013/014、GIT-003/004、D41。保存schema・閾値・allowlist・fixture期待値不変。waiver none。全証拠はD側恒久archiveへSHA/path/commit対応付きで追収載する。
