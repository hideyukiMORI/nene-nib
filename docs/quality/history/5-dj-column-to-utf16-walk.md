# 5-dj — 描画のcolumn→UTF16は表示行の端点を順に進む（Issue #360・ADR 0097）

出典: docs/quality/gate-proofs.md の節 5-dj（2026-10-16 に D50 で移動・本文は移動時のまま）

main6ab76d2から先行設計e3764cf、製品ca4ad398d6ddd0c9665b3a9a8a97c2722863af93を採用。規則はADR0097のARC/CPP/QLT各ID、GIT-003/004・D41。私有`LineUtf16Evaluation`を一つ置き、Rendererだけが検証済みDisplayLineを同期借用する。byte/column/UTF16 unitsを同じmove_toで進め、新たに進んだ区間だけ正本next_code_point/utf16_lengthで数える。後退はresetし、0列は1へ、終端超過は実終端へ畳む。

旧byte_of_column/utf16_offsetを削除し、選択/検索/current/caret/置換/IMEのcolumn入力を同じ経路へ統一。検索と置換の順序端点列は一つの局所変換器を共有する。IMEのbyte挿入位置とUTF16 baseは同じ元表示行から得る。既存byte→UTF16/逆変換/表示写像の関数本文は逐語一致。所有copy/decoder/cache/公開試験口を追加せず、layout/字形/fallback/描画順/clip/色は変更しない。CMakeは新cppのui target登録だけ。schema/fixture/警告/flags/基準/許容/抑制不変、waiver none。

### 直接検証・再利用と初回失敗

作業根は`D:/NeNeNib/worktrees/360-renderer-positions`、原記録は`D:/NeNeNib/outputs/20261010-renderer-positions`。下記相対script名はこの原記録配下を指す。恒久化後は末尾のsnapshot配下へ同じ構造で保存される。

- `cmake -S <WT> -B <WT>/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`と`cmake --build <WT>/build --target NeNeNib -j 2`は初回116steps成功。正規clang-cl19.1.5/C++23/MT/WX/tidy/ASan/UBSan/no-recoverを実compile commandsで確認。変更4sourceのclang-format、`python -X utf8 <WT>/out/360/focused-conformance.py`（source4/waivers/File API target graph）、`git diff --check`は0違反。source524の指紋を保存した。
- 対象は実Rendererの描画境界なのでnib_windowや無関係な全unit/coverageを実行しない。試験専用public/friendも足さない。`python -X utf8 eng/protected-diff.py --base 6ab76d2 --head ca4ad39`は1853fixturesの変更/追加/削除0、protected files none。45selectorsの全実数は未測。私有型へGUIから渡せない0列/範囲外はコードレビューで確認し、runtime全入力済みとしない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref ca4ad398d6ddd0c9665b3a9a8a97c2722863af93`は初回116steps、141.574秒で成功。通常Release1448960bytes/SHA `c866a25c222637fd1f7ed41b68e3ae504dea0ca9603fd9e0fbcf7ffd14f88a65`。Debug17030656bytes/SHA `9d9445da9963b0c5949cda2f7e93b912fe11db17baa20969c3631098ca549d8e`。
- 比較旧版はsnapshot-358の333086c/通常Release/SHA `bdb24ff17b458fbf7863516de434cdaf2855e8cc5e634b5a927c9c9462f5f6ab`。333086c→6ab76d2の製品/test/CMake/eng/flagsは一致。measurement-identity.json（SHA `d35f87c6e2de1fe7e83a9e8540d47d67d60a05ba230240a3153e8e00937645e2`）とpost-measurement-auditで前後のsource5/harness9/canonical7/exe3のSHA不変を確認。文書/push/review/mergeではこの成功を再利用し、工程変更だけの再試験をしない。
- `python verify-360-boundaries.py --label before/debug/after --executable <fixed>`、`verify-360-ime.py`、`verify-360-ime-both-sides.py`を各版で実行し、`--compare`で前後を照合。GUI26と実IME4+4、計34場面の本文/statusは全0画素差。GUI20刺激はすべて差あり。通常/順逆VISUAL/矩形/zero-length/current/Tab/BMP/補助平面/結合文字/制御置換、空行/行末bar/block、入力欄を含む。親は結合block/混在denseと実IMEcomposition/Space画像を目視した。
- 実日本語IMEは既存SendInput経路でNIHONGO→Space→Enter。日/補助文字prefixの挿入位置と、制御置換が前後にある別条件を固定。underlineは0→207→0と0→204→0、Space accent282/285が前後同じ。全文書/設定不変、確定後の未保存破棄・元IME open0へ復帰・正常終了、Debug診断なし。任意IME候補の同値や書記素の完全性は主張しない。
- `python compare-360.py --smoke <label> --before <fixed>`の各版4条件smokeは入力対応と画像復帰の道具確認として成功（各argvはrun-gui/recordに収載）。build並行のsmoke時間を性能評価へ使わない。初回beforeの外部harnessは準備EnterのWM_CHARを数え忘れ、8実mark対7期待で失敗。他3条件はprocess-wide DPI初期化の反復で製品起動前に失敗。原script/rawを保持し、prep4/DPI初回だけへ訂正したbefore-v2を別folderで確認。製品の失敗/試料除外/成功までの再計測ではない。
- 独立設計/道具/製品/受理レビューを保存。製品P0/P1/P2なし。道具P2の暖機frame自動確認不足は独立`validate-360-marks.py`で補い、before/debug/after各8暖機、正式48trialの96暖機と960計測inputすべて次input前frame到達を確認。計測列の到達と値を原marksから照合した。

実装者のsource-evidence初回は親所有ADR indexの許可一覧漏れで失敗し、訂正script/rawを別保存。推測path不在、rgのliteral wildcard、過大な読取出力もjournalに残す。製品build/実GUI/固定比較/正式4の初回失敗はなく、結果を成功へ寄せる閾値変更もない。

### 固定の製品応答比較

`python compare-360.py --before <333086c固定通常Release> --after <ca4ad39固定通常Release>`を事前planで一度実行。4条件それぞれABBA×3、12process×20交互n/N、120対応組。各processは準備4input、暖機nN2input、測定20inputの計26mark。0.4秒間隔。既存`input_received`から次input前の最初の`frame_presented`までを採る。入力/controller/Renderer/Present呼出しまでを含み、GPU完了や変換kernelだけの値ではない。起動/準備/最初の検索を測定値へ含めない。

全48process成功、960計測sample/96暖機を保持。各条件の全3画像を最初のAへ照合し33比較、4条件計132比較は全差0。準備→暖機nは変化あり、nN後は準備と一致。本文/設定/絶対path/DPI120/client1280×800/機械/電源を固定。前後環境はi9-10850K/RTX3090/HP推奨電源（GUID48684d4a-8524-4093-8a63-ea7132b79c1c）、対象build/GUIは停止しheavies空。環境snapshotから全区間の任意外部活動不在へは一般化しない。全試料・遅い組・原marks・rawを保持、欠測/試料除去/外側retryなし。

| 条件（CRLF終端） | before中央値µs | after中央値µs | 対応比中央値 | 比の最小〜最大 | 短縮/同値/遅い（120組） |
| --- | ---: | ---: | ---: | ---: | ---: |
| `a `×4096 | 171614.5 | 18483 | 0.107151734 | 0.097882830〜0.135728519 | 120/0/0 |
| `日a🖋 `×2048 | 164741.5 | 14316.5 | 0.086843481 | 0.074385014〜0.110529236 | 120/0/0 |
| `a\x01 `×1024 | 101766 | 64808 | 0.630091959 | 0.587961356〜0.690603197 | 120/0/0 |
| `a `×32 | 646 | 637 | 0.990460076 | 0.637500000〜1.807812500 | 70/0/50 |

密な3条件の短縮を採用理由にする。通常行の中央値差は9µsと小さく、50組は遅い。全入力/体感改善とは言わない。置換条件はまだ約64.8msを要する。RSS/実確保回数、他機械/フォント/DPI、任意入力は未測。

### 既存基準での正式速度

`python run-formal-360.py`から正規`python eng/measure-speed.py --check --executable <ca4ad39通常Release> --bench <下記> --repetitions 5`を事前順で一度実行。今回body/caretを通る4条件だけを選び、起動/open/paletteは変更関数の直接リスクを増やさないため再実行しない。指紋bc8a356f37c68491の実機既存基準、25%許容/2ms床の実判定、reference SHA `ce1f2bc26887bebf0f29c5d683422f82586dc912658d1190dd5cf6ec92a4ef43`は不変。参考記録だけの成功ではない。

| bench | 5試行のms（順番どおり） | 中央値ms | 判定 |
| --- | --- | ---: | --- |
| key-to-frame-single | 0.636, 0.621, 0.632, 0.647, 0.589 | 0.632 | 0退行/0計測不能 |
| key-to-frame-burst-200 | 2.302, 2.173, 2.129, 2.223, 2.220 | 2.220 | 0退行/0計測不能 |
| key-to-frame-burst-200-16mib | 2.216, 2.331, 2.346, 2.946, 1.847 | 2.331 | 0退行/0計測不能 |
| key-to-frame-single-long-line | 5.960, 5.954, 5.908, 6.115, 5.975 | 5.960 | 0退行/0計測不能 |

各benchの全5summaryとlog、固定source/exe/SHAを保存。正規道具は最終trialのmarksだけを保持するため、全5trialのmarksがあるとは記さない。snapshotに重複して残る以前のbench JSONは対象benchを選び、追加試料として数えない。

### 収載と後続

恒久先`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-360`に原記録/初回失敗/全GUI/marks/両build設定/DebugとRelease製品/source/レビューを元先SHA照合して収載した。842files/52860599bytes、manifest SHA `7c2bbcbd525c9887e83e8be70541e89fa9830e61a25ed33764348507ca383952`。旧baselineはsnapshot-358に保持。文書/Git/CI/main/整理の後続証跡はacceptance-360へ追補する。再生成できるobject/libraryだけを除外する。

#358はPR #359で11:57:20 JSTにmain6ab76d2へ統合、本体同期/受理tree同一/恒久収載と監査を終えた。追加358worktreeと不要な一時出力は削除済み、branch/commit保持。ここで再実行しない。

#360のCI/main/必要反映後にDの作業木と不要出力の絶対path/取込/未保存/未追跡/ignored/唯一成果/稼働参照/linksを確認して整理する。公開契約/schema変更なし、waiver none。hideの停止指示まで継続する依頼に従い、次の独立候補は置換色tintの描画経路を調べる。現時点では未設計/未起票/未実装/未計測で速度利益を主張しない。
