# ADR 0137 — 未保存の編集は 30 秒ごとに裏で退避し、正常に閉じたら消し、起動で残っていれば 1 回聞く

- 状態: 提案（設計席 2026-10-16。実装は #515 で 3 工程: application → adapters → ui）
- 日付: 2026-10-16
- Issue: #515（施主決定 D47・外部講評 P0-3）
- 規則: ARC-003 / ARC-004 / ARC-007 / ARC-010、CPP-002 / CPP-005 / CPP-013、QLT-012、ADR 0004 / 0010 / 0059 / 0060（決定 8）/ 0062 / 0133 / 0135

## 文脈

P0-1（#492・ADR 0133）と P0-2（#493）で「閉じる経路」は未保存の編集を黙って捨てなくなった。プロセスが殺される経路（強制終了・電源断・クラッシュ・OS の再起動の強行）は退避でしか守れない。施主は D47 で「30 秒ごとに設定の置き場へ自動退避し、正常に閉じたら消す。起動で残っていれば『前回の未保存を戻しますか』を 1 回聞く」と決めた。

守るべき既存の形: 時刻とタイマーは ui / adapters にしかない（ARC-007）。スレッドは `Win32Worker` の 1 本で、仕事の種類ごとに型のある port を application に宣言し、完了は中身の無い窓メッセージ → 意図 `WorkCompleted` → `collect()`（ADR 0062）。起動・開く・切り替え・保存・打鍵の道でファイルに触れない（ADR 0060 の決定 8・契約 `verify_quiet_paths`）。未保存は `Document.saved_position` と履歴の位置の一致で決まる（ADR 0010 の決定 7）。

## 決定

1. **合図は ui のタイマー、判断は application の純関数。** ui は `SetTimer(autosave_timer, 30 000 ms)` を窓の作成時に 1 本張り、`WM_TIMER` で意図 `AutosaveTick` を送る（引数なし・時刻を運ばない）。`AutosaveTick` は `WorkCompleted` と同じく「使う人の操作ではない」ので、`keeps_tab_walk` と `begin_intent` の両方に書き、Ctrl+Tab の歩き・知らせ・変換・Vim の待ちと録画・`.`・undo を動かさない。
2. **退避する単位はタブで、書くのは「前回の退避から変わった未保存のタブ」だけ。** application は `autosave_plan(state, ledger)` の純関数で、各タブについて `AutosaveStep`（`store` / `discard` / `keep`）を決める。`store` は `save_state != saved` かつ `history の改訂番号 != ledger に覚えた退避の改訂番号`。改訂番号は core の `EditHistory::revision()`（編集・undo・redo のたびに 1 増える・まとめられた編集でも増える・`sealed` では変わらない）で、履歴の**位置**では比べない（続けて打った編集は `coalesce` で前の単位にまとまり位置が動かないので、位置の比較は取りこぼす・実装席の指摘 2026-10-16）。`discard` は保存済みになった・閉じたタブに退避が残っているとき。まだ読んでいないタブ（`UnloadedDocument`）は編集されたことが無いので ledger に無く `keep`（万一 ledger にあれば `discard`）。`ledger` は application が持つ `AutosaveLedger`（タブの退避番号 → 退避した改訂番号）で、1 打鍵の道では触らない。
3. **タブの名指しは退避番号 `AutosaveId`。** `Document` に `std::optional<AutosaveId> autosave` を足さず、`EditorState` がタブを足すときに振る通し番号（閉じても再利用しない）を `DocumentView` と同じ束で持つ。帯の位置・パス・題名で名指ししない（並べ替え・無題・同じパスの 2 タブで壊れる）。
4. **書くのは型のある port `AutosavePort`、置き場は設定の隣。** `store(AutosaveSnapshot)`（番号・パス（あれば）・題名・保存と同じ符号化で作った本文のバイト列・キャレットの位置）は頼んですぐ戻り、worker が書く（ADR 0062 の決定 5 と同じ形・`FolderPort` の `list` / `collect` に倣う）。`discard(AutosaveId)` も worker。失敗は `collect()` で `WorkCompleted` のときに引き、ステータスバーの 1 行「自動退避できませんでした」を**成功するまで 1 回だけ**出す（毎 30 秒に出さない）。置き場は `%LOCALAPPDATA%\NeNeNib\autosave\<自分の pid の 16 進>\`、中身は一覧 `autosave.v1`（`session.v2` / `history.v1` と同じ行の形: `version=1`・タブごとに `entry=<番号>,<キャレットの行>,<桁>,<番号>.body,<パス（無題なら空）>` と `title=<題名>` の 2 行）と本文 `<番号>.body`。本文は保存と同じ経路（一時ファイル → 置き換え・ADR 0010 / 0135）で書く。
5. **正常に閉じたら消す。** `EndSession`（窓を閉じる・最後のタブを閉じる・描画を失って閉じる・OS のセッションの終わり）で controller が `ports_.autosave.clear()` を呼び、adapter は進行中の `store` の終わりを合図（event）で待ってから自分の pid のフォルダを消す。待つのは窓が閉じていく途中の UI スレッドで、上限は 1 回の書き込みの時間（`session.v2` の同期の書きと同じ位置）。
6. **起動で残っていれば 1 回聞く。** 残骸は `autosave\` の下の「pid が自分でなく、そのプロセスが生きていない」フォルダ（生きているかは ADR 0135 / #507 と同じ 1 本の判定・`OpenProcess`）。ui は窓を作って見せた後、`controller.autosave_remnants()`（フォルダの一覧 1 回・本文は読まない）が空でなければ `MessageBoxW`（はい / いいえ）で「前回の未保存を戻しますか」を 1 回だけ聞く。**はい**: 意図 `RestoreAutosave` → controller が残骸の本文を開くのと同じ復号で読み（`open_document` の経路・符号化と改行は自動判別）、パスがあればそのパスのタブとして・無ければ無題として帯の末尾に足し、`saved_position` は無し（未保存の印）、最初に戻したタブをアクティブにする。読めなかった残骸は「戻せませんでした: <題名>」の 1 行で知らせて飛ばす。**いいえ**: 意図 `DiscardAutosave`。どちらも終わったら残骸のフォルダを消す。引数ありの起動でも聞く（D47）。
7. **起動・開く・切り替え・保存・打鍵の道でファイルに触れない契約は、起動のフォルダの一覧 1 回だけ広げる。** `verify_quiet_paths` に `AutosavePort` の呼ばれた回数を足し、起動で `remnants()` 1 回、ほかは 0 を数える。`AutosaveTick` の道は別の契約（`--autosave`）で、変わっていなければ `store` を呼ばないことを数える。
8. **試験は替え玉だけ。** application の契約は `ScriptedAutosave`（呼び出しの記録・失敗の注入）。adapters の試験は隔離した一時フォルダで、待つのはセマフォ、時計を読まない（CTest の `TIMEOUT` が止まりを落とす）。ui の確認は `eng/verify-window.py --autosave`（未保存を作る → 35 秒待つ代わりに `WM_TIMER` を直接送る → プロセスを殺す → 起動して問いが出る → はい で本文が戻る）。

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

- application: `--autosave`（plan の表: 未保存で変わった → store、変わっていない → keep、保存済み → discard、閉じた → discard、UnloadedDocument → keep）・`--application`・`--background-work`（`AutosaveTick` は歩きを切らない）・`verify_quiet_paths` の拡張。
- adapters: 一覧と本文の往復、進行中の store の後の clear、生きていない pid のフォルダだけが残骸、生きている pid は飛ばす。
- ui: `verify-window.py --autosave`。速さ: `startup-first-frame` と `key-to-frame-single` の退行なし（起動は一覧 1 回・打鍵の道は触らない）。

## 限界

- 退避は 30 秒の粒度で、最後の 30 秒の編集は守れない。
- 本文の置き場が別ボリューム（`%LOCALAPPDATA%` が無い）なら退避できず、1 行の知らせだけ。
- 64 MiB の上限は保存と同じ。退避のフォルダの容量は見張らない。
- 2 つの Nib が同じファイルを編集していて片方が殺されたとき、残骸は次の起動で問われ、戻すと同じパスのタブが 2 つになり得る（今日の「同じファイルを 2 つの窓で開く」と同じ）。
- 失敗した退避の再試行は次の変更まで（帳面は store を頼んだ時点で進むので、worker で失敗した本文は次に本文が変わるまで書き直さない）。
