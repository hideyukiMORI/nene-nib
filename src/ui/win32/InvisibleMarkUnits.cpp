// 不可視文字の欄 → 描いた layout の UTF-16 の位置（ADR 0193 の決定 10・#1977）。
#include "InvisibleMarkUnits.hpp"

#include "DisplayLine.hpp"
#include "InvisibleMark.hpp"
#include "InvisibleSpan.hpp"
#include "LineView.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

namespace nenenib::ui::win32
{
namespace
{
// 描画の文字列を前から歩く位置（バイト・code point・UTF-16 の単位）。
using Walk = std::tuple<std::size_t, std::size_t, UINT32>;

[[nodiscard]] bool continuation(char byte) noexcept
{
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

// code point の位置 point まで進める（後ろへは戻らない・区間は前から順）。
void walked(std::string_view text, Walk &walk, std::size_t point) noexcept
{
    auto &[byte, at, units] = walk;
    while (at < point && byte < text.size())
    {
        // 4 バイトの字（U+10000 以上）は UTF-16 の 2 単位。
        units += static_cast<unsigned char>(text[byte]) >= 0xF0U ? 2U : 1U;
        ++byte;
        while (byte < text.size() && continuation(text[byte]))
        {
            ++byte;
        }
        ++at;
    }
}

// 区間 span の 1 字ごとの印（置き換えられた字は飛ばす）。
void marked(const core::DisplayLine &display, const core::InvisibleSpan &span, Walk &walk,
            std::vector<InvisibleMark> &marks)
{
    for (std::size_t column = span.begin.value; column < span.end.value; ++column)
    {
        if (core::is_replaced(display, column - 1))
        {
            continue;
        }
        walked(display.text, walk, core::display_position(display, column - 1));
        marks.push_back(InvisibleMark{std::get<2>(walk), span.kind});
    }
}
} // namespace

std::vector<InvisibleMark> invisible_marks_of(const application::LineView &line)
{
    std::vector<InvisibleMark> marks;
    if (!line.invisibles.has_value())
    {
        return marks;
    }
    Walk walk{0, 0, 0};
    for (const core::InvisibleSpan &span : line.invisibles.value().spans)
    {
        marked(line.display, span, walk, marks);
    }
    return marks;
}

UINT32 line_end_unit(const application::LineView &line)
{
    Walk walk{0, 0, 0};
    walked(line.display.text, walk, line.display.text.size());
    return std::get<2>(walk);
}

std::optional<float> line_break_left(float x, float width, float right) noexcept
{
    if (x > right)
    {
        return std::nullopt;
    }
    return std::min(x, right - width);
}
} // namespace nenenib::ui::win32
