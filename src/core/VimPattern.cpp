#include "VimPattern.hpp"

#include "DisplayWidth.hpp"
#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimCase.hpp"
#include "VimCharacterBoundary.hpp"
#include "VimPatternAtomKind.hpp"
#include "VimPatternCapture.hpp"
#include "VimPatternCompiler.hpp"
#include "VimPatternEvaluation.hpp"
#include "VimPatternGroup.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternMachine.hpp"
#include "VimPatternMark.hpp"
#include "VimPatternMarkKind.hpp"
#include "VimPatternOpcode.hpp"
#include "VimPatternParse.hpp"
#include "VimPatternQuantity.hpp"
#include "VimPatternRepeat.hpp"
#include "VimPatternRoute.hpp"
#include "VimPatternTrace.hpp"
#include "VimWordClass.hpp"
#include "VimWordMotion.hpp"
#include "VirtualColumn.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
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
// 量 `\{` `\+` `\=` `\?` は parse が先に読むので、ここに来るのは対の無い `\}` だけ。
constexpr std::array<char32_t, 2> group_escapes{U'(', U')'};
constexpr std::array<char32_t, 1> quantifier_escapes{U'}'};
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
    case VimPatternAtomKind::composing:
    case VimPatternAtomKind::cluster:
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
    return VimPatternAtom{kind, 0, false, VimPatternRepeat::once, 0, 0, false, false};
}

[[nodiscard]] VimPatternAtom literal_atom(char32_t code) noexcept
{
    return VimPatternAtom{
        VimPatternAtomKind::literal, code, false, VimPatternRepeat::once, 0, 0, false, false};
}

[[nodiscard]] std::uint32_t class_at(std::string_view line, std::size_t at)
{
    return vim_character_class(code_point_at(line, Offset{at}), VimWordClass::word);
}

// 手前の Vim の 1 文字（ADR 0053）の先頭の種類。Vim の reg_prev_class は mb_head_off で結合文字を
// 越えて基底の文字を見る（`-` + U+0301 の後ろの `a` は語の始まり・#1146 の実測）。
[[nodiscard]] std::uint32_t previous_class(std::string_view line, std::size_t at)
{
    return class_at(line, vim_character_start(line, Offset{at}).value);
}

// `\<`。語の文字で、前が無いか種類が変わるところ。種類の表は 1 つ（ADR 0031 の決定 2）。
// 前は Vim の 1 文字の頭で見る（reg_prev_class の mb_head_off・`⚠️a` の a は語の頭・#985）。
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
    return at == 0 || previous_class(line, at) != here;
}

// `\>`。直前が語の文字で、後ろが無いか種類が変わるところ。
[[nodiscard]] bool ends_a_word(std::string_view line, std::size_t at)
{
    if (at == 0)
    {
        return false;
    }
    const std::uint32_t before = previous_class(line, at);
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
    atoms.push_back(VimPatternAtom{VimPatternAtomKind::set, 0, negated, VimPatternRepeat::once,
                                   first, members.size(), false, false});
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
            atoms.push_back(VimPatternAtom{VimPatternAtomKind::set, 0, negated,
                                           VimPatternRepeat::once, saved, ranges.size() - saved,
                                           true, false});
            // 閉じ大括弧の直後の結合文字は閉じ大括弧と 1 文字として読み飛ばす（Vim の
            // nfa_regatom の `MB_PTR_ADV(regparse)` は utfc_ptr2len で進む・`/[e]` + U+0301 は
            // `e` にも `é` にも（`ic` なら `É` にも）当たる・#1146 の実測）。集合は 1 文字の残りの
            // 結合文字まで食べるので、後ろに比べる原子は残らない。
            return vim_character_end(pattern, Offset{scan}).value;
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
// （#1147・Vim 9.1 の NFA_COMPOSING は `mc == sta->c` で比べる）。`[e]` の集合は直後の結合文字を
// 読み飛ばして畳む（`/[e]` + U+0301 は `E` + U+0301 に当たる・parsed_set・実測）。`e*` +
// 結合文字の `e` も別の原子として畳むのは Vim の nfa_regatom の読みで、Vim 9.1 の実測
// （`:set ic` で E486）とは合わない残差（ADR 0032 追補 7）。
void marked_exact(std::vector<VimPatternAtom> &atoms) noexcept
{
    for (std::size_t index = 0; index < atoms.size(); ++index)
    {
        const bool before_mark = index + 1 < atoms.size() &&
                                 composing_literal(atoms.at(index + 1)) &&
                                 atoms.at(index).kind == VimPatternAtomKind::literal &&
                                 atoms.at(index).repeat == VimPatternRepeat::once;
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

// 星を量指定子として読めるか。幅の無い原子の後ろでは文字そのものになる（量の後ろの星は E871 で
// parsed_item が先に断る）。
[[nodiscard]] bool repeatable(const std::vector<VimPatternAtom> &atoms) noexcept
{
    return !atoms.empty() && atoms.back().repeat == VimPatternRepeat::once &&
           !zero_width(atoms.back().kind);
}

// パターンの結合文字が直前の literal（基底か、基底に続く結合文字）の続きか。Vim の nfa_regatom は
// 基底の文字と直後の結合文字の列を 1 つの原子にする（その形は今どおり literal の列で比べる）。
// 直前の項目が量（after_multi・`\{0}` も）なら続きにしない（量の写しの最後の原子は基底でない）。
[[nodiscard]] bool follows_base(const std::vector<VimPatternAtom> &atoms, bool after_multi) noexcept
{
    return !after_multi && !atoms.empty() && atoms.back().kind == VimPatternAtomKind::literal &&
           atoms.back().repeat == VimPatternRepeat::once;
}

// 基底の続きでない結合文字（#1506）。直前が結合文字だけの原子ならその列に足し、直前が `.` なら
// `.` を消して結合文字だけの原子にする（Vim の nfa_regatom の `c == Magic('.') &&
// utf_iscomposing(peekchr())`）。ほか（先頭・量と幅の無い原子の後ろ）は新しい原子を始める。
void appended_mark(char32_t code, bool after_multi, std::vector<VimPatternAtom> &atoms,
                   std::vector<VimPatternRange> &ranges)
{
    const bool open =
        !after_multi && !atoms.empty() && atoms.back().repeat == VimPatternRepeat::once;
    const bool continued = open && atoms.back().kind == VimPatternAtomKind::composing;
    if (open && atoms.back().kind == VimPatternAtomKind::any)
    {
        atoms.pop_back();
    }
    if (!continued)
    {
        atoms.push_back(VimPatternAtom{VimPatternAtomKind::composing, 0, false,
                                       VimPatternRepeat::once, ranges.size(), 0, false, false});
    }
    ranges.push_back(VimPatternRange{code, code});
    ++atoms.back().range_count;
}

// 群の閉じ `\)` と選択 `\|`。
constexpr std::string_view group_close = "\\)";
constexpr std::string_view branch_bar = "\\|";

// `^` は先頭でだけ錨、`$` は末尾でだけ錨、ほかはただの文字（ADR 0032 の決定 4 の追記）。size は
// `$` が錨になる位置（パターンの末尾か、`\)` の手前・ADR 0168 の決定 4）。
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

// 星。量として読めば次の位置、文字なら値なし。量の後ろの星は E871、`\{0}` の後ろの星は E866
// （Vim の nfa_regpiece は `\{0}` で早く戻るので、星が原子の位置で読まれる・ADR 0184 の決定 2）。
[[nodiscard]] std::optional<std::expected<std::size_t, VimPatternFailure>>
starred(char32_t code, std::size_t next, VimPatternParse &parse)
{
    if (code != U'*')
    {
        return std::nullopt;
    }
    if (parse.after_multi)
    {
        return std::unexpected(VimPatternFailure::multi_after_multi);
    }
    if (parse.after_emptied)
    {
        return std::unexpected(VimPatternFailure::misplaced);
    }
    // 群の閉じの直後の星は群に付く（ADR 0184 の決定 3）。
    if (parse.after_close)
    {
        parse.marks.back().quantity = VimPatternQuantity{0, std::nullopt, false};
        parse.multi = true;
        return next;
    }
    if (!repeatable(parse.atoms))
    {
        return std::nullopt;
    }
    parse.atoms.back().repeat = VimPatternRepeat::star;
    parse.multi = true;
    return next;
}

// 直前の項目が選択 `\|` か番号の無い群の端か（ADR 0184 の決定 3）。その後ろの結合文字は前の
// 原子の続きにせず、結合文字だけの原子を始める（Vim の nfa_regatom は枝と群の始まりで新しい原子を
// 読む）。番号のある群の端は今どおり透す（決定 1）。
[[nodiscard]] bool fenced(const VimPatternParse &parse) noexcept
{
    if (parse.marks.empty() || parse.marks.back().at != parse.atoms.size())
    {
        return false;
    }
    return parse.marks.back().kind == VimPatternMarkKind::branch ||
           !parse.marks.back().capture.has_value();
}

// 基底の続きでない結合文字なら結合文字だけの原子に足して真（fresh は直前の項目が量か、選択と
// 番号の無い群の端）。
[[nodiscard]] bool loose_mark(char32_t code, bool fresh, VimPatternParse &parse)
{
    if (display_width(code) != DisplayWidth::zero || follows_base(parse.atoms, fresh))
    {
        return false;
    }
    appended_mark(code, fresh, parse.atoms, parse.ranges);
    return true;
}

[[nodiscard]] std::expected<std::size_t, VimPatternFailure>
parsed_item(std::string_view pattern, std::size_t at, VimPatternParse &parse)
{
    std::vector<VimPatternAtom> &atoms = parse.atoms;
    std::vector<VimPatternRange> &ranges = parse.ranges;
    const char32_t code = code_point_at(pattern, Offset{at});
    const std::size_t next = next_code_point(pattern, Offset{at}).value;
    if (code == U'\\')
    {
        return parsed_escape(pattern, next, atoms, ranges);
    }
    if (loose_mark(code, parse.after_multi || parse.after_emptied || fenced(parse), parse))
    {
        return next;
    }
    if (code == U'~')
    {
        return std::unexpected(VimPatternFailure::previous_substitute);
    }
    auto star = starred(code, next, parse);
    if (star.has_value())
    {
        return std::move(star).value();
    }
    if (code == U'[')
    {
        const auto set = parsed_set(pattern, at, atoms, ranges);
        if (set.has_value())
        {
            return set.value();
        }
    }
    // `$` は群の閉じと選択の手前でも錨（Vim の peekchr・ADR 0184 の決定 3）。
    const std::string_view rest = pattern.substr(next);
    const bool closes = rest.starts_with(group_close) || rest.starts_with(branch_bar);
    atoms.push_back(plain_atom(code, at, next, closes ? next : pattern.size()));
    return next;
}

// at が量の `\` か（`\+` `\=` `\?` `\{`・ADR 0184 の決定 2）。
[[nodiscard]] bool quantifier_escape(std::string_view pattern, std::size_t at) noexcept
{
    return at + 1 < pattern.size() && pattern[at] == '\\' &&
           (pattern[at + 1] == '+' || pattern[at + 1] == '=' || pattern[at + 1] == '?' ||
            pattern[at + 1] == '{');
}

// 群の閉じ `\)`（at は `\` の位置）。最も内側の開きを閉じ、閉じの印を立てる。直後の量は群に付く
// （closed・ADR 0184 の決定 3）。対の無い閉じは E55。
[[nodiscard]] std::expected<std::size_t, VimPatternFailure> closed_group(std::size_t at,
                                                                         VimPatternParse &parse)
{
    if (parse.opens.empty())
    {
        return std::unexpected(VimPatternFailure::unmatched_close);
    }
    const auto capture = parse.marks.at(parse.opens.back()).capture;
    parse.opens.pop_back();
    if (capture.has_value())
    {
        parse.groups.at(capture.value()).end = parse.atoms.size();
    }
    parse.marks.push_back(
        VimPatternMark{VimPatternMarkKind::close, parse.atoms.size(), capture, std::nullopt});
    parse.opened = false;
    parse.opened_plain = false;
    parse.closed = true;
    return at + 2;
}

// 数字の列を読む（at から・桁が無ければ値なし）。大きな回数は飽和させる（上限の検査は開くとき）。
[[nodiscard]] std::optional<std::size_t> read_count(std::string_view pattern, std::size_t &at)
{
    constexpr std::size_t saturated = std::size_t{1} << 40U;
    constexpr std::size_t radix = 10;
    std::optional<std::size_t> count;
    while (at < pattern.size() && pattern[at] >= '0' && pattern[at] <= '9')
    {
        const auto digit = static_cast<std::size_t>(pattern[at] - '0');
        count = std::min(count.value_or(0) * radix + digit, saturated);
        ++at;
    }
    return count;
}

// `\{` の中（at は `{` の直後）。Vim の read_limits と nfa_regpiece の読み: `-` は最短・`\{n}` は
// n 回・`\{n,}` は n 回以上・`\{,m}` は m 回まで・`\{}` `\{,}` は 0 回以上・閉じは `}` か `\}`・
// 最小が最大より大きければ入れ替える（`a\{3,1}` は `a\{1,3}`）。ほかは brace（E554）。
[[nodiscard]] std::expected<std::pair<VimPatternQuantity, std::size_t>, VimPatternFailure>
read_braces(std::string_view pattern, std::size_t at)
{
    const bool lazy = at < pattern.size() && pattern[at] == '-';
    at += lazy ? 1 : 0;
    const std::optional<std::size_t> first = read_count(pattern, at);
    std::optional<std::size_t> maximum = first;
    if (at < pattern.size() && pattern[at] == ',')
    {
        ++at;
        maximum = read_count(pattern, at);
    }
    at += at < pattern.size() && pattern[at] == '\\' ? 1 : 0;
    if (at >= pattern.size() || pattern[at] != '}')
    {
        return std::unexpected(VimPatternFailure::brace);
    }
    std::size_t minimum = first.value_or(0);
    if (maximum.has_value() && minimum > maximum.value())
    {
        std::swap(minimum, maximum.value());
    }
    return std::pair{VimPatternQuantity{minimum, maximum, lazy}, at + 1};
}

// 量（at は `\`）を読み、量と次の位置を返す。
[[nodiscard]] std::expected<std::pair<VimPatternQuantity, std::size_t>, VimPatternFailure>
read_quantity(std::string_view pattern, std::size_t at)
{
    const char mark = pattern[at + 1];
    if (mark == '+')
    {
        return std::pair{VimPatternQuantity{1, std::nullopt, false}, at + 2};
    }
    if (mark == '=' || mark == '?')
    {
        return std::pair{VimPatternQuantity{0, 1, false}, at + 2};
    }
    return read_braces(pattern, at + 2);
}

// 原子の列の [first, end) の中に構造の印（群の端と選択）があるか（量を掛ける 1 文字を畳むと
// 群の端がずれる）。
[[nodiscard]] bool splits_a_group(const std::vector<VimPatternMark> &marks, std::size_t first,
                                  std::size_t end) noexcept
{
    return std::ranges::any_of(marks, [first, end](const VimPatternMark &mark)
                               { return mark.at > first && mark.at < end; });
}

// 量を掛ける 1 文字（ADR 0184 の決定 2）。最後の原子が基底に続く結合文字の literal なら、基底と
// 結合文字の列を cluster 1 つに畳む（Vim の 1 文字の単位・`é\+` は `éé` に当たる）。基底の無い
// 結合文字（`\` で書いた先頭の結合文字）はその 1 つに掛ける。
[[nodiscard]] std::expected<void, VimPatternFailure> clustered(VimPatternParse &parse)
{
    std::vector<VimPatternAtom> &atoms = parse.atoms;
    if (!composing_literal(atoms.back()))
    {
        return {};
    }
    std::size_t base = atoms.size() - 1;
    while (base > 0 && composing_literal(atoms.at(base)))
    {
        --base;
    }
    const VimPatternAtom &head = atoms.at(base);
    if (composing_literal(head) || head.kind != VimPatternAtomKind::literal ||
        head.repeat != VimPatternRepeat::once)
    {
        return {};
    }
    if (splits_a_group(parse.marks, base, atoms.size()))
    {
        return std::unexpected(VimPatternFailure::quantifier);
    }
    VimPatternAtom cluster{
        VimPatternAtomKind::cluster, head.code, false, VimPatternRepeat::once, parse.ranges.size(),
        atoms.size() - 1 - base,     false,     false};
    for (std::size_t index = base + 1; index < atoms.size(); ++index)
    {
        parse.ranges.push_back(VimPatternRange{atoms.at(index).code, atoms.at(index).code});
    }
    atoms.resize(base);
    atoms.push_back(cluster);
    return {};
}

// 量を原子の写しに開く（Vim 9.1 の nfa_regpiece と同じ形）: 最小の回数だけ量の無い写し、上限が
// 無ければ `*`（最短なら `\{-}`）を 1 つ、あれば残りの回数だけ `\=`（最短なら `\{-0,1}`）。
// 幅の無い原子は、最小が 1 以上なら 1 つのまま、0 なら消す（`^\+a` は `^a`・`\<\=a` は `a`）。
// 開いた後の原子の数が上限を超えれば too_complex。
// 最小の回数の後に残る写しの量（上限が無ければ星・あれば `\=`・最短ならその最短の形）。
[[nodiscard]] VimPatternRepeat tail_repeat(VimPatternQuantity quantity) noexcept
{
    if (quantity.maximum.has_value())
    {
        return quantity.lazy ? VimPatternRepeat::lazy_optional : VimPatternRepeat::optional;
    }
    return quantity.lazy ? VimPatternRepeat::lazy_star : VimPatternRepeat::star;
}

// 最後の原子（食べる原子 1 つ）を写しに開く。
[[nodiscard]] std::expected<void, VimPatternFailure> copied(VimPatternParse &parse,
                                                            VimPatternQuantity quantity)
{
    std::vector<VimPatternAtom> &atoms = parse.atoms;
    const VimPatternAtom unit = atoms.back();
    atoms.pop_back();
    const std::size_t tail =
        quantity.maximum.has_value() ? quantity.maximum.value() - quantity.minimum : 1;
    if (atoms.size() + quantity.minimum + tail > parse.atom_limit)
    {
        return std::unexpected(VimPatternFailure::too_complex);
    }
    atoms.insert(atoms.end(), quantity.minimum, unit);
    VimPatternAtom rest = unit;
    rest.repeat = tail_repeat(quantity);
    atoms.insert(atoms.end(), tail, rest);
    return {};
}

[[nodiscard]] std::expected<void, VimPatternFailure> opened_quantity(VimPatternParse &parse,
                                                                     VimPatternQuantity quantity)
{
    std::vector<VimPatternAtom> &atoms = parse.atoms;
    if (zero_width(atoms.back().kind))
    {
        if (quantity.minimum == 0)
        {
            atoms.pop_back();
        }
        return {};
    }
    const auto cluster = clustered(parse);
    if (!cluster.has_value())
    {
        return cluster;
    }
    return copied(parse, quantity);
}

// 量の前の項目の検査（失敗の順は Vim の nfa_regatom / nfa_regpiece・ADR 0184 の決定 2）。
[[nodiscard]] std::optional<VimPatternFailure> quantifier_placement(VimPatternParse &parse)
{
    if (parse.after_multi)
    {
        return VimPatternFailure::multi_after_multi;
    }
    const bool opened = std::exchange(parse.opened, false);
    const bool opened_plain = std::exchange(parse.opened_plain, false);
    if (opened || opened_plain || parse.after_emptied ||
        (parse.atoms.empty() && !parse.after_close))
    {
        return VimPatternFailure::misplaced;
    }
    return std::nullopt;
}

// 量の項目 `\+` `\=` `\?` `\{...}`（at は `\`・ADR 0184 の決定 2・3）。失敗は Vim の nfa_regatom /
// nfa_regpiece の順に揃える: 量の後ろの量は E871、付く原子が無い（先頭・`\(` `\%(` `\|` の直後・
// `\{0}` の後ろ）なら E866、`\{` の中の不正は E554。群の閉じの直後の量は群に付く（閉じの印に持ち、
// 命令の列に組むときに写しへ開く）。
[[nodiscard]] std::expected<std::size_t, VimPatternFailure>
parsed_quantifier(std::string_view pattern, std::size_t at, VimPatternParse &parse)
{
    const auto placement = quantifier_placement(parse);
    if (placement.has_value())
    {
        return std::unexpected(placement.value());
    }
    const auto quantity = read_quantity(pattern, at);
    if (!quantity.has_value())
    {
        return std::unexpected(quantity.error());
    }
    if (parse.after_close)
    {
        parse.marks.back().quantity = quantity.value().first;
    }
    const auto opened_atoms = parse.after_close ? std::expected<void, VimPatternFailure>{}
                                                : opened_quantity(parse, quantity.value().first);
    if (!opened_atoms.has_value())
    {
        return std::unexpected(opened_atoms.error());
    }
    const bool emptied = quantity.value().first.maximum == std::optional<std::size_t>{0};
    parse.multi = !emptied;
    parse.emptied = emptied;
    return quantity.value().second;
}

// at が `\%(`（番号の無い群の開き・ADR 0184 の決定 3）か。
[[nodiscard]] bool plain_group_open(std::string_view pattern, std::size_t at) noexcept
{
    return at + 2 < pattern.size() && pattern[at] == '\\' && pattern[at + 1] == '%' &&
           pattern[at + 2] == '(';
}

// 群の開き（`\(` と `\%(`）の印を立てる。capture は番号のある群の番号。
void opened_group(std::optional<std::size_t> capture, VimPatternParse &parse)
{
    parse.opens.push_back(parse.marks.size());
    parse.marks.push_back(
        VimPatternMark{VimPatternMarkKind::open, parse.atoms.size(), capture, std::nullopt});
    parse.opened = capture.has_value();
    parse.opened_plain = !capture.has_value();
}

// 選択 `\|` の印を立てる。直後は `\(` の直後と同じ（`*` は文字・`^` は錨・量は E866・Vim の
// peekchr の prevchr == Magic('|')）。
void branched(VimPatternParse &parse)
{
    parse.marks.push_back(
        VimPatternMark{VimPatternMarkKind::branch, parse.atoms.size(), std::nullopt, std::nullopt});
    parse.opened = true;
    parse.opened_plain = false;
}

// 群の開き `\(` `\%(` と閉じ `\)` と選択 `\|` を読む（ADR 0168 の決定 4・ADR 0184 の決定 3）。
// 構造の項目でなければ値なし、読めば次の位置。10 個目の `\(` は E872。
[[nodiscard]] std::optional<std::expected<std::size_t, VimPatternFailure>>
parsed_group(std::string_view pattern, std::size_t at, VimPatternParse &parse)
{
    if (plain_group_open(pattern, at))
    {
        opened_group(std::nullopt, parse);
        return at + 3;
    }
    if (at + 1 >= pattern.size() || pattern[at] != '\\' ||
        (pattern[at + 1] != '(' && pattern[at + 1] != ')' && pattern[at + 1] != '|'))
    {
        return std::nullopt;
    }
    if (pattern[at + 1] == ')')
    {
        return closed_group(at, parse);
    }
    if (pattern[at + 1] == '|')
    {
        branched(parse);
        return at + 2;
    }
    if (parse.groups.size() == vim_pattern_group_limit)
    {
        return std::unexpected(VimPatternFailure::too_many_groups);
    }
    parse.groups.push_back(VimPatternGroup{parse.atoms.size(), vim_pattern_group_open});
    opened_group(parse.groups.size() - 1, parse);
    return at + 2;
}

// `\(` と `\|` の直後の `*`（文字）と `^`（錨）、`\%(` の直後の `^`（錨）と `*`（付く原子の無い量・
// E866）。Vim の magic と同じ（ADR 0168 の決定 4・ADR 0184 の決定 3・2026-11-13 に実測）。
[[nodiscard]] std::optional<std::expected<std::size_t, VimPatternFailure>>
opened_item(std::string_view pattern, std::size_t at, VimPatternParse &parse)
{
    const bool opened = std::exchange(parse.opened, false);
    const bool opened_plain = std::exchange(parse.opened_plain, false);
    if (opened_plain && pattern[at] == '*')
    {
        return std::unexpected(VimPatternFailure::misplaced);
    }
    if ((!opened && !opened_plain) || (pattern[at] != '*' && pattern[at] != '^'))
    {
        return std::nullopt;
    }
    parse.atoms.push_back(pattern[at] == '*' ? literal_atom(U'*')
                                             : atom_of(VimPatternAtomKind::line_start));
    return at + 1;
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

// 前置き（パターンの at より前）が `\c` `\C` だけか（Vim の at_start が残る）。
[[nodiscard]] bool only_case_escapes(std::string_view pattern, std::size_t at) noexcept
{
    for (std::size_t index = 0; index < at; index += 2)
    {
        if (!case_escape(pattern, index))
        {
            return false;
        }
    }
    return true;
}

// `\c` `\C` の直後の量は付く原子が無い（E866）。Vim の nfa_regconcat は `\c` を項目の間で読み、
// 次の nfa_regatom が量を「Misplaced」で断る。星はパターンの先頭と `\(` の直後だけ文字のまま
// （skipchr_keepstart が at_start と prevchr を残す・`^\c*` は E866・2026-11-13 に Vim 9.1
// で実測）。
[[nodiscard]] bool misplaced_after_case(std::string_view pattern, std::size_t at,
                                        const VimPatternParse &parse)
{
    if (quantifier_escape(pattern, at))
    {
        return true;
    }
    return pattern[at] == '*' && !parse.opened && !only_case_escapes(pattern, at);
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
// 次の項目を parse に読み、次の位置を返す。区切りは offset として断る。
[[nodiscard]] std::expected<std::size_t, VimPatternFailure>
parsed_next(std::string_view pattern, std::size_t at, VimSearchDirection separator,
            VimPatternParse &parse)
{
    if (code_point_at(pattern, Offset{at}) == separator_of(separator))
    {
        return std::unexpected(VimPatternFailure::offset);
    }
    if (case_escape(pattern, at))
    {
        forced_case(code_point_at(pattern, Offset{at + 1}), parse.forced);
        parse.cased = true;
        return at + 2;
    }
    // 直前の項目が量だったか（量の後ろの量・`\{0}` の後ろの量を断る・ADR 0184 の決定 2）。
    // `\c` `\C` は原子を足さないので、この印を動かさない。
    const bool cased = std::exchange(parse.cased, false);
    parse.after_close = std::exchange(parse.closed, false);
    parse.after_multi = std::exchange(parse.multi, false);
    parse.after_emptied = std::exchange(parse.emptied, false);
    if (escaped_separator(pattern, at, separator))
    {
        parse.opened = false;
        parse.opened_plain = false;
        parse.atoms.push_back(literal_atom(separator_of(separator)));
        return at + 2;
    }
    if (cased && misplaced_after_case(pattern, at, parse))
    {
        return std::unexpected(VimPatternFailure::misplaced);
    }
    if (quantifier_escape(pattern, at))
    {
        return parsed_quantifier(pattern, at, parse);
    }
    auto group = parsed_group(pattern, at, parse);
    if (group.has_value())
    {
        return std::move(group).value();
    }
    auto opened = opened_item(pattern, at, parse);
    if (opened.has_value())
    {
        return std::move(opened).value();
    }
    return parsed_item(pattern, at, parse);
}

// 選択か群に付く量を含むか（命令の列に組むパターン・ADR 0184 の決定 1・3）。
[[nodiscard]] bool needs_program(const std::vector<VimPatternMark> &marks) noexcept
{
    return std::ranges::any_of(
        marks, [](const VimPatternMark &mark)
        { return mark.kind == VimPatternMarkKind::branch || mark.quantity.has_value(); });
}

// 食べる原子 1 つが食べる最長のバイト数（cluster は基底と結合文字の数だけ・幅の無い原子は 0）。
[[nodiscard]] std::size_t atom_bytes(const VimPatternAtom &atom) noexcept
{
    const std::size_t points = atom.kind == VimPatternAtomKind::cluster ? 1 + atom.range_count : 1;
    return zero_width(atom.kind) ? 0 : longest_code_point * points;
}

// 道を parse の結果から 1 回だけ決める（ADR 0184 の決定 1・route の答え）。
[[nodiscard]] VimPatternRoute route_of(const std::vector<VimPatternAtom> &atoms, bool lazy,
                                       bool program) noexcept
{
    if (program)
    {
        return VimPatternRoute::program;
    }
    if (lazy)
    {
        return VimPatternRoute::lazy_states;
    }
    return std::ranges::any_of(atoms, [](const VimPatternAtom &atom)
                               { return atom.repeat != VimPatternRepeat::once; })
               ? VimPatternRoute::states
               : VimPatternRoute::sequence;
}

// 命令 pc から当たりまでの最長（longest は pc より後ろの命令の答え）。後ろへの跳び（`*` の輪）なら
// 決まらないので値なし。
[[nodiscard]] std::optional<std::size_t> longest_from(const VimPatternInstruction &instruction,
                                                      std::size_t pc,
                                                      const std::vector<VimPatternAtom> &atoms,
                                                      const std::vector<std::size_t> &longest)
{
    switch (instruction.opcode)
    {
    case VimPatternOpcode::atom:
        return atom_bytes(atoms.at(instruction.first)) + longest.at(pc + 1);
    case VimPatternOpcode::split:
        if (instruction.first <= pc || instruction.second <= pc)
        {
            return std::nullopt;
        }
        return std::max(longest.at(instruction.first), longest.at(instruction.second));
    case VimPatternOpcode::jump:
        return instruction.first <= pc ? std::nullopt
                                       : std::optional{longest.at(instruction.first)};
    case VimPatternOpcode::open:
    case VimPatternOpcode::close:
        return longest.at(pc + 1);
    case VimPatternOpcode::accept:
        return std::size_t{0};
    }
    std::unreachable();
}

// 命令の列の一致の長さの上限（#556）。後ろへの跳び（`*` の輪）があれば決まらないので値なし。跳びは
// 前へだけなので、後ろの命令から順に「そこから当たりまでの最長」を引く。
[[nodiscard]] std::optional<std::size_t>
program_longest(const std::vector<VimPatternInstruction> &program,
                const std::vector<VimPatternAtom> &atoms)
{
    std::vector<std::size_t> longest(program.size() + 1, 0);
    for (std::size_t pc = program.size(); pc > 0; --pc)
    {
        const auto bytes = longest_from(program.at(pc - 1), pc - 1, atoms, longest);
        if (!bytes.has_value())
        {
            return std::nullopt;
        }
        longest.at(pc - 1) = bytes.value();
    }
    return longest.at(0);
}
} // namespace

VimPattern::VimPattern(VimPatternParse &&parse, std::vector<VimPatternInstruction> program,
                       bool ignore_case)
    : atoms_(std::move(parse.atoms)), ranges_(std::move(parse.ranges)),
      groups_(std::move(parse.groups)), ignore_case_(ignore_case),
      lazy_(std::ranges::any_of(atoms_,
                                [](const VimPatternAtom &atom) { return lazy(atom.repeat); })),
      program_(std::move(program)), route_(route_of(atoms_, lazy_, !program_.empty())),
      program_longest_(program_longest(program_, atoms_))
{
}

bool VimPattern::is_zero_width(VimPatternAtomKind kind) noexcept
{
    return zero_width(kind);
}

bool VimPattern::loops(VimPatternRepeat repeat) noexcept
{
    switch (repeat)
    {
    case VimPatternRepeat::star:
    case VimPatternRepeat::lazy_star:
        return true;
    case VimPatternRepeat::once:
    case VimPatternRepeat::optional:
    case VimPatternRepeat::lazy_optional:
        return false;
    }
    std::unreachable();
}

bool VimPattern::skippable(VimPatternRepeat repeat) noexcept
{
    return repeat != VimPatternRepeat::once;
}

bool VimPattern::lazy(VimPatternRepeat repeat) noexcept
{
    switch (repeat)
    {
    case VimPatternRepeat::lazy_star:
    case VimPatternRepeat::lazy_optional:
        return true;
    case VimPatternRepeat::once:
    case VimPatternRepeat::star:
    case VimPatternRepeat::optional:
        return false;
    }
    std::unreachable();
}

std::optional<std::size_t> VimPattern::longest_bytes() const noexcept
{
    if (route_ == VimPatternRoute::program)
    {
        return program_longest_;
    }
    std::size_t bytes = 0;
    for (const VimPatternAtom &atom : atoms_)
    {
        if (loops(atom.repeat))
        {
            return std::nullopt;
        }
        bytes += atom_bytes(atom);
    }
    return bytes;
}

std::optional<std::string> VimPattern::literal_bytes() const
{
    // 選択と群の量の命令の列は原子を順に並べた列ではない（ADR 0184 の決定 3）。
    if (atoms_.empty() || route_ == VimPatternRoute::program)
    {
        return std::nullopt;
    }
    std::string bytes;
    for (const VimPatternAtom &atom : atoms_)
    {
        if (atom.kind != VimPatternAtomKind::literal || atom.repeat != VimPatternRepeat::once ||
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
    // `\<` `\>` は Vim の 1 文字の境でだけ真（結合文字の上では偽・#1146）。
    case VimPatternAtomKind::word_start:
        return ends_a_character(line, at) && starts_a_word(line, at);
    case VimPatternAtomKind::word_end:
        return ends_a_character(line, at) && ends_a_word(line, at);
    case VimPatternAtomKind::literal:
    case VimPatternAtomKind::any:
    case VimPatternAtomKind::set:
    case VimPatternAtomKind::composing:
    case VimPatternAtomKind::cluster:
        return false;
    }
    std::unreachable();
}

std::expected<VimPattern, VimPatternFailure> VimPattern::parse(std::string_view pattern,
                                                               VimSearchDirection separator,
                                                               VimPatternCase letter_case,
                                                               VimPatternLimits limits)
{
    // 歩き（code_point_at）は検証済みの UTF-8 が前提なので、入口で確かめる（#906）。
    if (!validate_utf8(pattern).has_value())
    {
        return std::unexpected(VimPatternFailure::invalid_utf8);
    }
    VimPatternParse parse;
    parse.atom_limit = limits.atoms;
    std::size_t at = 0;
    while (at < pattern.size())
    {
        const auto next = parsed_next(pattern, at, separator, parse);
        if (!next)
        {
            return std::unexpected(next.error());
        }
        at = next.value();
    }
    // 閉じない群は最も内側の開きで断る（Vim の nfa_reg は内側から終わりを確かめる）。
    if (!parse.opens.empty())
    {
        return std::unexpected(parse.marks.at(parse.opens.back()).capture.has_value()
                                   ? VimPatternFailure::group
                                   : VimPatternFailure::unclosed_plain_group);
    }
    marked_exact(parse.atoms);
    const bool ignore_case = ignores_case(letter_case, parse.forced, pattern);
    // 選択か群の量を含むパターンだけを命令の列に組む（ADR 0184 の決定 1・3）。
    std::vector<VimPatternInstruction> program;
    if (needs_program(parse.marks))
    {
        auto compiled = VimPatternCompiler::compiled(parse.atoms, parse.marks, limits.instructions);
        if (!compiled.has_value())
        {
            return std::unexpected(compiled.error());
        }
        program = std::move(compiled).value();
    }
    return VimPattern(std::move(parse), std::move(program), ignore_case);
}

// 一致の終わりが文字の途中（本文の側にだけ結合文字が続く）なら一致にしない。Vim の NFA の
// NFA_MATCH と同じく終わりだけを見るので、`/e.` は `.` が結合文字を食べて当たる（#1138）。
// 一致の始まりも同じ境でだけ試す（Vim の regexec の列は mb_ptr2len で 1 文字ずつ進む・#1146）。
bool VimPattern::ends_a_character(std::string_view line, std::size_t end) noexcept
{
    // ASCII のバイトは結合文字にならないので、表を引くのは ASCII の外の側だけ。
    constexpr unsigned char ascii_limit = 0x80;
    return end >= line.size() || static_cast<unsigned char>(line[end]) < ascii_limit ||
           !vim_character_continues(line, Offset{end});
}

// at の code point を食べた後の位置。`.` と集合は Vim の 1 文字の残り（直後に続く結合文字）まで
// 食べ、literal は code point 1 つだけを食べる（Vim の NFA の clen は utfc_ptr2len で、literal の
// 一致だけが utf_ptr2len に縮める・`/.y` と `/[e]` は `é` に当たる・#1146 の実測）。
std::size_t VimPattern::eaten(std::string_view line, VimPatternAtomKind kind, std::size_t at)
{
    std::size_t next = next_code_point(line, Offset{at}).value;
    if (!whole_character(kind))
    {
        return next;
    }
    while (!ends_a_character(line, next))
    {
        next = next_code_point(line, Offset{next}).value;
    }
    return next;
}

bool VimPattern::whole_character(VimPatternAtomKind kind) noexcept
{
    switch (kind)
    {
    case VimPatternAtomKind::any:
    case VimPatternAtomKind::set:
    case VimPatternAtomKind::composing:
    case VimPatternAtomKind::cluster:
        return true;
    case VimPatternAtomKind::literal:
    case VimPatternAtomKind::line_start:
    case VimPatternAtomKind::line_end:
    case VimPatternAtomKind::word_start:
    case VimPatternAtomKind::word_end:
        return false;
    }
    std::unreachable();
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

// 結合文字だけの原子（#1506）。at の code point（基底でも、1 文字の途中の結合文字でもよい）は
// 比べずに飛ばし、同じ Vim の 1 文字に続く結合文字を Vim の MAX_MCO（6）個まで集めて、原子の
// 結合文字が全部その中にあれば当たる（順は問わない・本文の側に余る結合文字は問わない・大小は
// 畳まない）。Vim 9.1 の nfa_regmatch の NFA_COMPOSING の `utf_iscomposing(sta->c)` の枝と同じ。
// 読むのは at から始まる 1 文字の中だけで、行を写さない。
bool VimPattern::carries_marks(std::string_view line, const VimPatternAtom &atom,
                               std::size_t at) const
{
    constexpr std::size_t most_marks = 6;
    std::array<char32_t, most_marks> marks{};
    std::size_t count = 0;
    std::size_t next = next_code_point(line, Offset{at}).value;
    while (count < most_marks && !ends_a_character(line, next))
    {
        marks.at(count) = code_point_at(line, Offset{next});
        ++count;
        next = next_code_point(line, Offset{next}).value;
    }
    const std::span<const char32_t> held(marks.data(), count);
    for (std::size_t index = 0; index < atom.range_count; ++index)
    {
        if (!contains(held, ranges_.at(atom.first_range + index).first))
        {
            return false;
        }
    }
    return true;
}

// cluster（ADR 0184 の決定 2）の結合文字が、at の基底の後ろにこの順でちょうど並んで 1 文字が
// 終わるか（大小を畳まない・量の無い `é` の literal の列が 1 文字の全体に当たるときと同じ）。
// 読むのは at から始まる 1 文字の中だけ。
bool VimPattern::spells_cluster(std::string_view line, const VimPatternAtom &atom,
                                std::size_t at) const
{
    std::size_t next = next_code_point(line, Offset{at}).value;
    for (std::size_t index = 0; index < atom.range_count; ++index)
    {
        if (next >= line.size() ||
            code_point_at(line, Offset{next}) != ranges_.at(atom.first_range + index).first)
        {
            return false;
        }
        next = next_code_point(line, Offset{next}).value;
    }
    return ends_a_character(line, next);
}

bool VimPattern::consumes(std::string_view line, const VimPatternAtom &atom, std::size_t at,
                          char32_t code) const
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
    case VimPatternAtomKind::composing:
        return carries_marks(line, atom, at);
    case VimPatternAtomKind::cluster:
        return code == atom.code && spells_cluster(line, atom, at);
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
    return consumes(line, atom, at, code_point_at(line, Offset{at}))
               ? std::optional<std::size_t>{eaten(line, atom.kind, at)}
               : std::nullopt;
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
    // 量の無い列は後戻りしない列の道、量（`*` と開いた量の写し）を含む列は状態集合の道（ADR 0093・
    // ADR 0184 の決定 1・2）、選択か群の量を含むパターンは命令の列の道（決定 3）。道は parse の
    // 結果そのものから決める（route の 1 本）。
    switch (route_)
    {
    case VimPatternRoute::sequence:
        return sequenced(line, from);
    case VimPatternRoute::states:
    case VimPatternRoute::lazy_states:
    {
        VimPatternEvaluation evaluation(*this, line);
        return evaluation.matched(from);
    }
    case VimPatternRoute::program:
    {
        VimPatternMachine machine(*this, line);
        return machine.matched(from);
    }
    }
    std::unreachable();
}

std::optional<VimPatternMatch> VimPattern::sequenced(std::string_view line, std::size_t from) const
{
    std::size_t at = from;
    while (at <= line.size())
    {
        // 始まりは Vim の 1 文字の先頭だけ（結合文字の上からは試さない・#1146）。
        const auto end = ends_a_character(line, at) ? matched_sequence(line, at) : std::nullopt;
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

VimPatternRoute VimPattern::route() const noexcept
{
    return route_;
}

std::size_t VimPattern::instruction_count() const noexcept
{
    return program_.size();
}

std::size_t VimPattern::atom_count() const noexcept
{
    return atoms_.size();
}

std::size_t VimPattern::group_count() const noexcept
{
    return groups_.size();
}

std::optional<VimPatternCaptures> VimPattern::captured(std::string_view line, VimPatternMatch match,
                                                       std::size_t cell_limit) const
{
    VimPatternCaptures captures{};
    captures.at(0) = match;
    if (groups_.empty())
    {
        return captures;
    }
    if (route_ == VimPatternRoute::program)
    {
        return traced(line, match, cell_limit);
    }
    if (match.end < match.begin || (atoms_.size() + 1) * (match.end - match.begin + 1) > cell_limit)
    {
        return std::nullopt;
    }
    VimPatternCapture walk(*this, line, match);
    // match は照合器の答えなので道は必ずある。無ければ群を黙って空にせず値なし。
    if (!walk.walked())
    {
        return std::nullopt;
    }
    for (std::size_t group = 0; group < groups_.size(); ++group)
    {
        const VimPatternGroup &range = groups_.at(group);
        captures.at(group + 1) =
            VimPatternMatch{walk.positions_.at(range.first), walk.positions_.at(range.end)};
    }
    return captures;
}

// 命令の列の道の群の範囲（ADR 0184 の決定 4）。升（命令・位置）の数が上限を超える一致は探さずに
// 値なし（群を黙って空にしない・呼ぶ側が置換全体を断る）。
std::optional<VimPatternCaptures> VimPattern::traced(std::string_view line, VimPatternMatch match,
                                                     std::size_t cell_limit) const
{
    if (match.end < match.begin || program_.size() * (match.end - match.begin + 1) > cell_limit)
    {
        return std::nullopt;
    }
    VimPatternCaptures captures{};
    captures.at(0) = match;
    VimPatternTrace trace(*this, line, match);
    if (!trace.walked(captures))
    {
        return std::nullopt;
    }
    return captures;
}
} // namespace nenenib::core
