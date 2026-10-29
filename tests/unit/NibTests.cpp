// core / application だけを対象にした単体テスト（QLT-013）。OS 資源には触れない。
// --coverage-negative は失敗系を全部省く。その実行が QLT-009 の閾値で落ちることが反例である。
// 時間は測らない（<chrono> は ARC-007 でここに書けない）。1 MB と 1
// 万行は「終わること」だけを見る。
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstdio>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
// 既定実行（引数なし）が回す scope 専用の契約。selector は「その scope だけを速く回す」絞り込みで、
// 契約そのものは既定実行にも載る（Issue #97）。新しい scope を足したら、下の selector の表と対に
// してここへも 1 行足す。fixture の再生は verify_vim_fixtures が全件行うので、ここには載せない。
// 契約を持たない scope（--vim-visual-yank）と、既に載っている契約を別の切り口で束ねただけの scope
// （--vim-open-line-external / --vim-open-line-recovery / --vim-line-jump-recovery）は出てこない。
// 表は関数の外に置く（CPP-012・関数長の上限は表の行を数えない形で守る）。
constexpr std::array<std::pair<std::string_view, void (*)()>, 72> contracts{{
constexpr std::array<std::pair<std::string_view, void (*)()>, 69> contracts{{
    {"--vim-wrapped", verify_vim_wrapped_contracts},
    {"--wrapped-view", verify_wrapped_view_scope},
    {"--vim-dot", verify_vim_dot_contracts},
    {"--vim-register-snapshot", verify_vim_register_snapshot_contracts},
    {"--vim-macro", verify_vim_macro_contracts},
    {"--vim-random", verify_vim_random_scope},
    {"--vim-window", verify_vim_window_contracts},
    {"--vim-clipboard", verify_vim_clipboard_contracts},
    {"--vim-search", verify_vim_search_contracts},
    {"--vim-search-highlight", verify_vim_search_highlight_contracts},
    {"--vim-search-incremental", verify_vim_search_incremental_contracts},
    {"--vim-text-objects", verify_vim_text_object_contracts},
    {"--vim-replace", verify_vim_replace_contracts},
    {"--vim-marks", verify_vim_mark_contracts},
    {"--vim-visual-wanted", verify_vim_visual_wanted_contracts},
    {"--vim-open-lines", verify_vim_open_line_contracts},
    {"--vim-character-search", verify_vim_character_search_contracts},
    {"--vim-line-jumps", verify_vim_line_jump_contracts},
    {"--vim-virtual-column", verify_vim_virtual_column_contracts},
    {"--vim-characters", verify_vim_character_contracts},
    {"--vim-case", verify_vim_case_contracts},
    {"--vim-visual-block", verify_vim_block_contracts},
    {"--tabs", verify_tabs_contracts},
    {"--split", verify_split_contracts},
    {"--session", verify_session_contracts},
    {"--history", verify_history_contracts},
    {"--bookmarks", verify_bookmarks_contracts},
    {"--background-work", verify_background_work_contracts},
    {"--open-async", verify_open_async_contracts},
    {"--language-server", verify_language_server_contracts},
    {"--autosave", verify_autosave_contracts},
    {"--autosave-plan-property", verify_autosave_plan_property_scope},
    {"--ex-files", verify_ex_files_contracts},
    {"--ordinary-characters", verify_ordinary_character_contracts},
    {"--ex-document", verify_ex_document_contracts},
    {"--ex-write-path", verify_ex_write_path_contracts},
    {"--ex-random", verify_ex_random_scope},
    {"--bell", verify_bell_contracts},
    {"--operations", verify_operations_scope},
    {"--guide", verify_guide_scope},
    {"--exit-plan", verify_exit_plan_scope},
    {"--virtual-columns", verify_virtual_column_index_scope},
    {"--grid-line", verify_grid_line_scope},
    {"--display", verify_display_contracts},
    {"--markdown", verify_markdown_scope},
    {"--language", verify_language_scope},
    {"--preview", verify_preview_contracts},
    {"--lexer", verify_lexer_scope},
    {"--lexer-property", verify_lexer_property_scope},
    {"--html-markdown", verify_html_markdown_scope},
    {"--markdown-property", verify_markdown_property_scope},
    {"--detected-text-property", verify_detected_text_property_scope},
    {"--rich-paste", verify_rich_paste_contracts},
    {"--syntax", verify_syntax_scope},
    {"--syntax-cache-property", verify_syntax_cache_property_scope},
    {"--text-buffer-property", verify_text_buffer_property_scope},
    {"--vim-pattern-property", verify_vim_pattern_property_scope},
    {"--title-bar-layout-property", verify_title_bar_layout_property_scope},
    {"--grid-cells-property", verify_grid_cells_property_scope},
    {"--tabs-property", verify_tabs_property_scope},
    {"--explorer-tree", verify_explorer_tree_scope},
    {"--explorer-layout", verify_explorer_layout_scope},
    {"--explorer", verify_explorer_contracts},
    {"--palette-match-property", verify_palette_match_property_scope},
    {"--command-match-property", verify_command_match_property_scope},
    {"--vim-number-property", verify_vim_number_property_scope},
    {"--editing-property", verify_editing_property_scope},
    {"--json", verify_json_scope},
    {"--json-property", verify_json_property_scope},
    {"--lsp-frames", verify_lsp_frames_scope},
    {"--lsp-messages", verify_lsp_messages_scope},
    {"--key-map", verify_key_map_scope},
}};

void verify_vim_scope_contracts()
{
    for (const auto &scope : contracts)
    {
        scope.second();
    }
}

void verify_file_text_scope()
{
    verify_detected_text_contracts();
    verify_text_buffer_read_contracts();
    verify_document_read_contracts();
}

constexpr std::array<std::pair<std::string_view, void (*)()>, 95> scopes{{
constexpr std::array<std::pair<std::string_view, void (*)()>, 92> scopes{{
    {"--body-layout", verify_body_layout_scope},
    {"--vim-wrapped", verify_vim_wrapped_scope},
    {"--wrapped-view", verify_wrapped_view_scope},
    {"--utf8", verify_utf8_scope},
    {"--status-items", verify_status_items_scope},
    {"--file-path", verify_file_path_scope},
    {"--buffer-range", verify_buffer_scope},
    {"--newline-index", verify_newline_index_scope},
    {"--file-text", verify_file_text_scope},
    {"--file-save", verify_document_save_contracts},
    {"--vim-recorded-keys", verify_vim_recorded_keys_scope},
    {"--edit-history", verify_edit_history_scope},
    {"--application", verify_application_scope},
    {"--frame-selection", verify_frame_selection_scope},
    {"--utf16", verify_utf16_scope},
    {"--display-line", verify_display_line_scope},
    {"--vim-dot", verify_vim_dot_scope},
    {"--vim-register-snapshot", verify_vim_register_snapshot_contracts},
    {"--vim-macro", verify_vim_macro_scope},
    {"--vim-random", verify_vim_random_scope},
    {"--vim-window", verify_vim_window_scope},
    {"--vim-clipboard", verify_vim_clipboard_scope},
    {"--vim-search", verify_vim_search_scope},
    {"--vim-search-highlight", verify_vim_search_highlight_scope},
    {"--vim-search-incremental", verify_vim_search_incremental_scope},
    {"--vim-text-objects", verify_vim_text_object_scope},
    {"--vim-replace", verify_vim_replace_scope},
    {"--vim-marks", verify_vim_mark_scope},
    {"--vim-visual-yank", verify_vim_visual_yank_scope},
    {"--vim-visual-wanted", verify_vim_visual_wanted_scope},
    {"--vim-open-lines", verify_vim_open_line_scope},
    {"--vim-open-line-external", verify_vim_open_line_external_scope},
    {"--vim-open-line-recovery", verify_vim_open_line_recovery},
    {"--vim-character-search", verify_vim_character_search_scope},
    {"--vim-line-jumps", verify_vim_line_jump_scope},
    {"--vim-line-jump-recovery", verify_vim_line_jump_recovery},
    {"--vim-virtual-column", verify_vim_virtual_column_scope},
    {"--vim-characters", verify_vim_character_scope},
    {"--vim-case", verify_vim_case_scope},
    {"--themes", verify_theme_scope},
    {"--user-theme-selection", verify_user_theme_selection},
    {"--user-theme-values", verify_user_theme_values},
    {"--command-palette", verify_command_palette},
    {"--ex-settings", verify_ex_settings},
    {"--ex-files", verify_ex_files_scope},
    {"--ordinary-characters", verify_ordinary_character_scope},
    {"--ex-document", verify_ex_document_scope},
    {"--ex-write-path", verify_ex_write_path_contracts},
    {"--ex-random", verify_ex_random_scope},
    {"--bell", verify_bell_contracts},
    {"--vim-visual-block", verify_vim_block_scope},
    {"--tabs", verify_tabs_scope},
    {"--split", verify_split_scope},
    {"--session", verify_session_scope},
    {"--history", verify_history_scope},
    {"--bookmarks", verify_bookmarks_scope},
    {"--background-work", verify_background_work_scope},
    {"--open-async", verify_open_async_scope},
    {"--language-server", verify_language_server_scope},
    {"--autosave", verify_autosave_scope},
    {"--autosave-plan-property", verify_autosave_plan_property_scope},
    {"--operations", verify_operations_scope},
    {"--guide", verify_guide_scope},
    {"--exit-plan", verify_exit_plan_scope},
    {"--virtual-columns", verify_virtual_column_index_scope},
    {"--grid-line", verify_grid_line_scope},
    {"--display", verify_display_scope},
    {"--markdown", verify_markdown_scope},
    {"--language", verify_language_scope},
    {"--preview", verify_preview_scope},
    {"--lexer", verify_lexer_scope},
    {"--lexer-property", verify_lexer_property_scope},
    {"--html-markdown", verify_html_markdown_scope},
    {"--markdown-property", verify_markdown_property_scope},
    {"--detected-text-property", verify_detected_text_property_scope},
    {"--palette-match-property", verify_palette_match_property_scope},
    {"--command-match-property", verify_command_match_property_scope},
    {"--vim-number-property", verify_vim_number_property_scope},
    {"--rich-paste", verify_rich_paste_contracts},
    {"--syntax", verify_syntax_scope},
    {"--syntax-cache-property", verify_syntax_cache_property_scope},
    {"--text-buffer-property", verify_text_buffer_property_scope},
    {"--vim-pattern-property", verify_vim_pattern_property_scope},
    {"--title-bar-layout-property", verify_title_bar_layout_property_scope},
    {"--grid-cells-property", verify_grid_cells_property_scope},
    {"--tabs-property", verify_tabs_property_scope},
    {"--explorer-tree", verify_explorer_tree_scope},
    {"--explorer-layout", verify_explorer_layout_scope},
    {"--explorer", verify_explorer_scope},
    {"--editing-property", verify_editing_property_scope},
    {"--json", verify_json_scope},
    {"--json-property", verify_json_property_scope},
    {"--lsp-frames", verify_lsp_frames_scope},
    {"--lsp-messages", verify_lsp_messages_scope},
    {"--key-map", verify_key_map_scope},
}};

[[nodiscard]] bool verify_selected_scope(std::string_view command)
{
    for (const auto &[name, verify] : scopes)
    {
        if (command == name)
        {
            verify();
            return true;
        }
    }
    return false;
}

int report()
{
    if (failure_count() != 0)
    {
        std::fprintf(stderr, "Nib unit tests: %zu of %zu checks failed\n", failure_count(),
                     check_count());
        return 1;
    }
    std::printf("Nib unit tests passed: %zu checks over text, caret, history, state and "
                "controller\n",
                check_count());
    return 0;
}
} // namespace
} // namespace nenenib::tests

using nenenib::tests::report;
using nenenib::tests::verify_command_palette;
using nenenib::tests::verify_controller_intents;
using nenenib::tests::verify_display_line;
using nenenib::tests::verify_display_line_views;
using nenenib::tests::verify_display_text_accepts_ascii;
using nenenib::tests::verify_editor_state;
using nenenib::tests::verify_ex_settings;
using nenenib::tests::verify_look;
using nenenib::tests::verify_palette;
using nenenib::tests::verify_selected_scope;
using nenenib::tests::verify_text_and_caret;
using nenenib::tests::verify_user_theme_selection;
using nenenib::tests::verify_user_theme_values;
using nenenib::tests::verify_vim_scope_contracts;

int main(int argc, char **argv)
{
    const std::string_view command = argc == 2 ? std::string_view{argv[1]} : std::string_view{};
    if (verify_selected_scope(command))
    {
        return report();
    }
    verify_display_text_accepts_ascii();
    verify_palette();
    verify_editor_state();
    if (command == "--coverage-negative")
    {
        return report();
    }
    nenenib::tests::verify_detected_text_contracts();
    nenenib::tests::verify_newline_index_scope();
    verify_text_and_caret();
    verify_display_line();
    verify_display_line_views();
    verify_controller_intents();
    verify_vim_scope_contracts();
    verify_ex_settings();
    verify_command_palette();
    verify_user_theme_values();
    verify_user_theme_selection();
    verify_look();
    return report();
}
