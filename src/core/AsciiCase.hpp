#pragma once

#include <algorithm>
#include <string_view>

namespace nenenib::core
{
// 補完の引数（テーマ名・字体名）の照合と名前順の正本（#696・ADR 0022 決定 5 の追補）。畳むのは
// ASCII の A〜Z だけで、locale や display_width の表に依らない（ARC-007）。

// ASCII の大文字を小文字に畳んだバイト（それ以外のバイトはそのまま）。
[[nodiscard]] constexpr unsigned char ascii_folded(char byte) noexcept
{
    const auto value = static_cast<unsigned char>(byte);
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value - 'A' + 'a') : value;
}

// text が prefix で始まるか（ASCII の大文字小文字を区別しない）。
[[nodiscard]] constexpr bool ascii_prefix_ignoring_case(std::string_view text,
                                                        std::string_view prefix) noexcept
{
    return prefix.size() <= text.size() && std::ranges::equal(prefix, text.substr(0, prefix.size()),
                                                              {}, ascii_folded, ascii_folded);
}

// 名前順: ASCII の大文字小文字を畳んで比べ、同じならバイト順（全順序）。
[[nodiscard]] constexpr bool ascii_folded_before(std::string_view left,
                                                 std::string_view right) noexcept
{
    if (std::ranges::equal(left, right, {}, ascii_folded, ascii_folded))
    {
        return left < right;
    }
    return std::ranges::lexicographical_compare(left, right, {}, ascii_folded, ascii_folded);
}
} // namespace nenenib::core
