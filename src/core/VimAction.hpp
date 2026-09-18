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
    redo
};
} // namespace nenenib::core
