# ADR 0133 — 窓の終わり方の契約: 閉じる経路は 1 本で、描画を失っても OS が終わっても未保存の編集を黙って捨てない

- 状態: 提案（設計席 2026-10-15。実装は #492・#493 で）
- 日付: 2026-10-15
- Issue: #492・#493（外部講評 2026-10-15 の P0-1・P0-2・共通指示 §4-1）
- 規則: ARC-001 / ARC-012、CPP-002 / CPP-005 / CPP-017、QLT-013、ADR 0007 / 0010 / 0059

## 文脈

正常な保存と閉じ方は堅い（`close_window()` が未保存のタブを順に映して「保存しますか」を出し、`leave()` が一覧を書いて `DestroyWindow` する）。抜けているのは異常な終わり方で、外部講評の事実確認（`D:\NeNeNib\outputs\winapps-review-20261015\fact-check.md`）で次が事実と分かった。

- `abandon()`（`EditorWindow.cpp:2147`）は `renderer_.reset()` の後に `DestroyWindow` を直接呼ぶ。確認も一覧も通らない。呼び手はサイズ変更の失敗・文字組みの失敗・DPI 変更の失敗・描画の失敗（device lost 以外か作り直しの 2 度目）の 4 か所で、サイズと DPI の経路は device lost でも作り直しを試さない。
- `WM_QUERYENDSESSION` を処理していない。`WM_ENDSESSION` は一覧と履歴を書くだけで、未保存の編集は確認なく失われる。

静的なゲート（依存の向き・命名・網羅性）は終わり方を検査できない。仕様に書かれていない終わり方は経路として存在しないので、ここに契約を書く。

## 決定

1. **本窓を壊すのは `EditorWindow::leave(SessionEnd)` の 1 か所だけ。** `DestroyWindow(window_)` を呼ぶのはここと、作りかけの窓（初期化の途中で失敗した `EditorWindow`）を後始末するデストラクタだけで、`abandon()` からの直接の呼び出しをやめる。デストラクタは一覧を書かない（使う人の文書がまだ無く、書くと誤った一覧になる）。`leave` は一覧を書く（`remember_session`）→ `DestroyWindow` の順のまま。閉じる理由は `SessionEnd` に閉じた値として持つ（既存の `window_closed` に、描画を失った `rendering_lost` を足す。OS の終了は既存の値のまま）。
2. **終わり方ごとのデータの扱いは次の表のとおりで、表は application の純関数 `exit_plan(ExitCause, unsaved_tabs)` が返す閉じた `enum class ExitPlan` で表す。** ui はその値に従って動くだけで、ui に表を書かない（テストは `--exit-plan` で全組み合わせ）。

   | 経路 | 契約 |
   |---|---|
   | 使う人が閉じる（×・Alt+F4・`WM_CLOSE`） | 今日どおり: 未保存のタブを順に映して「保存しますか」（保存／捨てる／取消）。取消か保存の失敗でそこで止め、窓は続く。`confirm_each_tab`。最後のタブの ×・`:tabclose`・`:q` はタブを閉じる確認（`close_tab`）を通ってから `leave(last_tab_closed)` で、`exit_plan` を経由しない（確認は既に済んでいる） |
   | 描画を失う（device lost・サイズ変更の失敗・DPI 変更の失敗・文字組みの失敗） | まず作り直しを **1 回** 試す（`present()` は今日どおり device lost のときだけ、サイズ・DPI・文字組みの 3 経路は失敗の種類を問わず、同じ `recover()`＝古い renderer を先に解放してから `start_rendering`）。描画を失った後の確認の途中でまた文字組みが失敗しても作り直さない（二重の終わりを防ぐ）。だめなら renderer を捨てた上で、未保存のタブを順に「描画できなくなったので閉じます。保存しますか」（保存／捨てる。**取消は無い**: 窓は描けないので続けられない）。保存に失敗したら理由を見せてもう一度聞く。全部済んだら `leave(rendering_lost)`。`confirm_each_tab_without_cancel` |
   | OS が終わる（`WM_QUERYENDSESSION`） | 未保存のタブがあれば `ShutdownBlockReasonCreate(window, "保存されていない変更があります")` を立てて `FALSE` を返す（OS が「このアプリが終了を妨げています」を見せる）。無ければ `TRUE`。使う人が保存・閉じるで未保存が無くなったら `ShutdownBlockReasonDestroy`。`WM_ENDSESSION`（確定）は今日どおり一覧を書く。`block_session_end` ／ `leave_now` |
   | プロセスが殺される（タスクマネージャ・クラッシュ） | 該当なし。守るには定期的な退避（講評 P0-3）が要り、それは新しい機能なので施主判断（この ADR の範囲外） |
   | 途中でキャンセル（長い I/O） | 該当なし（`FilePort` は同期・ADR 0127 の計画の範囲） |
   | 書き込みが途中で失敗（ディスク満杯・権限なし） | 今日どおり: 一時ファイルに書いて置き換えるので元のファイルは残る。理由を見せて窓は続く（ADR 0010） |

3. **描画を失った後の確認は OS のダイアログ（`MessageBoxW`・ファイルダイアログ）で出す。** 本窓は描けないままでよい（Mica の面）。確認の文言は「描画できなくなったので閉じます」で始め、取消のボタンを出さない（`MB_YESNO`）。
4. **`WM_QUERYENDSESSION` の後に窓が続いた場合、未保存が無くなる瞬間（保存・タブを閉じる・捨てる）に `ShutdownBlockReasonDestroy` を呼ぶ。** 呼ぶのは `leave` と `deliver` の末尾の 1 か所ずつ（未保存の数が 0 になったとき）と、`query_session_end` が `leave_now` を返すとき（前の問い合わせで立てた理由を外す）の 3 か所。`WM_QUERYENDSESSION` は `end_window` を通らず、`exit_plan(session_end, 未保存の数)` の値を自分で見て `FALSE` か `TRUE` を返すだけで窓を壊さない（`end_window` は `leave_now` を `DestroyWindow` として扱うので、通すと未保存 0 の問い合わせで窓を壊してしまう）。
5. **受理: `--exit-plan` の契約（全経路 × 未保存の有無）、ui の自己レビュー（`DestroyWindow` の呼び出しが `leave` だけであることを grep で）、Debug（tidy / ASan / UBSan）、`--application` の既存契約。** 窓ありの確認（device lost の注入）は `eng/verify-window.py` には足せない（注入の口が無い）ので、施主の試用で「リモートデスクトップの切り替え」か「ディスプレイドライバの再起動（Win+Ctrl+Shift+B）」を 1 回行い、未保存の文書で確認が出ることを見る。OS の終了は、未保存の文書でサインアウトを始めて止まる → 取消して Nib で保存 → もう一度サインアウトを始めて止まらない、を 1 回見る。

## 却下した選択肢

- 描画を失ったら未保存の内容を自動で退避ファイルに書く: 置き場・形式・復元の UI が要り、新しい機能（P0-3）。施主判断の後に足す。今日の確認（OS のダイアログ）で未保存を守れる。
- 作り直しを何度も再試行する: 2 度目も失敗する環境では無限に待つ。1 回で十分で、だめなら確認へ。
- `WM_QUERYENDSESSION` で自分で「保存しますか」を出す: OS の終了中にダイアログを出すとそのまま殺される場合がある。`ShutdownBlockReasonCreate` で止めて、使う人が戻ってきてから今日の確認を通す（folio と同じ）。
- ui に終わり方の表を書く: テストできない（ADR 0077 で窓を作れない）。純関数にして application で守る。

## 検証

- application: `exit_plan` の全組み合わせ（`--exit-plan`）。`EndSession` の既存契約（一覧を書く回数）が変わらないこと。
- ui: `DestroyWindow(window_)` が `leave` の 1 か所だけ（自己レビュー・grep）。Debug のビルド（tidy）。
- 施主の試用（決定 5）。

## 限界

描画を失った後の確認は、作り直しに失敗する環境（ドライバが戻らない）で出るので、使う人はその場で保存先を選ぶ。保存先が選べない（ダイアログも出ない）環境では守れない。プロセスが殺される経路は守れない（P0-3・施主判断）。`ShutdownBlockReasonCreate` は Vista 以降の API で、OS が理由を見せるかは OS の設定による。

## 追補 2026-10-18（#665）: Vim の問いの往復の破れは作り直さない

### 文脈

`survives_layout` は配達の `layout_failure` を一律に「文字組みの失敗」とみなし `recover()` を 1 回呼んでいた。Vim の問いの往復（ADR 0129 / 0144）で返る失敗のうち「viewport を既に渡したのに `viewport_request` が立った」「同じ行の画面行数・同じ (行, 段) の画面行をもう一度問われた（答えが問いに当たらない）」は device や layout の失敗ではなく engine と controller の契約の破れで、renderer を作り直しても直らない。#664 の実測でこの経路が「処理落ち」（作り直しの一瞬のちらつき）に見えた。

### 決定

1. **`TextLayoutFailure` を 2 値にする。** `unavailable`（layout を作れない・device や保持の都合。今どおり `recover()` を 1 回）と `inconsistent`（往復の契約の破れ）。分岐はどちらも `switch` で全列挙し、`default` を書かない（CPP-002）。frame に載せるかは `blocks_drawing` の 1 本が決める。
2. **`inconsistent` を返すのは往復の判定の純関数 `VimRoundTrip.hpp` の 3 つだけ。** `vim_viewport_unasked`（viewport の二重要求）・`vim_rows_unasked`（同じ行の画面行数の問い直し）・`vim_span_unanswered`（同じ (行, 段) の画面行の答え直し）。`row_span` 自身が返す失敗（#664 の番兵が外れた等）は `unavailable` のまま（layout の側の問題として作り直す）。往復の回数の上限は持たない（ADR 0129 の決定 2 の追補）。
3. **ui の `survives_layout` は `inconsistent` では `recover()` を呼ばず、配達の残りを写す。** 鍵は application が拒否済み（`VimRepeatFailure::refused`）で、知らせは `step_vim` が 1 行の知らせの口（`with_command_message`・「開けませんでした」と同じ道）に「内部の不整合: 画面行の問い（鍵は無効）」を置く。文言は application の `EditorController.cpp` の知らせの constexpr 群に 1 行。
4. **`layout_failure` の消え方を 2 つに分けて固定する。** `unavailable` は frame にも載り、ui の作り直し（`recover()` → `start_rendering` の `apply`）の先頭の消去で消える。`inconsistent` は frame に載せず（`draw_frame` が描画の失敗 `RenderFailure::directwrite` に読み替えて窓を閉じる道へ流さない）、`apply` が配達を作った時点で消す（配達に 1 回だけ載る・次の `apply` の消去に頼らない）。

### 理由

契約の破れは作り直しでは直らないので、作り直しても同じ鍵でまた破れ、使う人にはちらつきだけが見える。鍵を拒否して知らせれば、本文も画面も変わらず、使う人は次の鍵を打てる。破れを見つけるのは Debug の契約（`--application`）で、製品では知らせの 1 行が手がかりになる。

### 検証

`--application` の `verify_vim_round_trip_judgements`（3 つの判定と `blocks_drawing`。viewport の二重要求は本物の core の engine には無い形なので純関数だけで確かめる）と `verify_vim_inconsistent_round_trip`（替え玉 `ScriptedTextLayouts::misnumber_rows` で番号の問いにだけ違う段を答え、`gj` が配達に `inconsistent` を 1 回だけ載せ・次の配達と frame には載らず・鍵は効かず・知らせが立つこと。`reject` の `unavailable` は今どおり配達と frame に載ること）。
