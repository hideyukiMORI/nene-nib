# 5-dr — 固定ステータスラベルの一時コピーを省く（Issue #382・ADR0105）

出典: docs/quality/gate-proofs.md の節 5-dr（2026-10-16 に D50 で移動・本文は移動時のまま）

2026-10-10 22:41〜23:13 JST。前件#380はPR381/必須Check38056459543でmain a866f710a6ca108398da882ca3fbfb4e4b17ceecへ統合。snapshot-380の439records/112725384bytesをSHA照合収載し追加WT/OUTを22:41:11に監査整理した。初回archiveはGUI inputのsource211→destination274文字でcopy2 WinError3となり243個の部分コピーを残した。原script/log/部分成果を保持し、拡張長pathと既存hash一致時の再利用だけを加えた別resumeで完了。manifest SHA a42cbe11a7ded395757d65974a86afc0ef89569b0f86d9bc130ebbfe1ff5c273。唯一成果・稼働参照・link・未保存なし、695ignoredを分類して整理、branch/commit保持。統合後の記録はacceptance-380/closeout.md。

対象はARC-001/004/007/008/011、CPP-002/004/005/007/008/011/012/014/016、QLT-001/002/004/007/008/010/012/013/014、CNF-006、GIT-001〜004、ADR0010/0021/0022/0070/0082/0105。waiver none。公開API/保存schema/設定/基準/許容/依存許可/抑制は不変。

取得前ADR92245e9、harness c2c7cdc6db606a98c5639a02a0e93e6fa9a684f4、製品9c6031aa2532f6c4ee951b8baca94a5abc610918。StatusItems.cppだけ4追加3削除で、private fixedをconst std::string&からstd::string_viewに替え、encoding/endingの明示的な一時std::stringを2つ省く。string_view include以外に意味変更なし。数値format/std::to_string/固定array/表示順は不変で、検証・所有化はDisplayText::parse一経路のまま。caret一時stringを同期call中だけ借り、parseが所有するため寿命を跨がない。to_chars/stack bufferや公開factory追加、TabTitleのclip変更は今回未実験で見送る。

WT=`D:/NeNeNib/worktrees/382-status-labels`、OUT=`D:/NeNeNib/outputs/20261010-status-labels`、branch=`refactor/382-status-labels`。恒久先はwave3のsnapshot-382、統合/整理証拠はacceptance-382。収載後はsnapshotのfiles/outputs/20261010-status-labelsから元pathに対応させて読み、rawのpathを書き換えない。

### 変更から選んだ検証

| コマンド・範囲 | 回帰リスク・選定理由 | 実結果 |
| --- | --- | --- |
| `python OUT/source-proof.py`、`python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_status_items_inputs_and_registry tests.conformance.test_compare_probes.ProbeComparisonTests.test_status_items_invalid_names tests.conformance.test_compare_probes.ProbeComparisonTests.test_frame_document_inputs_and_registry` | 新6のinput/hash/不正名/登録、旧76入力と区間、既存unit assertion/defaultの不変性 | source証明0、Python3tests初回0。旧76/selector/template/CMakeの追加以外不変、旧関数block SHA db8d5d4703142f479ba48dd76a0d9cb2155bdc49addc241ba9be0b21c580b50c |
| 変更9test sourceの`clang-format --dry-run --Werror`、`python OUT/scoped-static.py`、`python OUT/dispatch-proof.py` | typed登録と新閉enumの網羅性/規約 | 整形・source・waiver違反0、新utf8_crlf欠落の実compiler1/正例0、初回checker0 |
| `pwsh -NoProfile -File OUT/build.ps1 -Phase before-release -Configuration Release -BuildDir build/probes-before-release`、同after-release/build/probes-after-release、`python OUT/smoke-before.py` | 同一正規最適化/計測器で前後値・保持arrayの寿命を確認 | 両Release初回0、before新6+旧frame短各1iteration初回0。smokeの時間を利益にしない |
| PowerShell内の`& OUT/build.ps1 -Phase debug -Configuration Debug -BuildDir build/382-debug -Targets @('nib_perf_probes','nib_tests','NeNeNib')`、`pwsh -NoProfile -File OUT/verify-debug.ps1` | 新しい借用が呼出し後に残らず3表示項目/数値/呼出し元を保つ | 正規tidy/ASan/UBSan/no-recover、既存--status-items8checks、新6+旧frame短各1iteration全0。最初のnative pwsh -Fileは配列bindingで本体前に失敗、下記へ記録 |
| 製品整形、`python OUT/source-proof-product.py`、`python OUT/check-product.py`、`python eng/symbols.py --build-dir build/probes-after-release --require core application` | private借用+2temporary省略だけ、数値/parse所有化/他src不変、依存逆流なし | 期待する全file変換一致、source/実CMake graph0、2libraries違反0 |
| `python -X utf8 OUT/run-comparison.py`、`python -X utf8 OUT/audit-comparison.py` | 3利益と大きな行桁/別encoding/直接frameの費用 | 初回7条件各120組、全事前条件成立。全84process/1680durations/3360marks/840pairsを原記録から再計算して一致 |
| `pwsh -NoProfile -File ./eng/build-release.ps1 -Ref 9c6031a`、`python OUT/capture.py normal-release-post build/release-9c6031a` | 実アプリの正規Releaseで表示と正式速度を確認 | 初回0、out/release/9c6031a.json、706source/実flags/tools/exe SHA保存。console一部は切れており完全stdout保存とはしない |
| `python -X utf8 OUT/gui-status.py <normal-exe> <sha>`、`python -X utf8 OUT/audit-gui.py` | 3encoding×2endingの表示と本文、実mode/外部入力/正常終了 | 初回0、6対全client0px、12全文physical bytes一致、96checkpoint、4実modeRGB、2正常終了。親16PNG全目視 |
| `python -X utf8 OUT/formal-speed.py --check --bench key-to-frame-single --executable <normal-exe>`、`python -X utf8 OUT/audit-formal.py` | 共通status callerを普通入力のframeで代表確認 | 初回5有効/欠測0/退行0/計測不能0、全5rawから再計算一致。他7指標は今回未測 |
| `python eng/protected-diff.py --base a866f71 --head 9c6031a` | fixture/基準/規約/許可/既存scopeの変更を検出 | exit0、1853→1853、metadata/追加/削除/変更0、protected none。exe/build未指定、46scope未測で全件成功ではない |

既存verify_status_itemsはverify_look経由の既定実行だけだった。verify_status_items_scope wrapperと--status-itemsを追加し既存8assertionsを直接呼ぶ。defaultのverify_look呼出し・旧45selectors・既存assertionsは不変、重複assertionは追加しない。既定全件checksは未測。

before probe SHA be941865ed3a3b5d0d8830b9628a57bf77a529314cdb5ea786119f9c3d3021c8、after 59ce399dd4e711efa4a5842d6e3bbce4eb7b300ff4e5989c3ace4b13b3f69d6a、通常Release 7de1cd6822718c8a63f17d6df02cdd13c87502fb8ab6b5b11ecbbd52ff1477ee。GUI beforeはsnapshot-380のrelease-b82b0ed（SHA d83401cc64fe51459356875bc12211d4bd690977a649dcea0afec8aedcb04d7d）、srcはmain a866/beforeと同一。前後191compile commandsはbuildpath/commit metadataだけ相違、706sourceとharness/入力/toolsを保存照合した。指紋bc8a356f37c68491、i9-10850K/RTX3090/120dpi/HP推奨電源の前後snapshotは一致。連続環境監視とはしない。

### 固定比較と費用

旧76の後へStatusItemsWorkloadの6行を同じtyped dispatch/TimingPortで追加。inputはencoding|ending|line|columnのASCII metadata、値は型付きdescriptorで渡し区間中にparseしない。全literal/hash/checksumは先行ADR0105に固定。status3項目のconst array生成・破棄1024回+bytes/code point長さchecksumだけを区間内とし、前後の独立literalと初回保持arrayの確認を区間外に置く。既存frame-document-short-saved-256は入力31bytes/FNV3165114504819470625/checksum14592/既存oracleを保持した直接caller費用。

| 条件 | before / after中央値µs | 対応比中央値 | 対応差中央値µs | 短縮/120 | cycle比中央値 |
| --- | ---: | ---: | ---: | ---: | --- |
| status-items-utf8-crlf-1024 | 108.0 / 104.0 | 0.9629629630 | -4.0 | 110 | 0.9629629630 / 0.9813949463 / 0.9629629630 |
| status-items-bom-lf-1024 | 111.0 / 107.0 | 0.9727272727 | -3.0 | 104 | 0.9648879649 / 0.9818181818 / 0.9727272727 |
| status-items-sjis-crlf-1024 | 191.0 / 187.0 | 0.9819112078 | -3.5 | 104 | 0.9842105263 / 0.9739583333 / 0.9841687552 |
| status-items-max-utf8-lf-1024 | 306.0 / 307.0 | 1.0098684211 | 3.0 | 28 | 1.0114219412 / 1.0132450331 / 0.9967213115 |
| status-items-large-bom-crlf-1024 | 201.0 / 199.0 | 0.9900497512 | -2.0 | 78 | 1.0000000000 / 0.9950000000 / 0.9900497512 |
| status-items-sjis-lf-1024 | 107.0 / 104.0 | 0.9719626168 | -3.0 | 109 | 0.9719626168 / 0.9813084112 / 0.9719626168 |
| frame-document-short-saved-256 | 272.0 / 277.0 | 1.0146789485 | 4.0 | 13 | 1.0110294118 / 1.0124255232 / 1.0183823529 |

20iterations/ABBA3、全7条件各120組を一度取得。最初3利益は比<1・全3cycle比<1・短縮>=90/120、残り4費用は比<=1.10かつ差<=50µs/固定区間を満たす。最大行桁1024回+3µs/約0.99%、直接frame256回+4µs/約1.47%を代償として明記して採用する。大きなBOM条件のcycle1=1や短縮78組も残し、費用を利益条件へ付け替えない。局所の約1.8〜3.7%短縮をUI全体や単発の高速化率にしない。除外/補完/再取得なし。

### GUI・正式速度・失敗記録

取得前script SHA a53b73d85ee8fafd5c316bc4d348bc64d3b8375975f9ad150dd64de9fde4e9d6、preflight/runtime plan/AST全scene一致。本文はa日+Tab+end+改行+last、UTF-8/UTF-8 BOM/Shift_JIS×CRLF/LFの物理bytes14/13/17/16/13/12を固定。6引数openから実Vimへ切替、:tabnext 1〜6とgg0、全本文:w別pathの保存でも文書identityを保持。最初5画像はVim NORMAL、最後sjis-lfは実通常mode。全1280×800client無mask/2560点非被覆/IME閉/LastInput不変、行1桁1。6対0px、12copyの物理bytes/hash、96checkpoint、4実modeRGB、設定/元input不変、2正常終了を監査。親は全16PNGを直接表示して目視した。audit.jsonの目視待ちtrueは原監査時点として残し、visual-qa.mdへ目視完了を別記録する。

正式singleは[.641,.644,.603,.626,.616]ms、中央値.626、基準.906/上限2.906。5/5有効、欠測0/退行0/計測不能0。正規measure-speedの出力先と上書き前raw退避だけのwrapperで、全5rawの202input/第2input→直後frame/連番/hashを監査して一致。他7指標は今回未測、過去成功を今回の実測へ置き換えない。取得中はbuild/test/GUI/hash/copyを並走しない。

Debug最初のnative pwsh -File呼出しは配列-Targetsのparameter bindingで台本本体前に失敗し、pre.json/builddirなしを確認。tool transcript転記をraw/debug-invocation-initial.mdへ保存したが別ファイルの原stdout保存とはしない。同じPowerShell内の直接&呼出しへ訂正して未開始のbuildを実行し成功。source/台本は変更しない。推測path/glob・誤PowerShell optionの読取失敗、広すぎる出力切れもjournalへ保持する。今回の固定比較/GUI/正式は各初回成功で、前件#380の失敗を上書きしない。

### 採否・再利用・限界

3利益/4費用、既存8checks、関連probe、画面/本文同値、正式singleが成立し技術受理。親が全source差分/寿命/独立oracle/型dispatchとraw全数・全画像・正式全5を自己レビューした。private引数と2表現の軽微な変更で未解決の判断事項がなく、現行hide指定に従い一律の独立reviewは追加しない。文書8件の限定CNF-006/waiverと空白を確認し通常PR/必須CI/main/恒久収載/監査整理へ進む。関連source/tests/入力/依存/環境が9c6031aから不変の成功は工程/文書/SHAだけで繰り返さない。

元26候補の20採用/2実験不採用/4未実験とは別の追加実験。数値format最適化、TabTitle、C8入力中parse保持、R8、IO6〜IO8は未実験、#365/#373未解決。全字体/DPI/資源枯渇/RSS/実capacityは未測。公開API/schema/永続cacheなし、waiver none。統合/収載後、絶対D path/取込/未保存・未追跡・ignored/唯一成果/稼働参照/linkを監査して追加物を整理し、branch/commit保持。hideの停止まで改善を続ける。
