#include "VimParagraphStart.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "TextBuffer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
// Vim 9.1 の既定の 'paragraphs' と 'sections' を 2 文字ずつに切った表（optiondefs.h の
// p_para と p_sections）。2 文字めの空白は「行の終わりか空白」に当たる（inmacro）。
constexpr std::array<std::string_view, 21> nroff_macros{"IP", "LP", "PP", "QP", "P ", "TP", "HP",
                                                        "LI", "Pp", "Lp", "It", "pp", "lp", "ip",
                                                        "bp", "SH", "NH", "H ", "HU", "nh", "sh"};

// 行頭の `.` と後ろの 2 文字を読む窓。
constexpr std::size_t head_bytes = 3;

// `.` の後ろの名前（1 か 2 バイト・行の終わりで切れる）が表の 1 つに当たるか（Vim の inmacro）。
[[nodiscard]] bool names_macro(std::string_view macro, std::string_view name)
{
    if (macro.at(0) != name.at(0))
    {
        return false;
    }
    if (name.size() > 1 && macro.at(1) == name.at(1))
    {
        return true;
    }
    return macro.at(1) == ' ' && (name.size() == 1 || name.at(1) == ' ');
}

[[nodiscard]] bool nroff_macro(std::string_view name)
{
    return !name.empty() && std::ranges::any_of(nroff_macros, [name](std::string_view macro)
                                                { return names_macro(macro, name); });
}
} // namespace

bool vim_paragraph_start(const TextBuffer &text, LineNumber line)
{
    const Offset start = text.line_start(line);
    const Offset end = text.line_end(line);
    if (start == end)
    {
        return true;
    }
    const std::string head =
        text.text_range(start, Offset{std::min(end.value, start.value + head_bytes)});
    const char first = head.at(0);
    return first == '\f' || (first == '.' && nroff_macro(std::string_view{head}.substr(1)));
}
} // namespace nenenib::core
