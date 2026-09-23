#pragma once

#include <cstdint>

namespace nenenib::core
{
// オペレータが取れる移動の閉じた一覧（ADR 0012 の決定 5）。範囲はこの値ごとに決まる。
// word_end_for_change は cw の特例（語の上では ce の範囲になる。Vim の nv_wordcmd の flag）で、
// 鍵からは引けない＝オペレータが change のときに engine が w から差し替える値である。
enum class VimMotion : std::uint8_t
{
    left,
    down,
    up,
    right,
    line_start,
    line_end,
    next_word,
    previous_word,
    word_end,
    word_end_for_change,
    first_non_blank,
    screen_top,
    screen_middle,
    screen_bottom,
    document_first,
    document_last,
    next_line,
    previous_line,
    // `<Space>` `<BS>`。行をまたぐ exclusive な文字単位の移動（ADR 0049 の決定 1・2）。
    wrap_right,
    wrap_left
};
} // namespace nenenib::core
