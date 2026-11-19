#include "HeadingSlug.hpp"

#include "AsciiCase.hpp"
#include "HeadingSlugTable.hpp"
#include "HeadingSource.hpp"
#include "Offset.hpp"
#include "PercentDecoding.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
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

using CodePointRange = std::pair<char32_t, char32_t>;

// 生成物の表が昇順で重ならず隣り合わないこと（二分探索の前提・eng/heading-slug-table.py
// が併せる）。
[[nodiscard]] consteval bool ascending_apart() noexcept
{
    char32_t previous_end = 0;
    bool first = true;
    for (const CodePointRange &range : heading_slug_removed_ranges)
    {
        if (range.second < range.first || (!first && range.first <= previous_end + 1U))
        {
            return false;
        }
        previous_end = range.second;
        first = false;
    }
    return true;
}
static_assert(ascending_apart(), "the removed ranges ascend, do not overlap and do not touch");

// github-slugger の regex.js が消す code point か（表は生成物の 1 つ）。
[[nodiscard]] bool removed_code_point(char32_t value) noexcept
{
    const auto after =
        std::ranges::upper_bound(heading_slug_removed_ranges, value, {}, &CodePointRange::first);
    return after != heading_slug_removed_ranges.begin() && value <= std::prev(after)->second;
}

// 1 つの code point text[at, next) を名札に写す（github-slugger の slug() の 3 つ: ASCII の大文字を
// 畳む・表の字を消す・空白を `-` に）。
void append_slugged(std::string &slug, std::string_view text, Offset at, Offset next)
{
    const char first = text.at(at.value);
    if (static_cast<unsigned char>(first) < 0x80U)
    {
        const unsigned char folded = ascii_folded(first);
        if (folded == ' ')
        {
            slug.push_back('-');
        }
        else if (!removed_code_point(folded))
        {
            slug.push_back(static_cast<char>(folded));
        }
        return;
    }
    if (!removed_code_point(code_point_at(text, at)))
    {
        slug.append(text.substr(at.value, next.value - at.value));
    }
}

// 重複を数える前の名札。text は名札に数える見出しの文字（正しい UTF-8）。
[[nodiscard]] std::string base_slug(std::string_view text)
{
    std::string slug;
    for (Offset at{0}; at.value < text.size();)
    {
        const Offset next = next_code_point(text, at);
        append_slugged(slug, text, at, next);
        at = next;
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

std::vector<MarkdownHeading> heading_slugs(std::span<const HeadingSource> sources)
{
    std::vector<MarkdownHeading> headings;
    headings.reserve(sources.size());
    Occurrences occurrences;
    for (const HeadingSource &source : sources)
    {
        headings.push_back(
            MarkdownHeading{source.block, unique_slug(occurrences, base_slug(source.text))});
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
