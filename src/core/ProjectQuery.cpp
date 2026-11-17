#include "ProjectQuery.hpp"

#include "AsciiCase.hpp"
#include "LiteralPattern.hpp"
#include "VimPatternCase.hpp"
#include "VimSearchDirection.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr char ascii_upper(char byte) noexcept
{
    return byte >= 'a' && byte <= 'z' ? static_cast<char>(byte - 'a' + 'A') : byte;
}

// from 以降で heads のどれかのバイトが最初に現れる位置（無ければ text.size()）。char_traits の
// 検索は memchr を core の外へ出すので、許可シンボルに載っている std の検索で引く（ARC-003）。
[[nodiscard]] std::size_t first_of(std::string_view text, std::size_t from, std::string_view heads)
{
    const std::string_view rest = text.substr(std::min(from, text.size()));
    return text.size() - rest.size() +
           static_cast<std::size_t>(std::ranges::find_first_of(rest, heads) - rest.begin());
}

// text の from 以降で、ASCII を小文字に畳んだ needle と ASCII を畳んで等しい最初の位置。
[[nodiscard]] std::optional<std::size_t> folded_found(std::string_view text, std::size_t from,
                                                      std::string_view needle)
{
    const std::array<char, 2> heads{needle.front(), ascii_upper(needle.front())};
    const std::string_view head{heads.data(), heads.front() == heads.back() ? 1U : 2U};
    for (std::size_t at = first_of(text, from, head); at < text.size();
         at = first_of(text, at + 1, head))
    {
        if (text.size() - at >= needle.size() &&
            std::ranges::equal(text.substr(at, needle.size()), needle, {}, ascii_folded,
                               ascii_folded))
        {
            return at;
        }
    }
    return std::nullopt;
}

// 絞り込みのバイト列が from 以降に最初に現れる位置。
[[nodiscard]] std::optional<std::size_t> needle_found(std::string_view text, std::size_t from,
                                                      const VimPatternNeedle &needle)
{
    if (needle.ascii_folded)
    {
        return folded_found(text, from, needle.bytes);
    }
    const std::string_view rest = text.substr(from);
    const auto hit = std::ranges::search(rest, needle.bytes);
    if (hit.empty())
    {
        return std::nullopt;
    }
    return from + static_cast<std::size_t>(hit.begin() - rest.begin());
}

[[nodiscard]] std::expected<std::optional<VimPattern>, VimPatternFailure>
pattern_of(std::string_view input)
{
    const bool slashed = input.starts_with('/');
    const std::string_view source = slashed ? input.substr(1) : input;
    if (source.empty())
    {
        return std::optional<VimPattern>{};
    }
    auto parsed =
        slashed ? VimPattern::parse(source, VimSearchDirection::forward, VimPatternCase::sensitive)
                : VimPattern::parse(literal_pattern(source, VimSearchDirection::forward),
                                    VimSearchDirection::forward, VimPatternCase::ignore);
    if (!parsed.has_value())
    {
        return std::unexpected(parsed.error());
    }
    return std::optional<VimPattern>{std::move(parsed).value()};
}
} // namespace

ProjectQuery::ProjectQuery(std::optional<VimPattern> pattern)
    : pattern_(std::move(pattern)),
      needle_(pattern_.has_value() ? pattern_.value().required_bytes() : std::nullopt)
{
}

std::expected<ProjectQuery, VimPatternFailure> ProjectQuery::parse(std::string_view input)
{
    auto pattern = pattern_of(input);
    if (!pattern.has_value())
    {
        return std::unexpected(pattern.error());
    }
    return ProjectQuery(std::move(pattern).value());
}

bool ProjectQuery::empty() const noexcept
{
    return !pattern_.has_value();
}

std::optional<VimPatternMatch> ProjectQuery::first_in_line(std::string_view line) const
{
    if (!pattern_.has_value())
    {
        return std::nullopt;
    }
    return pattern_.value().matched(line, 0);
}

std::optional<std::size_t> ProjectQuery::candidate(std::string_view text, std::size_t from) const
{
    if (from >= text.size() || !pattern_.has_value())
    {
        return std::nullopt;
    }
    if (!needle_.has_value())
    {
        return from;
    }
    return needle_found(text, from, needle_.value());
}
} // namespace nenenib::core
