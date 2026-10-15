# 5-ec — 長行単打の内訳と総2msの見通し（Issue #406）

出典: docs/quality/gate-proofs.md の節 5-ec（2026-10-16 に D50 で移動・本文は移動時のまま）

### 判断と境界

**現条件では厳しい**。通常Releaseの長行単打はA5.8875/H6.3525/V6.4180ms、H/Vは総2msへ4.3525/4.4180ms（68.52/68.84%）の短縮が必要。計測版の文字組みはH3.98785/V4.81755ms、試料ごとの占有率中央値70.27/72.63%。同一試料の他の費用を固定した文字組み削減率は中央値93.21/95.19%を要する仮想計算となり、約4.3ms削減を実現した証拠はない。旧判定はhold、閾値不変。恒久高速化/split製品実装へ進まない。

詳しい全7区分・支配関数・候補と理論上限・揺れ/負荷は[報告](../single-breakdown-2026-10-11.md)、全試料と原監査は[JSON](../single-breakdown-2026-10-11.json)、測定前固定と容量欠陥への一度の改訂は[計画](../../plans/2026-10-11-single-breakdown.md)。前回#404/main18f3c4bから今回だけを実行、単体作業。

WM_CHAR input_received→Present(1,0)後renderer復帰のframe_presented。post→受信/GPU実行/発光は含まない。#404の1行6217bytes/1000行、1280×800/DPI120/既定Cascadia Code13.5pt/新profile/通常mode、先頭w暖機→a単打。H/Vは同じframe再利用（H9行×2、V19行×2）、独立文書の性能ではない。

関連: ARC-001/ARC-003/ARC-007/ARC-011/ARC-012、CPP-002/CPP-004/CPP-013/CPP-016/CPP-017、QLT-001/QLT-010/QLT-012/QLT-013、GIT-001〜004、ADR0011/0021/0068/0069/0071/0073/0075/0084。時計/環境/出力はWin32TimingAdapter、UIは既存TimingPortへの節目。通常版と測定版のsource/flags/exe同一性を区別し、製品へ計測経路を統合しない。Waivers: none。

### 実行と再利用

OUTはD:/NeNeNib/outputs/20261011-single-breakdown。下表のscriptはOUT内、修正版はcorrected内。全コマンド・cwd・時刻・native exit・原streamsは各command-capture/<label>/、画素比較はvisual-comparison.json。恒久先はD:/NeNeNib/evidence/single-breakdown-20261011/snapshot-406/files/outputs/20261011-single-breakdown。

| 対象・直接の退行リスク | コマンド / label | 実結果 |
| --- | --- | --- |
| 計測点の構文・静的解析、Release flags | 各probe WTで `pwsh -NoProfile -File eng/build-release.ps1 -Ref <40hex>` / build-a,h,v | 初版3本とadapter修正版3本、各117/117 exit0。初版b444c3c/e6e1d4a/cb71eb0、修正版608c9e2/16a2e6f/2bc9b3d。clang-cl/tidy19.1.5、並列数2 |
| 計測変更の整形 | toolchainの `clang-format --dry-run --Werror <変更ファイル>` / format-a,format-hv、corrected/format-a | exit0。修正版はadapter2files、H/Vの同一bytesで再利用 |
| 時計等の中核流入・変更規約 | `python -B -X utf8 check-source.py` / source-check、`python -B -X utf8 eng/symbols.py --build-dir build/release-b444c3c --require core application` / symbols-fileapi | 初版3×11files規約0、core/application2libs違反0。修正版は `python -B -X utf8 -` からconformance.source_checksをadapter6filesへ適用し規約0（corrected/source-check.json）、中核/flags不変でsymbols再利用 |
| 欠測・不正入れ子・時刻単位・dropの誤受理 | `python -B -X utf8 audit.py --self-check` / helper-check | 正例3/反例11、exit0。修正版のdetail/stats/selfcheck AST同一、parser-reuse.jsonで再利用 |
| 文書/依存/ビルド条件の混入 | `python -B -X utf8 prepare.py` / prepare | 各系列exit0。全6構成の200 commands一致、旧A0/H0/V0の正規Release SHA一致。旧fixtureを再利用 |
| 対象単打と負荷対照 | `python -B -X utf8 run.py` / fixed-run | 初版51枠33有効/18欠測。修正版は長行42枠42有効、追加再試行0。native exit0を2ms合格に読まない |
| 総和・境界・揺れ・環境の監査 | `python -B -X utf8 audit.py` / failed-run-audit、corrected/audit | 初版の欠測18を保持。修正版5guards true、全18詳細試料のexclusive区分残差0ticks。A0/AQ/H0/HPの前後半は固定安定条件超過 |
| 修正した詳細記録の開始/終了・集計誤り | `python -B -X utf8 verify-boundaries.py` / corrected/boundary-audit | 18/18、2個目input〜frame外の追加節目0、drop0/clockFailed0。別interval-tree差引きと一致 |
| 入力後の表示退行 | `python -B -X utf8 eng/compare-frames.py <0.png> <P.png>` ×7 / visual-comparison.json、全14原PNG目視 | 初版4対/修正版3対の無mask全画素0px。wa、通常mode、行1桁3、A19/H18/V38行表示。独立文書の受理ではない |
| 記録の数値・リンク・規則整合 | `python -B -X utf8 verify-docs.py`、`git diff --check` | 結果はcommand-capture/document-checksへ保存。製品テストは文書保存のために繰り返さない |

修正版の測定前CPU3本<10%、compiler/linker/ninja不在。計測中はビルド/hash/copyを止めた。初版の起動詳細で固定65536容量が尽きた欠陥に対して、先行計画commit240ee15後にadapterの追加節目を測定単打だけへ限定した。初版全raw/source/exeと50依存bytes凍結を残し、修正版42試行と混ぜない。空文書9対照は初版だけで修正版の空負荷を測ったとはしない。

AQ−A0対応中央値−0.0730msは閾値以下だが、AP−AQ+0.1505/AP−A0+0.7755/HP−H0−0.6140/VP−V0+0.4020msは事前定義で無視できない。揺れと計測器による区間別偏りがあり、一律差引き・通常版への内訳移植はしない。fallback/glyph callback各1081、OS委譲12、変更行layout_collect1という回数と、全文keyで変更行を再構築するsource経路から支配区間を特定した。文字列準備やcallbackの局所改善は、全消去の上限でも約4.3msに不足。文字組みの部分更新は設計・意味同一性・効果未検証。

準備失敗: capture引数位置でnative未起動、symbols用File API未生成（同flags configureで回復、再buildなし）、初回依存凍結のWindows path判定で部分保存、修正formatの未生成script path。session-journal/原ログを保持し、実行できなかったcommandを成功扱いしない。全件テスト/旧72枠/16MiB/200字/正式8指標は実行していない。

### 保存・終了

本体は計画/報告/JSON/gate-proofs/日報/引き継ぎの6文書。製品挙動・API/settings schema不変、診断rawだけticks/frequency/drop/clockFailed/detailを追加。計測source11filesはtest/406-trace-a,h,vに保持しmainへ統合しない。全旧新build/metadata/raw/失敗/script/source/Git bundleをSHA付きで恒久収載し、4worktreeと一時OUTの絶対path・dirty/untracked/ignored・唯一成果・稼働参照/linkを監査して整理する。実統合/収載/整理はD:/NeNeNib/evidence/single-breakdown-20261011/acceptance-406/closeout.md。branch/commit保持。この区切りで停止し、次候補を自動開始しない。
