#include "SubstituteAll.hpp"

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "VimSearch.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// line 行の長さのある当たりを前から順に visit(行, 範囲) へ渡す（vim_line_matches の 1 本）。位置は
// 本文のバイト。
template <typename Visit>
void each_line_match(const TextBuffer &text, const VimPattern &pattern, std::size_t line,
                     Visit &visit)
{
    const Offset start = text.line_start(LineNumber{line});
    for (const auto &found : vim_line_matches(text.line_text(LineNumber{line}), pattern))
    {
        if (found.begin.value < found.end.value)
        {
            visit(line, OffsetRange{Offset{start.value + found.begin.value},
                                    Offset{start.value + found.end.value}});
        }
    }
}

// lines（最初の行と最後の行・両端を含む）の当たりを前から順に。
template <typename Visit>
void each_match(const TextBuffer &text, const VimPattern &pattern,
                std::pair<std::size_t, std::size_t> lines, Visit &&visit)
{
    for (std::size_t line = lines.first; line <= lines.second; ++line)
    {
        each_line_match(text, pattern, line, visit);
    }
}

// 当たりの外の本文 kept と、count 個の piece が limit に収まるか。掛け算をせずに判定する。
[[nodiscard]] bool fits(std::size_t kept, std::size_t count, std::size_t piece,
                        std::size_t limit) noexcept
{
    return kept <= limit && (piece == 0 || count <= (limit - kept) / piece);
}
} // namespace

std::expected<Edit, SubstituteFailure> substituted_all(const TextBuffer &text,
                                                       const VimPattern &pattern,
                                                       std::string_view replacement,
                                                       std::size_t limit)
{
    // 1 周目は数えるだけ: 当たりの数・覆うバイトの和・最初と最後の当たりの行と端。
    std::size_t count = 0;
    std::size_t removed = 0;
    std::size_t first_line = 0;
    std::size_t last_line = 0;
    Offset at{};
    Offset end{};
    each_match(text, pattern, std::pair<std::size_t, std::size_t>{1, text.line_count()},
               [&](std::size_t line, const OffsetRange &range)
               {
                   if (count == 0)
                   {
                       first_line = line;
                       at = range.begin;
                   }
                   last_line = line;
                   end = range.end;
                   ++count;
                   removed += range.end.value - range.begin.value;
               });
    if (count == 0)
    {
        return std::unexpected(SubstituteFailure::no_match);
    }
    if (!fits(text.size_bytes() - removed, count, replacement.size(), limit))
    {
        return std::unexpected(SubstituteFailure::too_large);
    }
    // 2 周目は最初と最後の当たりの行だけを照合し直して置き換える。
    std::string inserted;
    inserted.reserve(end.value - at.value - removed + count * replacement.size());
    Offset copied = at;
    each_match(text, pattern, std::pair{first_line, last_line},
               [&](std::size_t, const OffsetRange &range)
               {
                   inserted += text.text_range(copied, range.begin);
                   inserted += replacement;
                   copied = range.end;
               });
    return Edit{at, text.text_range(at, copied), std::move(inserted), at};
}
} // namespace nenenib::core
