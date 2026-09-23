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
void verify_vim_scope_contracts()
{
    constexpr std::array<std::pair<std::string_view, void (*)()>, 12> contracts{{
        {"--vim-dot", verify_vim_dot_contracts},
        {"--vim-search", verify_vim_search_contracts},
        {"--vim-search-highlight", verify_vim_search_highlight_contracts},
        {"--vim-search-incremental", verify_vim_search_incremental_contracts},
        {"--vim-text-objects", verify_vim_text_object_contracts},
        {"--vim-replace", verify_vim_replace_contracts},
        {"--vim-visual-wanted", verify_vim_visual_wanted_contracts},
        {"--vim-open-lines", verify_vim_open_line_contracts},
        {"--vim-character-search", verify_vim_character_search_contracts},
        {"--vim-line-jumps", verify_vim_line_jump_contracts},
        {"--vim-virtual-column", verify_vim_virtual_column_contracts},
        {"--vim-visual-block", verify_vim_block_contracts},
    }};
    for (const auto &scope : contracts)
    {
        scope.second();
    }
}

[[nodiscard]] bool verify_selected_scope(std::string_view command)
{
    constexpr std::array<std::pair<std::string_view, void (*)()>, 21> scopes{{
        {"--display-line", verify_display_line_scope},
        {"--vim-dot", verify_vim_dot_scope},
        {"--vim-search", verify_vim_search_scope},
        {"--vim-search-highlight", verify_vim_search_highlight_scope},
        {"--vim-search-incremental", verify_vim_search_incremental_scope},
        {"--vim-text-objects", verify_vim_text_object_scope},
        {"--vim-replace", verify_vim_replace_scope},
        {"--vim-visual-yank", verify_vim_visual_yank_scope},
        {"--vim-visual-wanted", verify_vim_visual_wanted_scope},
        {"--vim-open-lines", verify_vim_open_line_scope},
        {"--vim-open-line-external", verify_vim_open_line_external_scope},
        {"--vim-open-line-recovery", verify_vim_open_line_recovery},
        {"--vim-character-search", verify_vim_character_search_scope},
        {"--vim-line-jumps", verify_vim_line_jump_scope},
        {"--vim-line-jump-recovery", verify_vim_line_jump_recovery},
        {"--vim-virtual-column", verify_vim_virtual_column_scope},
        {"--user-theme-selection", verify_user_theme_selection},
        {"--user-theme-values", verify_user_theme_values},
        {"--command-palette", verify_command_palette},
        {"--ex-settings", verify_ex_settings},
        {"--vim-visual-block", verify_vim_block_scope},
    }};
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
