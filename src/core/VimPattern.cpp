#include "VimPattern.hpp"

#include "DisplayWidth.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimCase.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimPatternAtomKind.hpp"
#include "VimPatternEvaluation.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// vim_character_class の記号の組（0 は空白・1 は記号）。語の文字はこれより大きい値になる。
constexpr std::uint32_t symbol_group = 1;

// 食べる原子 1 つが食べる最長のバイト数（UTF-8 の 1 code point）。
constexpr std::size_t longest_code_point = 4;

// 未対応の `\` の組（ADR 0032 の決定 4）。表に無い英数字も未対応で、記号は文字そのものになる。
constexpr std::array<char32_t, 2> group_escapes{U'(', U')'};
constexpr std::array<char32_t, 5> quantifier_escapes{U'{', U'}', U'+', U'=', U'?'};
constexpr std::array<char32_t, 4> magic_escapes{U'v', U'V', U'm', U'M'};
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
    return VimPatternAtom{kind, 0, false, false, 0, 0, false, false};
}

[[nodiscard]] VimPatternAtom literal_atom(char32_t code) noexcept
{
    return VimPatternAtom{VimPatternAtomKind::literal, code, false, false, 0, 0, false, false};
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
    atoms.push_back(VimPatternAtom{VimPatternAtomKind::set, 0, negated, false, first,
                                   members.size(), false, false});
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
                                           ranges.size() - saved, true, false});
            return next_code_point(pattern, Offset{scan}).value;
        }
        scan = parsed_member(pattern, scan, ranges);
    }
    ranges.resize(saved);
    return std::nullopt;
}

[[nodiscard]] bool composing_literal(const VimPatternAtom &atom) noexcept
{
    return atom.kind == VimPatternAtomKind::literal &&
           display_width(atom.code) == DisplayWidth::zero;
}

// 結合文字（幅 0・ADR 0053 と同じ表）と、その直前の繰り返しの無い literal を畳まない印にする
// （#1147・Vim 9.1 の NFA_COMPOSING は `mc == sta->c` で比べる）。`[e]` の集合は別の原子なので
// 畳む（`/[e]` + U+0301 は `E` + U+0301 に当たる・実測）。`e*` + 結合文字の `e` も別の原子として
// 畳むのは Vim の nfa_regatom の読みで、Vim 9.1 の実測（`:set ic` で E486）とは合わない残差
// （ADR 0032 追補 7）。
void marked_exact(std::vector<VimPatternAtom> &atoms) noexcept
{
    for (std::size_t index = 0; index < atoms.size(); ++index)
    {
        const bool before_mark =
            index + 1 < atoms.size() && composing_literal(atoms.at(index + 1)) &&
            atoms.at(index).kind == VimPatternAtomKind::literal && !atoms.at(index).repeated;
        atoms.at(index).exact = before_mark || composing_literal(atoms.at(index));
    }
}

// `\c` と `\C`（#1041）。原子を足さず、パターン全体の大小を決める。`\c` は `\C` より強い
// （Vim の regcomp は RF_ICASE を先に見る・`/\C\cabc` は大小を畳む・実測）。
void forced_case(char32_t code, std::optional<VimPatternCase> &forced) noexcept
{
    if (code == U'c')
    {
        forced = VimPatternCase::ignore;
        return;
    }
    if (forced != VimPatternCase::ignore)
    {
        forced = VimPatternCase::sensitive;
    }
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

// `smartcase` の「大文字がある」（Vim の pat_has_uppercase）。`\` とそれに続く 1 文字は数えない
// （`\S` `\W` `\C` は大文字ではない）。`[B]` の中の文字は数える（実測）。大文字は小文字の写像を
// 持つ文字（VimCase の表の 1 つ）。
[[nodiscard]] bool has_uppercase(std::string_view pattern)
{
    std::size_t at = 0;
    while (at < pattern.size())
    {
        const char32_t code = code_point_at(pattern, Offset{at});
        at = next_code_point(pattern, Offset{at}).value;
        if (code == U'\\')
        {
            at = at < pattern.size() ? next_code_point(pattern, Offset{at}).value : at;
            continue;
        }
        if (vim_to_lower(code) != code)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool ignores_case(VimPatternCase letter_case, std::optional<VimPatternCase> forced,
                                std::string_view pattern)
{
    switch (forced.value_or(letter_case))
    {
    case VimPatternCase::sensitive:
        return false;
    case VimPatternCase::ignore:
        return true;
    case VimPatternCase::smart:
        return !has_uppercase(pattern);
    }
    std::unreachable();
}

// 大小を畳むとバイト列の一致で絞れない文字（英字と ASCII の外）。
[[nodiscard]] bool cased_byte(char32_t code) noexcept
{
    return code >= 0x80 || (code >= U'A' && code <= U'Z') || (code >= U'a' && code <= U'z');
}

// 項目の始まりの `\c` か `\C`（#1041）。原子を足さない。
[[nodiscard]] bool case_escape(std::string_view pattern, std::size_t at) noexcept
{
    return at + 1 < pattern.size() && pattern[at] == '\\' &&
           (pattern[at + 1] == 'c' || pattern[at + 1] == 'C');
}

[[nodiscard]] char32_t separator_of(VimSearchDirection direction) noexcept
{
    return direction == VimSearchDirection::forward ? U'/' : U'?';
}

// `\` の付いた区切り（Vim の skip_regexp が文字そのものにする・`?` の検索の `\?` も・Issue #961）。
[[nodiscard]] bool escaped_separator(std::string_view pattern, std::size_t at,
                                     VimSearchDirection separator)
{
    const std::size_t next = next_code_point(pattern, Offset{at}).value;
    return code_point_at(pattern, Offset{at}) == U'\\' && next < pattern.size() &&
           code_point_at(pattern, Offset{next}) == separator_of(separator);
}
} // namespace

VimPattern::VimPattern(std::vector<VimPatternAtom> atoms, std::vector<VimPatternRange> ranges,
                       bool ignore_case)
    : atoms_(std::move(atoms)), ranges_(std::move(ranges)), ignore_case_(ignore_case)
{
}

bool VimPattern::is_zero_width(VimPatternAtomKind kind) noexcept
{
    return zero_width(kind);
}

std::optional<std::size_t> VimPattern::longest_bytes() const noexcept
{
    std::size_t bytes = 0;
    for (const VimPatternAtom &atom : atoms_)
    {
        if (atom.repeated)
        {
            return std::nullopt;
        }
        bytes += zero_width(atom.kind) ? 0 : longest_code_point;
    }
    return bytes;
}

std::optional<std::string> VimPattern::literal_bytes() const
{
    if (atoms_.empty())
    {
        return std::nullopt;
    }
    std::string bytes;
    for (const VimPatternAtom &atom : atoms_)
    {
        if (atom.kind != VimPatternAtomKind::literal || atom.repeated ||
            (ignore_case_ && cased_byte(atom.code)))
        {
            return std::nullopt;
        }
        append_utf8(bytes, atom.code);
    }
    return bytes;
}

bool VimPattern::anchored(std::string_view line, const VimPatternAtom &atom, std::size_t at)
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

std::expected<VimPattern, VimPatternFailure> VimPattern::parse(std::string_view pattern,
                                                               VimSearchDirection separator,
                                                               VimPatternCase letter_case)
{
    // 歩き（code_point_at）は検証済みの UTF-8 が前提なので、入口で確かめる（#906）。
    if (!validate_utf8(pattern).has_value())
    {
        return std::unexpected(VimPatternFailure::invalid_utf8);
    }
    std::vector<VimPatternAtom> atoms;
    std::vector<VimPatternRange> ranges;
    std::optional<VimPatternCase> forced;
    std::size_t at = 0;
    while (at < pattern.size())
    {
        if (code_point_at(pattern, Offset{at}) == separator_of(separator))
        {
            return std::unexpected(VimPatternFailure::offset);
        }
        if (escaped_separator(pattern, at, separator))
        {
            atoms.push_back(literal_atom(separator_of(separator)));
            at += 2;
            continue;
        }
        if (case_escape(pattern, at))
        {
            forced_case(code_point_at(pattern, Offset{at + 1}), forced);
            at += 2;
            continue;
        }
        const auto next = parsed_item(pattern, at, atoms, ranges);
        if (!next)
        {
            return std::unexpected(next.error());
        }
        at = next.value();
    }
    marked_exact(atoms);
    return VimPattern(std::move(atoms), std::move(ranges),
                      ignores_case(letter_case, forced, pattern));
}

// 一致の終わりが文字の途中（本文の側にだけ結合文字が続く）なら一致にしない。Vim の NFA の
// NFA_MATCH と同じく終わりだけを見るので、`/e.` は `.` が結合文字を食べて当たる（#1138）。
bool VimPattern::ends_a_character(std::string_view line, std::size_t end) noexcept
{
    // ASCII のバイトは結合文字にならないので、表を引くのは ASCII の外の側だけ。
    constexpr unsigned char ascii_limit = 0x80;
    return end >= line.size() || static_cast<unsigned char>(line[end]) < ascii_limit ||
           !vim_character_continues(line, Offset{end});
}

bool VimPattern::in_ranges(const VimPatternAtom &atom, char32_t code) const
{
    bool found = false;
    for (std::size_t index = 0; index < atom.range_count; ++index)
    {
        const VimPatternRange range = ranges_.at(atom.first_range + index);
        found = found || (code >= range.first && code <= range.last);
    }
    return found;
}

// 大小を畳む `[...]` は、文字か畳んだ形かその大文字が区間に入れば成員（`[^a]` は `A` を外す・
// 実測）。Vim は区間の各文字を畳んで比べるので、区間の側にだけ別の綴りがある組（`[ς]` と `σ`）は
// 当たらない（区間を歩かない・#1041 の限界）。
bool VimPattern::in_set(const VimPatternAtom &atom, char32_t code) const
{
    bool found = in_ranges(atom, code);
    if (!found && ignore_case_ && atom.bracketed)
    {
        const char32_t folded = vim_folded(code);
        found = in_ranges(atom, folded) || in_ranges(atom, vim_to_upper(folded));
    }
    return found != atom.negated;
}

bool VimPattern::consumes(const VimPatternAtom &atom, char32_t code) const
{
    switch (atom.kind)
    {
    case VimPatternAtomKind::any:
        return true;
    case VimPatternAtomKind::literal:
        return code == atom.code ||
               (ignore_case_ && !atom.exact && vim_folded(code) == vim_folded(atom.code));
    case VimPatternAtomKind::set:
        return in_set(atom, code);
    // 幅の無い原子は 1 文字も食べない。
    case VimPatternAtomKind::line_start:
    case VimPatternAtomKind::line_end:
    case VimPatternAtomKind::word_start:
    case VimPatternAtomKind::word_end:
        return false;
    }
    std::unreachable();
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
    return consumes(atom, code) ? std::optional<std::size_t>{next} : std::nullopt;
}

std::optional<std::size_t> VimPattern::matched_sequence(std::string_view line, std::size_t at) const
{
    for (const VimPatternAtom &atom : atoms_)
    {
        const auto next =
            is_zero_width(atom.kind)
                ? (anchored(line, atom, at) ? std::optional<std::size_t>{at} : std::nullopt)
                : stepped(line, atom, at);
        if (!next.has_value())
        {
            return std::nullopt;
        }
        at = next.value();
    }
    return at;
}

std::optional<VimPatternMatch> VimPattern::matched(std::string_view line, std::size_t from) const
{
    if (from > line.size())
    {
        return std::nullopt;
    }
    if (std::ranges::any_of(atoms_, [](const VimPatternAtom &atom) { return atom.repeated; }))
    {
        VimPatternEvaluation evaluation(*this, line);
        return evaluation.matched(from);
    }
    std::size_t at = from;
    while (at <= line.size())
    {
        const auto end = matched_sequence(line, at);
        if (end.has_value() && ends_a_character(line, end.value()))
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
