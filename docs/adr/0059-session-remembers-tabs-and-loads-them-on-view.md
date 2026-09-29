# ADR 0059 — 前回のタブは別のファイルに覚え、起動のときは見ていたタブだけを読む

- 状態: 受理（設計席 2026-09-30・Issue #252 / #253・**施主決定 D24〜D27**）
- 日付: 2026-09-30
- Issue: #252（覚える）・#253（戻す）
- 影響する規則: FR-009 / FR-005 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-005 / CPP-011 / QLT-001 / QLT-012 / QLT-013 / QLT-014
- 前提（仕様の決定）: **D9・FR-009「タブの復元は保存済みファイルだけ」**（未保存の新規タブが溜まるのを避ける）・D1（起動の速さ）・D22（最後の 1 つのタブを閉じたら窓を閉じる）・D23（Ctrl+Tab は最近使った順）。本 ADR はこれらを変えない。
- 前提（ADR）: [ADR 0056](0056-tabs-park-inactive-documents-behind-the-active-one.md)（複数タブ・脇に置いた束・決定 13 の起動）・[ADR 0058](0058-ctrl-tab-walks-tabs-in-recent-order.md)（使った順）・[ADR 0020](0020-versioned-editor-settings-and-point-font-size.md)（設定の port と adapter・保存形式は adapters に閉じる）・[ADR 0013](0013-startup-shows-the-window-before-the-device.md)（窓を先に見せる起動）・[ADR 0010](0010-file-slice-fileport-encoding-detection-atomic-save.md)（`FilePort`・失敗は閉じた値）

## 文脈

仕様は「タブの復元は保存済みファイルだけ」（D9・FR-009）の 1 行で、いつ戻すか・何を覚えるか・無くなったファイルをどうするか・いつ読むかは決まっていなかった。2026-09-30 に設計席が選択肢と良い点・悪い点を説明し、施主が決めた。

| 決定 | 内容 |
| --- | --- |
| D24 | ファイルを指定して起動したときは、指定したファイルだけを開く。前回のタブを戻すのは、何も指定せずに起動したときだけ |
| D25 | 覚えるのは、タブの並び・どれを見ていたか・各タブのカーソルと画面の位置。外でファイルが変わっていたら範囲の中へ寄せる |
| D26 | 前回のファイルが無くなっていたら、ステータスバーに 1 行だけ知らせる（窓は止めない） |
| D27 | 前回のタブのファイルは、そのタブを見るときに読む。起動のときに読むのは最後に見ていたタブだけ。無くなったファイルは、そのタブを見ようとしたときに知らせて閉じる |

現物の調査（`out/probes/probe-restore-2026-09-30.md`）で分かったこと:

- 起動引数のファイルは、窓を見せる前に controller のコンストラクタが同期で読む。全部を起動のときに読む形だと、窓が見えるまでの時間（基準 35 ms）がタブの数と大きさで延びる（16 MiB の 1 本で約 50 ms）。
- 設定（`settings.v1`）は 4 つのキーだけの版つきの形で、未知のキーは拒否する。書くのは変更のたびで、終了のときに書く経路は無い。
- 脇に置いた束 `DocumentState` は「読み込み済みの完全な文書」だけで、「まだ読んでいない文書」を表す型は無い。
- 窓が終わる道は 4 つ（窓を閉じる・最後の 1 本を閉じる・描画の失敗・デストラクタ）。終わる直前に application へ送る意図は無い。`WM_ENDSESSION` は扱っていない。
- 速さの計測と実機の検査の道具は、同じ profile のフォルダを使い回す。復元が入ると、16 MiB を開いた試行の次の起動が 16 MiB を戻し、基準値が壊れる。

## 決定

**前回のタブは設定とは別のファイル `session.v1` に覚える。書くのは窓が閉じるとき。読むのは、ファイルを指定せずに起動したとき。戻したタブは「まだ読んでいない文書」として帯に並び、初めて見るときに読む。起動の重さはタブの数に依らない。**

1. **覚える値（application）**: `Session { std::vector<SessionTab> tabs; std::size_t active; }` と `SessionTab { core::FilePath path; core::TextPosition caret; core::LineNumber first_visible; std::size_t recency; }`（1 型 1 ファイル）。`tabs` は帯の順、`active` は `tabs` の中の位置、`recency` は使った順の順位（0 がいちばん最近）。パスの無いタブ（無題）は入れない（D9）。パスがあって未保存の変更があるタブは入れる（窓を閉じるときに保存するか捨てるかを確かめ済みで、戻るのはディスクの上の中身）。
2. **port と adapter**: `SessionPort`（application）は `read() → std::expected<std::optional<Session>, SessionFailure>` と `write(const Session &) → std::expected<void, SessionFailure>`。`SessionFailure` は閉じた enum（`location_unavailable` `unreadable` `too_large` `malformed` `unsupported_version` `unwritable`）。adapter は `src/adapters/win32/Win32SessionAdapter`、場所は設定と同じ親の `%LOCALAPPDATA%/NeNeNib/session.v1`。保存形式（UTF-8 のテキスト・`version=1`・1 行 1 タブ・パスは行の最後の欄で行末まで）は adapters に閉じる（ADR 0020 の決定 3 と同じ置き方）。欄の区切りは `,` で、1 行は `<カーソルの行>,<桁>,<画面の先頭の行>,<使った順の順位>,<パス>`（どれも 1 始まり・順位は 0 始まり）。設定と同じ親のフォルダの組み立ては `beside_local_settings` の 1 本（設定の場所 `local_settings_path` の親に名前を付ける）で、テーマと一覧の adapter が通る。親のフォルダを作るのは設定と一覧の adapter が共用する `ensure_parent_directory` の 1 本（#252 の工程 2）。上限は 256 タブ・1 MiB。書きは `FilePort::write`（一時ファイルと置き換え）。
   - 設定のような「外で変わっていたら上書きしない」は持たない。窓を 2 つ開いていたら、後から閉じたほうの一覧が残る。
   - 版が違う・壊れている・大きすぎる・パスが絶対でないものは、全体を読まなかったことにする（一部だけ戻さない）。
3. **書く時（窓が閉じるとき）**: 意図 `EndSession { SessionEnd reason }`（`SessionEnd` は `window_closed` / `last_tab_closed` の閉じた enum）。controller は状態から `Session` を作って `SessionPort::write` へ渡す。
   - `window_closed`（窓を閉じる・OS の終了）: 開いているタブのうちパスのあるものを全部。
   - `last_tab_closed`（最後の 1 つのタブを使う人が閉じて窓が閉じる・D22）: 空の一覧（使う人はそのタブを閉じた）。
   - ui は、窓を壊す直前の 1 か所（`close_window` と最後の 1 本の `close_tab` が通る 1 本の関数）と `WM_ENDSESSION`（終了が確定したとき）で送る。描画の失敗（`abandon`）とデストラクタでは送らない（前の一覧が残る）。
   - 書けなかったときは何も出さない（窓は閉じていく途中）。
   - 普通の編集・切り替え・タブの開閉では書かない。強制終了のときは、前に窓を閉じたときの一覧が残る。
4. **まだ読んでいない文書（application）**: `UnloadedDocument { core::FilePath path; core::TextPosition caret; core::LineNumber first_visible; DocumentView view; }`。脇に置く列 `parked_` の要素を「読み込み済みの束か、まだ読んでいない文書か」の閉じた和型 `ParkedTab`（`std::variant<std::shared_ptr<const DocumentState>, std::shared_ptr<const UnloadedDocument>>`）にする。1 打鍵で写すのは今までどおり参照の列だけ。
   - 題名は `tab_title_for(path, saved)`。まだ読んでいない文書に未保存の印は付かない。窓を閉じるときの確認の対象にもならない。
   - 一覧（ADR 0057）と帯は、読み込み済みかどうかを区別せずに描く。
5. **見るときに読む（controller の切り替えの 1 か所）**: タブが切り替わる道（クリック・`gt` `gT`・Ex・一覧・Ctrl+Tab の歩き・閉じた後の隣・同じファイルを開く）はどれも controller の同じ 1 本を通る。行き先がまだ読んでいない文書なら、切り替える前に `FilePort` で読み、束にして列の同じ位置へ置き換えてから、今までどおり切り替える。
   - カーソルと画面の位置は、読んだ本文の範囲の中へ寄せる（行が無ければ最後の行・桁が行の終わりを越えたら行の終わり・文字の途中なら文字の先頭）。Vim モードなら今の切り替えと同じくキャレットを寄せる。
   - 読めなかったとき（D26・D27）: そのタブを帯から外し（使った順からも）、`command_message` に 1 行知らせる（ファイルの名前と理由）。切り替えは起きず、今のタブのまま。
   - 閉じた後の隣が読めなかったときは、それも外して次の隣を試す。1 本も残らなければ空の無題を 1 本置く（窓は閉じない）。
   - **形（#253）**: 読むのは controller の 1 本 `reach_tab(position)`（読めたら束に置き換えて真・読めなければ外して知らせて偽）。タブが切り替わる入口（`SwitchTab`・`WalkRecentTab`・`CloseTab` の後の隣・同じファイルを開く）は、状態を動かす前にこれを呼ぶ。状態（`EditorState`）はファイルに触れない: まだ読んでいない文書へは切り替えられず、指されたら範囲の外と同じく何も変えない。置き換えは `with_loaded(position, 束)`、外すのは `with_dropped(position)`。
   - **知らせの文言**: 1 件は「開けませんでした: <ファイルの名前>」、同じ意図の中で 2 件以上なら「開けませんでした: <最後のファイルの名前>（ほか N 件）」。理由は書かない（1 行に収める）。ファイルを開く失敗のダイアログ（ui）と同じく日本語。
   - Vim の `gt` `gT` の行き先が読めなかったときは、タブは動かず 1 行知らせる。engine は本数と位置だけを借用しているので、この失敗では再生を打ち切らない（残る穴）。
6. **戻す時（起動）**: ファイルの引数が 1 つも無いとき（D24）、controller は `SessionPort::read` の一覧を、全部「まだ読んでいない文書」として帯に並べ、`active` のタブを決定 5 の道で読む。読めなければ外して 1 行知らせ、右隣（無ければ左隣）を試す。1 本も残らなければ空の無題が 1 本。使った順は `recency` の順位から作る。
   - ファイルの引数があるときは、今までどおり引数だけを開く（ADR 0056 の決定 13）。一覧は読まない。窓を閉じるときには、そのとき開いているタブで一覧を書き換える。
   - 一覧そのものが読めなかったとき（壊れている・版が違う）は、空の無題 1 本で始めて 1 行知らせる。一覧が無い（初めての起動）ときは何も知らせない。
   - `--measure` の起動を特別に扱わない（計ったものと使う人の起動を同じにする）。
   - **手順（#253）**: 空の無題 1 本の状態に、一覧のタブを帯の順で「まだ読んでいない文書」として右へ並べ（`with_restored`）、`active` のタブを `reach_tab` で読んで切り替え、最初の無題を外す。使った順は、その後で `recency` の順位から作る。読めたタブが 1 本も無ければ、無題がそのまま残る。
7. **計測と検査の道具**: 道具は自分の profile を持つので、exe を起動する 1 本の関数（`eng/window_driver.py` の `start`）が、起動の前に profile の `session.v1` を消す（既定）。復元そのものを確かめる検査だけが、消さない指定で起動する。これで今の 6 本のベンチと今の検査は、復元が入っても同じものを測る。
   - 復元したときの起動の重さ（タブ 20 本の一覧で起動）は、ベンチに足すまで設計席が merge の前に手で測る（QLT-014）。
8. **縦切り（2 本の Issue・どちらの後も main は動く状態）**:
   1. **覚える（#252）**: 決定 1〜3 と 7。窓を閉じると `session.v1` が書かれる。まだ読まないので、使う人から見える動きは変わらない。
   2. **戻す（#253）**: 決定 4〜6 と、実機の検査（起動 → ファイルを 3 つ開く → 閉じる → 引数なしで起動 → タブが 3 本・見ていたタブがアクティブ・カーソルの行 → ファイルを 1 つ消して起動 → 切り替えたときに 1 行の知らせ）。
   - 縦切りを切るときの条件（ADR 0056 の節）: 1 本目の後の main に、書いたものを確認なしで失う道は無い（書くだけで読まない）。
9. **範囲の外（後続）**: 窓の位置と大きさ・未保存の本文の退避（D9 が決めている: 戻さない）・複数の窓・外で書き換えられたファイルの検知・一覧を切る設定（`:set norestore`）・復元したときの起動のベンチ・クラッシュしたときの一覧（定期的な書き出し）・読み込んでいないタブを裏で先に読むこと。

## 強制

- 契約（#252 の分）: **active**。`Session` の作り方（無題を入れない・帯の順・active の位置・使った順）と `EndSession` の 2 つの理由は `tests/unit/SessionTests.cpp` の `verify_untitled_and_single` `verify_mixed_band` `verify_positions_and_unsaved` `verify_after_close` `verify_last_tab_closed` `verify_no_write_on_ordinary_intents` `verify_write_failure_changes_nothing` `verify_walk_settles_before_writing`（scope `nib_tests --session` と既定実行・CTest `nib_unit`）。adapter の往復と壊れた入力の拒否は `tests/adapters/SessionAdapterTests.cpp` の `verify_round_trips` `verify_line_ends` `verify_rejected_headers` `verify_rejected_tabs` `verify_limits` `verify_adapter_round_trip` `verify_adapter_failures`（CTest `nib_sessions`）。
- 契約（#253 の分）: **planned**。まだ読んでいない文書への切り替え（読む・位置を寄せる・読めないときに外して知らせる）・閉じた後の隣・起動の 3 つの枝（引数あり・一覧あり・一覧なし）。
- 閉じた和型の写し漏れ（#252 の分）: **active**。`EditorIntent`（`EndSession`）は `EditorController::apply` の `std::visit`、`SessionEnd` は `ended_session` の `default` の無い `switch`（CPP-002）。`SessionFailure` は今は写す `switch` が無く、6 つの値を adapter が返すことを `nib_sessions` の契約が確かめる（写す所ができたら `default` の無い `switch` で書く）。
- 閉じた和型の写し漏れ（#253 の分）: `ParkedTab` は **planned**（実装で active・`std::visit` の網羅性）。
- core と application が OS とファイルに触れないこと: **active**（既存の `eng/symbols.py`。ファイルに触れるのは adapters の `FilePort` と `SessionPort` の実装だけ）。
- 起動の重さがタブの数に依らないこと: **planned**（決定 7。ベンチに足すまでは設計席が手で測る）。
- 実機の確認: **planned**（#253 で `eng/verify-window.py` に節を足す。機械の必須 check ではない）。
- fixture: **不能**（oracle の対象ではない）。既存の fixture は不変（`eng/protected-diff.py`）。

## 結果

得られるもの: 何も指定せずに起動すると、前に開いていたファイルがタブで戻り、見ていたタブの見ていた行が出る。起動の重さはタブの数に依らない。ファイルを 1 つ指定して開くときは、前のタブが付いてこない。
失うもの・残る穴: 脇に置く列の要素が 2 つの形になる（切り替えの 1 か所だけが両方を知る）。無くなったファイルに気づくのは、そのタブを見るとき。強制終了のときは、その回に開いたタブは一覧に入らない。ファイルを指定して起動して閉じると、前の一覧は書き換わる。窓を 2 つ開いていたら、後から閉じたほうの一覧だけが残る。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 起動のときに全部のファイルを読む | 施主が「見るときに読む」と決めた（D27）。起動の重さがタブの数と大きさで延びる |
| `settings.v1` に欄を足す | 設定は 4 つのキーの版つきの形で未知のキーを拒否する。書く時機（変更のたび）と競合の扱い（外で変わっていたら上書きしない）が一覧と違う。版を上げると古い設定が読めなくなる |
| タブの開閉のたびに一覧を書く | カーソルの位置は打鍵のたびに変わる。開閉のたびに書くとディスクへの書き込みが増え、1 意図の重さが変わる。強制終了への備えは後続（定期的な書き出し） |
| `--measure` の起動では一覧を読まない・書かない | 計るための起動と使う人の起動が別の道になる。道具が自分の profile の一覧を消すほうが、製品に計測のための枝を作らない |
| 起動のときにファイルがあるかだけ確かめる | ファイルごとに OS へ問い合わせる（ネットワークのドライブでは遅い）。読むときに分かる |
| まだ読んでいない文書を、空の本文の束として置く | 空の本文と「まだ読んでいない」の区別が付かず、保存すると中身を失う道ができる。型で分ける |
