# 5-cz — 読込・状態・履歴・描画の閉じた比較（Issue #320〜#324・#330・#333・#335）

出典: docs/quality/gate-proofs.md の節 5-cz（2026-10-16 に D50 で移動・本文は移動時のまま）

hideが2026-10-09に設計・判断をAstraの設計サナへ委ね、背景席での実装・調査・検証を指定した。その後「閉じた小さな部分の対応前後で、規約内の高速化を試す」と指定した。設計サナが先行ADR・同一harnessの比較計画・採否を担当し、実装と別の席が各差分をレビューした。分担は今回の指定範囲だけで、次の既定にはしない。

**経路と退行。** #320は検証済み所有値DetectedTextからTextBufferへ移し、改行索引と実FilePortの不要な初期化を減らす。#321は一時EditorStateを正典の更新経路へ渡す。#322は意図の反映値EditorDeliveryから可視行を分け、実描画/当たり判定でframeを作る。#323は確定済み履歴、Vimレジスタ本文、記録した鍵列を私有の不変値として共有する。#330は同じUTF8 validatorでASCII連続区間を数える。#324/#335は行番号・描画先・mode文字組みを寿命内で保持し、block caretも本文glyph経路へ通す。文字コード・BOM・改行・保存印・旧snapshot・undo/redo・Vim記録/再生・選択・clip・失敗時の再試行を直接の退行対象とした。

先行設計はADR0080/0081/0083/0084/0085/0086/0088/0089/0090。内部公開APIと所有者の変更を記録し、製品の保存schema・基準値・許容・抑制・allowlistは変更しない。比較器の追加4workloadはADR0082に先行追記し、旧8workloadを変更せずDebug/Release各warmup1/sample1、比較器9テスト、独立レビューを確認した。

| 対象と回帰の範囲 | 実行コマンドと成功結果（各作業木のout/reportsが全command/logを記録） |
| --- | --- |
| #320 所有移管・read長さ・encoding/保存 | 固定toolchain Debug対象build、`build/nib_tests.exe --file-text` 293、buildをcwdに `nib_adapter_tests.exe --files` 74 |
| #321 状態更新・一意の要求・公開caller | `build/nib_tests.exe --application` 15749、`--tabs` 291、`--operations` 516、`--background-work` 89、`--vim-search-incremental` 141 |
| #322 delivery/frame・IME snapshot・公開caller | 同じ5対象が15980/291/516/89/141。旧試験20fileの554呼出しをapply_frameへ機械置換した証拠を保存、期待値不変 |
| #323 履歴 | `--edit-history` 50、`--application` 15980。確定後の旧値、undo/redo、分岐、保存印を確認 |
| #323 レジスタ | `--vim-register-snapshot` 150、`--application` 15980、`--vim-macro` 1735、`--vim-clipboard` 191。raw入力/取得後の変更から隔離し、copy-only xvalueでも元を保つ |
| #323 鍵列 | `--vim-recorded-keys` 44、`--application` 15980、`--vim-dot` 1619、`--vim-macro` 1735、`--vim-search-incremental` 141。64鍵境界、旧値、分岐、search文字所有を確認 |
| #330 UTF8 | `--utf8` 2646、`--file-text` 293。word境界/端数/offset/NUL/非ASCII/不正列。既存scalar検査を維持 |
| #324 描画・UTF16 | `--utf16` 68、`--display-line` 1114303、`nib_window_tests.exe` 1560/0 failure/0 not measured。C4復元後もdisplay-lineだけ再確認 |
| #335 block clip・mode資源 | `nib_window_tests.exe` 2280/0 failure/0 not measured。既存180組×4clipの720比較を追加、Debug製品もbuild成功 |
| 依存/規約/保護 | 各採用差分でconformance/File API 0、固定clang-format/whitespace成功。変更したcore/applicationのsymbols 2libraries/0。protected fixture1853件の内容/metadata/増減は不変。全scope件数比較は未測 |

初回失敗と修正は捨てていない。#320の追加試験が後続fixtureを壊した順序、#321/#322/#324の追加試験のlint/include、#323 factoryのinline解析と新試験のuse-after-move/nesting、#330の初期configure/buildは各報告と最初のlogに残す。抑制や基準変更で通していない。copy-only値を再読する新試験は非const xvalueを明示し、将来本物のmoveで元が壊れれば落ちる契約を維持した。

**固定前後比較。** 同じtoolchain・入力・harnessでABBA 3block。通常20sample/実行（各側120）、大きい履歴・register・2000鍵は3（各側18）。warmup1は区間外、本文/undo/redo/countを区間外で照合し、全sample・対応比・metadata・exe/入力hash・marksを保存。下表は局所処理の中央値、単位us。別batchの数字を直列に足した改善率や起動全体の改善率にはしない。

| 候補と固定処理 | before → after | 対応after/before比中央値 | 判断 |
| --- | --- | --- | --- |
| #320 メモリFilePortから16MiBを開く | 49063.5 → 23901.5 | 0.488439 | 採用 |
| #321 通常200入力 | 1048 → 541 | 0.517074 | 採用 |
| #321 16MiB削除後200入力 | 5515758 → 1365361 | 0.244605 | コピー減、残る崖は#323で処理 |
| #322 通常200入力 | 572 → 287 | 0.514500 | 採用 |
| #323 履歴1MiB/16MiB削除後200入力 | 95850.5 → 301 / 1511898.5 → 304.5 | 0.003129 / 0.000210 | 保持サイズ由来の崖を除去 |
| #323 register1MiB/16MiB保持200入力 | 196116 → 3716.5 / 2858266 → 3582 | 0.019023 / 0.001286 | 採用 |
| #323 録画200/2000入力 | 2454.5 → 1123 / 150990.5 → 13902.5 | 0.459314 / 0.092829 | 採用、全ての入力費用を定数化したとはしない |
| #330 ASCII16.8MB検証 | 15741 → 1345.5 | 0.086486 | 採用 |
| #330 日本語6.219MB検証 | 5812.5 → 5626 | 0.965417 | 分布が重なり改善の主張なし |
| #330 メモリFilePortから16MiBを開く | 26135.5 → 10069 | 0.393079 | 採用 |
| #324 C4表示行の再encode省略 | 20 → 21 | 1.050000 | 改善せず不採用、元の実装へ復元 |
| #333 ThinLTO buffer/open/入力/表示 | 7105→7266.5 / 11271.5→11566.5 / 512.5→520.5 / 20→18 | 1.009759 / 1.007808 / 0.964231 / 0.900000 | 主要区間の明瞭な改善なし、製品不採用 |

#335は窓なしprobeで描画を測れないため、同じ多言語80行でNORMAL移動/録画移動をABBA・各側120入力、全12起動/45marks照合で比較した。input_received→frame_presentedは16988→16798us（0.978760）、17317→16614us（0.957283）。分布が重なるので高速化の量は主張せず、同じ保持資源と描画経路への集約として採用。全22本文/ステータス画像比較は0画素差。R2/R4/R5/R6個別の速度量は未測で、統合の正式8本と画素/契約で確認する。

batch3の冒頭8秒にregister席のRelease conformanceが重なった。`environment-note.json`に時刻を記録し、sampleは一つも捨てず測り直していない。register普通入力308.5→391us（対応比1.26418）を清浄な単独比較や改善として使わない。履歴の普通入力290→317.5usなど小さい区間の揺れも原記録に保持し、正式受理は5-daで判断する。外部案件の機械負荷は管理していない。

#333は実bitcodeのsymbolsが303違反。address表示を読み取り診断で補正してもcompiler由来`__ImageBase`がcore/application各1件残った。parser/allowlistを広げず不採用。構築と12workloadの短い正しさ確認、PE import同一、15872bytes縮小は確認したが、採用しない候補の全Release契約は実行していない。ADR0087は実験記録だけを収載、ThinLTO flagsは統合しない。

証拠は `D:/NeNeNib/evidence/speed-optimizations-20261009/`。3batchのplan/result/rawと各component out/report/製品/probe/独立レビューをsource-commit-mappingとSHA manifestで対応付ける。作業木名や文書commitをexeのcommitと同一視しない。候補C5〜C9/IO9、D2/R8/B3、tint_runs等は別調査であり、この工程ですべての候補を実験済みとはしない。
