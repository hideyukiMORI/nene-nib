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
    // VISUAL の 3 つ（ADR 0018 の決定 7）。表は NORMAL と同じ 1 つで、動作ごとに「VISUAL では
    // どうするか」を分ける。`o` は NORMAL では何もしない（行を開くのはこの縦切りに無い）。
    visual,
    visual_line,
    swap_visual_ends
};
} // namespace nenenib::core
