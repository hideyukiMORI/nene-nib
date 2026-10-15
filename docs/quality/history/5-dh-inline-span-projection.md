# 5-dh — 行内の選択・検索spanの投影（#355 / 固定比較器 #356）

出典: docs/quality/gate-proofs.md の節 5-dh（2026-10-16 に D50 で移動・本文は移動時のまま）

main163c314で残る一致端点ごとの行頭prefix計数を、private `LineSpanEvaluation`一つへ移した。ADR0095 d7a136bと固定5条件のADR0082追記1dc3e55/独立期待JSONを実装前に固定。製品d368ae2、harness2bcaa30を統合したclean fdae17da073ecac7c6a63f734fcd959e15153fb8が実測source。行本文をLineViewへmoveした後に同期借用し、既存code_point_countで端点の差分を前進計数、後戻りは行頭へresetする。選択を先に、同じinstanceで既存非重複一致を投影する。旧span_ofを除去し、clip/absent/改行+1の意味は維持。+1は返り値だけで累積桁へ混入しない。renderer/UTF8/matcher/公開API/永続状態は変更しない。

規則 ARC-001/003/004/005/008/011/012、CPP-003/005/008/012/014/016、QLT-001/008/012/013/014、GIT-003/004・D41。waiver none。変更はapplication helper/line_view、直接unit selector/境界契約、既存perf targetの5入力とそのPython定義、対応文書だけ。schema変更なし。

### 対象検証と再利用

- `cmake -S . -B build/355-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug`と`cmake --build build/355-debug --target nib_tests --parallel 2`：正規toolchain/tidy/ASan/UBSanの初回124steps成功。`nib_tests.exe --frame-selection`56、`--vim-search-highlight`83、`--vim-search-incremental`141、`--vim-visual-block`1105、計1385checksが初回成功。clip/空行/CRLF/LF/末尾CR、原文と表示の桁、後方選択から前方の一致へのreset、preview/currentと矩形callerを直接確認。無関係な全回帰は実施しない。
- `python implementation/scoped-static.py`：変更8source/文書/waiver/実CMake File APIの規約違反0。`python eng/symbols.py --build-dir build/355-debug --require core application`：完成2libraries違反0。変更8C++のclang-format dry-run/Werrorと差分空白が成功。既存5関数のassert本文を逐語保持し、薄いbundleを既定から一度だけ呼ぶ監査が成功。旧44selectorは同じ入口を保持し、新frame-selectionで45へ増加した。
- `python -m unittest tests.conformance.test_compare_probes.ProbeComparisonTests.test_stage_six_frame_inputs`：新5固定入力の1test成功。old48監査は既存enum/name/registry/旧probe全文/main全文/Python入力prefixの一致。新5は正規DebugとRelease before/Release afterで各1sample+warmupの完全結果/独立checksum/metadata/marksを照合し全成功。旧48のruntimeを回し直したとはしない。
- #356初回Debugはdisplay_equals認知複雑度12>10を拒否。固定starts算術をprivate関数へ分けて2bcaa30、期待/区間/checksumを変更せず通常Debug増分10steps/Release113steps成功。旧48監査の初回はbodyのbraces込み60とinterior58の計数差で失敗し、監査だけを訂正。両初回logを保持し規則/flags/抑制は変えない。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/355-line-span-projection`：通常製品Release115steps成功。exe1448960bytes、SHA256 `ffeed056755897ddfa2e83a0178ec6d16a6163da0e552b4f7896e1f2868f1b41`。同じbuild/release-fdae17dへ`cmake --build ... --target nib_perf_probes --parallel 2`の追加10stepsだけで比較器を作った。製品library/configurationを共用し、別flagsの製品を作っていない。probe before SHA256 `7a8c7034e63a42a58670c9b9241d2abc58f3464d69604042bd49e1374ea7c5d2`、after `41e386b2f7669cd2d7f0e7b69e923b16229fcecc268f5bc8bf86aac985685131`。
- `python verify-355-boundaries.py --label before/after --executable <fixed exe>`と`--compare`：19場面の本文/ステータス0画素差、13刺激遷移は非0、正常終了/文書とprofile不変。通常跨行選択/各改行/逆向きVISUAL/矩形/検索preview/hlsearch切替/空行を確認し画像も目視。初回v1ではnative CtrlVとpost文字列の同期が混じって矩形の刺激が不成立。全初回19画像/scriptを保持し、v2でその入力同期だけ分け、前後19場面を確認した。待機時間を速度値に使わない。
- `python eng/protected-diff.py --base 163c31434777aa9f0817dcf6238e2301e8a8553b --head fdae17da073ecac7c6a63f734fcd959e15153fb8`：exit0、fixture1853→1853、metadata/deleted/changed/added0、protected files none。out/protected/fdae17d.json。旧44scopeの全checks実数は未測、新frame-selectionもこの保護比較では未測（直接56checksとは別記録）。全scope実数不変は主張しない。
- 独立製品reviewは借用寿命/旧式/後戻り/+1/既存assertと実ログを確認しP0/P1/P2なし。設計席の別作者harness reviewも固定期待/実測区間/初回修正を確認し阻害なし。`source-identity.json`でd368ae2→fdaeのsrc/unit/support/依存/flags不変、CMake差分はperf source追加だけ、前後harness全Git objects一致を保存。文書/push/review/mergeの工程変更では1385/GUI/正式速度を繰り返さない。

### 固定5条件の比較と採否

`python compare-355.py`が正規`eng/compare-probes.py`を一条件一度だけ呼ぶ。各frame()一回、20iterations×3ABBA、片側120sample/120対応組、process毎warmup1は区間外、timeout180秒を先行planへ固定。新5の各完全出力を固定算術で照合し、準備/検査/checksum/破棄は区間外。全5observed、欠測/外側retry/試料除去なし。

| 条件 | before中央値µs | after中央値µs | 対応比中央値 | 短縮/同値/遅い組（全120） |
| --- | ---: | ---: | ---: | ---: |
| dense ASCII 8192一致 | 112165 | 931.5 | 0.008294700 | 120/0/0 |
| dense混合 4096一致 | 77143 | 690.5 | 0.008950449 | 120/0/0 |
| sparse末尾 1一致 | 489.5 | 470.5 | 0.960045772 | 107/1/12 |
| 通常選択のみ | 241 | 235 | 0.970649766 | 91/2/27 |
| dense検索＋VISUAL | 111932 | 938.5 | 0.008396492 | 120/0/0 |

対応比の範囲は順に0.007720671〜0.011910845、0.008394531〜0.011328202、0.663841808〜1.392197125、0.660919540〜1.070247934、0.008002316〜0.010862597。dense3条件は全120組で短縮。疎/通常選択は中央値約4%/3%短縮だが遅い組も残り、小差を全入力/体感の保証にしない。描画や入力応答全体、RSS/capacityの改善量は未測。配列/共有cache/永続状態を増やさず、固定個数の借用位置だけで同じ原始を使う設計、完全結果一致と直接検証を合わせて採用する。

`python run-formal-355.py`は事前formal-355-plan.jsonの編集4条件を正規`eng/measure-speed.py --check --executable <fixed> --bench <name> --repetitions 5`で実行。空/短い反復/16MiB/長い日本語行がline_viewの共通frame生成と直接編集callerを覆う。起動/openにもframe共用があるが新しい持続状態はなく、別の起動/I/O/一覧処理を含む追加4本を繰り返す必要はない。検索/選択の固有費用は固定5/GUIで別確認した。

同じ指紋bc8a356f37c68491/実reference/25%許容/2ms床で4本とも終了0、各1bench判定/0退行/0計測不能。中央値msはsingle0.632、burst200 2.095、16MiBburst200 2.254、long-line5.764。全sample JSON/log/利用可能な最終marksを保存、全試行marksの保存とはしない。環境前後i9-10850K/RTX3090/120dpi/HP推奨電源、重いprocess0、他席のbuild/test/static/GUI/重いcopy/hash停止。基準値や許容を変更していない。

### 収載と残る作業

恒久先`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-355/`へ464file/109,888,672bytesを元先SHA照合して収載。manifest SHA256 `ca6a47f3c00c986a23a511fb16596955199e199c56ffba2aa90157b421b1516a`。全原出力/初回失敗/script/レビュー/前後probe/製品とDebug exe/2枝source/実build設定を保持。旧製品GUI baselineはsnapshot-353に保持。後続のprotected/最終文書/CI/main/削除前監査は`acceptance-355/`へ追補する。

技術受理時点ではCI/main反映と2作業木の整理が残る。枝/commitを保持し、main一致、絶対Dパス、未保存/未追跡/ignored/唯一成果物/稼働参照/リンクを確認してから削除する。今後の候補はrendererのUTF16端点反復と場所検索の連結であり、今回の採用へ混ぜない。

### #355 / #356のmain反映と整理（11:15 JST）

[PR #357](https://github.com/hideyukiMORI/nene-nib/pull/357)は[必須CI run38016069217](https://github.com/hideyukiMORI/nene-nib/actions/runs/38016069217)成功後、11:12:58 JSTにsquash merge。main1aff4150d9c0a9b794d01cf3020b8cc8f946936fへff同期し、受理HEAD1d1fa01dfac8bc429a35572a7ad643b14bae481bと全tree一致、本体cleanを確認。#355/356はclosed。製品/検証内容はfdae17dから不変で、工程理由の再試験をしていない。

acceptance-355へPR/CI/最終main source/後続protectedと文書/統合監査を追補。355/356の2worktreeは絶対Dパス、clean/未追跡無し、links/稼働参照無しを監査。ignored357/371件中out37/55件は全archive hash一致。固定source/製品/probe/Debug exe/build設定を保持し、生成cacheを分類して2worktreeを削除、枝/commit保持を確認した。不要となった一時出力297file、archive結果JSON、依頼書2本も全て元先SHA/稼働参照/linksを確認して削除。恒久snapshot-355とacceptance-355が正本で、以前の作業パスは存在しない。

文書生成script初回はPython encoding名utf8-sigの誤りで本文変更前に停止し、utf-8-sigへ訂正した。統合監査初回のADR0082 pathはclosed-probesという不在名で空diffだったため、初期JSONを保存し、正しいscoped-probesを両commitに存在確認した上で一致を確認した。いずれも製品・試料を変更/再実行していない。全監査/整理JSONとscriptはacceptance-355へ保持。
