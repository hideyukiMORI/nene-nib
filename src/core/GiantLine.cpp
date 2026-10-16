#include "GiantLine.hpp"

#include "VimCharacterBoundary.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
constexpr std::size_t longest_sequence = 4;

[[nodiscard]] bool continues(char byte) noexcept
{
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

[[nodiscard]] std::size_t sequence_length(char byte) noexcept
{
    const auto lead = static_cast<unsigned char>(byte);
    if (lead < 0x80U)
    {
        return 1;
    }
    if (lead >= 0xF0U)
    {
        return longest_sequence;
    }
    return lead >= 0xE0U ? 3 : 2;
}

// 先頭の継続 byte の欠片を飛ばした最初の境目。
[[nodiscard]] std::size_t first_boundary(std::string_view utf8) noexcept
{
    std::size_t at = 0;
    while (at < utf8.size() && at < longest_sequence && continues(utf8[at]))
    {
        ++at;
    }
    return at;
}

// 末尾の欠けた sequence を除いた最後の境目。
[[nodiscard]] std::size_t last_boundary(std::string_view utf8, std::size_t lower) noexcept
{
    std::size_t lead = utf8.size();
    while (lead > lower && utf8.size() - lead < longest_sequence)
    {
        --lead;
        if (!continues(utf8[lead]))
        {
            return lead + sequence_length(utf8[lead]) <= utf8.size() ? utf8.size() : lead;
        }
    }
    return utf8.size();
}

[[nodiscard]] std::size_t walked_back(std::string_view view, std::size_t at,
                                      std::size_t steps) noexcept
{
    for (std::size_t step = 0; step < steps && at > 0; ++step)
    {
        --at;
        while (at > 0 && continues(view[at]))
        {
            --at;
        }
    }
    return at;
}

[[nodiscard]] std::size_t walked_forward(std::string_view view, std::size_t at,
                                         std::size_t steps) noexcept
{
    for (std::size_t step = 0; step < steps && at < view.size(); ++step)
    {
        at = std::min(at + sequence_length(view[at]), view.size());
    }
    return at;
}

// at を含む Vim の 1 文字の先頭（at が境目なら at）。
[[nodiscard]] std::size_t character_floor(std::string_view view, std::size_t at) noexcept
{
    if (at == 0 || at >= view.size())
    {
        return at;
    }
    const auto start = vim_character_start(view, Offset{at});
    return vim_character_end(view, start).value == at ? at : start.value;
}

// at を含む Vim の 1 文字の終わり（at が境目なら at）。
[[nodiscard]] std::size_t character_ceiling(std::string_view view, std::size_t at) noexcept
{
    if (at == 0 || at >= view.size())
    {
        return at;
    }
    return vim_character_end(view, vim_character_start(view, Offset{at})).value;
}
} // namespace

OffsetRange giant_line_window(std::string_view utf8, Offset anchor, std::size_t half) noexcept
{
    const auto lower = first_boundary(utf8);
    const auto upper = std::max(last_boundary(utf8, lower), lower);
    const auto view = utf8.substr(lower, upper - lower);
    auto at = std::clamp(anchor.value, lower, upper) - lower;
    while (at > 0 && at < view.size() && continues(view[at]))
    {
        --at;
    }
    const auto begin = character_floor(view, walked_back(view, at, half));
    const auto end = character_ceiling(view, walked_forward(view, at, half));
    return OffsetRange{Offset{lower + begin}, Offset{lower + std::max(end, begin)}};
}
} // namespace nenenib::core
