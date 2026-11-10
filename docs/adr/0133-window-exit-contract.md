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

## 追補 2026-11-10（#1282）: 「保存しますか」の文言は自分で持つ（TaskDialogIndirect の custom button）

### 文脈

hide の実機（Windows 11 Pro 26300・日本語・試用 exe `NeNeNib-main-b40ea348.exe`・2026-11-04 夜）で、未保存の文書を閉じたときの「保存しますか」のボタンが「OK／さらに詳しく／キャンセル」と出た。コードは `MessageBoxW(..., MB_YESNOCANCEL | MB_ICONWARNING)` で、Win32 の `MB_*` にこの組は無い。`MessageBoxW` のボタンの文言は OS が決めるので、OS の build の描き方が変わると Nib の意図（保存する／保存しない／取消）が使う人に伝わらない。同じ機械の別の席（2026-11-10・Debug の exe・自分の profile）では「はい(Y)／いいえ(N)／キャンセル」と出て、再現の条件はまだ分かっていない（切り分けは設計席と hide）。

### 決定

1. **「保存しますか」の 2 つの形（決定 2 の閉じる確認と、決定 3 の描画を失った後の確認）は ui/win32 の `ask_save(owner, title, SaveQuestion)` の 1 本で出す。** 形は `TaskDialogIndirect` の custom button で、`unsaved` は「保存する(&Y)／保存しない(&N)／キャンセル」、`rendering_lost` は「保存する(&Y)／保存しない(&N)」（取消は無い）。既定のボタンは先頭（「保存する」・`MB_DEFBUTTON1` と同じ）。Esc・Alt+F4・× は `unsaved` だけでキャンセル（`TDF_ALLOW_DIALOG_CANCELLATION`）、`rendering_lost` では効かない（`MB_YESNO` と同じ）。アクセスキーは MessageBox の Y / N を保つ。
2. **ボタンの id は MessageBox と同じ `IDYES` / `IDNO` / `IDCANCEL`。** 押された id → 答え `SaveAnswer { save, discard, cancel }` は純関数 `save_answer_of` の 1 本で、箱を出せなかった（`TaskDialogIndirect` の失敗・文の変換の失敗）・知らない id はキャンセルに倒す。id は同じで、道具（`eng/window_driver.py` の `dismiss_dialog`）は TaskDialog（窓の class は `#32770` のまま）に `TDM_CLICK_BUTTON` を送る（TaskDialog は posted の `WM_COMMAND` を受けない）。答えの後の一歩は純関数 `save_step(SaveQuestion, SaveAnswer)` の 1 本（保存する → 保存の道・保存しない → 捨てて閉じる・キャンセル → `unsaved` では閉じない）で、`confirm_discard` と `confirm_lost_tab` が `switch` で写し、`default` を書かない（CPP-002）。**`rendering_lost` で箱を出せなかった（キャンセル）ときは、窓は閉じるが未保存を捨てたことにしない（`EndSession.unsaved_discarded` を立てず、自動退避を残して次の起動で問う・ADR 0137 の決定 5）。** 箱の入れ子のループの間に閉じる確認（`unsaved`）がもう一度来たら、2 つ目の箱を出さずにキャンセルを返す（`ask_save` の入口の 1 か所）。`exit_plan` と `SessionEnd` は変えない。
3. **文は core の `save_question_text(SaveQuestionText)` の 1 か所（UTF-8）。** 文の名前の閉じた enum で、表示言語の表（#1019 の `ui_texts`）へはこの値ごと移す。ui/win32 に文の別表を置かない。
4. **comctl32 v6 は manifest の依存（`Microsoft.Windows.Common-Controls` 6.0.0.0）で選び、DLL は遅延読み込み（`/DELAYLOAD:comctl32.dll`・ADR 0130 と同じ形）にする。** 起動の道に DLL の読み込みを足さない（検証を参照）。ui/win32 の platformLibraries に `comctl32` を足す（`eng/architecture.json`）。

### 却下した選択肢

- 箱を出すときだけ activation context を作る（`CreateActCtxW` と別の manifest の資源）: 起動の道は manifest の依存と同じく変わらず、コードと資源が増える。manifest の依存は遅延読み込みと組んで起動に DLL を足さないので、こちらを採る理由が無い。
- 自前の小さな窓: 描画を失った後（決定 3）でも出せることが要り、OS の箱の方が確か。アクセシビリティ・DPI・キーボード操作を自分で持つことになる。
- `MessageBoxW` のまま `SetWindowsHookEx` でボタンの文字を書き換える: OS の箱の内部の作りに依り、#1282 と同じ種類の壊れ方をする。

### 検証

`nib_window_tests --save-question`（ボタンの id → 答え・2 つの形のボタンの並びと既定・文の名前・各ボタンの id が文の意味の答えになること・`save_step` の 6 通り（キャンセルは `unsaved` で閉じず、`rendering_lost` で閉じるが捨てない））。起動の後の module の一覧（Debug の exe・自分の profile・前後）と `dumpbin /dependents`・箱の PNG・`WM_COMMAND IDNO` で道具が答えられることは #1282 の PR に記録する。

## 追補 2026-11-10（#1340）: 閉じると決まったら先に窓を隠してから書く

### 文脈

設計席の実測（D の HDD の道具の profile）で、`WM_CLOSE` から窓が画面から消えるまでに 0.18〜1.13 秒かかった。席 I17 が閉じる道に一時の節目（commit しない）を入れて Release で測った内訳（`D:\NeNeNib\scripts\20261110\1340\hide-time.py`・`WM_CLOSE` の post と `IsWindowVisible` の 2 ms ごとの読みだけ・空の文書 30 回と長い文書 20 回・同じ機械で別の席のビルドが動いていた回を含むので参考）:

| 段 | 空の文書（中央値 / 最大） | 長い文書（中央値 / 最大） |
|---|---|---|
| 言語サーバーの停止・読みの取消・自動退避の flush | 0.0 ms | 0.0 ms |
| 一覧 `session.v2` の書き込み | 565 / 12,440 ms | 457 / 3,556 ms |
| 履歴 `history.v1`（読む → 足す → 書く） | 0.0 ms（パスが無い） | 133 / 335 ms |
| 自動退避の片付け（`clear`） | 0.1 ms | 0.1 ms |
| `DestroyWindow`（窓が消えるのはこの中） | 11 ms | 11 ms |
| 窓を壊してから `run` を抜けるまで（worker・renderer・device の解放） | 15 ms | 15 ms |

時間を使うのは一覧と履歴の書き込み（保存ごとの `FlushFileBuffers` と write-through の改名）だけで、どれも窓が見えたまま UI スレッドで待っていた。worker の終わりと device の解放は窓が消えた後で、合わせて約 25 ms。

### 決定

1. **閉じる手順は ui/win32 の純関数 `closing_steps(ClosingRoute)` の列で、`EditorWindow::close_through` が 1 段ずつ写す。** `window_closes`（`leave`: ×・Alt+F4・最後のタブ・描画を失った終わり）は「隠す（`ShowWindow(SW_HIDE)`）→ 一覧・履歴・自動退避を書く（`EndSession`）→ OS の終了を止める理由を外す → 壊す」、`session_ends`（`WM_ENDSESSION` の確定）は「隠す → 書く」で窓を壊さない（ADR 0059 の決定 3）。`destroy_window` の段を持つのは `window_closes` だけで、それを渡すのは `leave` だけ（決定 1 の「本窓を壊すのは `leave` の 1 か所」は変えない。`DestroyWindow` の字面は `close_step` の 1 か所）。
2. **隠すのは未保存の確認を通り抜けた後。** 確認（`end_window` の `exit_plan`・最後のタブの `close_tab`）はこの列の前で、取り消せば列に入らず窓は見えたまま。`exit_plan` と `SessionEnd` は変えない。
3. **OS の終了を止める理由は書き終えてから外す**（今日と同じ順）。書いている間に OS が終了を進めても、理由が残っていれば止まる。
4. **書き込みの中身・順・失敗の扱いは変えない**（ADR 0059 / 0060 / 0137）。`remember_session` は今日どおり結果を捨て、隠した後に失敗しても知らせる相手はいない（今日も窓を壊す直前で何も出していない）。

### 却下した選択肢

- 書き込みを worker に移して窓をすぐ壊す: 終わる process は書き終わりを待たずに抜けると一覧と履歴を失う（ADR 0137 の同期の退避と同じ理由）。待つなら UI スレッドで待つのと同じで、窓を隠せば見た目の待ちは消える。
- flush や write-through の改名を外す: 停電・強制終了で一覧が壊れる（ADR 0135）。
- `DestroyWindow` を先にして後で書く: 決定 1 の「一覧を書いてから壊す」順が変わり、窓の handle を借りている adapter（クリップボード・裏の仕事の合図）が壊れた窓を指したまま書き込みが走る。隠すだけなら窓は生きたままで、今日の順のどこも変わらない。

### 検証

`nib_window_tests --closing`（2 つの道の列・隠すのが書き込みより前・止める理由は書いた後・壊すのは `window_closes` の最後の 1 回だけ・`session_ends` は壊さない・閉じる確認のキャンセルは `stay` で列に入らない）。前後の数字（同じ測り方・Release）: `WM_CLOSE` から窓が見えなくなるまで 空 567 → 2.6 ms・長い 540 → 2.6 ms（中央値・最大 12,444 → 2.7 ms）。process が終わるまでは書き込みの揺れが支配し（同じ条件で交互に測った空の 10 回: 前 532 / 後 403 ms の中央値）、隠す段そのものは約 10 ms。

### 限界

隠した後も書き込みが終わるまで process は残る（別の席のビルドで HDD が混んだ回は一覧の書き込みが 16〜19 秒、3 回は 60 秒で打ち切った）。その間に使う人が Nib を起動し直すと、新しい窓は書き終わる前の一覧（前回のもの）を読む。今日も窓が見えたまま同じだけ待っていたので、失うものは増えないが、待ちが目に見えなくなる。書き込み自体の遅さは #424 の領域。
