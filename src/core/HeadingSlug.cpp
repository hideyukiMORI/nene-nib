#include "HeadingSlug.hpp"

#include "AsciiCase.hpp"
#include "Offset.hpp"
#include "PercentDecoding.hpp"
#include "PreviewBlockKind.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
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

// 名札から消す非 ASCII の字の範囲（閉区間・設計席の決め 2026-11-19・表はこの 1 つ）。日本語の
// 文書で効く CJK と全角の句読点・記号と、絵文字だけを消す（GitHub の github-slugger は Unicode の
// 句読点と記号の全部を消すが、その一般カテゴリの表は持たない）。全角の英数字（U+FF10〜U+FF19・
// U+FF21〜U+FF3A・U+FF41〜U+FF5A）はこの表の隙間なので残る。
using CodePointRange = std::pair<char32_t, char32_t>;
constexpr std::array<CodePointRange, 10> removed_code_points{{
    {U'‐', U'‧'},                   // ダッシュ・引用符・…（一般句読点）
    {U'　', U'〿'},                  // CJK の記号と句読点（全角の空白・、。「」など）
    {U'！', U'／'},                 // 全角の ！〜／（（）を含む）
    {U'：', U'＠'},                 // 全角の ：〜＠
    {U'［', U'｀'},                 // 全角の ［〜｀
    {U'｛', U'･'},                  // 全角の ｛〜・と半角の句読点
    {U'‍', U'‍'},               // ZERO WIDTH JOINER（絵文字の結合）
    {U'☀', U'➿'},                  // その他の記号と装飾記号（絵文字）
    {U'️', U'️'},                     // VARIATION SELECTOR-16（絵文字の見せ方）
    {U'\U0001F000', U'\U0001FAFF'}, // 絵文字の面
}};

[[nodiscard]] bool removed_code_point(char32_t value) noexcept
{
    return std::ranges::any_of(removed_code_points, [value](const CodePointRange &range)
                               { return value >= range.first && value <= range.second; });
}

// 1 つの code point text[at, next) を名札に写す。
void append_slugged(std::string &slug, std::string_view text, Offset at, Offset next)
{
    const char first = text.at(at.value);
    if (static_cast<unsigned char>(first) >= 0x80U)
    {
        if (!removed_code_point(code_point_at(text, at)))
        {
            slug.append(text.substr(at.value, next.value - at.value));
        }
        return;
    }
    const unsigned char folded = ascii_folded(first);
    if (kept_ascii(folded))
    {
        slug.push_back(static_cast<char>(folded));
    }
    else if (ascii_space(first))
    {
        slug.push_back('-');
    }
}

// 重複を数える前の名札。text は解析の結果の表示の文字（正しい UTF-8）。
[[nodiscard]] std::string base_slug(std::string_view text)
{
    const auto first =
        static_cast<std::size_t>(std::ranges::find_if_not(text, ascii_space) - text.begin());
    const auto last = static_cast<std::size_t>(
        std::ranges::find_if_not(text.rbegin(), text.rend(), ascii_space).base() - text.begin());
    const std::string_view kept = text.substr(first, last > first ? last - first : 0U);
    std::string slug;
    for (Offset at{0}; at.value < kept.size();)
    {
        const Offset next = next_code_point(kept, at);
        append_slugged(slug, kept, at, next);
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
