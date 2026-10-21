// 翻訳単位をまたいで呼ぶ検証の宣言（ADR 0042 決定 2）。scope の入口・契約・既定実行の入口と、
// 2 つの scope が共有する検証だけを置く。表は NibTests.cpp の 1 か所。
#pragma once

#include "../vim/VimFixture.hpp"

#include <string_view>

namespace nenenib::tests
{
void verify_body_layout_scope();
void verify_wrapped_view_scope();
void verify_vim_wrapped_scope();
void verify_vim_wrapped_contracts();
// UTF 検証と表示値/UTF-16の直接の境界（ADR 0086）。
void verify_utf8_scope();
// ADR 0122: 分割位置、EOF、未完列の所有と進捗の分岐。--utf8と既定実行が同じ契約を呼ぶ。
void verify_utf8_stream_contracts();

// ADR 0080: 判定済み所有値と、読み込み・改行索引の既存契約。
void verify_detected_text_contracts();
void verify_text_buffer_read_contracts();
void verify_document_read_contracts();

// Utf16Tests.cpp（長さの測定と既存変換の直接境界、ADR 0083）。
void verify_utf16_scope();
void verify_utf16_conversions();
// ADR 0085: 履歴の所有・単位の合成・分岐・保存境界。
void verify_edit_history_scope();

// ADR 0089: 記録した鍵列の所有・境界・snapshot 分岐。
void verify_vim_recorded_keys_scope();

// DisplayLineTests.cpp
void verify_display_line();
void verify_display_line_views();
void verify_display_line_scope();

// EditorDeliveryTests.cpp
void verify_delivery_contracts();

// TabsTests.cpp
void verify_tabs_contracts();
void verify_tabs_scope();
// SplitTests.cpp
void verify_split_contracts();
void verify_split_scope();

// SessionTests.cpp
void verify_session_contracts();
void verify_session_scope();

// BookmarkTests.cpp
void verify_bookmarks_contracts();
void verify_bookmarks_scope();

// HistoryTests.cpp
void verify_history_contracts();
void verify_history_scope();

// DisplayTests.cpp（ADR 0140 の決定 2・3・5）
void verify_display_contracts();
void verify_display_scope();

// BackgroundWorkTests.cpp
void verify_background_work_contracts();
void verify_background_work_scope();

// AutosaveTests.cpp
void verify_autosave_contracts();
void verify_autosave_scope();

// OperationsTests.cpp
void verify_operations_scope();

// GuideTests.cpp
void verify_guide_scope();

// LexerTests.cpp（#815・段 2 の ADR 0156 の決定 1〜6・lex_line）
void verify_lexer_scope();

// SyntaxTests.cpp（#816・ADR 0156 の決定 7〜10・14・見えている行の色の区間）
void verify_syntax_scope();

// MarkdownTests.cpp（ADR 0151 の決定 5〜8・parse_markdown）
void verify_markdown_scope();

// HtmlToMarkdownTests.cpp（ADR 0157 の決定 5〜7・markdown_from_html）
void verify_html_markdown_scope();

// RichPasteTests.cpp（ADR 0157・貼り方は文書の言語とクリップボードの HTML で決まる）
void verify_rich_paste_contracts();

// LanguageTests.cpp（ADR 0154・文書の言語）
void verify_language_scope();

// PreviewTests.cpp（ADR 0155・Markdown のプレビューの application の側）
void verify_preview_contracts();
void verify_preview_scope();

// ExitPlanTests.cpp（ADR 0133）
void verify_exit_plan_scope();

// VimDotTests.cpp
void verify_vim_dot_contracts();
void verify_vim_dot_scope();

// VimMacroTests.cpp
void verify_vim_register_snapshot_contracts();
void verify_vim_macro_contracts();
void verify_vim_macro_scope();

// VimWindowTests.cpp（ADR 0148 の決定 7）
void verify_vim_window_contracts();
void verify_vim_window_scope();

// VimClipboardTests.cpp
void verify_vim_clipboard_contracts();
void verify_vim_clipboard_scope();

// VimSearchSnapshotTests.cpp
void verify_vim_search_snapshot_contracts();

// VimSearchTests.cpp
void verify_vim_search_contracts();
void verify_vim_search_scope();

// VimSearchHighlightTests.cpp
void verify_vim_search_highlight_contracts();
void verify_vim_search_highlight_scope();

// VimSearchIncrementalTests.cpp
void verify_vim_search_incremental_contracts();
void verify_vim_search_incremental_scope();

// VimTextObjectTests.cpp
void verify_vim_text_object_contracts();
void verify_vim_text_object_scope();

// VimMarkTests.cpp
void verify_vim_mark_contracts();
void verify_vim_mark_scope();

// VimReplaceTests.cpp
void verify_vim_replace_contracts();
void verify_vim_replace_scope();

// VimVisualYankTests.cpp
void verify_vim_visual_yank_scope();

// VimVisualWantedTests.cpp
void verify_vim_visual_wanted_contracts();
void verify_vim_visual_wanted_scope();

// VimOpenLineTests.cpp
void verify_open_line_round_trip(std::string_view initial, std::string_view keys,
                                 std::string_view expected);
void verify_vim_open_line_movement();
void verify_vim_open_line_fixtures();
void verify_vim_open_line_capacity();
void verify_vim_open_line_external_input();
void verify_vim_open_line_external_edit();
void verify_vim_interrupt_discards();
void verify_vim_open_line_contracts();
void verify_vim_open_line_scope();

// VimOpenLineExternalTests.cpp
void verify_vim_open_line_external_scope();

// VimOpenLineRecoveryTests.cpp
void verify_vim_open_line_recovery();

// VimCharacterSearchTests.cpp
void verify_vim_character_search_waiting();
void verify_vim_character_search_contracts();
void verify_vim_character_search_scope();

// VimLineJumpTests.cpp
void verify_vim_line_jump_waiting();
void verify_vim_line_jump_continuations();
void verify_vim_line_jump_operator_undo();
void verify_vim_line_jump_contracts();
void verify_vim_line_jump_scope();

// VimLineJumpRecoveryTests.cpp
void verify_vim_line_jump_recovery();

// VimCharacterBoundaryTests.cpp
void verify_vim_character_contracts();
void verify_vim_character_scope();

// VimVirtualColumnTests.cpp
void verify_vim_virtual_column_contracts();
void verify_vim_virtual_column_scope();

// UserThemeSelectionTests.cpp
void verify_user_theme_selection();

// UserThemeValuesTests.cpp
void verify_user_theme_values();

// CommandPaletteTests.cpp
void verify_command_palette();

// ExSettingsTests.cpp
void verify_ex_settings();

// ExFileTests.cpp
void verify_ex_files_contracts();
void verify_ex_files_scope();

// VimVisualBlockTests.cpp
void verify_vim_block_contracts();
void verify_vim_block_scope();

// OrdinaryCharacterTests.cpp
void verify_ordinary_character_contracts();
void verify_ordinary_character_scope();

// CoreTests.cpp
void verify_buffer_scope();
// NewlineIndexTests.cpp
void verify_newline_index_scope();

// VirtualColumnIndexTests.cpp / GridLineTests.cpp（ADR 0140・ADR 0139・#544）
void verify_virtual_column_index_scope();
void verify_grid_line_scope();
void verify_caret_movement_contracts();
void verify_display_text_accepts_ascii();
void verify_history_coalescing();
void verify_history_absorbing();
void verify_palette();
void verify_text_and_caret();
void verify_look();
void verify_status_items_scope();
void verify_theme_scope();
void verify_file_path_scope();

// ApplicationTests.cpp
void verify_frame_selection_scope();
void verify_document_save_contracts();
void verify_application_scope();
void verify_editor_state();
void verify_controller_intents();

// ExDocumentTests.cpp
void verify_ex_document_contracts();
void verify_ex_document_scope();

// ExWritePathTests.cpp
void verify_ex_write_path_contracts();

// BellTests.cpp
void verify_bell_contracts();

// VimEngineTests.cpp
void verify_vim_fixture(const VimFixture &fixture);
void verify_vim_insert_undo_unit();
void verify_vim_change_undo_unit();
void verify_vim_insert_motion_breaks_the_unit();
void verify_vim_put_line_endings();
void verify_vim_crlf();
void verify_vim_step_edges();
void verify_vim_viewport_mode_edges();
void verify_vim_insert_page_move_breaks_undo();
void verify_vim_visual_step_edges();
void verify_vim_engine();
} // namespace nenenib::tests
