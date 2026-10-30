#include "GoToLineInput.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <limits>
#include <system_error>

namespace nenenib::core
{
namespace
{
// ASCII の 10 進の数字だけの空でない欄の値。収まらない数は最大に飽和する（`:N` と同じ・#847）。
[[nodiscard]] std::optional<std::size_t> decimal_of(std::string_view digits)
{
    if (digits.empty() ||
        !std::ranges::all_of(digits, [](char byte) { return byte >= '0' && byte <= '9'; }))
    {
        return std::nullopt;
    }
    std::size_t number = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
    return parsed.ec == std::errc{} ? number : std::numeric_limits<std::size_t>::max();
}
} // namespace

std::optional<GoToLineTarget> go_to_line_target(std::string_view text)
{
    const auto separator = text.find(':');
    const auto line = decimal_of(text.substr(0, separator));
    if (!line.has_value())
    {
        return std::nullopt;
    }
    if (separator == std::string_view::npos)
    {
        return GoToLineTarget{line.value(), std::nullopt};
    }
    const auto column = decimal_of(text.substr(separator + 1));
    if (!column.has_value())
    {
        return std::nullopt;
    }
    return GoToLineTarget{line.value(), column};
}
} // namespace nenenib::core
