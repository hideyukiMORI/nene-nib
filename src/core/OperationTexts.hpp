#pragma once

#include "EditorOperation.hpp"
#include "OperationText.hpp"
#include "UiMessage.hpp"

#include <array>

namespace nenenib::core
{
// 操作の名前・説明・短い名前の表（ADR 0078 の決定 5・ADR 0123 の追補 2026-11-13 の決定 6）。
// 文は文の表の名前で持ち、画面の言語の文は ui_text が引く。EditorOperation の順に全値を 1 回ずつ
// 持つ（OperationTexts.cpp の static_assert が守る）。鍵の表示名はここに書かず key_chord_label で
// 作る。読み（ひらがな）は画面に出ない検索の鍵なので operation_readings（OperationReadings.hpp）。
inline constexpr std::array<OperationText, 38> operation_texts{{
    {EditorOperation::open_file, UiMessage::operation_open_file_name,
     UiMessage::operation_open_file_description},
    {EditorOperation::save, UiMessage::operation_save_name, UiMessage::operation_save_description},
    {EditorOperation::save_as, UiMessage::operation_save_as_name,
     UiMessage::operation_save_as_description},
    {EditorOperation::new_tab, UiMessage::operation_new_tab_name,
     UiMessage::operation_new_tab_description},
    {EditorOperation::close_tab, UiMessage::operation_close_tab_name,
     UiMessage::operation_close_tab_description},
    {EditorOperation::recent_tab, UiMessage::operation_recent_tab_name,
     UiMessage::operation_recent_tab_description},
    {EditorOperation::recent_tab_back, UiMessage::operation_recent_tab_back_name,
     UiMessage::operation_recent_tab_back_description},
    {EditorOperation::list_files, UiMessage::operation_list_files_name,
     UiMessage::operation_list_files_description, UiMessage::operation_list_files_short_name},
    {EditorOperation::list_operations, UiMessage::operation_list_operations_name,
     UiMessage::operation_list_operations_description,
     UiMessage::operation_list_operations_short_name},
    {EditorOperation::toggle_bookmark, UiMessage::operation_toggle_bookmark_name,
     UiMessage::operation_toggle_bookmark_description},
    {EditorOperation::undo, UiMessage::operation_undo_name, UiMessage::operation_undo_description},
    {EditorOperation::redo, UiMessage::operation_redo_name, UiMessage::operation_redo_description},
    {EditorOperation::font_larger, UiMessage::operation_font_larger_name,
     UiMessage::operation_font_larger_description},
    {EditorOperation::font_smaller, UiMessage::operation_font_smaller_name,
     UiMessage::operation_font_smaller_description},
    {EditorOperation::font_reset, UiMessage::operation_font_reset_name,
     UiMessage::operation_font_reset_description},
    {EditorOperation::toggle_mode, UiMessage::operation_toggle_mode_name,
     UiMessage::operation_toggle_mode_description},
    {EditorOperation::split_vertical, UiMessage::operation_split_vertical_name,
     UiMessage::operation_split_vertical_description},
    {EditorOperation::split_horizontal, UiMessage::operation_split_horizontal_name,
     UiMessage::operation_split_horizontal_description},
    {EditorOperation::next_pane, UiMessage::operation_next_pane_name,
     UiMessage::operation_next_pane_description},
    {EditorOperation::close_pane, UiMessage::operation_close_pane_name,
     UiMessage::operation_close_pane_description},
    {EditorOperation::toggle_preview, UiMessage::operation_toggle_preview_name,
     UiMessage::operation_toggle_preview_description},
    {EditorOperation::paste_plain, UiMessage::operation_paste_plain_name,
     UiMessage::operation_paste_plain_description},
    {EditorOperation::paste_html, UiMessage::operation_paste_html_name,
     UiMessage::operation_paste_html_description},
    {EditorOperation::toggle_explorer, UiMessage::operation_toggle_explorer_name,
     UiMessage::operation_toggle_explorer_description},
    {EditorOperation::choose_explorer_root, UiMessage::operation_choose_explorer_root_name,
     UiMessage::operation_choose_explorer_root_description},
    {EditorOperation::insert_date_time, UiMessage::operation_insert_date_time_name,
     UiMessage::operation_insert_date_time_description},
    {EditorOperation::choose_encoding, UiMessage::operation_choose_encoding_name,
     UiMessage::operation_choose_encoding_description},
    {EditorOperation::choose_line_ending, UiMessage::operation_choose_line_ending_name,
     UiMessage::operation_choose_line_ending_description},
    {EditorOperation::find, UiMessage::operation_find_name, UiMessage::operation_find_description},
    {EditorOperation::find_next, UiMessage::operation_find_next_name,
     UiMessage::operation_find_next_description},
    {EditorOperation::find_previous, UiMessage::operation_find_previous_name,
     UiMessage::operation_find_previous_description},
    {EditorOperation::replace, UiMessage::operation_replace_name,
     UiMessage::operation_replace_description},
    {EditorOperation::go_to_line, UiMessage::operation_go_to_line_name,
     UiMessage::operation_go_to_line_description},
    {EditorOperation::preview_full, UiMessage::operation_preview_full_name,
     UiMessage::operation_preview_full_description},
    {EditorOperation::print, UiMessage::operation_print_name,
     UiMessage::operation_print_description},
    {EditorOperation::register_shell, UiMessage::operation_register_shell_name,
     UiMessage::operation_register_shell_description},
    {EditorOperation::unregister_shell, UiMessage::operation_unregister_shell_name,
     UiMessage::operation_unregister_shell_description},
    {EditorOperation::choose_language, UiMessage::operation_choose_language_name,
     UiMessage::operation_choose_language_description},
}};

// 操作の文字の行。switch ではなく表を引く。
[[nodiscard]] const OperationText &operation_text(EditorOperation operation) noexcept;
} // namespace nenenib::core
