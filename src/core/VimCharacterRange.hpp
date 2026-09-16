#pragma once

#include <cstdint>

namespace nenenib::core
{
// 語の種類を決める code point の区間（Vim の utf_class_tab に対応する表の 1 行）。
// group が同じ文字どうしが 1 つの語になる。0 は空白、1 は記号、2 は語の文字。
struct VimCharacterRange
{
    char32_t first;
    char32_t last;
    std::uint32_t group;
};
} // namespace nenenib::core
