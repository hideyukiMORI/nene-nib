#include "VimVisualRange.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "TextPosition.hpp"
#include "Utf8.hpp"
#include "VimRegisterKind.hpp"

#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] LineNumber line_of(const TextBuffer &text, Offset at)
{
    return text.position_of(at).line;
}

// inclusive な端の次。行の内容の終わり（NUL の桁）に載っていれば改行そのものを越える。
[[nodiscard]] Offset after(const TextBuffer &text, Offset at)
{
    const LineNumber line = line_of(text, at);
    const Offset end = text.line_end(line);
    if (at.value >= end.value)
    {
        return text.line_terminator_end(line);
    }
    const std::string rest = text.text_range(at, end);
    return Offset{at.value + next_code_point(rest, Offset{0}).value};
}
} // namespace

VimMotionRange vim_visual_range(const TextBuffer &text, const Selection &selection, VimMode mode)
{
    const OffsetRange ordered = selection_range(selection);
    switch (mode)
    {
    case VimMode::normal:
    case VimMode::insert:
    // 矩形は 1 つの範囲では表せない。行ごとの範囲を決めるのは vim_block_range で、呼ぶ側は
    // モードで先に分かれる（ADR 0035 の決定 2）。ここへ来たらキャレットの所の空の範囲になる。
    case VimMode::visual_block:
        return VimMotionRange{OffsetRange{selection.caret, selection.caret},
                              VimRegisterKind::characters};
    case VimMode::visual:
        return VimMotionRange{OffsetRange{ordered.begin, after(text, ordered.end)},
                              VimRegisterKind::characters};
    case VimMode::visual_line:
        return VimMotionRange{OffsetRange{text.line_start(line_of(text, ordered.begin)),
                                          text.line_end(line_of(text, ordered.end))},
                              VimRegisterKind::lines};
    }
    std::unreachable();
}
} // namespace nenenib::core
