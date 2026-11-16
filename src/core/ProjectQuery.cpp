#include "ProjectQuery.hpp"

#include "Utf8.hpp"
#include "VimPatternCase.hpp"
#include "VimSearchDirection.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] constexpr char ascii_lower(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

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

[[nodiscard]] bool has_ascii_letter(std::string_view text) noexcept
{
    return std::ranges::any_of(text,
                               [](char byte) { return ascii_lower(byte) != ascii_upper(byte); });
}
} // namespace

ProjectQuery::ProjectQuery(std::string needle, bool folds, std::optional<VimPattern> pattern)
    : needle_(std::move(needle)), folds_(folds), pattern_(std::move(pattern))
{
}

std::expected<ProjectQuery, VimPatternFailure> ProjectQuery::parse(std::string_view input)
{
    if (!input.starts_with('/'))
    {
        if (!validate_utf8(input).has_value())
        {
            return std::unexpected(VimPatternFailure::invalid_utf8);
        }
        std::string folded(input);
        std::ranges::transform(folded, folded.begin(), ascii_lower);
        const bool folds = has_ascii_letter(folded);
        return ProjectQuery(std::move(folded), folds, std::nullopt);
    }
    const std::string_view source = input.substr(1);
    if (source.empty())
    {
        return ProjectQuery({}, false, std::nullopt);
    }
    auto pattern =
        VimPattern::parse(source, VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!pattern.has_value())
    {
        return std::unexpected(pattern.error());
    }
    std::string bytes = pattern.value().literal_bytes().value_or(std::string{});
    return ProjectQuery(std::move(bytes), false, std::move(pattern).value());
}

bool ProjectQuery::empty() const noexcept
{
    return !pattern_.has_value() && needle_.empty();
}

std::optional<std::size_t> ProjectQuery::found(std::string_view text, std::size_t from) const
{
    if (!folds_)
    {
        const std::string_view rest = text.substr(from);
        const auto hit = std::ranges::search(rest, needle_);
        if (hit.empty())
        {
            return std::nullopt;
        }
        return from + static_cast<std::size_t>(hit.begin() - rest.begin());
    }
    // 先頭のバイトの大小 2 通りを探し、残りを畳んで比べる。
    const std::string_view rest = std::string_view{needle_}.substr(1);
    const std::array<char, 2> heads{needle_.front(), ascii_upper(needle_.front())};
    const std::string_view head{heads.data(), heads.front() == heads.back() ? 1U : 2U};
    for (std::size_t at = first_of(text, from, head); at < text.size();
         at = first_of(text, at + 1, head))
    {
        if (text.size() - at >= needle_.size() &&
            std::ranges::equal(text.substr(at + 1, rest.size()), rest, {}, ascii_lower))
        {
            return at;
        }
    }
    return std::nullopt;
}

std::optional<VimPatternMatch> ProjectQuery::first_in_line(std::string_view line) const
{
    if (pattern_.has_value())
    {
        return pattern_.value().matched(line, 0);
    }
    if (needle_.empty())
    {
        return std::nullopt;
    }
    const auto at = found(line, 0);
    if (!at.has_value())
    {
        return std::nullopt;
    }
    const std::size_t end = at.value() + needle_.size();
    return VimPatternMatch{at.value(), end, at.value(), end};
}

std::optional<std::size_t> ProjectQuery::candidate(std::string_view text, std::size_t from) const
{
    if (from >= text.size())
    {
        return std::nullopt;
    }
    if (pattern_.has_value() && needle_.empty())
    {
        return from;
    }
    if (needle_.empty())
    {
        return std::nullopt;
    }
    return found(text, from);
}
} // namespace nenenib::core
