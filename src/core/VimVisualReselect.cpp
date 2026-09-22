#include "VimVisualReselect.hpp"

#include "Column.hpp"
#include "LineNumber.hpp"
#include "TextPosition.hpp"
#include "VimCharacterExtent.hpp"
#include "VimColumnWish.hpp"
#include "VimLineExtent.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <variant>

namespace nenenib::core
{
namespace
{
// 記録した行数ぶん下の行。文書の終わりで足りなければ最終行まで畳む（Vim も同じ・実測）。
[[nodiscard]] LineNumber line_after(const TextBuffer &text, LineNumber first, std::size_t lines)
{
    const std::size_t step = lines > 1 ? lines - 1 : 0;
    return LineNumber{std::min(first.value + step, text.line_count())};
}

// 文字単位の端。`$` の記録は行の内容の終わり（Vim が NUL を置く桁）まで＝ vim_visual_range が
// 改行まで含める。桁の記録は 1 行なら桁の個数、複数行なら最終行の絶対桁（実測）。
// offset_of が行の内容の終わりで止まるので、桁が足りない行は自然にそこへ畳まれる。
[[nodiscard]] Offset reselected_end(const TextBuffer &text, const TextPosition &at,
                                   LineNumber last, const VimCharacterExtent &extent)
{
    switch (extent.wish)
    {
    case VimColumnWish::at_line_end:
        return text.line_end(last);
    case VimColumnWish::at_column:
        break;
    }
    const Column column = extent.lines > 1
                              ? extent.column
                              : Column{at.column.value + extent.column.value - 1};
    return text.offset_of(TextPosition{last, column});
}

[[nodiscard]] Selection reselected(const TextBuffer &text, Offset caret,
                                   const VimCharacterExtent &extent)
{
    const TextPosition at = text.position_of(caret);
    return Selection{caret,
                     reselected_end(text, at, line_after(text, at.line, extent.lines), extent)};
}

[[nodiscard]] Selection reselected(const TextBuffer &text, Offset caret, VimLineExtent extent)
{
    const LineNumber last = line_after(text, text.position_of(caret).line, extent.lines);
    return Selection{caret, text.line_start(last)};
}
} // namespace

Selection vim_visual_reselect(const TextBuffer &text, Offset caret, const VimVisualExtent &extent)
{
    return std::visit([&](const auto &value) { return reselected(text, caret, value); }, extent);
}
} // namespace nenenib::core
