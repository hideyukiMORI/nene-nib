# 5-di — 場所照合は文字列を連結せず既知の区間を借りる（Issue #358・ADR 0096）

出典: docs/quality/gate-proofs.md の節 5-di（2026-10-16 に D50 で移動・本文は移動時のまま）

main1aff415で`CommandChoice::listed_score`が名前不一致のたびに作っていたdetail＋区切り＋labelの所有stringを除いた。先行ADR0096 e352780、実行前の2scopeへの限定058af42、初回製品08116c3、初回の代償と固定区間数の追試設計fddb89aを経て、最終製品333086cf2f441f592b7cc0ff015080129f78e5fcを技術受理する。規則ARC-001/003/004/005/008/012、CPP-003/005/008/012/016、QLT-001/008/012/013/014、GIT-003/004・D41。waiver none。

完結したUTF8文字列を1/3個のstack arrayで同期借用し、`std::span<const std::string_view, Count>`を受ける一つのprivate template scorerへ渡す。区間数だけを型で保持し、特殊化/Count分岐/別の意味経路は作らない。scalar走査は既存関数の本文を維持し、区間内fromを写してglobal begin/endへ戻す。byte長/gap/初gap4倍/ASCII空白/大小同一視、名前優先、場所罰点、同点元順は不変。公開候補の防御copy/所有、subset再利用条件、harness53、renderer/IME、schema/fixture/基準/allowlist/抑制を変更しない。

### 直接検証と初回記録

- 初回は`cmake -S . -B build/358-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`、`cmake --build build/358-debug --target nib_tests --parallel 2`の正規Debug124steps成功。`nib_tests.exe --command-palette`495、`--operations`516、計1011checksが初回成功、stderr空。Unicode3区間横断/byte gap/初gap4倍の独立期待3assertだけ追加し、除けば旧test全文と同一。既存scalar本文も同一。Ex候補の共通scorerはcommand-palette内で確認し、scorerを通らないex-settingsは実行前に除いた。
- 第2候補は同じDebugの増分3stepsと同じ2scope1011checksが初回成功。指定の2template/2span型/2callと整形以外のsource tokenは初回と一致、全test/public/CMake/flags/harness不変。型と製品sourceが変わったため実行結果は第2候補を使う。元の試験を工程変更だけで反復したものではない。
- 両候補とも通常clang-cl19.1.5/C++23/MT/W4/WX/tidy/ASan/UBSan/no-recover。変更sourceの`clang-format --dry-run --Werror`、canonical source/document/waiver checks、`git diff --check`、完成coreのsymbols1library/0違反が成功。初回protectedは1853fixture差分0/protected files none、45selectorsの全実数はexe無しで未測。無関係な全回帰/coverage/全gateは実行しない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/358-palette-segments`を各clean sourceで初回実行し、両115steps成功。同じRelease buildへ`cmake --build ... --target nib_perf_probes --parallel 2`を追加10stepsだけ実行し成功。初回製品SHA `d7709837add2894e2a6196bade05d20af476c8b2b80a3ab90915bc80e59054df`、最終製品SHA `bdb24ff17b458fbf7863516de434cdaf2855e8cc5e634b5a927c9c9462f5f6ab`。最終1448960bytes、probe SHA `4fd1724e05f682c2f4e38b05bd9e33fdba353a2a95f6976d40beb582c8e3d13b`/1343488bytes。
- 旧probeはsnapshot-355のfdae17d/SHA `41e386b2f7669cd2d7f0e7b69e923b16229fcecc268f5bc8bf86aac985685131`を再利用。source-identityで各pathの両commitへの存在を確かめ、performance全tree/flags/referenceを同一、製品差分をCommandChoice.cppだけと確認。各版の正しさsmoke3はcanonical metadata/全5000位置または50行/input/caret/scopeと独立checksumを照合し全成功。smokeの時間を性能採否に使わない。
- `python verify-358-palette.py --label before/after --executable <fixed>`と`--compare`で、各候補17場面の本文/statusは旧版と0画素差、16刺激遷移は非0。名前/場所/名前優先/ASCII/跨separator補助平面文字/空白/不一致/Ex候補/操作キー・読み・F1を確認。正常終了、9文書/設定不変。WM_CHARはUTF16単位に分けて補助平面文字を送り、製品driverを変えない。第2候補は同じ絶対文書path/内容/操作/設定/DPI/寸法の旧版画像をSHA同一で再利用し、候補側だけ実行。親が絵文字の1件とF1ほぞん2件を目視した。
- 独立設計/初回製品/第2候補差分reviewはいずれもP0/P1/P2なし。実装者と別席がscalar/座標/score/所有/旧assertと実logを確認。第2候補は初回意味reviewを再利用し、型と新1011結果を追加確認。成功結果は関連source/test/依存/環境が不変なら文書/push/review/mergeで再実行しない。

読取時の推測filename不在、親の準備script内で実行前に訂正したharness path、作者manifest準備のJS構文失敗2件はjournal/read-failures/tool-failuresに残した。製品compile/runtime失敗や計測の再試行とは区別する。両候補の製品build/test・GUI・固定比較は初回成功、試料の除去や期待の緩和はない。

### 固定3比較と代償

各候補は事前planの同じ3条件を`python compare-358.py`から正規`eng/compare-probes.py`で一度ずつ実行。20iterations×3ABBA/片側120sample・120対応組、processごとのwarmup1は区間外、timeout180秒。名前/場所はlisted_positions一回、末尾絞込はqx→qxyのinserted一回。準備/全結果確認/checksum/破棄は区間外。全observed、欠測/外側retry/試料除去なし。

| 候補・条件 | before中央値µs | after中央値µs | 対応比中央値 | 短縮/同値/遅い組（全120） |
| --- | ---: | ---: | ---: | ---: |
| 初回・名前5000 | 332 | 363.5 | 1.089419127 | 14/0/106 |
| 初回・場所5000 | 1348 | 963 | 0.710424595 | 119/0/1 |
| 初回・末尾絞込5000→50 | 38 | 40 | 1.054054054 | 7/2/111 |
| 固定Count・名前5000 | 347 | 325.5 | 0.928876560 | 94/0/26 |
| 固定Count・場所5000 | 1348.5 | 893 | 0.657614200 | 120/0/0 |
| 固定Count・末尾絞込5000→50 | 37 | 39 | 1.054054054 | 6/2/112 |

初回の比範囲は名前0.771167048〜1.994428969、場所0.573700306〜1.002259036、絞込0.487804878〜1.605263158。最終は順に0.531147541〜1.710227273、0.348124244〜0.881057269、0.661016949〜2.461538462。全値は各comparisons/JSONに残し、初回と最終という異なる実験の中央値から直接対応比を作らない。

初回は場所利益があっても名前が約9%遅いため採用を保留し、既知の1/3という個数を実行時に消さず型で保つ別設計を先行した。最終は場所が全120組で短縮し中央値差約0.456ms、名前も今回の対応中央値で約7%短縮。一方、末尾絞込は約2µs/5.4%増加が112組に残る。この代償を含めて固定Count案を採用する。名前には遅い26組もあり、小差を全入力の保証にしない。全条件改善/悪化なしとは記載しない。

### 正式速度と受理

各候補の`python run-formal-358.py`は正規`eng/measure-speed.py --check --executable <fixed> --bench key-to-frame-palette-5000 --repetitions 5`を一度実行。変更scorerと直接の入力→frame callerを覆う。本文編集/起動/IO/rendererの実装は不変なので無関係な他7本は追加しない。実指紋bc8a356f37c68491、reference SHA `ce1f2bc26887bebf0f29c5d683422f82586dc912658d1190dd5cf6ec92a4ef43`、許容25%/床2msは不変。

初回は中央値1.984ms/min1.915/max2.208、最終は1.852ms/min1.752/max1.999、各5有効試料・1bench判定/0退行/0計測不能/exit0。最終試料は1.999/1.852/1.752/1.864/1.803ms。各5枚のPNGは同じSHAで、親が暖機fと1/5000を目視。all-sample JSONと利用可能な最終marksを保存し、5試行それぞれのmarksを保持したとはしない。前後i9-10850K/RTX3090/120dpi/HP推奨、重いprocess0、全席build/test/static/GUI/大きいcopy/hashを停止。

hideの今回の全権委任のもと、設計席の実機確認・独立レビュー・固定比較/正式速度を根拠に技術受理。恒久証跡は`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-358/`、初回を含む全raw/両候補source/製品/probe/Debug/画面/レビュー5441file/129075666bytesを元先SHA照合して収載した。manifest SHA `dd89f5c9294fe9a0c45058c4985eca4bd874fd639ab00a1378948ea62b335129`。最終文書/PR/CI/main/削除監査はacceptance-358へ追補する。

残る限界は完結UTF8の私有1/3区間に限定した借用、固定入力外の性能、RSS/実確保回数、候補作成・到着全体。任意byte断片のdecoderではない。rendererのUTF16前方走査は次の調査だけで、今回の採用に混ぜない。CI/main反映後、未保存/未追跡/ignored/唯一成果物/稼働参照/リンク/絶対Dパス/取込を監査して作業木を整理し、branch/commitを保持する。
