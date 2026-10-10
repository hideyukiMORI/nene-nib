# ADR 0103 — 検索プレビューの位置変換は一画面に一度だけ行う

- 状態: accepted（技術受理・2026-10-10）
- 日付: 2026-10-10
- Issue: #378
- 基準: main dff0d5cecc48c2d07931a259b810f2cfc6da7549

## 問題と根拠

main dff0d5c の EditorController::line_view は検索patternがある可視行ごとに同じpreviewed_offsetを求める。TextPositionからoffsetへの変換は行頭からUTF-8境界を走査するため、長い行の後方をincsearchでpreviewすると同じprefix走査を行数ぶん繰り返す。

## 意図する結果と正典経路

visible_linesの同期const呼出しで、有効な行範囲かつselected patternがあるときだけ既存previewed_offset(...).value_or(selection.caret)を一度求め、private line_viewへcore::Offsetとして渡す。frameを越す状態を増やさず、既存の検索/投影/描画値を維持する。

## 規則と範囲

ARC-001/002/003/007/008/011、CPP-002/004/006/008/011/012/016、QLT-001/002/004/007/008/010/012/013/014、GIT-001〜004。製品はapplication EditorController.cpp/hpp。関連ADR0037/0041/0043/0075/0082/0095/0100/0102を維持する。waiver none。

## 取得前の受入条件

新しい閉enumの7 workloadを既存65登録の後へ同一dispatch/TimingPortで追加する。全条件const frame生成・破棄と固定field checksumを64回。本文は先頭行が日×1024 + z + Tab + 🖋 + U+0001、続く119行はz x、CRLF区切り、末尾改行なし。short条件だけ日を1文字とする。

1. preview-long-30 / preview-long-120: /z入力途中、30/120行viewport。利益条件。
2. preview-short-30: 短prefixで/z入力途中。
3. confirmed-30: z確定後、caretを1/1へ戻す。
4. unsearched-30: 検索履歴/入力なし。
5. absent-30: /q入力途中、一致なし。
6. disabled-30: incsearch offで/z入力途中。

期待値は製品parser/変換から生成せずliteralから独立に作る。本文全体、行順、表示文字列/map、選択、matches/current、caret、入力行、mode/履歴/設定を前後の区間外で照合する。同じharnessでbefore/afterを作る。

各20iterations・ABBA3・各120対応組を一度だけ取得。2利益条件は対応比中央値<1、全cycle比<1、短縮>=90/120。5費用条件は対応比中央値<=1.10かつ対応差中央値<=50us/64frames。失敗/欠測/全試料を保存し、除外・穴埋め・通るまでの再実行を行わない。条件未達は不採用、外乱/欠測は保留。

## 検証計画

新probe登録/入力のPython対象試験、実compilerの新enum欠落反例、既存65登録/区間/入力不変証明。正規Release/Debug+tidy+ASan/UBSan、新7probe各1iteration、既存--vim-search-highlight/--vim-search-incremental（投影・preview更新/取消/hlsearch/方向/無効入力）。製品差分/規約/実依存/symbolを確認する。

通常Releaseでpreviewの位置/更新/取消/確定・hlsearch off・incsearch off・無効入力・通常表示の限定GUIを事前台本で固定し全製品画素と本文を比較する。正式速度はkey-to-frame-singleとkey-to-frame-single-long-lineを選ぶ。selectedなしのframe共通経路に追加引数/分岐が入る影響を、普通/長行の直接入力→frameで覆う。他のstartup/file/palette callerも同じframeを通るが、selectedなしの同じ変更枝を普通/長行入力で代表確認する。他指標は今回未測とし、前の成功を今回の実測へ置き換えない。基準/許容は不変。

必要な独立レビューは読取専用。単体実装、D:/NeNeNib/worktrees/<issue>-preview-caret と D:/NeNeNib/outputs/20261010-preview-caret を利用。採否・PR/CI/main・恒久収載後、未保存/ignored/唯一成果/稼働参照/linkを監査して作業物を整理、branch/commit/原記録保持。全実験・失敗・判断を日報/引き継ぎへ残す。

## やらないこと

永続cache、検索入力のparse保持、検索/UTF-8/描画algorithm変更、schema変更、#365/#373、既存保留の再測定、全件回帰。

## 結果と採否

共通harness28bc1ff、製品13655cbで固定7条件を一度だけ取得した。長30/120行の64frame生成・破棄は対応比中央値0.337326/0.177893、全3cycleで短縮し各120/120組が短縮した。5費用条件も比<=1.10かつ差<=50µsを満たした。224checksと新7Debug probe、通常Releaseの12画面全画素/全本文一致、正式単発.630ms/長行6.060ms（各5有効・欠測0・基準内）、独立読取レビューに基づき採用する。数値は局所区間でありUI全体の短縮率ではない。

取得前台本のSHAはLF表現、実保存はCRLFだったため初回監査が失敗した。実bytesは取得前runtime planに一致し、CRLFだけの正規化で元SHAに一致することを証明して監査を修正した。台本/画像/原planを変更せず、再取得もしない。詳細と全command/失敗/再利用は[gate-proofs 5-dp](../quality/gate-proofs.md#5-dp--検索プレビューの位置変換を一画面に一度へ移すissue-378adr0103)。他6正式指標、全字体/DPI、RSSは今回未測。永続状態/公開API/schemaの追加なし、#365/#373は未解決、waiver none。
