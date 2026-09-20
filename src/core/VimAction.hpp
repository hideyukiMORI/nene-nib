#pragma once

#include <cstdint>

namespace nenenib::core
{
// NORMAL の鍵が表から引く動作の閉じた一覧（ADR 0012 の決定 5 / CPP-012）。
enum class VimAction : std::uint8_t
{
    move_left,
    move_down,
    move_up,
    move_right,
    move_line_start,
    move_line_end,
    move_next_word,
    move_previous_word,
    move_word_end,
    move_first_non_blank,
    move_screen_top,
    move_screen_middle,
    move_screen_bottom,
    move_document_first,
    move_document_last,
    scroll_half_down,
    scroll_half_up,
    scroll_page_down,
    scroll_page_up,
    remove_character,
    remove_operator,
    change_operator,
    yank_operator,
    put_after,
    put_before,
    remove_to_line_end,
    change_to_line_end,
    yank_line,
    insert_before,
    insert_after,
    insert_at_line_start,
    insert_at_line_end,
    undo,
    redo,
    // 同じo/OをNORMALでは開行、VISUALでは端点交換へ写す（ADR 0028）。
    visual,
    visual_line,
    open_line_below,
    open_line_above,
    find_character_forward,
    find_character_backward,
    till_character_forward,
    till_character_backward,
    repeat_character_search,
    repeat_character_search_opposite,
    prefix_g,
    open_command_line
};
} // namespace nenenib::core
