#pragma once

#include <cstdint>

namespace nenenib::core
{
// イージーモーションの動作の閉じた 9 つ（ADR 0163 の決定 2）。`\` の後の 1 鍵で選ぶ（表は
// VimKeyTable の vim_easymotion_kind_for）。f F s は跳び先を決める文字を 1 つ待つ（決定 4）。
enum class EasyMotionKind : std::uint8_t
{
    // `w` `b` `e`。語の頭（前・後）と語の尾（前）。語の切れ目は `w` と同じ語の分類。
    word_start_after,
    word_start_before,
    word_end_after,
    // `j` `k`。下・上の行の最初の非空白。
    line_below,
    line_above,
    // `f{文字}` `F{文字}` `s{文字}`。前・後・前後両方の文字。
    character_after,
    character_before,
    character_around,
    // `l`。見えている全部の語の頭（前後両方）。
    word_start_around
};
} // namespace nenenib::core
