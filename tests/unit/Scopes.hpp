// 翻訳単位をまたいで呼ぶ検証の宣言（ADR 0042 決定 2）。scope の入口・契約・既定実行の入口と、
// 2 つの scope が共有する検証だけを置く。表は NibTests.cpp の 1 か所。
#pragma once

#include "../vim/VimFixture.hpp"

#include <string_view>

namespace nenenib::tests
{
// DisplayLineTests.cpp
void verify_display_line();
void verify_display_line_views();
void verify_display_line_scope();

// TabsTests.cpp
void verify_tabs_contracts();
void verify_tabs_scope();

// SessionTests.cpp
void verify_session_contracts();
void verify_session_scope();

// BookmarkTests.cpp
void verify_bookmarks_contracts();
void verify_bookmarks_scope();

// HistoryTests.cpp
void verify_history_contracts();
void verify_history_scope();

// BackgroundWorkTests.cpp
void verify_background_work_contracts();
void verify_background_work_scope();

// OperationsTests.cpp
void verify_operations_scope();

// GuideTests.cpp
void verify_guide_scope();

// VimDotTests.cpp
void verify_vim_dot_contracts();
void verify_vim_dot_scope();

// VimMacroTests.cpp
void verify_vim_macro_contracts();
void verify_vim_macro_scope();

// VimClipboardTests.cpp
void verify_vim_clipboard_contracts();
void verify_vim_clipboard_scope();

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
void verify_caret_movement_contracts();
void verify_display_text_accepts_ascii();
void verify_history_coalescing();
void verify_history_absorbing();
void verify_palette();
void verify_text_and_caret();
void verify_look();

// ApplicationTests.cpp
void verify_document_save_contracts();
void verify_application_scope();
void verify_editor_state();
void verify_controller_intents();

// ExDocumentTests.cpp
void verify_ex_document_contracts();
void verify_ex_document_scope();

// ExWritePathTests.cpp
void verify_ex_write_path_contracts();

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
