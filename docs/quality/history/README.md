# docs/quality/history — gate-proofs の Issue ごとの経緯

`docs/quality/gate-proofs.md` から 2026-10-16 に施主決定 D50（Issue #517）で移した、Issue ごとの経緯の節（1 節 1 ファイル）。本文は移動時のまま。

| 節 id | 題 | Issue | ADR | ファイル |
|---|---|---|---|---|
| 5-cc | Ctrl+P の同じフォルダ | #272 | ADR 0062 | [5-cc-palette-same-folder.md](5-cc-palette-same-folder.md) |
| 5-cd | 明示ブックマークとCtrl+Pの共有候補 | #278 | ADR 0063 | [5-cd-bookmarks-palette-shared.md](5-cd-bookmarks-palette-shared.md) |
| 5-ce | Exから共通のファイル一覧 | #280 | ADR 0064 | [5-ce-ex-common-file-list.md](5-ce-ex-common-file-list.md) |
| 5-cf | 通常モードの結合文字境界 | #282 | ADR 0065 | [5-cf-normal-combining-boundary.md](5-cf-normal-combining-boundary.md) |
| 5-cg | Exの保存と終了 | #284 | ADR 0066 | [5-cg-ex-save-quit.md](5-cg-ex-save-quit.md) |
| 5-ch | 名前付きEx保存 | #286 | ADR 0067 | [5-ch-ex-named-save.md](5-ch-ex-named-save.md) |
| 5-ci | 名前付きEx仕様の追補とsplit前の区切り | #288 |  | [5-ci-ex-named-spec-split-boundary.md](5-ci-ex-named-spec-split-boundary.md) |
| 5-cj | splitの下準備と限定した描画費用 | #290 | ADR 0068 | [5-cj-split-prep-render-cost.md](5-cj-split-prep-render-cost.md) |
| 5-ck | 長い行の文字組み再利用 | #291 | ADR 0069 | [5-ck-long-line-text-layout-reuse.md](5-ck-long-line-text-layout-reuse.md) |
| 5-cl | ステータスの固定枠と単打分布 | #291 | ADR 0070 | [5-cl-status-fixed-slots-single-key.md](5-cl-status-fixed-slots-single-key.md) |
| 5-cm | 字体選択・可視字形・表示幅とsplit再評価 | #291 | ADR 0071・0073・0074 | [5-cm-font-glyph-width-split-reeval.md](5-cm-font-glyph-width-split-reeval.md) |
| 5-cn | 表示幅索引のmain構成への分離 | #294 |  | [5-cn-display-width-index-main-config.md](5-cn-display-width-index-main-config.md) |
| 5-co | ステータス固定枠のmain構成への分離 | #295 |  | [5-co-status-slots-main-config.md](5-co-status-slots-main-config.md) |
| 5-cp | 改善一式の手動試用を開始 | #291 |  | [5-cp-improvement-bundle-trial-start.md](5-cp-improvement-bundle-trial-start.md) |
| 5-cq | 改善一式の統合の受理 | #291 | ADR 0075 | [5-cq-improvement-bundle-acceptance.md](5-cq-improvement-bundle-acceptance.md) |
| 5-cr | SDK が固定した COM の署名の印 | #299 | ADR 0076 | [5-cr-sdk-abi-signature-mark.md](5-cr-sdk-abi-signature-mark.md) |
| 5-cs | 長い行の 1 打鍵のベンチと実機の基準値 | #298 | ADR 0075 | [5-cs-long-line-keystroke-bench.md](5-cs-long-line-keystroke-bench.md) |
| 5-ct | 描画の保持の契約を窓なしの試験で守る | #300 | ADR 0077 | [5-ct-render-retention-contract.md](5-ct-render-retention-contract.md) |
| 5-cu | F1 の操作の一覧と、操作の 1 つの表 | #303 | ADR 0078 | [5-cu-f1-operation-list.md](5-cu-f1-operation-list.md) |
| 5-cv | 操作表の案内と旧設定を残す移行 | #304 | ADR 0079 | [5-cv-guide-operation-table-migration.md](5-cv-guide-operation-table-migration.md) |
| 5-cw | F1 の鍵の文字を本文の倍率で拡大しない | #312 | ADR 0078 | [5-cw-f1-key-text-no-scale.md](5-cw-f1-key-text-no-scale.md) |
| 5-cx | Ctrl+P の面の「候補なし」を行の名前の欄に書く | #309 | ADR 0060 | [5-cx-palette-no-candidates.md](5-cx-palette-no-candidates.md) |
| 5-cy | 固定した処理を窓なしで前後比較する道具 | #329 | ADR 0082 | [5-cy-fixed-compare-tool.md](5-cy-fixed-compare-tool.md) |
| 5-cz | 読込・状態・履歴・描画の閉じた比較 | #320〜#324 #330 #333 #335 |  | [5-cz-closed-compare-load-state-render.md](5-cz-closed-compare-load-state-render.md) |
| 5-da | 高速化一式の統合受理 | #334 #336 |  | [5-da-speedup-bundle-acceptance.md](5-da-speedup-bundle-acceptance.md) |
| 5-db | 行取得・候補照合・文字コード変換の固定比較 | #337〜#341 |  | [5-db-line-fetch-match-encoding.md](5-db-line-fetch-match-encoding.md) |
| 5-dc | 行取得・候補照合・文字コード変換の統合受理 | #341 #342 |  | [5-dc-line-fetch-match-encoding-acceptance.md](5-dc-line-fetch-match-encoding-acceptance.md) |
| 5-dd | 保存・断片収集・検索列挙の第三陣局所比較 | #346〜#349 |  | [5-dd-third-wave-local-compare.md](5-dd-third-wave-local-compare.md) |
| 5-de | 第三陣3件の統合と直接実機確認 | #351 |  | [5-de-third-wave-merge-device-check.md](5-de-third-wave-merge-device-check.md) |
| 5-df | 桁位置と照合器の固定比較 | #348 #350 |  | [5-df-column-matcher-fixed-compare.md](5-df-column-matcher-fixed-compare.md) |
| 5-dg | 桁位置と照合器の統合受理 | #353 |  | [5-dg-column-matcher-acceptance.md](5-dg-column-matcher-acceptance.md) |
| 5-dh | 行内の選択・検索spanの投影 | #355 #356 |  | [5-dh-inline-span-projection.md](5-dh-inline-span-projection.md) |
| 5-di | 場所照合は文字列を連結せず既知の区間を借りる | #358 | ADR 0096 | [5-di-place-match-borrow-ranges.md](5-di-place-match-borrow-ranges.md) |
| 5-dj | 描画のcolumn→UTF16は表示行の端点を順に進む | #360 | ADR 0097 | [5-dj-column-to-utf16-walk.md](5-dj-column-to-utf16-walk.md) |
| 5-dk | 着色文字も既存の字形経路で描く | #362 | ADR 0098 | [5-dk-colored-text-glyph-path.md](5-dk-colored-text-glyph-path.md) |
| 5-dl | 完全画面外の着色clip省略は実験不採用 | #364 | ADR0099 | [5-dl-offscreen-clip-skip-rejected.md](5-dl-offscreen-clip-skip-rejected.md) |
| 5-dm | 画面外clipの独立再評価は撮影不成立と入力外乱で保留 | #372 | ADR0101 | [5-dm-offscreen-clip-reeval-held.md](5-dm-offscreen-clip-reeval-held.md) |
| 5-dn | 表示行一覧の既存範囲を一度予約する | #367 | ADR0100 | [5-dn-display-lines-reserve.md](5-dn-display-lines-reserve.md) |
| 5-do | 確定検索の解析結果を不変値として共有する | #376 | ADR0102 | [5-do-search-parse-immutable-share.md](5-do-search-parse-immutable-share.md) |
| 5-dp | 検索プレビューの位置変換を一画面に一度へ移す | #378 | ADR0103 | [5-dp-search-preview-position-once.md](5-dp-search-preview-position-once.md) |
| 5-dq | 一時の文書表示値をframeへ所有移管する | #380 | ADR0104 | [5-dq-document-view-ownership-transfer.md](5-dq-document-view-ownership-transfer.md) |
| 5-dr | 固定ステータスラベルの一時コピーを省く | #382 | ADR0105 | [5-dr-status-label-copy-removal.md](5-dr-status-label-copy-removal.md) |
| 5-ds | 統合件名は検証したPRタイトルから取得する | #384 |  | [5-ds-merge-subject-from-pr-title.md](5-ds-merge-subject-from-pr-title.md) |
| 5-dt | ファイル名の探索を末尾の区切りで止める | #386 | ADR0106 | [5-dt-filename-scan-stop-separator.md](5-dt-filename-scan-stop-separator.md) |
| 5-du | タブ表示は省略後の範囲だけを所有する | #388 | ADR0107 | [5-du-tab-display-truncated-range.md](5-du-tab-display-truncated-range.md) |
| 5-dv | 完成したコマンド入力の所有を移す | #392 | ADR0108 | [5-dv-command-input-ownership.md](5-dv-command-input-ownership.md) |
| 5-dw | Ex補完の候補は一致後に所有する | #394 | ADR0109 | [5-dw-ex-completion-owned-after-match.md](5-dw-ex-completion-owned-after-match.md) |
| 5-dx | 補完prefixを同期呼出しの間だけ借用する | #396 | ADR0110 | [5-dx-completion-prefix-borrow.md](5-dx-completion-prefix-borrow.md) |
| 5-dy | パレット候補の所有値をmoveで渡す | #398 | ADR0111 | [5-dy-palette-candidate-move.md](5-dy-palette-candidate-move.md) |
| 5-dz | 一致した操作候補だけ表示値を所有する | #400 | ADR0112 | [5-dz-matched-operation-candidates-own.md](5-dz-matched-operation-candidates-own.md) |
| 5-ea | ステータス位置の数値を局所領域から所有する | #402 | ADR0113 | [5-ea-status-position-numbers-local.md](5-ea-status-position-numbers-local.md) |
| 5-eb | 最新実装で旧split描画試験を再評価 | #404 |  | [5-eb-old-split-render-reeval.md](5-eb-old-split-render-reeval.md) |
| 5-ec | 長行単打の内訳と総2msの見通し | #406 |  | [5-ec-long-line-keystroke-breakdown.md](5-ec-long-line-keystroke-breakdown.md) |
| 5-ed | 正式リリースまでのロードマップ | #408 |  | [5-ed-release-roadmap.md](5-ed-release-roadmap.md) |
| 5-ee | 折り返しと通常モードのドラッグ | #410 | ADR 0114 | [5-ee-wrap-normal-drag.md](5-ee-wrap-normal-drag.md) |
| 5-ef | ベータZIPの正規梱包経路 | #413 | ADR 0115 | [5-ef-beta-zip-packaging.md](5-ef-beta-zip-packaging.md) |
| 5-eg | 初回ベータの公開と再取得 | #410 #413 |  | [5-eg-first-beta-release.md](5-eg-first-beta-release.md) |
| 5-eh | ファイルのドロップとbeta.2 | #416 | ADR 0116 | [5-eh-file-drop-beta2.md](5-eh-file-drop-beta2.md) |
| 5-ei | 配色95種と行番号の背景 | #419 | ADR 0117 | [5-ei-colorschemes-95-line-number-bg.md](5-ei-colorschemes-95-line-number-bg.md) |
| 5-ej | 映画の着想30種を追加し125配色へ | #421 | ADR 0117 | [5-ej-cinema-30-colorschemes.md](5-ej-cinema-30-colorschemes.md) |
| 5-ek | 紹介用の実機画面3枚 | #423 |  | [5-ek-showcase-screens.md](5-ek-showcase-screens.md) |
| 5-el | CIはマージ時だけ、文書のみなら不要 | #430 | ADR 0118 | [5-el-ci-on-merge-only.md](5-el-ci-on-merge-only.md) |
| 5-em | 本文と行番号欄の間に左余白 | #434 |  | [5-em-body-left-padding.md](5-em-body-left-padding.md) |
| 5-en | 改行索引の受理と既存連打基準超過 | #446 #460 |  | [5-en-newline-index-acceptance.md](5-en-newline-index-acceptance.md) |
| 5-eo | 分割UTF-8は機能確認後、性能未受理で保持 | #452 |  | [5-eo-split-utf8-held.md](5-eo-split-utf8-held.md) |
| 5-ep | 宣言サイズまで読み込んでから成功を返す | #463 | ADR0124 | [5-ep-read-declared-size.md](5-ep-read-declared-size.md) |
| 5-eq | 私有退避の資源寿命を製品外で確認 | #465 | ADR0125 | [5-eq-private-backup-resource-lifetime.md](5-eq-private-backup-resource-lifetime.md) |
