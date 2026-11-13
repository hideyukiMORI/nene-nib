# ADR 0137 — 未保存の編集は 30 秒ごとに裏で退避し、正常に閉じたら消し、起動で残っていれば 1 回聞く

- 状態: 受理（設計席 2026-10-16・施主決定 D47。実装は #515 の 3 工程（application → adapters → ui）で main・PR #626（元 #530）・2026-10-17・hide の試用 OK）
- 日付: 2026-10-16
- Issue: #515（施主決定 D47・外部講評 P0-3）・#735（決定 9・外部講評 2026-10-20）
- 規則: ARC-003 / ARC-004 / ARC-007 / ARC-010、CPP-002 / CPP-005 / CPP-013、QLT-012、ADR 0004 / 0010 / 0059 / 0060（決定 8）/ 0062 / 0133 / 0135

## 文脈

P0-1（#492・ADR 0133）と P0-2（#493）で「閉じる経路」は未保存の編集を黙って捨てなくなった。プロセスが殺される経路（強制終了・電源断・クラッシュ・OS の再起動の強行）は退避でしか守れない。施主は D47 で「30 秒ごとに設定の置き場へ自動退避し、正常に閉じたら消す。起動で残っていれば『前回の未保存を戻しますか』を 1 回聞く」と決めた。

守るべき既存の形: 時刻とタイマーは ui / adapters にしかない（ARC-007）。スレッドは `Win32Worker` の 1 本で、仕事の種類ごとに型のある port を application に宣言し、完了は中身の無い窓メッセージ → 意図 `WorkCompleted` → `collect()`（ADR 0062）。起動・開く・切り替え・保存・打鍵の道でファイルに触れない（ADR 0060 の決定 8・契約 `verify_quiet_paths`）。未保存は `Document.saved_position` と履歴の位置の一致で決まる（ADR 0010 の決定 7）。

## 決定

1. **合図は ui のタイマー、判断は application の純関数。** ui は `SetTimer(autosave_timer, 30 000 ms)` を窓の作成時に 1 本張り、`WM_TIMER` で意図 `AutosaveTick` を送る（引数なし・時刻を運ばない）。`AutosaveTick` は `WorkCompleted` と同じく「使う人の操作ではない」ので、`keeps_tab_walk` と `begin_intent` の両方に書き、Ctrl+Tab の歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない。
2. **退避する単位はタブで、書くのは「前回の退避から変わった未保存のタブ」だけ。** application は `autosave_plan(state, ledger)` の純関数で、各タブについて `AutosaveStep`（`store` / `discard` / `keep`）を決める。`store` は `save_state != saved` かつ `history の改訂番号 != ledger に覚えた退避の改訂番号`。改訂番号は core の `EditHistory::revision()`（編集・undo・redo のたびに 1 増える・まとめられた編集でも増える・`sealed` では変わらない）で、履歴の**位置**では比べない（続けて打った編集は `coalesce` で前の単位にまとまり位置が動かないので、位置の比較は取りこぼす・実装席の指摘 2026-10-16）。`discard` は保存済みになった・閉じたタブに退避が残っているとき。まだ読んでいないタブ（`UnloadedDocument`）は編集されたことが無いので ledger に無く `keep`（万一 ledger にあれば `discard`）。`ledger` は application が持つ `AutosaveLedger`（タブの退避番号 → 退避した改訂番号）で、1 打鍵の道では触らない。
3. **タブの名指しは退避番号 `AutosaveId`。** `Document` に `std::optional<AutosaveId> autosave` を足さず、`EditorState` がタブを足すときに振る通し番号（閉じても再利用しない）を `DocumentView` と同じ束で持つ。帯の位置・パス・題名で名指ししない（並べ替え・無題・同じパスの 2 タブで壊れる）。
4. **書くのは型のある port `AutosavePort`、置き場は設定の隣。** `store(AutosaveSnapshot)`（番号・パス（あれば）・題名・保存と同じ符号化で作った本文のバイト列・キャレットの位置）は頼んですぐ戻り、worker が書く（ADR 0062 の決定 5 と同じ形・`FolderPort` の `list` / `collect` に倣う）。`discard(AutosaveId)` も worker。adapter は `store` / `discard` の仕事が終わるたびに、成功でも失敗でも結果 `AutosaveOutcome`（番号と、失敗なら `AutosaveFailure`）を 1 件積み、`WorkCompleted` の合図（フォルダと共用の中身の無い窓メッセージ・ADR 0062）を 1 回出す。controller は `WorkCompleted` で `collect()` の結果を全部引き、失敗が 1 つでもあればステータスバーの 1 行「自動退避できませんでした」を**成功するまで 1 回だけ**出し（毎 30 秒に出さない）、失敗が無く成功が 1 つでもあれば知らせの印を下ろす（次の失敗でまた 1 回出る）。結果の無い合図（フォルダの分）では印を動かさない。失敗した番号は帳面から外し、本文が変わらなくても次の合図で書き直す（独立レビュー F3）。置き場は `%LOCALAPPDATA%\NeNeNib\autosave\<自分の pid の 16 進>\`、中身は一覧 `autosave.v1`（`session.v2` / `history.v1` と同じ行の形: `version=1`・タブごとに `entry=<番号>,<キャレットの行>,<桁>,<番号>.body,<パス（無題なら空）>` と `title=<題名>` の 2 行）と本文 `<番号>.body`。本文は保存と同じ経路（一時ファイル → 置き換え・ADR 0010 / 0135）で書く。
5. **消すのは「未保存のタブが 1 つも残らないか、使う人が「保存しない」と答えて閉じた `EndSession`」だけ。** `EndSession`（窓を閉じる・最後のタブを閉じる・OS のセッションの終わり）は欄 `unsaved_discarded`（既定は偽）を持つ。ui は確かめてから閉じる道（`close_window` と描画を失った `abandon` が通る `end_window`、最後のタブの `close_tab` が通る `leave`・ADR 0133）で、確かめ終えた後に未保存のタブが残っていれば真を立てる（残るのは「保存しますか」に「いいえ」と答えたものだけ）。確かめずに閉じる OS の `WM_ENDSESSION` では偽のまま送る。controller は application の純関数 `clears_autosave(state, intent)`（`AutosavePlan` の隣・読み込み済みのタブに未保存が無いか `intent.unsaved_discarded` のときだけ真）に従い、真のときだけ `ports_.autosave.clear()` を呼ぶ。確かめずに未保存が残る終わり方（OS の `WM_ENDSESSION` の強行）では消さず、残骸として次の起動で問う（独立レビュー F1）。描画を失って閉じるときの「保存しない」も同じ（`leave(SessionEnd::rendering_lost)` で捨てた印を立てて消す）。`clear()` の adapter は進行中の `store` の終わりを合図（event）で待ってから自分の pid のフォルダを消す。待つのは窓が閉じていく途中の UI スレッドで、上限は 1 回の書き込みの時間（`session.v2` の同期の書きと同じ位置）。
6. **起動で残っていれば 1 回聞く。** 残骸は `autosave\` の下の「pid が自分でなく、そのプロセスが生きていない」フォルダ（生きているかは ADR 0135 / #507 と同じ 1 本の判定・`OpenProcess`）。ui は窓を作って見せた後、`controller.autosave_remnants()`（フォルダの一覧 1 回・本文は読まない）が空でなければ `MessageBoxW`（はい / いいえ）で「前回の未保存を戻しますか」を 1 回だけ聞く。**はい**: 意図 `RestoreAutosave` → controller が残骸の本文を開くのと同じ復号で読み（`open_document` の経路・符号化と改行は自動判別）、パスがあればそのパスのタブとして・無ければ無題として帯の末尾に足し、`saved_position` は無し（未保存の印）、最初に戻したタブをアクティブにする。読めなかった残骸は「戻せませんでした: <題名>」の 1 行で知らせて飛ばす。戻したタブは次の合図を待たずにその場で `autosave_plan` を回して `store` を頼み、その後に残骸を消す（独立レビュー F2）。**いいえ**: 意図 `DiscardAutosave`。どちらも終わったら残骸のフォルダを消す。引数ありの起動でも聞く（D47）。
7. **起動・開く・切り替え・保存・打鍵の道でファイルに触れない契約は、起動のフォルダの一覧 1 回だけ広げる。** `verify_quiet_paths` に `AutosavePort` の呼ばれた回数を足し、起動で `remnants()` 1 回、ほかは 0 を数える。`AutosaveTick` の道は別の契約（`--autosave`）で、変わっていなければ `store` を呼ばないことを数える。
8. **試験は替え玉だけ。** application の契約は `ScriptedAutosave`（呼び出しの記録・失敗の注入）。adapters の試験は隔離した一時フォルダで、待つのはセマフォ、時計を読まない（CTest の `TIMEOUT` が止まりを落とす）。ui の確認は `eng/verify-window.py --autosave`（未保存を作る → 35 秒待つ代わりに `WM_TIMER` を直接送る → プロセスを殺す → 起動して問いが出る → はい で本文が戻る・未保存を作って閉じ「いいえ」と答える → 次の起動で問いが出ない）。
9. **確かめずに未保存が残る終わり方では、一覧より先に 1 回退避して書き終えるのを待つ（#735・外部講評 2026-10-20）。** 製品で `clears_autosave(state, intent)` が偽になるのは OS の `WM_ENDSESSION` の強行だけ（描画を失った終わりは `leave` が「保存しますか」を通して捨てた印を立てるので通らない・#741 のレビュー I1。契約は `rendering_lost` でも同じ形で固定する）。controller は `accept(EndSession)` で `settle_follow` の後、一覧を書く前に `autosave_now()` を 1 回回し（変わったタブだけを `store`・帳面の改訂番号で比べる・決定 2）、`AutosavePort::flush()` で worker の待ち行列と進行中の `store` / `discard` が片付くまで待ってから、一覧を書き履歴に記録する（順はそのまま）。`flush` は置き場を消さず、以後の仕事も受ける（`clear` と違い閉じない）。adapter は頼まれてまだ片付いていない仕事の数を棚で数え、0 になると立つ event を `WaitForSingleObject` の上限 3,000 ms（`WM_ENDSESSION` の猶予の内）で待つ。時計は読まない。ワーカーの前に載ったほかの仕事（フォルダの列挙）も待ちに含まれる。上限を越えたか書けなかったら、そのまま終わる（残骸は前回の退避のまま・次の起動で問う）。`clears_autosave` が真なら今までどおり `clear()` だけで、`autosave_now` も `flush` も呼ばない。

## 却下した選択肢

- 時刻で「古い残骸」を決める: 時計に頼る。生きていない pid で決める（ADR 0135 と同じ）。
- 変わっていなくても 30 秒ごとに書く: 16 MiB を 30 秒ごとに写す。変わったタブだけ。
- UI スレッドで同期に書く: 16 MiB の書きが打鍵の途中に 10〜40 ms 止める。worker に出す。
- 本文を専用の形（LF・UTF-8）で書く: 戻す経路が 2 本になる。保存と同じ符号化で書き、開くのと同じ復号で読む。
- 退避の有無を設定にする: D47 に無い。必要なら別の施主決定。
- 履歴の位置で「変わった」を決める: `coalesce` でまとめられた編集と undo の後の同じ位置で取りこぼす。改訂番号。
- ledger に位置・件数・先頭の編集の参照を覚える: `EditHistory` の内部を外に見せる。整数 1 つで足りる。
- tick で履歴の単位を閉じる: 決定 1（undo を動かさない）に反する。
- 帯の位置やパスでタブを名指しする: 並べ替え・無題・同じパスの 2 タブで壊れる。通し番号。
- 残骸を黙って戻す / 黙って消す: D47 は「1 回聞く」。

## 検証

- application: `--autosave`（plan の表: 未保存で変わった → store、変わっていない → keep、保存済み → discard、閉じた → discard、UnloadedDocument → keep・確かめずに未保存の残る `EndSession` は `clear` しない・残らないか `unsaved_discarded` なら 1 回・復元の直後に `store` してから `forget`・決定 9: 強行の `EndSession` と `rendering_lost` は store → flush → 一覧の書きの順・変わっていないタブは store しない・捨てた終わり方と未保存の無い終わり方は `clear` だけで flush しない）・`--application`・`--background-work`（`AutosaveTick` は歩きを切らない）・`verify_quiet_paths` の拡張。
- adapters: 一覧と本文の往復、進行中の store の後の clear、flush が順番待ちと進行中の仕事の後に戻り閉じないこと・塞がったワーカーでは上限で戻ること、生きていない pid のフォルダだけが残骸、生きている pid は飛ばす。
- ui: `verify-window.py --autosave`。速さ: `startup-first-frame` と `key-to-frame-single` の退行なし（起動は一覧 1 回・打鍵の道は触らない）。

## 限界

- 退避は 30 秒の粒度で、最後の 30 秒の編集は守れない（OS の終了の強行と描画を失った終わりは決定 9 で終わる前に 1 回退避する。プロセスが殺される経路は守れない）。決定 9 の待ちは 3 秒が上限で、16 MiB を超える本文やワーカーの前の長い仕事があると書き終わらずに終わり得る。
- 本文の置き場が別ボリューム（`%LOCALAPPDATA%` が無い）なら退避できず、1 行の知らせだけ。
- 64 MiB の上限は保存と同じ。退避のフォルダの容量は見張らない。
- 2 つの Nib が同じファイルを編集していて片方が殺されたとき、残骸は次の起動で問われ、戻すと同じパスのタブが 2 つになり得る（今日の「同じファイルを 2 つの窓で開く」と同じ）。
- 戻した直後に殺されると、戻した本文は失われ得る。`store` は頼んですぐ戻り、残骸の `forget` は worker の書きの終わりを待たないので、書きが終わるまでの短い隙間（ms の桁）は守れない（決定 6）。
- 残骸のフォルダの pid が別のプロセスに再利用されて生きていると、そのフォルダは問われない（そのプロセスが終わった後の起動で問われる）。→ 今の形のフォルダでは追補 #1634 で直した（作成時刻が違えば残骸と読む）。前の版の形 `<pid>` のフォルダには、この限界が残る。
- 一覧 `autosave.v1` が壊れて本文だけが残ったフォルダは問わず、消さない。

## 追補（2026-10-29・#993・ADR 0159 の決定 7・L-4）

- 退避の上限は `core::maximum_decoded_document_bytes`（64 MiB）の 1 つ。計画 `autosave_plan` は未保存で変わった本文がこれを超えると `oversized` を返し、書かず・前の退避も消さず、知らせ「64 MiB を超える文書は自動退避しません」を文書（退避番号）ごとに 1 回だけ出す（鳴らさない）。読み戻しの上限は、その本文を最も膨らむ符号化（UTF-16 は 2 倍）にして BOM を足した派生の定数 `core::maximum_encoded_autosave_bytes`（書けた退避を必ず戻せる・#1095 のレビュー R1-2・設計席が本文を整える）。

## 追補 2026-11-13（#1625）: 最後の砦 — 捕まえていない例外で落ちる前に、決定 9 と同じ退避を 1 回だけ通す

### 文脈

製品には捕まえていない例外の受け口が無かった。窓の手続き（`EditorWindow::procedure`・`CoverWindow::procedure`）と `Win32Worker::run` は `noexcept` で、`set_terminate` も `catch (...)` も無い。どの例外でも `std::terminate` → `abort`（0xC0000409）で落ち、最後の退避（30 秒ごと）より後の編集を失っていた（#1616 の長い `:s` の落ちで実際に起きた・独立レビュー V-1623 の (c)）。#1616 は 1 か所の直しで塞いだが、同じ種類の穴が将来また入ったときの害を小さくする。

### 決定

1. **手続きは 1 つ、置くのは合成ルート。** `src/adapters/win32/LastResortArming` が `std::terminate` の手続きを 1 つ持つ。`std::set_terminate` は UCRT ではスレッドごとの欄（`__acrt_getptd()->_terminate`・SDK 10.0.26100.0 の `ucrt/misc/terminate.cpp` で確かめた）なので、置く口は 2 つになる。UI スレッドは合成ルートが controller を作った直後に作る `LastResortArming`（UI スレッドの番号と退避の口 1 つを覚える）。ワーカーは合成ルートが最初の仕事として投げる device の生成の仕事の頭の 1 行 `arm_last_resort_thread()`。post の順と回数は変えない（ADR 0131）。core と application には手続きを足さない。
2. **対象は C++ の `std::terminate` だけ。** 誰にも捕まらなかった例外と、`noexcept` を例外が抜けたとき。SEH（アクセス違反・stack の溢れ・`__fastfail`）は `SetUnhandledExceptionFilter` を置かないので受けない。壊れたメモリの上でファイルを書く危険が大きく、stack の溢れでは手続きを動かす stack も無いため（後続の判断）。
3. **UI スレッドのときだけ退避する。** 手続きは呼ばれたスレッドが覚えた UI スレッドと同じときだけ退避の口を呼ぶ。ワーカーで呼ばれたら、状態は UI スレッドの持ち物なので触れず、退避せずに既定の終わり方へ渡す。
4. **退避は決定 9 と同じ道を 1 回だけ。** controller の公開の口 `back_up_before_defect()` が、計画を回して store と discard を頼み（`run_autosave_steps`・`autosave_now` と共用）、`AutosavePort::flush()`（上限 3,000 ms）で待ち、`collect()` の結果を引く。全部書けたら真（手続きはこの値を使わない・契約が見る）。`flush` は片付いたかを返す（`[[nodiscard]] bool`・時間切れなら偽）。一覧・履歴・読みの取消・言語サーバーには触れず、`accept(EndSession)` を通さない。
5. **意図の途中の状態で前の退避を上書きしない。** `apply` は 1 意図の中で `state_` を何度も書き換える。マクロと `.` の再生は鍵ごとに本文を写してから最後に 1 単位に畳むので、途中で投げると本文はその途中の値になる（例: `@a` = `ddp` が `dd` の後で投げると行が消えたまま）。これを退避すると、前の退避を同じ `<番号>.body` に上書きしてしまう。そこで `apply` の入口が、いちばん外の意図のときだけ印 `IntentMark`（アクティブなタブの退避番号・その改訂番号・タブの数・切り替えの回数）を書き、深さを 1 つ増やす。出口は `return` の直前で深さを 1 つ戻す。RAII にはしないので、例外で抜けると深さは 0 より大きいまま残り、手続きは「意図の途中」と読む。足したのは整数の代入だけで、確保は無い。計画は application の純関数 `last_resort_plan(state, ledger, 印, 深さ)` の 1 つ:
   - 深さが 0（意図の外）: `autosave_plan` そのまま。
   - 意図の中で、アクティブのタブ・タブの数・切り替えの回数が入口と同じ: 改訂番号も同じなら `autosave_plan` そのまま。進んでいればそのタブだけ `keep`（前の退避を残す）にして、ほかのタブは書く。
   - 意図の中で、どれかが入口と違う: 何も書かない（flush は呼ぶ）。
   **規則: 意図の中で落ちたとき、書いてよいのは「入口の時点から 1 つも変わっていないと言えるタブ」だけで、それは「切り替えの回数が入口と同じ ⇒ アクティブ以外のタブには触れていない」で見分ける。** 編集の経路はアクティブな文書にしか触れない（ADR 0056）。アクティブでないタブの束が変わるのは、そのタブが一度アクティブになって（広げて）から置かれたときだけで、アクティブを置いてほかの束を広げるのは `EditorState::switched_to` の 1 か所（`with_switched`・`with_walked`・もう 1 つの枠への移り）なので、ここで回数 `tab_switches()` を 1 つ進める。タブを足す・閉じる道（`with_new_tab`・`with_closed`）は回数を進めないが、タブの数か、アクティブの退避番号（通し番号・再利用しない）が入口と違ってしまうので、同じく何も書かない。行って戻る切り替え（マクロや `map` の `gt … gT`・`:tabnext` と `:tabprevious`）は、アクティブの番号・改訂番号・タブの数が入口と同じになるが、回数で分かる（レビュー V-1628 の M1・契約 `verify_backup_midway_returned`）。アクティブでないタブの本文を意図の途中で書き換える道がほかに無いことは、レビュー V-1628 が確かめた（すべて置換・`:bufdo` `:tabdo` は製品に無い・LSP は診断だけ・エクスプローラーは一覧の状態だけ・split のもう一方の枠は同じ文書のキャレットだけ・プレビューのタブは新しい退避番号・`with_loaded` は変更の無いタブ）。タブごとの改訂番号を入口で覚える形は、タブの数に比例する費用が 1 打鍵ごとにかかるので採らない。回数は状態の値（写しで運ばれる・束を複製しない・ADR 0180）で、比較にも一覧（`session.v2`）にも入れない。
6. **二重に入らない。** 手続きは入口で `InterlockedExchange` の印を立て、2 度目（退避の中で `noexcept` を抜けた・別のスレッドが同時に落ちた）は退避せずに既定の終わり方へ渡す。退避の口が投げた例外（`bad_alloc` を含む）は手続きが `catch (...)` で受ける。`<atomic>` は使わない（CNF-009）。
7. **終わり方は既定のまま。箱を出さず、窓に触れない。** 退避の後（ワーカー・2 度目の入りでは退避せずに）、手続きは置いたときに `std::set_terminate` が返した前の手続き（スレッドごとに覚える・UCRT の既定は `abort`）を呼び、無いか戻ったときは `std::abort()` を呼ぶ。自分の箱（`MessageBoxW`）も、窓の手続きの差し替え（`DefWindowProcW`）も、`TerminateProcess` も使わない（レビュー V-1628 の L1〜L4）。理由:
   - 箱を放置している間は process が生きていて、次の起動が退避を「生きている process の物」と読み、復元の問いを出さなかった（L3）。
   - 窓の手続きを替えると、そのスレッドの IME の窓まで替わった（L2）。
   - `TerminateProcess` では WER の記録と dump が無くなった（L1）。既定の `abort` なら、製品の Release は `_CALL_REPORTFAULT` で `__fastfail(FAST_FAIL_FATAL_APP_EXIT)`（0xC0000409）になり、WER の記録が残る（UCRT の `startup/abort.cpp` を読んで確かめた）。
   - 文を 3 言語で正確に書くのが難しかった（keep の枝・`clear()` の後・退避できなかったとき・L4）。使う人への知らせは次の起動の問い（決定 6）が担う。
   Debug の CRT は `abort` で「abort() has been called」の箱を出す（`_WRITE_ABORT_MSG` が Debug の既定）。箱が出るのは開発者の Debug ビルドだけで、製品の Release では出ない。子 process の試験の exe だけが `_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)` と `SetErrorMode` で箱と WER を止める（製品には入れない）。終了コードは CRT の値（Release の製品は 0xC0000409・試験の子は 3）で、自分では決めない。
8. **次の起動** は、いまの「前回の未保存を戻しますか」（決定 6）がそのまま出る。置き場は自分の pid のフォルダで、残骸と読む条件は「名前の pid が自分でなく、その process が生きていない」（`process_alive` は `GetExitCodeProcess` が `STILL_ACTIVE` でないことだけを見る）なので、終了コードの値には依らない。正常に閉じたときの `clear()`（`EndSession`）は置き場を消すので、最後の砦を通った process の置き場は残る。新しい問いは足さない。
9. **壊すと外す。** `LastResortArming` は controller より後に作り、先に壊れる。壊すときは UI スレッドの印と退避の口を外し、UI スレッドの手続きを前のものへ戻す。以後の `terminate` は既定のまま終わる（ワーカーの手続きは残るが、退避せずに既定へ渡すだけ）。

### 試験

- application の `--last-resort`（既定実行にも載る）。計画の枝（入れ子の深さ・閉じたタブの discard・行って戻った切り替えの回数を含む）を見る。controller の口を 4 通りで見る:
  - 意図の外: store → flush。一覧は書かず clear もしない。flush の時間切れと書けなかった結果は偽。
  - 本当に意図の途中から抜ける形: `@a` の再生の中の `:w` で、替え玉の `ScriptedFiles::throw_on_writes` が投げる。途中まで写したアクティブのタブは書かず、ほかの変わったタブだけ書く。
  - 再生の中の `gt` でアクティブが替わった: flush だけ。
  - 再生の中で `gt dd gT` と行って戻ってから投げた（`@a` = `gt dd gT :w<CR> gt P gT`）: flush だけ（直す前の HEAD では `store 1` が出て落ちることを確かめた）。
- adapters の `nib_adapters_last_resort`。子 process `nib_last_resort_child` が、製品と同じ `LastResortArming` / `arm_last_resort_thread` を置いて本当に `terminate` を起こす。親は子の終わりを上限 30 秒で待ち、固まれば殺して落とす（CTest の TIMEOUT 180）。終了コードの値は期待に書かず、0 でなく、子が手続きを通らずに戻った値でもないことだけを見る。
  - (a) UI スレッドで `noexcept` を抜ける: 退避の口が 1 回走る（替え玉が backup.txt を書く）。
  - (b) ワーカーの仕事が投げる（`Win32Worker::run` の `noexcept`）: 退避しない。
  - (c) 退避の口が投げる / 退避の中で `noexcept` を抜ける（2 度目の入り）/ UI スレッドが `std::mutex` を握ったまま落ち、退避が同じ mutex を取りに行く（adapter のロックの再入の形）: どれも退避は書かれず、固まらずに終わる。
  - (d) 本物の `Win32AutosaveAdapter`（置き場の下の `work/autosave`・本物のワーカーとファイルの port）で 1 本を退避して落ちる。親が同じ置き場の adapter で、次の起動と同じ `remnants()` → `read()` を通し、落ちた子の退避が残骸として 1 本並び、本文が読めることを見る。
  - Debug と Release の両方の exe で通ることを作業木で確かめた（CI の `ctest` は Release）。
- 製品の `NeNeNib.exe` を落とす試験は、窓を出すので作らない。施主の確認の手順は限界の項。

### 限界

- 状態そのものが壊れていれば（例外の前に不変条件が破れていた）、退避も壊れ得る。守るのは「途中まで写した意図の本文で前の退避を上書きしない」ことだけで、確定した状態の正しさまでは保証しない。
- 意図の途中で落ちたアクティブのタブは、最後の退避（30 秒ごと）より後の編集を失う（前のまま）。意図の途中で切り替えがあったときは、どのタブも前の退避のまま。
- UI スレッドが adapter のロックを握ったまま落ちたとき、同じロックをもう一度取ると MSVC STL の `std::mutex` は例外を投げる（子 process の試験の (c) で、固まらずに終わることを確かめた。例外の種類は確かめていない）。手続きはこれを受けて終える。ロックの外で止まる形（OS の資源の待ち）は flush の上限 3,000 ms のほかに守りが無い。
- SEH の落ち方・プロセスが殺される経路・`abort` の直接の呼び出しは守らない。ワーカーでの落ちは退避しない（UI スレッドの状態に触れないため）。ワーカーで落ちると process はすぐ終わり、UI スレッドの退避の途中でも止まる。
- WER が記録を集める間（`__fastfail` の後）に process がどれだけ残るか、WER の設定によって箱が出るかは確かめていない。残っている間に次の Nib を起動すると、その起動では復元の問いが出ない（次の次の起動で出る）。
- 施主の確認（後日・未実施）: 落ちる入力を持つ診断用の版（例: #1616 の直しを戻した Release の exe・自分の profile）で、未保存の文書を作って落とし、次の起動で「前回の未保存を戻しますか」が出て戻ることを 1 回見る。手順と SEH の判断は後続の Issue。

## 追補 2026-11-13（#1634）: 置き場の名前は pid と process の作成時刻の組

### 文脈

置き場は `autosave\<pid の 16 進>\` で、残骸かどうかは「名前の pid が自分でなく、その process が生きていない」で決めていた（決定 4・6）。Windows は pid を再利用する。次の Nib が、落ちた process と同じ pid を引くと、困ったことが起きる。

- 前の退避のフォルダを自分の物として扱い、起動で問わない。
- `store` が同じ番号の本文を上書きする。
- 正常に閉じると、`clear()` がフォルダごと消す。一度も store しなかった起動でも消す。

これで、未保存の本文を黙って失っていた。adapters の試験 `verify_reused_process_id` で、直す前の HEAD（`c970e47f`）では 3 件が落ちることを確かめた（V-1628b の F2）。

### 決定

1. **1 回の起動の印は pid と作成時刻の組。** フォルダ名は `<pid の小文字 16 進>-<作成時刻の小文字 16 進>`。作成時刻は `GetProcessTimes` の creation time で、100 ns 単位の 64 bit をそのまま入れる。
   - 自分の置き場の名前を作るのは `autosave_folder(root, ProcessInstance)` の 1 か所。store・discard・clear・最後の砦・決定 9 の退避は、どれもここから引く。
   - 自分の印（`current_process_instance`）は、adapter が自分の置き場の名前を最初に要るときに 1 回だけ読む。読むのは UI スレッドで、最初の store・discard・clear・remnants のどれか。窓が見えるまでの起動の道には、OS の呼び出しを足さない。
   - ファイルにもレジストリにも触れない。
2. **作成時刻は、等しいかだけを見る識別の値。** 時計を読むのではない。OS が process に持たせている属性で、大小も経過も見ない。
   - ADR 0135 の「時刻で決めない」は、古さ（何秒たったか）で回収を決めない、という意味なので、これに反しない（ADR 0135 に注記した）。
   - ARC-007 の「現在時刻」でもない。`GetProcessTimes` は conformance の `nondeterministicNames` に無く、adapters の `Win32TimingAdapter` が既に使っている。
3. **残骸の判定は純関数 `autosave_remnant_name(name, self, created_of)` の 1 本。** `created_of` は「pid → 作成時刻 | gone | denied」の口で、OpenProcess の 1 本の判定 `process_created` を使う。`process_alive`（ADR 0135）も同じ判定を読む。試験は替え玉で答える。
   - 今の形 `<p>-<t>`:
     - 自分の印と同じなら、自分の置き場。
     - `p` が自分で `t` が違えば、残骸（同じ pid の前の process は必ず終わっている）。
     - ほかの pid は、gone なら残骸。生きていて作成時刻が `t` と違えば、残骸（pid が再利用された）。等しければ生きている本人なので飛ばす。
     - denied は、生きている側に倒して飛ばす。
   - 前の版の形 `<p>`:
     - `p` が自分なら、無条件に残骸（今の版は前の形を書かない）。
     - ほかは今までどおり、gone のときだけ残骸。
   - 前の形を今の形へ書き換えない。復元の問いで拾い、拾った後は今の片付けの道（`forget`）で消える。
   - `read` と `forget` の location（`<フォルダ名>/<番号>`）は、両方の形を厳密に受ける。
4. **変えないもの。**
   - `autosave.v1` の中身と版。
   - application。location は不透明な文字列のまま。
   - 起動の道でファイルに触れる回数（根の一覧 1 回と、残骸ごとに一覧 1 回）。
   - 決定 7 の契約。
   - 一時ファイルの名前（ADR 0135・名前がぶつかれば `CREATE_NEW` が連番を進め、原本でもないので同じ穴は無い）。
   - README と PRIVACY の置き場の説明（`%LOCALAPPDATA%\NeNeNib\autosave\`）。
   - 追補 #1625 の決定 8 の「残骸と読む条件」は、この判定に置き換わる。

### 試験

- adapters の `nib_autosave`:
  - (i) 同じ pid の前の起動のフォルダ（前の形の番号 1・作成時刻の違う今の形の番号 2）がある所から起動すると、両方が残骸として出る。
  - 自分の store が前の本文を上書きしない。
  - `clear()` は自分のフォルダだけを消す。store しなかった起動の `clear()` は何も消さない。
  - 両方の形の残骸を `forget` すると、どちらのフォルダも消える。
  - (j) 判定の表（自分・作成時刻違い・前の形の自分・gone・生きている本人・再利用・denied・形の外）。
- `nib_adapters_autosave_pair_property`: 自分の pid の名前のフォルダを「残骸でない」とした行を、「この起動のフォルダ（pid と作成時刻が自分）は残骸でない・同じ pid の前の起動のフォルダは残骸」に直した。

### 限界

- 前の版の形のフォルダで、pid が生きている別の process に再利用されているものは、今までどおり問われない（その process が終わった後に問われる）。
- `OpenProcess` が拒否する process（別のユーザー・昇格）に pid が再利用されていると、今の形でも残骸と読めない（生きている側に倒す）。
