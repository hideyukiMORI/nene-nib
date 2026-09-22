#include "VimPattern.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimPatternAtomKind.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <utility>

namespace nenenib::core
{
namespace
{
// vim_character_class の記号の組（0 は空白・1 は記号）。語の文字はこれより大きい値になる。
constexpr std::uint32_t symbol_group = 1;

// 未対応の `\` の組（ADR 0032 の決定 4）。表に無い英数字も未対応で、記号は文字そのものになる。
constexpr std::array<char32_t, 2> group_escapes{U'(', U')'};
constexpr std::array<char32_t, 5> quantifier_escapes{U'{', U'}', U'+', U'=', U'?'};
constexpr std::array<char32_t, 4> magic_escapes{U'v', U'V', U'm', U'M'};
constexpr std::array<char32_t, 2> case_escapes{U'c', U'C'};
constexpr std::array<char32_t, 5> other_escapes{U'%', U'_', U'@', U'&', U'z'};

// `\d \D \w \W \s \S` の文字集合。表は 1 つで、大文字は否定だけが違う。
constexpr std::array<VimPatternRange, 1> digit_ranges{{{U'0', U'9'}}};
constexpr std::array<VimPatternRange, 4> word_ranges{
    {{U'0', U'9'}, {U'A', U'Z'}, {U'a', U'z'}, {U'_', U'_'}}};
constexpr std::array<VimPatternRange, 2> blank_ranges{{{U' ', U' '}, {U'\t', U'\t'}}};

[[nodiscard]] bool contains(std::span<const char32_t> set, char32_t code) noexcept
{
    for (const char32_t entry : set)
    {
        if (entry == code)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ascii_alphanumeric(char32_t code) noexcept
{
    return (code >= U'0' && code <= U'9') || (code >= U'A' && code <= U'Z') ||
           (code >= U'a' && code <= U'z');
}

[[nodiscard]] bool zero_width(VimPatternAtomKind kind) noexcept
{
    switch (kind)
    {
    case VimPatternAtomKind::literal:
    case VimPatternAtomKind::any:
    case VimPatternAtomKind::set:
        return false;
    case VimPatternAtomKind::line_start:
    case VimPatternAtomKind::line_end:
    case VimPatternAtomKind::word_start:
    case VimPatternAtomKind::word_end:
        return true;
    }
    std::unreachable();
}

[[nodiscard]] VimPatternAtom atom_of(VimPatternAtomKind kind) noexcept
{
    return VimPatternAtom{kind, 0, false, false, 0, 0};
}

[[nodiscard]] VimPatternAtom literal_atom(char32_t code) noexcept
{
    return VimPatternAtom{VimPatternAtomKind::literal, code, false, false, 0, 0};
}

[[nodiscard]] std::uint32_t class_at(std::string_view line, std::size_t at)
{
    return vim_character_class(code_point_at(line, Offset{at}), VimWordClass::word);
}

// `\<`。語の文字で、前が無いか種類が変わるところ。種類の表は 1 つ（ADR 0031 の決定 2）。
[[nodiscard]] bool starts_a_word(std::string_view line, std::size_t at)
{
    if (at >= line.size())
    {
        return false;
    }
    const std::uint32_t here = class_at(line, at);
    if (here <= symbol_group)
    {
        return false;
    }
    return at == 0 || class_at(line, previous_code_point(line, Offset{at}).value) != here;
}

// `\>`。直前が語の文字で、後ろが無いか種類が変わるところ。
[[nodiscard]] bool ends_a_word(std::string_view line, std::size_t at)
{
    if (at == 0)
    {
        return false;
    }
    const std::uint32_t before = class_at(line, previous_code_point(line, Offset{at}).value);
    if (before <= symbol_group)
    {
        return false;
    }
    return at >= line.size() || class_at(line, at) != before;
}

[[nodiscard]] bool anchored(std::string_view line, const VimPatternAtom &atom, std::size_t at)
{
    switch (atom.kind)
    {
    case VimPatternAtomKind::line_start:
        return at == 0;
    case VimPatternAtomKind::line_end:
        return at >= line.size();
    case VimPatternAtomKind::word_start:
        return starts_a_word(line, at);
    case VimPatternAtomKind::word_end:
        return ends_a_word(line, at);
    case VimPatternAtomKind::literal:
    case VimPatternAtomKind::any:
    case VimPatternAtomKind::set:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<VimPatternFailure> escape_failure(char32_t code) noexcept
{
    if (contains(group_escapes, code))
    {
        return VimPatternFailure::group;
    }
    if (code == U'|')
    {
        return VimPatternFailure::branch;
    }
    if (contains(quantifier_escapes, code))
    {
        return VimPatternFailure::quantifier;
    }
    if (contains(magic_escapes, code))
    {
        return VimPatternFailure::magic;
    }
    if (contains(case_escapes, code))
    {
        return VimPatternFailure::ignore_case;
    }
    if (contains(other_escapes, code) || ascii_alphanumeric(code))
    {
        return VimPatternFailure::escape;
    }
    return std::nullopt;
}

// `\d \D \w \W \s \S`。足したら真を返す。大文字は同じ区間の否定である。
[[nodiscard]] bool appended_escaped_set(char32_t code, std::vector<VimPatternAtom> &atoms,
                                        std::vector<VimPatternRange> &ranges)
{
    const bool negated = code == U'D' || code == U'W' || code == U'S';
    const char32_t lowered = negated ? static_cast<char32_t>(code + (U'a' - U'A')) : code;
    std::span<const VimPatternRange> members;
    if (lowered == U'd')
    {
        members = digit_ranges;
    }
    else if (lowered == U'w')
    {
        members = word_ranges;
    }
    else if (lowered == U's')
    {
        members = blank_ranges;
    }
    else
    {
        return false;
    }
    const std::size_t first = ranges.size();
    ranges.insert(ranges.end(), members.begin(), members.end());
    atoms.push_back(
        VimPatternAtom{VimPatternAtomKind::set, 0, negated, false, first, members.size()});
    return true;
}

// 文字集合の成員を 1 つ足して、次の位置を返す。a-z は区間、それ以外は 1 文字。
[[nodiscard]] std::size_t parsed_member(std::string_view pattern, std::size_t at,
                                        std::vector<VimPatternRange> &ranges)
{
    const char32_t code = code_point_at(pattern, Offset{at});
    const std::size_t next = next_code_point(pattern, Offset{at}).value;
    const std::size_t after = next_code_point(pattern, Offset{next}).value;
    if (next < pattern.size() && code_point_at(pattern, Offset{next}) == U'-' &&
        after < pattern.size() && code_point_at(pattern, Offset{after}) != U']')
    {
        ranges.push_back(VimPatternRange{code, code_point_at(pattern, Offset{after})});
        return next_code_point(pattern, Offset{after}).value;
    }
    ranges.push_back(VimPatternRange{code, code});
    return next;
}

// 開き大括弧から始まる文字集合。閉じる大括弧が無ければ何も足さず nullopt（開きは文字そのもの）。
[[nodiscard]] std::optional<std::size_t> parsed_set(std::string_view pattern, std::size_t at,
                                                    std::vector<VimPatternAtom> &atoms,
                                                    std::vector<VimPatternRange> &ranges)
{
    const std::size_t saved = ranges.size();
    std::size_t scan = next_code_point(pattern, Offset{at}).value;
    const bool negated = scan < pattern.size() && code_point_at(pattern, Offset{scan}) == U'^';
    if (negated)
    {
        scan = next_code_point(pattern, Offset{scan}).value;
    }
    const std::size_t first = scan;
    while (scan < pattern.size())
    {
        // 先頭の閉じ大括弧は成員そのもの（Vim も同じ）。集合を閉じるのは 2 つめ以降である。
        if (code_point_at(pattern, Offset{scan}) == U']' && scan != first)
        {
            atoms.push_back(VimPatternAtom{VimPatternAtomKind::set, 0, negated, false, saved,
                                           ranges.size() - saved});
            return next_code_point(pattern, Offset{scan}).value;
        }
        scan = parsed_member(pattern, scan, ranges);
    }
    ranges.resize(saved);
    return std::nullopt;
}

[[nodiscard]] std::expected<std::size_t, VimPatternFailure>
parsed_escape(std::string_view pattern, std::size_t at, std::vector<VimPatternAtom> &atoms,
              std::vector<VimPatternRange> &ranges)
{
    if (at >= pattern.size())
    {
        return std::unexpected(VimPatternFailure::escape);
    }
    const char32_t code = code_point_at(pattern, Offset{at});
    const std::size_t next = next_code_point(pattern, Offset{at}).value;
    if (appended_escaped_set(code, atoms, ranges))
    {
        return next;
    }
    if (code == U'<' || code == U'>')
    {
        atoms.push_back(
            atom_of(code == U'<' ? VimPatternAtomKind::word_start : VimPatternAtomKind::word_end));
        return next;
    }
    const auto failure = escape_failure(code);
    if (failure.has_value())
    {
        return std::unexpected(failure.value());
    }
    atoms.push_back(literal_atom(code));
    return next;
}

// 星を量指定子として読めるか。幅の無い原子と二重の星の後ろでは文字そのものになる。
[[nodiscard]] bool repeatable(const std::vector<VimPatternAtom> &atoms) noexcept
{
    return !atoms.empty() && !atoms.back().repeated && !zero_width(atoms.back().kind);
}

// `^` は先頭でだけ錨、`$` は末尾でだけ錨、ほかはただの文字（ADR 0032 の決定 4 の追記）。
[[nodiscard]] VimPatternAtom plain_atom(char32_t code, std::size_t at, std::size_t next,
                                        std::size_t size) noexcept
{
    if (code == U'.')
    {
        return atom_of(VimPatternAtomKind::any);
    }
    if (code == U'^' && at == 0)
    {
        return atom_of(VimPatternAtomKind::line_start);
    }
    if (code == U'$' && next == size)
    {
        return atom_of(VimPatternAtomKind::line_end);
    }
    return literal_atom(code);
}

[[nodiscard]] std::expected<std::size_t, VimPatternFailure>
parsed_item(std::string_view pattern, std::size_t at, std::vector<VimPatternAtom> &atoms,
            std::vector<VimPatternRange> &ranges)
{
    const char32_t code = code_point_at(pattern, Offset{at});
    const std::size_t next = next_code_point(pattern, Offset{at}).value;
    if (code == U'\\')
    {
        return parsed_escape(pattern, next, atoms, ranges);
    }
    if (code == U'~')
    {
        return std::unexpected(VimPatternFailure::previous_substitute);
    }
    if (code == U'*' && repeatable(atoms))
    {
        atoms.back().repeated = true;
        return next;
    }
    if (code == U'[')
    {
        const auto set = parsed_set(pattern, at, atoms, ranges);
        if (set.has_value())
        {
            return set.value();
        }
    }
    atoms.push_back(plain_atom(code, at, next, pattern.size()));
    return next;
}

[[nodiscard]] char32_t separator_of(VimSearchDirection direction) noexcept
{
    return direction == VimSearchDirection::forward ? U'/' : U'?';
}
} // namespace

VimPattern::VimPattern(std::vector<VimPatternAtom> atoms, std::vector<VimPatternRange> ranges)
    : atoms_(std::move(atoms)), ranges_(std::move(ranges))
{
}

std::expected<VimPattern, VimPatternFailure> VimPattern::parse(std::string_view pattern,
                                                               VimSearchDirection separator)
{
    std::vector<VimPatternAtom> atoms;
    std::vector<VimPatternRange> ranges;
    std::size_t at = 0;
    while (at < pattern.size())
    {
        if (code_point_at(pattern, Offset{at}) == separator_of(separator))
        {
            return std::unexpected(VimPatternFailure::offset);
        }
        const auto next = parsed_item(pattern, at, atoms, ranges);
        if (!next)
        {
            return std::unexpected(next.error());
        }
        at = next.value();
    }
    return VimPattern(std::move(atoms), std::move(ranges));
}

bool VimPattern::in_set(const VimPatternAtom &atom, char32_t code) const
{
    bool found = false;
    for (std::size_t index = 0; index < atom.range_count; ++index)
    {
        const VimPatternRange range = ranges_.at(atom.first_range + index);
        found = found || (code >= range.first && code <= range.last);
    }
    return found != atom.negated;
}

std::optional<std::size_t> VimPattern::stepped(std::string_view line, const VimPatternAtom &atom,
                                               std::size_t at) const
{
    if (at >= line.size())
    {
        return std::nullopt;
    }
    const char32_t code = code_point_at(line, Offset{at});
    const std::size_t next = next_code_point(line, Offset{at}).value;
    switch (atom.kind)
    {
    case VimPatternAtomKind::any:
        return next;
    case VimPatternAtomKind::literal:
        return code == atom.code ? std::optional<std::size_t>{next} : std::nullopt;
    case VimPatternAtomKind::set:
        return in_set(atom, code) ? std::optional<std::size_t>{next} : std::nullopt;
    // 幅の無い原子は 1 文字も食べない。
    case VimPatternAtomKind::line_start:
    case VimPatternAtomKind::line_end:
    case VimPatternAtomKind::word_start:
    case VimPatternAtomKind::word_end:
        return std::nullopt;
    }
    std::unreachable();
}

std::vector<std::size_t>
VimPattern::repeated_stops(std::string_view line, const VimPatternAtom &atom, std::size_t at) const
{
    std::vector<std::size_t> stops{at};
    std::size_t scan = at;
    for (auto next = stepped(line, atom, scan); next.has_value(); next = stepped(line, atom, scan))
    {
        scan = next.value();
        stops.push_back(scan);
    }
    return stops;
}

std::optional<std::size_t> VimPattern::matched_atoms(std::string_view line, std::size_t index,
                                                     std::size_t at) const
{
    if (index >= atoms_.size())
    {
        return at;
    }
    const VimPatternAtom atom = atoms_.at(index);
    if (zero_width(atom.kind))
    {
        return anchored(line, atom, at) ? matched_atoms(line, index + 1, at) : std::nullopt;
    }
    if (!atom.repeated)
    {
        const auto next = stepped(line, atom, at);
        return next.has_value() ? matched_atoms(line, index + 1, next.value()) : std::nullopt;
    }
    // 貪欲に伸ばしてから後戻りする（`/f.*e` が行頭から一致するのはこれ・実測）。
    const std::vector<std::size_t> stops = repeated_stops(line, atom, at);
    for (auto stop = stops.rbegin(); stop != stops.rend(); ++stop)
    {
        const auto end = matched_atoms(line, index + 1, *stop);
        if (end.has_value())
        {
            return end;
        }
    }
    return std::nullopt;
}

std::optional<VimPatternMatch> VimPattern::matched(std::string_view line, std::size_t from) const
{
    std::size_t at = from;
    while (at <= line.size())
    {
        const auto end = matched_atoms(line, 0, at);
        if (end.has_value())
        {
            return VimPatternMatch{at, end.value()};
        }
        if (at >= line.size())
        {
            return std::nullopt;
        }
        at = next_code_point(line, Offset{at}).value;
    }
    return std::nullopt;
}
} // namespace nenenib::core
