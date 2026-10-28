# ADR 0158 — `EditorController.cpp` と `VimStep.cpp` を意味の単位で複数の翻訳単位に分ける（振る舞いを変えない）

- 状態: 受理（施主 2026-10-28「全部推しでいい」・施主決定 D48 の切り方。hide の了承の後に着手。merge は CI と `protected-diff` 0 と全件の再利用で）
- 日付: 2026-10-28
- Issue: #992（D48 の分割）・親 #791（巨大ファイル 段 2 の前提）・LSP の計画 §3 の順 2
- 規則: ARC-001 / ARC-002 / ARC-012、CPP-011 / CPP-012、QLT-001 / QLT-010 / QLT-012、ADR 0021（差分の検証と結果の再利用）/ 0039（席は 1 工程）

## 文脈

`src/application/EditorController.cpp` は 5,201 行・223 の member の定義、`src/core/VimStep.cpp` は 5,941 行・357 の関数で、どちらも 1 つの翻訳単位にある。外部講評 P3' を受けて施主は 2026-10-16 に D48「開いている PR の merge の後に ADR で切り方（VimStep は鍵の表の単位・EditorController は意図の種類の単位）を決めてから行う」と決めた。今日（2026-10-28）の自走で同じファイルに 6 本の PR が重なり、乗せ替えのたびに衝突を解いた。巨大ファイル 段 2（開く道の非同期化・ADR 0159）と LSP の L0 / L1 は controller を大きく書き換えるので、その前に分ける。

分けても設計は変えない。`EditorController` は 1 つの class・1 つのヘッダのままで、member の定義を置くファイルを増やすだけ。`EditorView.cpp`（`caret_in_view` `visual_view` `move_visual` `accept(PointText)` の 4 本）が同じ形の先例である。

## 決定

1. **1 class・複数の翻訳単位。** `EditorController.hpp` は変えない（宣言の順も変えない）。定義を `src/application/EditorController<Kind>.cpp` に移す。Kind は意図の種類（`EditorIntent` の和型の並び）と、それを支える私有の関数の群で決める。CPP-011（1 ファイル 1 型）は型の数の規則で、member の定義を複数の `.cpp` に置くことは禁じない（`EditorView.cpp` の先例）。

   | ファイル | 入れるもの（`EditorController.hpp` の宣言の群） |
   |---|---|
   | `EditorController.cpp` | 構築・`apply` `apply_frame` `delivery` `frame` `documents` `is_composing` と意図の `std::visit` の写し・`begin_intent` `settle_tab_walk_before` と `keeps_tab_walk` の表・`WorkCompleted` `RefreshAppearance` `AdjustFontSize`・`shown_settings` `persist_settings` `command_message` `active_document_view` `guide_context`・読む口（`text` `vim_state` `split` `tab_walking` `title_bar_input(width, dpi)`） |
   | `EditorControllerEditing.cpp` | `InsertText` `MoveCaret` `PlaceCaret` `DeleteText` `NewLine` `SelectAll` `CancelSelection` `ClipboardAction` `HistoryAction` `ScrollLines` `VisibleLines` `SelectEditMode` `ComposeText` `CommitText` `CancelComposition`・`replace` `move_caret_to` `undo_edit` `redo_edit` `clipboard_written` `forget_syntax_before` |
   | `EditorControllerVim.cpp` | `VimKeyPress` `StoreVimRegister` `SearchHop`・`deliver_vim_key` `command_key`（3 本）`step_vim` `stepped_vim` `measured_vim_step` `answered_vim_request` `load_vim_clipboard` `send_vim_clipboard`・再生（`merge_replayed_edits` `close_replayed_unit` `reopen_replayed_unit` `queue_nested_replay`）・`perform(...)` の全オーバーロード・`apply_block` `interrupt_vim_insert` `vim_boundary` `settle_vim_caret`・マーク（`undone_marks` `check_vim_marks` `adopt_vim_marks`）・`update_search_preview` `restore_search_origin` |
   | `EditorControllerDocuments.cpp` | `open_document` `OpenDocument` `open_listed` `remember(paths)` `forget` `SaveDocument` `save_document` `read_document` `leave_document` `enter_document` `reach_tab` `close_active_tab` `report_unreached` `restore_session` `restore_tabs` `EndSession` `blank_untitled` `open_tab_of`・`ask_display` `SelectDisplay` `SelectLanguage` `SelectSyntax` `answer_language` `answer_syntax` `answer_current_value`・`write_ex_document` `write_ex_path` `refuse_modified_quit` `run_document_request` |
   | `EditorControllerTabs.cpp` | `NewTab` `SwitchTab` `WalkRecentTab` `SettleRecentTab` `CloseTab` `TitleBarWidth` `ScrollTabs` `PointTitleBar` `title_bar_input()` `reveal_active_tab`・枠（`SplitPane` `SwitchPane` `ClosePane` `ScrollPane` `close_operated_pane` `adjust_other` `run_window_command`） |
   | `EditorControllerCommand.cpp` | `CommandText` `EditCommand` `SubmitCommand` `CancelCommand` `PasteCommand` `fit_command_rows`・`submit`（3 本）`submitted_command` `confirmed_command` `remember`（行 2 本と面）`evaluate_command` `apply_unsaved_ex` `run_tab_request` `reject` `run_palette_request` `jump_to_line` `command_line_view` `command_line_active` |
   | `EditorControllerPalette.cpp` | `OpenCommandPalette` `OpenTabList` `OpenOperationList` `open_palette` `request_folder` `listed_in_folder` `in_folder` `append_folder_choices` `palette_entries` `palette_tabs` `append_bookmark_choices` `ToggleBookmark` `bookmark_target` `bookmark_toggle_allowed` `ActivateCommandChoice` `ScrollPaletteList` `submit_palette` `command_palette_view` `command_palette_active` |
   | `EditorControllerAutosave.cpp` | `AutosaveTick` `autosave_now` `RestoreAutosave` `DiscardAutosave` `restored_bundle` `store_planned` `store_autosave` `collect_autosave` `notify_autosave_failure` `autosave_remnants` |
   | `EditorControllerPreview.cpp` | `TogglePreview` `PreviewTick` `request_preview` `collect_preview` `follow_preview_on_scroll` `framed_scroll` |
   | `EditorControllerView.cpp`（`EditorView.cpp` を改名） | 今の 4 本と、`follow_caret` `follow_position` `settle_follow` `drop_pending_follow` `settled_scroll` `clamp_scroll` |

   宣言がこの表に無い member が出てきたら、呼び手がいちばん多い群へ入れ、PR 本文の表に書く（ADR は追補で表を直す）。

2. **無名名前空間の補助は、呼び手の群へ移す。** 1 つの群だけが呼ぶ補助はその `.cpp` の無名名前空間へ。2 つ以上の群が呼ぶ補助と定数（`maximum_file_bytes` など）は `src/application/EditorControllerDetail.hpp`（宣言・`inline constexpr` の定数）と `EditorControllerDetail.cpp`（定義）の 1 対に置く。`namespace nenenib::application::detail` の自由関数で、型は足さない（CPP-011）。この header は controller の `.cpp` だけが include する（ARC-002 の module は同じ `application`）。

3. **`VimStep.cpp` は鍵の表の単位。** 今の節の見出し（`// ---- 範囲` `オペレータ` `p / P` `矩形 VISUAL` `r` `鍵から動作へ` `レジスタの名前` `マクロ` `"` `.` `検索` `VISUAL` `テキストオブジェクト` `記録`）に沿って分ける。

   | ファイル | 節 |
   |---|---|
   | `VimStep.cpp` | 公開の関数（`vim_step` `vim_resting_from` `vim_reads_clipboard` ほか `VimStep.hpp` の宣言）・鍵から動作へ・NORMAL の特別な鍵・組み立て中の入力を捨てる |
   | `VimStepMotions.cpp` | 着地と移動（`landing_of` `moved_by` と語・行・画面・文字検索の着地） |
   | `VimStepRanges.cpp` | 範囲（`motion_range` と行単位・文字単位の範囲の作り） |
   | `VimStepOperators.cpp` | オペレータ・大小文字と字下げと行の結合・`D` `C`・レジスタの書き手（`registers_written` の周り） |
   | `VimStepPut.cpp` | `p` `P`・レジスタの名前・`"` |
   | `VimStepBlock.cpp` | 矩形 VISUAL |
   | `VimStepReplace.cpp` | `r` |
   | `VimStepSearch.cpp` | 検索 |
   | `VimStepVisual.cpp` | VISUAL |
   | `VimStepInsert.cpp` | INSERT |
   | `VimStepTextObjects.cpp` | テキストオブジェクト |
   | `VimStepRecording.cpp` | `.`・記録・マクロの録画と再生（ADR 0030 / 0033 / 0046） |

   群をまたぐ補助は `src/core/VimStepDetail.hpp` / `.cpp`（`namespace nenenib::core::detail`）に置く。表は `VimKeyTable.cpp` のまま（ADR 0042 の決定 6）。

4. **振る舞いを変えない。移すだけ。** 関数の本文を 1 文字も変えない（include と無名名前空間の囲い・`detail` の宣言だけが差分）。`git diff --color-moved=dimmed-zebra -M` で移動以外の差分が include と宣言だけであることを PR 本文に書く。`eng/protected-diff.py --base main --head <branch>` が 0（保護対象の差分 0・scope ごとの checks 数が前後で同じ）。`nib_tests` の既定実行と全 scope・`nib_window_tests`・`nib_adapter_tests` の checks 数が分ける前と同じ。`eng/symbols.py` 0・`eng/conformance.py` 0。関数の長さと複雑さの閾値（`.clang-tidy`）は触らない（QLT-010）。

5. **順と並行の規則。** controller を先（巨大ファイル 段 2 と LSP が触る）、`VimStep.cpp` を後（fuzz の直しが落ち着いてから）。分割の PR が開いている間、同じファイルを触る PR を出さない（設計席が席の依頼を止める）。分割は 1 ファイル 1 PR・1 席（ADR 0039）。乗せ替えは分割の PR の側で行う（ほかを待たせない）。

6. **大きさの門（planned → この PR で active）。** `eng/conformance.py` に「`src/**/*.cpp` と `tests/**/*.cpp` は 3,000 行以下」の検査を足し（CNF-014）、分けた直後に全ファイルが通ることを示す。閾値はこの ADR が決める（今の最大は `VimStep.cpp` の 5,941・分けた後の最大は 1,500 行前後の見込み）。門を足すのは弱めるのと逆で QLT-010 に反しない。超えたファイルを直す道は「この ADR の表に群を 1 つ足す」。

## 強制

- 振る舞いの不変: **active**（`protected-diff` 0・全件の checks 数の一致・CI）。
- ファイルの大きさ（CNF-014・3,000 行）: **active**（PR #1053・2 本目。`eng/conformance.py` の `translation_unit_size_checks`）。
- 群の表と実際のファイルの一致: レビュー事項（PR 本文の表）。

## 結果

得られるもの: 席が並行して触れる単位が 10 個前後になり、乗せ替えの衝突が意味の単位に閉じる。巨大ファイル 段 2 は `EditorControllerDocuments.cpp` と新しい port だけを触る。LSP の L1 は `EditorControllerLanguageServer.cpp` を足すだけになる。
失うもの・残る穴: ファイルを探す手間（`grep -n "EditorController::<名前>" src/application/EditorController*.cpp` で足りる）。`detail` の header が「何でも置く箱」になる恐れ（置けるのは 2 群以上が呼ぶ自由関数と定数だけ・型は置かない）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| class を分ける（`EditorController` を `DocumentController` `VimController` … に） | 状態（`EditorState`・port・券）を共有する設計の変更で、D48 の「分割」を越える。意図の写しの 1 本（ARC-004）を保つには 1 class が自然 |
| 行数で機械的に切る | 意味の無い境目は乗せ替えの衝突を減らさない |
| 分けないまま 段 2 へ | 段 2 の PR が 5,000 行のファイルの中で 300 行を動かし、並行する PR と衝突し続ける（今日の 6 本で実測） |
