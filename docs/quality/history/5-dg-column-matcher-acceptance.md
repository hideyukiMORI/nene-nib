# 5-dg — 桁位置と照合器の統合受理（#353）

出典: docs/quality/gate-proofs.md の節 5-dg（2026-10-16 に D50 で移動・本文は移動時のまま）

先行#351は[PR #352](https://github.com/hideyukiMORI/nene-nib/pull/352)で10:05:23 JSTに必須CI成功後squash merge。main `d5ef4ee3ed28b002d61d8b8a6aabcd90cc8bdeb8`へ同期し、347/349/351の3作業木は証拠収載/監査後に削除、枝とcommitは保持。受理JSON/CI/clean監査は恒久先`acceptance-351/`。5-deの進行中記載は当時の時点で、現在の状態は本節を優先する。

#353は#348Aと#350最終差分をd5ef4eeへ統合したsource `b8fd7cb9575a33a31aa06eb93749dbac75f16559`。VimSearchはmainと同一、matcher4filesは0bd669fと同一。Aの共有visitor、#349列挙と新照合器が同居するため、旧単独試験をそのまま使わず次の直接5scopeを一度確認した。

- `pwsh -NoProfile -File D:/NeNeNib/outputs/20261010-speed-stability/verify-353.ps1 -ExpectedHead b8fd7cb9575a33a31aa06eb93749dbac75f16559`：通常Debug/tidy/ASan/UBSan123steps、buffer-range282、ordinary-characters233、vim-search1409、vim-search-highlight76、vim-search-incremental141、計2141checks成功。ordinary-charactersは上下/ページ移動と編集callerを含む。対象source12と実CMake File APIの規約違反0、完成core/applicationのsymbols2libraries違反0。protected-diff base d5ef4ee/head b8fd7cbはfixture1853不変/protected filesnone、44scope全件は未測。出力はverify-353/と作業木out/protected。
- `pwsh -NoProfile -File eng/build-release.ps1 -Ref refactor/353-position-pattern-integration`：通常Release114steps成功、`out/release/b8fd7cb.json`。exe1448960bytes、SHA256 `73711B91ED238647F1B9A89B283E7C70AE7606149500E204839070B0E9680CEF`。buildの並行状況を速度比較に使わない。
- `python verify-353-boundaries.py --label before/after --executable <固定exe>`と`--compare`：先行351の保存済み製品と統合製品で、短/長行の上下移動、断片編集、非重複n/N、greedy、文字集合star、語境界、preview取消、空行、日本語後方検索、Vim縦移動の19場面を本文/ステータス0画素差で確認。7刺激遷移は非0、前後2process正常終了。候補単独4096文字語の*→n→Nを5場面で確認し正常終了。画像を目視し、元文書/profile不変。保存機能は今回の範囲外。
- GUI初回の12場面後にfixture鍵parserが語境界記法を拒否した。製品失敗とはせずscript/rawを保持し、v2はその入力だけwrite_text+Enterへ変更。旧版から全19場面を確認し直した。速度値へ転用しない。
- `python run-formal-353.py`：Aが変えた共通walkerを通る4編集条件をformal-353-plan.jsonに先行固定。4編集条件で、空/短い反復/大文書/長い行が共有するwalkerとframe生成を直接確認する。起動/openにもframeの共有経路はあるが、新しい持続状態はなく、別処理を多く含む4条件を追加する必要はないと判断した。正規`eng/measure-speed.py --check --bench`の各5sample、同じ指紋bc8a356f37c68491の実reference/25%許容/2ms床で全件終了0、0退行/0計測不能。中央値は1打鍵0.623ms、200打鍵2.104ms、16MiB200打鍵2.198ms、長い日本語行1打鍵5.435ms。raw全sample/log/終了時marksをformal-353へ保持し、全試行marks保存とはしない。この正式刺激でmatcherは測らず、別の局所比較と直接実機で確認する。
- 計測前後の環境JSON、事前固定plan、source/exe/reference/scriptのhashを保存。同機i9-10850K/RTX3090/120dpi、HP推奨plan不変、build/test/GUI/重いcopy/hashを止め他の重いprocess0。すべての試料/初回失敗を保持し、成功までの測り直しなし。
- 別作者のread-only独立レビューはA、#350初期、tradeoff、共用消費後と統合を確認しP0/P1/P2なし。`outputs/20261010-review-wave3/`に全報告。共用後のレビューは自身のA著作を除き、Aは既存の別作者レビューを使用。製品検証・正式4結果は関連source/test/toolchain/referenceが不変の文書更新/push/review/mergeで再実行しない。

残る限界はgreedy直接比較約35%悪化、有限入力の同値確認、原子数×本文長の仕事量、RSS未測。4vector要求はA/A/A+1/A+1で実capacityを測定したとはしない。schema/速度基準/fixture期待/抑制/allowlist変更なし、waiver none。恒久収載とCI/main反映は後続の受理記録へ追記する。

- #353恒久snapshotを`D:/NeNeNib/evidence/speed-optimizations-20261010-wave3/snapshot-353/`へ収載。1248file/285,653,640bytesのコピー元/先SHA一致、manifest SHA256 `72c90e21046512e2dc7c0786c0497e6ddd10a559210f3f1dcc64350cd86c78b7`。4枝のsource、原出力、前後probe、統合製品/Debug試験exe、全失敗/script/reviewを保持。消した347/349の旧exeは先行snapshot-1005のhash一致を確認し参照する。後続の文書/CI/main/整理はacceptance-353へ追補する。


### #353のmain反映と整理（10:37 JST確定）

[PR #354](https://github.com/hideyukiMORI/nene-nib/pull/354)は10:37:03 JSTにsquash merge、main `163c31434777aa9f0817dcf6238e2301e8a8553b`へff同期した。受理HEAD fcbe9a44567513a99a5fe31b731492778974466cとmainの全tree一致/cleanを確認。必須checkは[run38013811414](https://github.com/hideyukiMORI/nene-nib/actions/runs/38013811414)成功。初回run38013766761はPR titleのtype perfがGIT-003の規定外で拒否され、refactorへ直した編集イベントで成功した。製品/試験/ゲートは変更せず、初回logも保持。

恒久先`acceptance-353/`へPR/CI/最終main source/文書/枝別source一致/監査/整理JSONを追補した。346/348/350/353の4作業木は絶対Dパス、取込、clean/未追跡無し、links無し、稼働参照無しを確認。ignoredは813/460/596/321件、うちout196/26/30/13件は全archive hash一致。固定exe/sourceを収載した後のbuild/cacheは再生成可能と分類。初回監査は346のbuild-releaseが未分類で停止し、削除せず生成物を確認後に最終監査した。4worktreeを削除し全枝/commit保持を確認。結果はcleanup-result.json/worktrees-after.txt。後続の355/356だけは新しい未完了taskとしてDに作成した。

正式4条件の範囲説明を訂正した。原planの「起動/open/paletteの刺激は変更関数を呼ばない」は共有walkerからframeへの経路まで含めると厳密でない。4編集条件が同じ共有経路の直接境界を覆うという根拠で選定する。元plan/全sample/結果は改変せず、同じ実装への工程理由の再試験も行っていない。
