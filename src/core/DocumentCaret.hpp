#pragma once

#include "LineEnding.hpp"
#include "Offset.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace nenenib::core
{
// engine が LF で数えた本文 `utf8` を `at` に入れた後のキャレット `caret` を、文書の改行で直した
// 本文の上の位置へずらす（#1093・#1107）。CRLF は改行 1 つにつき 1 バイト長いので、数えるのは
// `at` からキャレットまでの改行だけ。controller の効果の写しと engine の矩形の写しが共用する 1 本。
[[nodiscard]] constexpr Offset document_caret_after(Offset at, std::string_view utf8, Offset caret,
                                                    LineEnding ending) noexcept
{
    const std::string_view typed = utf8.substr(0, caret.value - at.value);
    const auto newlines = static_cast<std::size_t>(std::ranges::count(typed, '\n'));
    return Offset{caret.value + newlines * (newline_of(ending).size() - 1)};
}
} // namespace nenenib::core
