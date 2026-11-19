#include "HeadingSlug.hpp"

#include "AsciiCase.hpp"
#include "PercentDecoding.hpp"
#include "PreviewBlockKind.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 名札 → それを元の名札として数えた回数（github-slugger の occurrences）。名札の順に並べ、
// 二分探索で引く。
using Occurrence = std::pair<std::string, std::size_t>;
using Occurrences = std::vector<Occurrence>;

constexpr std::size_t decimal_radix = 10U;

[[nodiscard]] constexpr bool ascii_space(char value) noexcept
{
    return value == ' ';
}

// 名札に残す ASCII の字か（英数字と `-` `_`）。
[[nodiscard]] constexpr bool kept_ascii(unsigned char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '-' ||
           value == '_';
}

// 重複を数える前の名札。
[[nodiscard]] std::string base_slug(std::string_view text)
{
    const auto first = std::ranges::find_if_not(text, ascii_space);
    const auto last = std::ranges::find_if_not(text.rbegin(), text.rend(), ascii_space).base();
    std::string slug;
    for (auto at = first; at < last; ++at)
    {
        const unsigned char folded = ascii_folded(*at);
        if (folded >= 0x80U || kept_ascii(folded))
        {
            slug.push_back(static_cast<char>(folded));
        }
        else if (ascii_space(*at))
        {
            slug.push_back('-');
        }
    }
    return slug;
}

[[nodiscard]] std::string decimal_text(std::size_t value)
{
    // 上の桁から書く（std::ranges::reverse は core の外へ __std_reverse_* を出す・ARC-003）。
    std::size_t scale = 1U;
    while (value / scale >= decimal_radix)
    {
        scale *= decimal_radix;
    }
    std::string digits;
    for (; scale != 0U; scale /= decimal_radix)
    {
        digits.push_back(static_cast<char>('0' + (value / scale) % decimal_radix));
    }
    return digits;
}

[[nodiscard]] Occurrences::iterator occurrence_at(Occurrences &occurrences, std::string_view slug)
{
    return std::ranges::lower_bound(occurrences, slug, {},
                                    [](const Occurrence &entry) -> std::string_view
                                    { return entry.first; });
}

[[nodiscard]] bool occurs(Occurrences &occurrences, std::string_view slug)
{
    const auto at = occurrence_at(occurrences, slug);
    return at != occurrences.end() && at->first == slug;
}

// 元の名札 original から、まだ現れていない名札を作って数えに載せる（github-slugger の slug）。
[[nodiscard]] std::string unique_slug(Occurrences &occurrences, const std::string &original)
{
    std::string result = original;
    while (occurs(occurrences, result))
    {
        const auto count = ++occurrence_at(occurrences, original)->second;
        result = original + "-" + decimal_text(count);
    }
    occurrences.insert(occurrence_at(occurrences, result), Occurrence{result, 0U});
    return result;
}
} // namespace

std::vector<MarkdownHeading> heading_slugs(std::span<const PreviewBlock> blocks)
{
    std::vector<MarkdownHeading> headings;
    Occurrences occurrences;
    std::size_t block = 0;
    for (const PreviewBlock &each : blocks)
    {
        if (each.kind == PreviewBlockKind::heading)
        {
            headings.push_back(
                MarkdownHeading{block, unique_slug(occurrences, base_slug(each.text))});
        }
        ++block;
    }
    return headings;
}

std::optional<std::size_t> heading_block_of(std::span<const MarkdownHeading> headings,
                                            std::string_view fragment)
{
    const std::string decoded = percent_decoded(fragment);
    const auto found =
        std::ranges::find_if(headings, [&](const MarkdownHeading &heading)
                             { return ascii_equal_ignoring_case(heading.slug, decoded); });
    if (found == headings.end())
    {
        return std::nullopt;
    }
    return found->block;
}
} // namespace nenenib::core
