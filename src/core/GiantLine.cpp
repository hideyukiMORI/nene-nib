#include "GiantLine.hpp"

#include "Utf8.hpp"
#include "VimCharacterBoundary.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace nenenib::core
{
namespace
{
// at を含む Vim の 1 文字の先頭（at が境目なら at）。
[[nodiscard]] std::size_t character_floor(std::string_view line, std::size_t at) noexcept
{
    if (at == 0 || at >= line.size())
    {
        return at;
    }
    const auto start = vim_character_start(line, Offset{at});
    return vim_character_end(line, start).value == at ? at : start.value;
}

// at を含む Vim の 1 文字の終わり（at が境目なら at）。
[[nodiscard]] std::size_t character_ceiling(std::string_view line, std::size_t at) noexcept
{
    const auto floor = character_floor(line, at);
    return floor == at ? at : vim_character_end(line, Offset{floor}).value;
}

[[nodiscard]] std::size_t walked_back(std::string_view line, std::size_t at,
                                      std::size_t steps) noexcept
{
    for (std::size_t step = 0; step < steps && at > 0; ++step)
    {
        at = previous_code_point(line, Offset{at}).value;
    }
    return at;
}

[[nodiscard]] std::size_t walked_forward(std::string_view line, std::size_t at,
                                         std::size_t steps) noexcept
{
    for (std::size_t step = 0; step < steps && at < line.size(); ++step)
    {
        at = next_code_point(line, Offset{at}).value;
    }
    return at;
}
} // namespace

OffsetRange giant_line_window(std::string_view line, Offset anchor, std::size_t half_width) noexcept
{
    auto at = std::min(anchor.value, line.size());
    while (at > 0 && !is_boundary(line, Offset{at}))
    {
        --at;
    }
    at = character_floor(line, at);
    const auto ordinal = code_point_count(line.substr(0, at));
    const auto center = (ordinal / giant_line_step) * giant_line_step;
    const auto first = center - std::min(center, half_width);
    const auto begin = walked_back(line, at, ordinal - first);
    const auto last = center + half_width;
    const auto end = walked_forward(line, at, last > ordinal ? last - ordinal : 0);
    return OffsetRange{Offset{character_floor(line, begin)}, Offset{character_ceiling(line, end)}};
}
} // namespace nenenib::core
