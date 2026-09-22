#include "VimVisualReselect.hpp"

#include "LineNumber.hpp"
#include "VimBlockExtent.hpp"
#include "VimCharacterExtent.hpp"
#include "VimColumnWish.hpp"
#include "VimLineExtent.hpp"
#include "VirtualColumn.hpp"

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
// 改行まで含める。桁の記録は 1 行なら桁の個数、複数行なら最終行の絶対桁（実測）。桁は仮想桁で、
// 1 行のときの起点は「Vim がキャレットを描く桁」＝ Tab の上なら最後の桁（ADR 0034 の決定 4）。
// offset_at_virtual_column が行の内容の終わりで止まるので、桁が足りない行はそこへ畳まれる。
[[nodiscard]] Offset reselected_end(const TextBuffer &text, Offset caret, LineNumber last,
                                    const VimCharacterExtent &extent)
{
    switch (extent.wish)
    {
    case VimColumnWish::at_line_end:
        return text.line_end(last);
    case VimColumnWish::at_column:
        break;
    }
    const VirtualColumn column =
        extent.lines > 1
            ? extent.column
            : VirtualColumn{caret_virtual_column(text, caret).value + extent.column.value - 1};
    return offset_at_virtual_column(text, last, column);
}

[[nodiscard]] Selection reselected(const TextBuffer &text, Offset caret,
                                   const VimCharacterExtent &extent)
{
    const LineNumber line = text.position_of(caret).line;
    return Selection{caret,
                     reselected_end(text, caret, line_after(text, line, extent.lines), extent)};
}

[[nodiscard]] Selection reselected(const TextBuffer &text, Offset caret, VimLineExtent extent)
{
    const LineNumber last = line_after(text, text.position_of(caret).line, extent.lines);
    return Selection{caret, text.line_start(last)};
}

// 矩形（ADR 0035 の決定 7）。左上をキャレットにして、同じ行数と桁数の矩形の右下を選ぶ。
// `$` は桁を持たないので最終行の内容の終わりへ置き、幅は覆う行から決め直される（実測）。
[[nodiscard]] Selection reselected(const TextBuffer &text, Offset caret,
                                   const VimBlockExtent &extent)
{
    const LineNumber last = line_after(text, text.position_of(caret).line, extent.lines);
    switch (extent.wish)
    {
    case VimColumnWish::at_line_end:
        return Selection{caret, text.line_end(last)};
    case VimColumnWish::at_column:
        break;
    }
    const VirtualColumn right{caret_virtual_column(text, caret).value + extent.width.columns - 1};
    return Selection{caret, offset_at_virtual_column(text, last, right)};
}
} // namespace

Selection vim_visual_reselect(const TextBuffer &text, Offset caret, const VimVisualExtent &extent)
{
    return std::visit([&](const auto &value) { return reselected(text, caret, value); }, extent);
}
} // namespace nenenib::core
