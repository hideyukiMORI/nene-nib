#include "VimCase.hpp"

#include "CaseMappingRange.hpp"
#include "CaseMappingTable.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nenenib::core
{
namespace
{
[[nodiscard]] consteval bool well_formed(std::span<const CaseMappingRange> table)
{
    for (std::size_t at = 0; at < table.size(); ++at)
    {
        const CaseMappingRange &range = table[at];
        if (range.first > range.last || (range.step != 1 && range.step != 2) ||
            (at > 0 && table[at - 1].last >= range.first))
        {
            return false;
        }
    }
    return true;
}

static_assert(well_formed(case_to_upper), "case_to_upper must be ascending and disjoint");
static_assert(well_formed(case_to_lower), "case_to_lower must be ascending and disjoint");

// first が value 以下の最後の行を二分探索で引き、その行の step の列に入っていれば delta を足す。
[[nodiscard]] char32_t mapped(std::span<const CaseMappingRange> table, char32_t value) noexcept
{
    const auto after = std::upper_bound(table.begin(), table.end(), value,
                                        [](char32_t wanted, const CaseMappingRange &range)
                                        { return wanted < range.first; });
    if (after == table.begin())
    {
        return value;
    }
    const CaseMappingRange &range = *(after - 1);
    if (value > range.last || (value - range.first) % range.step != 0)
    {
        return value;
    }
    return static_cast<char32_t>(static_cast<std::int64_t>(value) + range.delta);
}
} // namespace

char32_t vim_to_upper(char32_t value) noexcept
{
    return mapped(case_to_upper, value);
}

char32_t vim_to_lower(char32_t value) noexcept
{
    return vim_to_upper(value) != value ? value : mapped(case_to_lower, value);
}

char32_t vim_toggled(char32_t value) noexcept
{
    const char32_t upper = vim_to_upper(value);
    return upper != value ? upper : mapped(case_to_lower, value);
}
} // namespace nenenib::core
