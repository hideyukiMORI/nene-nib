# ADR 0062 — 同じフォルダは裏のワーカーが読み、面は絞り込みの結果を持って見えている行だけを載せる

- 状態: 受理（設計席 2026-10-02・Issue #270 / #271 / #272・**施主決定 D33・D34・D35**。D35 は 2026-10-03 の追記）
- 日付: 2026-10-02
- Issue: #270（面の重さ）・#271（ワーカーとフォルダの列挙）・#272（同じフォルダを面に出す）
- 影響する規則: FR-006 / ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-010 / CPP-002 / CPP-004 / CPP-011 / CPP-013 / CNF-009 / QLT-001 / QLT-012 / QLT-014
- 前提（仕様の決定）: **D5・FR-006「開いているタブ・ブックマーク・履歴・現在ファイルのフォルダを統合したファジー検索。行頭の記号で絞り込み（`/` 同じフォルダ）」**・D28（作る順の 2 番目が同じフォルダ）・D29（記号は `/`）・D12（採用した画: 行は名前・場所・補足「同じフォルダ」）・D1（今の Windows 11 で考えられる限りの速さ）・D26 / D30（無くなったファイルは 1 行知らせる）。本 ADR はこれらを変えない。
- 前提（ADR）: [ADR 0004](0004-ui-thread-plus-one-worker.md)（UI スレッド＋固定のワーカー 1 本・やり取りはメッセージだけ。動機に「Ctrl+P のファイル走査」。本 ADR が最初の実装で、完了の運び方を 1 点だけ変える）・[ADR 0060](0060-ctrl-p-lists-files-and-marks-select-the-source.md)（決定 10「同じフォルダの Issue で、列挙の上限と候補の結果を持つ形を決める」を本 ADR が決める）・[ADR 0061](0061-the-palette-takes-ime-input-and-opens-with-it-off.md)

## 文脈

Ctrl+P の 3 本目の出どころ「同じフォルダ」は、今見ているファイルと同じフォルダにあるファイルを一覧に出す。仕様に無い 2 点を、設計席が選択肢と良い点・悪い点の表で説明し、施主が決めた（2026-10-01。本 ADR の commit で仕様に D33・D34 として入れる）。

| 決定 | 内容 |
| --- | --- |
| D33 | 同じフォルダの一覧には、よく知られた「テキストでない」拡張子（画像・実行ファイル・圧縮・PDF など）だけを出さない。知らない拡張子と拡張子の無いファイルは出す |
| D34 | フォルダは裏で読む。Ctrl+P は必ずすぐ開き、読めた分から一覧に足す（「速さが売りのエディタだから」。設計席のおすすめは「開く瞬間に読む」だった） |

設計席が決めて施主に伝えたこと（異論なし）: 出すのは同じフォルダのファイルだけ（下のフォルダの中は見ない・フォルダの行は出さない）・隠しファイルとシステムファイルは出さない・何も打たないときは一覧の最後・開いているタブや履歴と同じファイルは 1 回だけ・無題のタブでは候補なし・件数の上限は実測して決め、超えたら 1 行知らせる・`/` は記号になる・並びは OS が返す順（読めた分から足すので、全体を名前順に並べ直さない）・後から行が足されても、選んでいる行は同じ候補のまま。

現物の調査（`out/probes/probe-samefolder-2026-10-01.md`・`probe-samefolder-ui-2026-10-01.md`）で分かったこと:

- 候補の列は面を開くときに 1 回作って `shared_ptr` で持つが、**絞り込みの結果は持たない**。1 つの入力で 2〜5 回作り直し（文字 2・矢印 3・行のクリック 5）、そのたびに当たった候補の全件を写して frame に載せる。描画は見えている行（最大 8）しか読まない。
- 選択は絞り込みの結果の中の位置で、候補に同一性の欄は無い。候補の列を後から伸ばす口は無い。
- ワーカー（ADR 0004）は実装が無い。スレッド・自前の窓メッセージ・タイマーは src に 0 件。並行性の検査（symbols の concurrency 分類・CNF-009）は active で、`src/adapters/win32` だけがスレッドと同期原始を書ける。application は `PostMessage` のシンボルも持てない。ui は adapters を include できない。
- フォルダの列挙の口は `FilePort` に無い。前例はテーマの `FindFirstFileW` の 1 か所（同期・128 件・隠し属性は見ない）。パスからフォルダを `FilePath` で返す関数と、拡張子を取る関数は core に無い。
- `send()` の中の `MessageBoxW` やファイルダイアログは入れ子のメッセージループを回す。その間に post されたメッセージは窓へ届く。
- 一覧から開いて `not_found` のとき、履歴に無いパスでも履歴の読み書きが走る（`forget`）。

## 決定

**フォルダを読むのは `src/adapters/win32` のワーカー 1 本で、application は型のある port に「頼む」と「受け取る」だけを持つ。面の候補の列は後ろへ伸び、選択は同じ候補のまま。絞り込みの結果は入力か列が変わったときに 1 回だけ作り、frame には見えている行だけを載せる。**

### A. 面の重さ（Issue #270・使う人から見える動きは変えない）

1. **絞り込みの結果を持つ（core）**: `CommandPalette` は、絞り込みの結果を作るのを「入力が変わったとき」と「候補の列が変わったとき」の 1 回だけにし、共有の参照で持つ。`choices()` `selected()` `moved` `selected_at` と frame は、持っている結果を読む（作り直さない）。ファイルの候補の結果は**候補の列の中の位置の列**で持ち、候補の写しを作らない。設定のコマンド（`:`）の結果は今の `palette_choices` の値を同じ形で持つ（件数が小さい）。値は不変で、遅延の計算や書き換えられる欄を持たない。
2. **frame には見えている行だけ（application）**: `CommandPaletteView` は、全件の代わりに「行の窓（最大 8 行）・窓の先頭の位置・選択の位置（全体の中）・全体の件数・案内」を持つ。8 は core の 1 つの定数で、面の配置（`palette_layout` の上限）と同じ値を読む。窓の先頭は `palette_first_visible` と同じ式を 8 行で計算する（低い窓で見える行が 8 より少ないときも、ui が描く行は必ずこの窓の中にある）。
3. **ui**: 描画は窓の行を読み、hit test と件数の表示（`M / N`）は件数と選択の位置だけを読む（今と同じ見た目・同じ動き）。
4. ADR 0060 の決定 10 の「必要なたびに作り直す」は、本決定で置き換える。

### B. ワーカーとフォルダの列挙（Issue #271・使う人から見える動きは変えない）

5. **ワーカーは 1 本（adapters）**: `src/adapters/win32` に、スレッド 1 本と要求のキュー 1 本を持つワーカーを足す。スレッドを起こす時機（合成のとき・最初の要求のとき）は #271 で決め、起動の速さのベンチが基準内であることを実測で確かめる。スレッド・ロック・条件変数・atomic はこの区画の中だけ（CPP-013・CNF-009）。後から増える裏の仕事（行の索引・ハイライト）も同じ 1 本に載せる。
6. **application が見るのは型のある port**: ADR 0004 の `WorkerPort` は、仕事の種類ごとの port として宣言する。本 ADR では `FolderPort` の 1 つ。
   - `list(FolderRequest)`: フォルダ（`core::FilePath`）と**券**（版番号・整数）を渡して、すぐ戻る。前の要求がまだ走っていれば、ワーカーは区切りでそれをやめる（新しい券だけが生きる）。
   - `collect()`: 読み終わった分（`FolderBatch` の列）を受け取る。無ければ空。UI スレッドだけが呼ぶ。
   - `FolderBatch` は 券・ファイルの列（`core::FilePath`）・進み具合（閉じた enum: `more` 続きがある / `complete` 読み終えた / `truncated` 上限で打ち切った / `failed` 読めなかった）。期待される失敗は例外にしない（ARC-010）。
7. **完了の運び方（ADR 0004 から変える 1 点）**: ADR 0004 は「`PostMessage` に所有権を載せた heap の値」としていた。本 ADR は、**窓メッセージは中身の無い「届いた」の合図だけ**にし、中身は application が `collect()` で引く。理由: ui は adapters の型を知れず、application は窓メッセージを知れない。所有権を `LPARAM` に載せると、窓が先に壊れたときの後始末と型の読み替えが ui に要る。合図だけなら、まとめて届いた分を 1 回で受け取れる。
   - 合図の出し方は、合成ルート（`src/app`）がワーカーに渡す（窓ができた後・`clipboard.bind` と同じ形）。メッセージの番号を持つのは ui の 1 か所で、adapters は番号も HWND も知らない。
   - ui は合図を受けたら、中身の無い意図 `WorkCompleted` を送る。**`send()` の途中（入れ子のメッセージループの中）で合図が届いたら、その場では送らず、`send()` が終わってから 1 回送る。**
8. **列挙の中身（adapters）**: 同じフォルダの直下のファイルだけ。フォルダ・隠し属性・システム属性は出さない。OS が返す順のまま、**1024 件ごと**に 1 つの batch にして渡す。**8192 件**で打ち切る（`truncated`）。フォルダが無い・読めないは `failed`。数字は #272 の実測で見直してよい（見直したら本 ADR に実測と一緒に書く）。
9. **終わり方**: 窓が閉じるとき、ワーカーは走っている列挙をやめて止まる（`TerminateThread` は使わない。止まっている同期の I/O は `CancelSynchronousIo` で起こす）。
10. **合成**: `EditorPorts` に `FolderPort` を足し、`src/app` が結ぶ。#271 の時点では controller は `list` も `collect` も呼ばない（呼ぶのは #272）。

### C. 同じフォルダを面に出す（Issue #272）

11. **記号と出どころ（core）**: `palette_marks` に `{'/', PaletteScope::folder, "フォルダ"}` を 1 行、`PaletteOrigin::folder`（行の右端は「同じフォルダ」）を 1 値足し、落ちた `switch` を直す。
12. **出さない拡張子の表（core・1 つ・D33）**: `constexpr` の表と純関数 1 本（名前の最後の `.` の後ろを ASCII の大文字小文字を区別せずに引く）。表に載せるのは、画像（png jpg jpeg gif bmp ico webp tif tiff heic psd）・実行と中間（exe dll sys com msi scr lib obj pdb o a so bin class pyc lnk）・圧縮（zip 7z rar gz tgz tar bz2 xz cab iso lzh）・文書（pdf doc docx xls xlsx ppt pptx odt ods odp）・音と動画（mp3 wav flac ogg m4a aac wma mp4 mov avi mkv wmv webm）・フォント（ttf otf ttc woff woff2）・データベース（db sqlite mdb accdb）。`.` の無い名前と表に無い拡張子は出す。開いているタブと履歴にはこの表を当てない（使う人が自分で開いたもの）。
13. **フォルダを取り出す関数（core・1 つ）**: `FilePath` から、列挙に渡せるフォルダを `FilePath` で返す（ルート直下は `C:\`）。表示用の `tab_folder_for` は今のまま。
14. **頼む時機（application）**: 面を開く 2 つの意図（`OpenCommandPalette`・`OpenTabList`）で、券を 1 つ進め、アクティブな文書にパスがあれば `list` を呼ぶ。無題なら呼ばない。起動・開く・切り替え・保存・打鍵では呼ばない（履歴と同じく契約で回数を数える）。
15. **受け取る（application）**: 意図 `WorkCompleted` で `collect()` を 1 回呼ぶ。券が今の券と同じで、面が開いている batch だけを使う（ほかは捨てる）。ファイルの列から、出さない拡張子・開いているタブ・履歴と同じファイルを除き、候補（名前・パス・場所・出どころ `folder`）にして、面の候補の列の**後ろへ足す**。
    - 重なりの比べ方: 先に、開いているタブと履歴のうち同じフォルダにあるものだけを選び、その少数とだけ `same_file` で比べる（全部の組を比べない）。
16. **列が伸びたときの選択（core）**: `CommandPalette` に、候補の列を後ろへ伸ばす 1 本を足す。伸ばした後の選択は、**伸ばす前に選んでいた候補と同じ候補**（列の中の位置で覚える。列は後ろへ伸びるだけなので位置は変わらない）。当たりが 1 件も無かった所へ足されたときは先頭。入力が変わったときに選択が先頭へ戻る規則は今のまま。
17. **打ち切りと失敗の見せ方（D35）**: `truncated` を受けたら、面の下の案内の所に「同じフォルダは N 件まで。残りは一覧に出ません」と 1 行出す。N は今の券で受け取った、拡張子と重複を除く前の件数（既定の上限 8192）。打ち切りは今の面の状態として持ち、入力・選択・絞り込みの後も面を開いている間は出し続ける。入力上限など別の知らせが出る意図ではそちらを優先し、次の意図で打ち切りの案内へ戻る。面を閉じると消え、開き直すと新しい券の結果が来るまで出ない。`failed` は何も出さない（候補が増えないだけ）。読み込み中の印は出さない（後続）。
18. **開く道**: 同じフォルダの候補も `open_listed` を通る（失敗は 1 行の知らせ）。`open_listed` は出どころを受け取り、履歴から外す（`forget`）のは履歴の候補のときだけにする。
19. **速さ**: 面の中の 1 打鍵を測るベンチ `key-to-frame-palette-5000` を 1 本足す（同じフォルダに 5000 個のファイルがある文書で面を開き、候補が出そろってから 1 文字打って次の frame まで）。基準値は実機と CI で測って `eng/perf-reference.json` に足す（QLT-014・ADR 0016）。`eng/measure-speed.py --bench <名前>` で計測・比較・採用の対象を選び、指定しない場合は従来どおり全本とする。共有の試行で複数の値が出る場合は必要な試行だけを動かし、記録するのは選んだ値だけ。過去の記録に新しいベンチが無いことは欠測ではなく未実行として扱う。一方、実行した試行が観測できないときは、基準値の有無によらず計測不能とする。
    - CI は通常の PR 必須 check で性能を測らない（QLT-012）。明示した `measure-palette-speed` ラベル、または手動起動の専用 workflow だけが新ベンチを同じ host で 3 回測り、記録を artifact に残す。各回は既定の 5 試行で、失敗したら止める。基準値を自動で採用しない。同じ指紋の 3 回の中央値の中央値を設計席が採用し、元の記録を示す（ADR 0016）。
    - 製品の測定節目には候補の完了と件数が無い。計測は列挙の待ち時間を 1.5 秒とし、暖機の `f` の後の `0` から最初の描画までを取る。待ったことだけを 5000 件の到着の証拠にせず、実機の件数確認と分けて記録する。遅いネットワークでの完了保証はしない。
20. **範囲の外（後続）**: 下のフォルダへ入る・ひとつ上へ・再帰の検索・読み込み中の印・フォルダが読めなかった知らせ・出さない拡張子を設定で変える・名前順の並べ替え・長いパス（`\\?\`）・更新日時・ワーカーに載せる 2 つ目の仕事。

## 強制

- 契約（面の重さ・#270）: **active**（#270）。`tests/unit/CommandPaletteTests.cpp` の `verify_palette_result_shared`（↑↓ と行の選択は前の結果を共有し、打つ・埋める・入力を変える編集（Backspace・←・Home）は結果を作り直すこと・タブの候補と設定のコマンドの両方で・観測の口 `shares_result_with`。`choice_at` は範囲の外で値なし・`rows(first, limit)` は結果の末尾で切れること）、`verify_palette_window_covers_rows`（見える行 1〜8・件数 0〜20・選択の全部で、ui が描く行が frame の窓の中にあること・見える行が 8 なら `palette_first_visible` と `palette_window_first` が同じこと・上限 `palette_row_limit` が 8）、`verify_palette_view_window`（タブ 12 本で面を開き選択を 0〜11 へ動かす各 frame で、`rows` は上限 8 件まで・`first` は `palette_window_first(selected)`・`selected` と `total` は結果の全体の数・`rows` の i 番目は全体の `first + i` 番目）。入口は scope `nib_tests --command-palette` と引数なしの既定の実行（CTest `nib_unit`）。候補の列が変わったときの契約は #272 の `verify_palette_extended_tail` で **active**（追加では結果を作り直し、空の追加は前の結果を共有する）。
- 契約（port を呼ばない道・#272）: **active**。`tests/unit/BackgroundWorkTests.cpp` の `verify_folder_quiet_paths` が、起動・開く・切り替え・保存・打鍵で `list` と `collect` が呼ばれず、面を開くと読み残しを捨てる `collect` が 1 回と `list` が 1 回（無題なら `collect` だけ）、`WorkCompleted` ごとに `collect` が 1 回であることを数える。scope `--background-work` 89 checks 成功。起動引数と復元の起動は `--history` の `verify_quiet_paths`（38 checks 成功）が見る。
- 契約（受け取り・#272）: **active**。同じファイルの `verify_folder_tail`・`verify_folder_stale`・`verify_folder_keeps_choice`・`verify_folder_order`・`verify_folder_commands_scope`・`verify_folder_open` が、拡張子・重複の除外、古い券・閉じた面・読み残しの破棄、追加後の選択と順、設定スコープの間に届く候補、開く道と履歴だけの削除を検査する。`verify_folder_truncated`・`verify_folder_truncated_stays`・`verify_folder_truncated_reopened` が、D35 の文言・入力後の持続・別の知らせの優先・閉じると消え開き直すとリセットされることを検査する。券・読み残し・履歴の削除条件を外した変異で 11 件の失敗を確認した。
- 契約（記号・拡張子・列の追加・#272）: **active**。`tests/unit/CommandPaletteTests.cpp` の `verify_palette_folder_scope`・`verify_unlisted_table`・`verify_unlisted_names`・`verify_folder_of`・`verify_palette_extended_tail`・`verify_palette_extended_keeps_choice`・`verify_palette_extended_edges`。`/`、70 件の拡張子、ルートと UNC、日本語、追加時の結果の作り直し・選択の同一性・空の結果・設定スコープを検査する（scope `--command-palette` 356 checks 成功）。先頭の `.` が唯一の点の名前（`.gitignore`・`.png`）は拡張子なしとして出す。
- 閉じた和型の写し漏れ: `PaletteScope::folder`・`PaletteOrigin::folder`・`FolderProgress` は **active**（`default` の無い `switch`・CPP-002）。`FolderProgress` を読むのは `EditorController::accept(const WorkCompleted &)` の 1 か所。Debug ビルドは警告 0 で成功した。
- 並行性の区画: symbols の concurrency 分類（core / application）と CNF-009 は **active**（今のまま）。ワーカーを足しても core と application にスレッドと `PostMessage` のシンボルが出ないことを、#271 の検証で確かめる。#271 でワーカーを足した後も、core と application の symbols は違反 0（gate-proofs の #271 の節）。
- adapters の試験（#271）: **active**（#271）。`tests/adapters/FolderAdapterTests.cpp`（target `nib_folder_tests`・CTest `nib_folders`・本物のフォルダと本物のワーカー）の `verify_batches`（1 batch 4 件で 8 件 → 4 `more`・4 `complete`／10 件 → 4・4・2 で最後だけ `complete`／空のフォルダは空の `complete` 1 つ・batch が券を運ぶ・OS が返す順）、`verify_limits`（上限を超えるフォルダは `truncated`・ちょうど上限は `complete`・上限が batch の区切りに当たっても空の batch を出さない・既定値が 1024 と 8192）、`verify_skipped`（下のフォルダ・隠し・システムを出さない・日本語の名前・区切りで終わるフォルダで区切りが二重にならない）、`verify_failed`（無いフォルダとファイルを指したパスは空の `failed` 1 つ）、`verify_new_ticket_stops_previous`（1 つ目の batch の合図の中で次の `list` を呼び、前の券は 1 つ目の batch だけで最後が無く、新しい券は最後まで読まれて `complete`）、`verify_stops_during_listing`（合図の中で列挙を止めたまま adapter を壊すと、以後は合図しない）、`verify_worker_stops_first`（列挙の途中でワーカーを先に壊しても戻り、読めた分は `collect` できる）、`verify_never_started`（要求の無いワーカーはスレッドを持たない・要求の前の `collect` は空）。待つのはすべて `std::counting_semaphore` で、`Sleep`・時計・タイムアウトの待ちは無い。止まらないときは CTest の制限時間（`CMakeLists.txt` の `TIMEOUT 60`）で落ちる。
- 契約（合図は使う人の操作の状態を動かさない・#271）: **active**（#271）。scope `nib_tests --background-work`（`tests/unit/BackgroundWorkTests.cpp`・替え玉 `ScriptedFolders`）の `verify_work_keeps_idle_and_notice`（通常モードの待機・面から開けなかった知らせ・Vim の Ex の失敗の知らせで、`WorkCompleted` の前後の表示値が同じで `list` が呼ばれない（#272 から `collect` だけが 1 回増える））、`verify_work_keeps_tab_walk`（Ctrl+Tab の歩きの途中で送っても歩きが続く）、`verify_work_keeps_palette_and_composition`（面の入力と選択・面の入力行の変換・本文の変換を動かさない）、`verify_work_keeps_vim_state`（`d|w` `2|dw` `"a|x"ap` `qa|xq@a` `x|.` `x|u` `ia|b<Esc>u` の `|` の所と通常モードの打鍵の間に挟んでも、挟まずに打った結果（本文の全体と表示値）と同じ）、`verify_folder_quiet_paths`（#272 から上記の面を開く時機と合図以外では `list` も `collect` も呼ばれない）。起動引数と復元の起動は scope `--history` の `verify_quiet_paths` がフォルダの口の回数も数える。入口は引数なしの既定の実行（CTest `nib_unit`）にも入る。合図の窓メッセージは #272 の実機で受信を確認した。入れ子のメッセージループ中の `send` の再入は未確認。
- 速さ（#272）: 実機 `bc8a356f37c68491` は **active**。`key-to-frame-palette-5000` の5試行は2.526〜2.715 ms、中央値2.630 msで、この1本の基準値を採用した。各試行の測定直前の画で5000件を目視確認。対象指定・欠測・基準値なしを含む計測器の64試験が成功した。CIの新ベンチは **planned**（基準値は未採用）。詳しい記録は [gate-proofs 5-cc](../quality/gate-proofs.md#5-cc--ctrlp-の同じフォルダissue-272adr-0062)。
- 実機の確認: **planned**（設計席が施主に確かめてから回す。機械の必須 check ではない）。#270 は設計席が 1 回限りのスクリプトで前後の画を比べた（gate-proofs の #270 の節）。#271 は使う人から見える動きを変えないので画は撮っていない。速さの 6 本は測った（gate-proofs の #271 の節）。
- fixture: **不能**（oracle の対象ではない）。既存の fixture は不変（`eng/protected-diff.py`）。
- 不能: ワーカーが受け取った値だけを読むこと（意味の検査・ADR 0004 のまま）。レビュー事項。

## 結果

得られるもの: Ctrl+P は、どこのフォルダのファイルを見ていても同じ速さで開く。同じフォルダのファイルを名前の数文字で開ける。候補が数千件でも、1 打鍵で作る絞り込みは 1 回で、frame に載るのは 8 行。裏の仕事を載せる場所（ワーカー 1 本と型のある port）ができ、次の仕事（行の索引・ハイライト）は port を 1 つ足す形になる。

失うもの・残る穴: 面を開いた直後の 1 枚目には同じフォルダの候補が無く、少し後で足される（普通のドライブでは次の描画まで）。並びは OS が返す順で、NTFS では名前順だがネットワークや FAT では名前順と限らない。8192 件を超えるフォルダでは、後ろのファイルが一覧に出ない。読み込み中と、読めなかったことは見えない。表に無い拡張子のバイナリは一覧に出て、選ぶと「開けませんでした」になるか、文字化けした本文で開く（今の開く道は NUL を含むファイルを拒まない）。ワーカーの中の動き（受け取った値だけを読むこと）は機械で守れない。

- #270 の実装: `CommandPalette` の public の `choices()`（全件の写し）は消し、読む口は `count()`・`choice_at(index)`・`rows(first, limit)` にした。結果を作るのは private の `filtered` の 1 本、選択だけを動かす道は `reselected` の 1 本で前の結果を共有する。絞り込みと順は `listed_positions`（候補の列の中の位置を返す）の 1 本で、`listed_choices` は無くなった。
- #270 の実装: 行数の上限は core の `palette_row_limit`（8）、frame の窓の先頭は `palette_window_first(selected)` で、`palette_first_visible` と同じ 1 つの式を呼ぶ。`CommandPaletteView` は `rows`・`first`・`selected`・`total`・`hint`。
- #270 の実機: タブ 12 本の面の画 15 枚（見える行が 3 と 8 の窓・送り・折り返し・絞り込み・設定のコマンド・行のクリック）は、前後で 1 画素も違わなかった。
- #271 の実装: ワーカーは `Win32Worker`（スレッド 1 本とキュー 1 本）。スレッドは最初の仕事を受けたときに起こす（合成のときには起こさない）。壊すときは、止める合図の後、スレッドが終わるまで「`CancelSynchronousIo` → 10 ms 待つ」を繰り返す。
- #271 の実装: フォルダの側は `Win32FolderAdapter`（`FolderPort` の実装）・`FolderShelf`（読めた分のたまり・世代・合図を 1 つのロックで守る）・`FolderListing`（1 つの要求の列挙）。世代は `list` のたびに進める adapter の中の数で、application の券とは別。合図は、たまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ。1 つの batch の件数と上限は adapter を作るときに渡せる値で、既定値は `folder_batch_files`（1024）と `folder_file_limit`（8192）。
- #271 の実装: 合図の窓メッセージの番号は `EditorWindow.cpp` の `work_message`（`WM_APP + 1`）の 1 か所。合成ルートは `EditorWindow::work_signal()` の値を adapter の `bind` に渡す。`send` は深さを数え、途中で届いた合図は、いちばん外の `send` の終わりで 1 回だけ `WorkCompleted` にして送る。
- #271 の実装: 意図 `WorkCompleted` は使う人の操作ではないので、controller の入口は Ctrl+Tab の歩きを続け、知らせを消さない。#271 では何もしなかった。#272 から `collect()` を 1 回呼び、今の券の結果を面へ写す。

## 却下した選択肢

| 選択肢 | 却下の理由 |
| --- | --- |
| 面を開く瞬間に、同期でフォルダを読む | 施主が裏で読むと決めた（D34）。遅い場所のファイルを見ているとき、Ctrl+P を開く操作が止まる |
| 完了を `PostMessage` に載せた heap の値で運ぶ（ADR 0004 の元の形） | ui が中身の型を読み替えて所有権を受ける。ui は adapters の型を知れない。窓が壊れた後の値が残る |
| 仕事を選ばない汎用の `WorkerPort`（関数を渡す） | application がワーカーに渡すものが値でなくなり、「受け取った値だけを読む」が型で言えない。port は仕事の種類ごとに型を持つ |
| 読み終わってから 1 回で渡し、名前順に並べる | 遅いフォルダでは最後まで何も出ない（D34 の「読めた分から」に反する） |
| 足すたびに全体を名前順に並べ直す | 候補の位置が動き、選択の同一性を位置で持てない。並べ直しの重さが件数に比例して毎回かかる |
| 列が伸びたら選択を先頭に戻す | 見えている選択と Enter で開くものが、読み込みの時機で食い違う |
| テキストと分かる拡張子だけを出す | 知らない拡張子のテキスト（設定・ログ）が出ない（D33） |
| 中身を読んでバイナリかを判定する | フォルダの全ファイルを開くことになる。列挙は名前と属性だけで済ませる |
| frame に全件を載せたまま、絞り込みの結果だけ持つ | 1 つの入力で frame を 2〜5 回作り、そのたびに数千件を写す |
