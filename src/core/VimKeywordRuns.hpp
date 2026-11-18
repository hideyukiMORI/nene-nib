#pragma once

#include "OffsetRange.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// INSERT の補完の語（ADR 0169 の決定 1）。語は `w` と同じ文字の種類（vim_character_class の
// word）で 2 以上の、同じ種類の Vim の 1 文字の連なり（Vim の insexpand.c の find_word_end と
// 正規表現の `\<`）。表は vim_character_class の 1
// つで、語の表と候補の走査はここを通る（ARC-001）。

// 種類 group が語の文字か（Vim の vim_iswordc。0 は空白・1 は記号）。
[[nodiscard]] constexpr bool vim_keyword_group(std::uint32_t group) noexcept
{
    return group >= 2;
}

// content の at にある code point の種類（word の分類）。at が content の外なら空白（0）。
[[nodiscard]] std::uint32_t vim_keyword_class_at(std::string_view content, std::size_t at) noexcept;

// content（改行を含んでよい・正しい UTF-8）の語の範囲を前から順に返す。行ごとに歩くので、行頭の
// 孤立した結合文字はそれ自身が 1 文字（vim_character_end の約束）。content の先頭は行頭か、
// 語の文字でない 1 文字の先頭であること（呼ぶ側が守る）。
[[nodiscard]] std::vector<OffsetRange> vim_keyword_runs(std::string_view content);

// range（vim_keyword_runs の 1 つ）が Vim の 1 文字を 2 つ以上持つか。語の表に載るのはこの語だけ
// （1 文字の語は接頭辞が 0〜1 文字のときの Vim の `\<\k\k` に当たらない）。
[[nodiscard]] bool vim_keyword_has_two_characters(std::string_view content,
                                                  const OffsetRange &range) noexcept;
} // namespace nenenib::core
