# 5-da — 高速化一式の統合受理（Issue #334・PR #336・D41）

出典: docs/quality/gate-proofs.md の節 5-da（2026-10-16 に D50 で移動・本文は移動時のまま）

採用差分を統合した製品commit `a64a5d473dffd72c32c4b5936e25073bc590ca27`。manual解決はADR索引、CMakeへの新TU、NibTestsの42selectors（旧35保持、新7）と既定契約接続。VimStepは受理済み2枝の正確な和、renderer/UIは#335の579e21bと同一。独立レビュー `D:/NeNeNib/outputs/334-review/review-334-integration.md` はP0/P1/P2なし。

**統合で追加確認した境界。** 最初の合成でfile-text295/utf8 2693/edit-history50/application15989、最後の合成でregister-snapshot150/recorded-keys44/application15989/macro1735が全成功。固定toolchain Debug `cmake --build build --target nib_tests NeNeNib --parallel 2`、`python -B eng/conformance.py --build-dir build` 0、`python -B eng/symbols.py --build-dir build --require core application` 2/0、protected1853件不変。結果はout/334-test-*.logと334-final-*.log。componentのdot/clipboard/search等は個々の契約の成功証拠として再利用し、共通状態の合成を新しいapplication/macroで確認した。依存全体が不変とは主張しない。renderer/WICは同一source/試験/環境なので#335の2280成功を再利用する。

**Release。** `CMAKE_BUILD_PARALLEL_LEVEL=1; pwsh -NoProfile -File eng/build-release.ps1 -Ref HEAD` が成功。`build/release-a64a5d4/NeNeNib.exe`、SHA256 `B977BC8791CBA1D1925ABB50B55EF2D441F5470857191E9FEDD6628DE820F285`、1455104bytes。以後は文書だけで、src/tests/eng/CMakeListsの一致を確認してReady/mergeで成功結果を再利用する。

**hideの機械で設計席が試用。** `D:/NeNeNib/scripts/334-visible-speed-visual-20261009.py` により#320の206769f（描画sourceは変更前mainと同じ）とa64a5d4を専用profile・120DPIで撮影。既存の文字/編集/undo/選択/scroll/字体/テーマ22場面に、録画、F1標準/24pt、Ctrl+P候補あり/なし、最小化復元、録画中resizeを加えた35場面。`--compare` はすべて本文/ステータス0画素差。afterはsettings.v2と正常終了0も確認、代表画像を設計席が目視した。beforeの撮影後に旧診断のsettings.v1読取りが落ちたため、全35画像と実settings.v2を別検証で確認した。撮り直さず、旧logも保存。beforeの正常終了は未観測と記録した。

`D:/NeNeNib/scripts/334-read-save-20261009.py` は実FilePortでUTF8/UTF8 BOM/CP932、CRLF/LFの本文を開きXを入力して保存し、全byte一致/dirty印解消/正常終了0を各確認。初回の既存1.2秒待ち検証は一時ファイル生成中に失敗した。製品を変更せず、完了状態を最大20秒観測する診断に直しbefore2a31bb4/aftera64a5d4の各3件を確認した（after観測0.95/0.83/0.11秒）。最初の失敗・runner・一時ファイルを残す。これは機能診断で速度の再試行や基準変更ではない。

**正式速度。** 読込・通常/Vim状態・frame・共有rendererに跨る差分なので、正式8本がそれぞれ異なる直接経路を覆う。全件テストではなく、この8本を一回だけ選んだ。build/runtime/大きいcopy終了後、`python -B eng/measure-speed.py --check --executable <上記Release>` が終了0。`out/speed/2026-10-09T13-38-37Z.json`、指紋bc8a356f37c68491（i9-10850K/RTX3090/120DPI）、各5sample、欠測0。palette画像に候補5000件を確認した。

| 正式bench | 中央値ms | min〜max ms |
| --- | --- | --- |
| startup-first-frame | 228.385 | 217.271〜256.032 |
| startup-window-shown | 37.163 | 34.963〜42.978 |
| key-to-frame-single | 0.545 | 0.511〜1.320 |
| key-to-frame-burst-200 | 1.781 | 1.615〜1.872 |
| open-large-file-16mib | 243.259 | 230.405〜298.825 |
| key-to-frame-burst-200-16mib | 1.859 | 1.793〜3.521 |
| key-to-frame-palette-5000 | 2.300 | 2.206〜3.886 |
| key-to-frame-single-long-line | 4.831 | 4.725〜6.316 |

8 checked / 0 regression / 0 unmeasurable。各5sampleと内訳を保持し、正式道具の最終marks保存を全試行raw保存とは呼ばない。旧基準の既存失敗#331を上書きせず、本候補の結果として別記録にした。perf-reference/許容は不変。

残る限界: GPU device lossと物理DPI遷移/COM失敗注入は未再現。activeな履歴の入力文字列、Vimの回数付き入力など残る成長費用は別候補。hide本人が操作したとは記録しない。独立レビュー・正式8本・設計席の実機試用を満たし、必須CIを確認してsquash mergeする。規則 ARC-001/002/004/005/007/008/011/012、CPP-003/005/007/008/012/016/017/018、QLT-001/012/013/014、GIT-003/004、D41。waiver none。
