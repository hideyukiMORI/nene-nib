# 5-dp — 検索プレビューの位置変換を一画面に一度へ移す（Issue #378・ADR0103）

出典: docs/quality/gate-proofs.md の節 5-dp（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-10、基準main dff0d5c、先行ADR ac64791、共通harness28bc1ff、製品13655cb。EditorController.cpp/hppの私有line_view引数にcore::Offsetを渡し、同じpreviewed_offset(...).value_or(selection.caret)をselected!=nullptrかつfirst<=lastのときvisible_linesで一度だけ求める。検索/表示/UTF-8走査の意味と経路は同じで、frameを越える状態を増やさない。

規則: ARC-001/002/003/007/008/011、CPP-002/004/006/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004。関連ADR0037/0041/0043/0075/0082/0095/0100/0102/0103。waiver none。公開API/鍵/保存schema、基準/許容/抑制/allowlistに変更なし。

WT=`D:/NeNeNib/worktrees/378-preview-caret`、OUT=`D:/NeNeNib/outputs/20261010-preview-caret`。恒久先は`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-378`、PR/CI/統合/整理は同階層`acceptance-378`。全試料、source/実flags/tools/環境、原失敗、取得前plan、画像、scriptを保存する。

### 変更範囲と検証

製品は2ファイル7追加5削除だけ。probeはPreviewCaretProbe.cpp/hpp・PreviewCaretWorkload.hppの追加と既存typed表/visitor/CMake/compare-probes/Python対象試験への7登録。旧65条件/型/selector/TimingPort区間/入力はsource-proof-harness.jsonで不変を確認。新oracleは本文literalから独立に組み、製品parser/位置変換を期待値生成に使わない。

| コマンド | 確認する退行・依存 | 実測結果と証拠 |
| --- | --- | --- |
| `python OUT/scoped-static.py` / clang-format dry-run（新関連6source） / 対象Python3 tests | 新登録の型/規約/入力/不正名、旧snapshot8登録の境界 | 初回Pythonは誤class名で未実行、正しいProbeComparisonTestsの3対象は成功。規約/format違反0、waiver none |
| `python OUT/source-proof.py` / `python OUT/dispatch-proof.py` | 旧65条件不変、新閉enumの網羅性が実compilerで強制されること | source一致。positive0、long_thirty欠落は-Wswitch/constexprで1、全体0。型混在/未知variant負例は不変template/selector証拠と#367成功を再利用 |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release` / `python OUT/smoke-before.py` | 変更前製品と独立oracle/新入力が整合すること | 正規toolchain/tidy初回成功、新7条件各1iteration成功。短い時間値は利益に使わない |
| 同build.ps1のafter-release（build/probes-after-release）/debug（build/378-debug） | 私有引数・新probeの実compile、正規sanitizer | 両初回0。Debugのaddress/undefined/no-recover実flagsはdebug-sanitizers.jsonに保存 |
| `pwsh -NoProfile -File OUT/verify-debug.ps1` | preview更新/取消/hlsearch/方向/無効入力、長UTF-8prefixと30/120行への値受渡し | --vim-search-highlight 83、--vim-search-incremental 141、計224checks成功。新7Debug probe各1iteration成功 |
| `python OUT/source-proof-product.py` / `python OUT/check-product.py` / 限定symbols | 製品が期待する2file変換だけ、規約/実CMake依存/core・application境界 | token変換一致、他source不変、2source/実graph/2libs違反0、waiver none |
| `python OUT/run-comparison.py` | 同一仕事の2利益・5費用 | 初回0。7条件×20iterations×ABBA3、各120組。84process/3360marks/1680durations/840pairs、全条件成立 |
| `pwsh -NoProfile -File eng/build-release.ps1 -Ref 13655cb` | 通常製品のcompile/tidy/link | 初回0、1.893s configure/269.613s build/271.506s total、1451520bytes。out/release/13655cb.json |
| `python OUT/gui-preview.py <normal-exe> <sha>` / `python OUT/audit-gui.py` | rendererへ届くcurrent/span/取消/確定/強調off/null/実mode、本文不変 | 12組全client0px、4全文copy一致、2正常終了。全30PNGを親目視。初回hash監査失敗と根拠は下記 |
| `python OUT/formal-speed.py --check --bench key-to-frame-single --executable <normal-exe>` / 同long-line / `python OUT/audit-formal.py` | selectedなしの共通frame枝の引数/guard費用を普通・長行で代表確認 | 2bench初回0、各5有効/欠測0/退行0/計測不能0。全10rawから値を再計算して一致 |
| `python eng/protected-diff.py --base dff0d5c --head 13655cb` | 既存fixture/基準/規約/許可を変えないこと | exit0、1853→1853、metadata/deleted/changed/added各0、protected none。exe/build未指定、全45scope未測。全件試験成功とはしない |

before probe SHA `d33599df368e4a713f1f01c8dfec1b0439455c1991ccb84509ce95e0e13f3805`、after `149707b0c6002bde35e5e5c0e5e05ae334e709cf34856c8f1aeb6cb8df089aeb`、通常Release `8b9465c118c370670a2f1e4845f533b2359779672e52bda228f6cfeffb26675a`。GUI beforeは#376のb26704e通常製品、SHA `0c68928eba770573a0b29578e03ec7dfb0552eac17a29b6173effb5b6b920f8b`で、src treeはmain/beforeと一致。実189compile commands（C++188/resource1）はbuildpath/commit metadataだけが異なり、前後700filesと入力/harness/hashは固定。指紋bc8a356f37c68491、i9-10850K/RTX3090/120dpi/HP推奨電源の前後snapshot一致。環境を連続監視した主張ではない。

### 固定比較

全条件64回のconst frame生成・破棄・field checksumを含む。長本文3674bytes/FNV3943064766360993406、短605bytes/FNV5659432768607348982。先頭行の日×1024（短1）+z+Tab+🖋+U+0001、続く119行z x、CRLF/末尾改行なし。実caretは1/1、active/zだけpreview currentが先頭zに現れる。全文/表示map/span/current/入力行/設定/履歴の独立oracleを前後区間外で照合した。

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| preview-caret-long-30-64 | 6331.5 / 2128.0 | 0.3373255659 | -4186.5 | 120 | 0.3332006623 / 0.3378116792 / 0.3369303623 |
| preview-caret-long-120-64 | 21124.0 / 3764.0 | 0.1778927843 | -17292.5 | 120 | 0.1763739943 / 0.1777455811 / 0.1825012846 |
| preview-caret-short-30-64 | 613.0 / 583.0 | 0.9525368249 | -29.0 | 109 | 0.9519965678 / 0.9562380311 / 0.9525368249 |
| preview-caret-confirmed-30-64 | 1981.0 / 1978.0 | 0.9979762194 | -4.0 | 78 | 0.9977292359 / 0.9977216401 / 0.9982301523 |
| preview-caret-unsearched-30-64 | 979.0 / 977.0 | 0.9989769818 | -1.0 | 61 | 0.9979423520 / 0.9969293756 / 1.0000000000 |
| preview-caret-absent-30-64 | 1684.0 / 1681.0 | 0.9976183369 | -4.0 | 75 | 0.9940369517 / 0.9982084011 / 0.9985117062 |
| preview-caret-disabled-30-64 | 979.0 / 976.0 | 0.9954158552 | -4.5 | 73 | 0.9923128981 / 1.0056326241 / 0.9912497865 |

長30/120の2利益は対応比<1、全3cycle<1、短縮>=90/120を満たす。費用5条件は比<=1.10、対応差<=50µs/64framesを満たす。未検索cycle3=1、disabled cycle2>1も残し、全条件全cycle短縮とはしない。除外/補完/再取得なし。独立読取レビューが全原marksを再構成してsummary一致。局所区間からUI全体や1回単独の改善率を算出しない。

### GUIと正式速度、原失敗

通常Releaseでpreview長prefix・不一致・Backspace復元・取消・確定・次一致・hlsearch off・無効構文・incsearch off・設定復元・通常mode・Vim復帰の12場面を一度取得。全1280×800 RGBが12対0px。親は前後24sceneと実mode6PNGを全目視、reviewerは全RGB/本文/74checkpointを独立監査し代表4画像を目視。248bytesの初回/最後4copy完全一致、2正常終了、各37checkpointでLastInput44172875/foreground/title/clientを確認。実RGBでVim→通常→Vim到達。閉IMEの限定確認であり#365の候補背後や#373の入力元は未解決。rawのvisualInspectionPending:trueは上書きせずvisual-qa.mdに関連付ける。

初回audit-guiのhash assertionは、prepare時のLF文字列表現とWindows保存CRLFの差で失敗した。実script SHA b68de5702d536ea9aae26f88e1dd362674af48f73f2d7b270f2b7a5dc03a3c7dは取得前runtime planに一致し、CRLF210個だけをLFへ変換したSHA50243cbb35eacddf1d832d41c4c103bd536470c2a93853230c5b937fe90046ffは元preflightに一致、sceneも同じ。初版audit-gui-initial.pyとgui-hash-diagnosis.md/jsonおよび実行記録を保持。現在のraw/audit-gui.stderr.txtは成功時0bytesで、初回stderr原本が残るとはしない。実bytes一致と限定した改行変換の両方を要求する監査修正後0。台本/画像/両planは不変、再操作/再撮影なし。

正式単発は[.630,.637,.630,.626,.612]ms、中央値.630（基準.906/上限2.906）。長行は[6.135,6.081,6.060,5.885,5.684]ms、中央値6.060（基準5.431/上限7.431）。各5/5有効・欠測0、退行0/計測不能0。正規measure-speedの出力先と上書き前raw保存だけをwrapperで変更、親と独立レビューが全10rawの第2input→直後frame/202input/連番/hashを監査して一致。選択理由はselectedなしで増えた同じ引数/分岐を普通・長行入力で覆うため。他の正式6指標は今回未測、前回8成功を今回の実測へ置換しない。測定中はbuild/test/GUI/hash/copyを並走しない。

### 採否・記録・再利用

2利益/5費用、直接契約、画面/本文同値、正式2指標が成立したため技術受理。独立設計/実装/全raw/GUI/正式レビューはP0/P1/P2なし。文書8件の限定CNF-006/waiverと空白を確認して通常PR/必須CI/main/収載/監査整理へ進む。関連source/tests/依存/環境が13655cbから不変の成功は文書commit/push/review/mergeだけで再実行しない。

本実験前にD3D single-thread device flagの旧ADR0013/同機実測を調べ、過去の-0.8ms/追測+0.5msから新採用の根拠を得ず未再実験で見送った。C8入力中parse保持は寿命/設定/編集copyを別に設計する必要があり未実装・未実験。本件は元26候補と別の追加実験で件数20採用/2不採用/4未実験を変えない。Python誤class名、推測path読取失敗/広すぎる検索の出力切れ、GUIhash監査失敗をjournalに保持。全字体/DPI/資源枯渇/RSSは未測。永続cacheなし。#365/#373は継続別件、waiver none。
