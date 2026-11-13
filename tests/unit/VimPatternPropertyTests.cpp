// scope `--vim-pattern-property` の性質の試験（#899・ADR 0032 / 0037 / 0041・ADR 0042）。
// 使う人が打つ検索の pattern を受ける入口 VimPattern::parse と、行に当てる matched に、固定 seed の
// 決定的な生成器（magic の構文の断片の列・ランダムなバイト列・断片を含む行・多バイト・4 KiB）で
// 作った入力を渡し、落ちない・値か閉じた失敗・一致は行の内で code point の境目・同じ入力で同じ
// 結果・literal_bytes は find と同じ位置・longest_bytes を超えない、を確かめる。
// 壊れた UTF-8 も生成する。壊れた pattern は parse が invalid_utf8 で断る（#906）。壊れた行は
// matched の前提（検証済みの UTF-8）の外なので外す（known_out_of_contract）。
// 後戻りの爆発は、照合器に歩数の数え口が無いので、星を並べた pattern が `a` の列（64 文字と
// 4 KiB）に答えを返すことだけを見る。時間は Release の probe の pattern-star-miss-1024 / 2048 /
// 4096 と pattern-multistar-miss-32 が測る（ADR 0093）。Vim との一致は oracle の領分で問わない。
// 原子 1 つに付く量（ADR 0184 の決定 2）は、乱数のパターンを試験の中の素朴な後戻りの模型と照合して
// 当たりと群の範囲が同じになることを見る（verify_quantifiers_follow_the_model）。
// 選択と群に付く量（決定 3）も同じく、木の形の後戻りの模型と照合する
// （verify_branches_follow_the_model）。状態集合の道の答えは、直す前の評価器の答えの
// 畳みと比べる（verify_state_route_keeps_its_answers・#1584）。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimPattern.hpp"
#include "VimPatternCaptures.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"
#include "VimPatternRoute.hpp"
#include "VimSearchDirection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::VimPattern;
using core::VimPatternCase;
using core::VimPatternFailure;
using core::VimPatternMatch;
using core::VimSearchDirection;
using Random = std::mt19937_64;
using Parsed = std::expected<VimPattern, VimPatternFailure>;
using Found = std::optional<VimPatternMatch>;
// pattern と行の組。
using Case = std::pair<std::string, std::string>;
using Make = Case (*)(Random &);
using Check = std::string_view (*)(const Case &);
using Unsupported = std::pair<std::string_view, VimPatternFailure>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 200U * rounds;
constexpr std::size_t large_cases = 4U * rounds;
constexpr std::size_t short_line_limit = 160U;
constexpr std::size_t large_line_bytes = 4096U;
constexpr std::size_t sampled_froms = 4U;

// 照合器が受ける magic の断片（ADR 0032 の決定 4・VimPatternAtomKind の原子）。
constexpr std::array<std::string_view, 40> supported_fragments{{
    "a",
    "b",
    "ab",
    "x",
    " ",
    "\t",
    ".",
    "a\\+",
    ".\\{-1,2}",
    "a*",
    ".*",
    "[abc]",
    "[^abc]",
    "[a-z]",
    "[^a-z0-9]",
    "[]]",
    "[^]a]",
    "[",
    "]",
    "[-]",
    "\\d",
    "\\D",
    "\\w",
    "\\W",
    "\\s",
    "\\S",
    "\\<",
    "\\>",
    "^",
    "$",
    "\\\\",
    "\\.",
    "\\*",
    "\\[",
    "\\$",
    "\\^",
    "日本",
    "\xE3\x81\x82*",
    "\xF0\x9F\x98\x80",
    "-",
}};

// 受けない断片と、末尾に置いたときの閉じた失敗（VimPatternFailure.hpp の一覧）。
constexpr std::array<Unsupported, 22> unsupported_fragments{{
    {"\\(", VimPatternFailure::group},
    {"\\)", VimPatternFailure::unmatched_close},
    {"\\%(a", VimPatternFailure::unclosed_plain_group},
    {"\\|\\+", VimPatternFailure::misplaced},
    {"\\(a\\)**", VimPatternFailure::multi_after_multi},
    {"\\(a\\)\\{x}", VimPatternFailure::brace},
    {"a\\{x}", VimPatternFailure::brace},
    {"a\\{1,2,3}", VimPatternFailure::brace},
    {"a**", VimPatternFailure::multi_after_multi},
    {"\\}", VimPatternFailure::quantifier},
    {"\\v", VimPatternFailure::magic},
    {"\\V", VimPatternFailure::magic},
    {"\\m", VimPatternFailure::magic},
    {"\\M", VimPatternFailure::magic},
    {"\\%d", VimPatternFailure::escape},
    {"\\_s", VimPatternFailure::escape},
    {"\\@", VimPatternFailure::escape},
    {"\\zs", VimPatternFailure::escape},
    {"\\n", VimPatternFailure::escape},
    {"\\", VimPatternFailure::escape},
    {"~", VimPatternFailure::previous_substitute},
    {"/e", VimPatternFailure::offset},
}};

constexpr std::string_view pattern_alphabet = "\\()|*+=?{},[]^$.<>~/-cCdDwWsSvVmMnz%_@&ab0 \t";
constexpr std::string_view ascii_alphabet = "aab b_x.*[]^$-\\ \t0123456789AZz!?~/";

// 行に混ぜる多バイト（2〜4 バイト・幅 0・壊れた列）。
constexpr std::array<std::string_view, 9> multibyte_pieces{{
    "\xC3\xA9",
    "日本",
    "\xE3\x81\x82",
    "\xF0\x9F\x98\x80",
    "\xCC\x81",
    "\xC3",
    "\x80\x80",
    "\xF0\x9F",
    "\xFF",
}};

// 星を並べた後戻りの爆発の候補（a の列には当たらない）。
constexpr std::array<std::string_view, 8> explosive_patterns{{
    "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b",
    ".*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*b",
    "[ab]*a*[a-z]*\\w*.*b",
    "a*a*a*a*a*a*a*a*$b",
    "\\<a*a*a*a*a*a*a*a*\\>b",
    "^.*a*.*a*.*a*.*c",
    "\\(a*\\)*b",
    "\\(a\\|aa\\)\\+b",
}};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char raw_byte(Random &random)
{
    return static_cast<char>(static_cast<unsigned char>(below(random, 256U)));
}

template <std::size_t Count>
[[nodiscard]] std::string_view pick(Random &random, const std::array<std::string_view, Count> &from)
{
    return from.at(below(random, Count));
}

// 受ける断片の列。ときどき 1 つを長く繰り返す（長い繰り返し・星の列）。
[[nodiscard]] std::string supported_pattern(Random &random)
{
    const std::size_t pieces = below(random, 12U);
    std::string pattern;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        const std::string_view fragment = pick(random, supported_fragments);
        const std::size_t repeat = below(random, 10U) == 0U ? 1U + below(random, 40U) : 1U;
        for (std::size_t copy = 0; copy < repeat; ++copy)
        {
            pattern.append(fragment);
        }
    }
    return pattern;
}

// 受ける断片と受けない断片を混ぜた列。
[[nodiscard]] std::string mixed_pattern(Random &random)
{
    const std::size_t pieces = 1U + below(random, 10U);
    std::string pattern;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        pattern.append(
            below(random, 5U) == 0U
                ? unsupported_fragments.at(below(random, unsupported_fragments.size())).first
                : pick(random, supported_fragments));
    }
    return pattern;
}

// 構文の記号とランダムなバイト（不正な UTF-8 を含む）。
[[nodiscard]] std::string byte_pattern(Random &random)
{
    const std::size_t length = below(random, 24U);
    std::string pattern;
    for (std::size_t index = 0; index < length; ++index)
    {
        pattern.push_back(below(random, 4U) == 0U
                              ? raw_byte(random)
                              : pattern_alphabet[below(random, pattern_alphabet.size())]);
    }
    return pattern;
}

// 行: ASCII・多バイト・壊れた列・pattern の断片をまぜて length バイトまで。
[[nodiscard]] std::string line_of(Random &random, std::size_t length)
{
    std::string line;
    const std::size_t flavor = below(random, 4U);
    while (line.size() < length)
    {
        const std::size_t roll = below(random, 8U);
        if (flavor != 0U && roll == 0U)
        {
            line.append(pick(random, multibyte_pieces));
        }
        else if (flavor == 2U && roll == 1U)
        {
            line.append(pick(random, supported_fragments));
        }
        else if (flavor == 3U && roll == 1U)
        {
            line.push_back(raw_byte(random));
        }
        else
        {
            line.push_back(ascii_alphabet[below(random, ascii_alphabet.size())]);
        }
    }
    return line;
}

[[nodiscard]] std::string short_line(Random &random)
{
    return below(random, 12U) == 0U ? std::string{}
                                    : line_of(random, below(random, short_line_limit));
}

Case supported_case(Random &random)
{
    std::string pattern = supported_pattern(random);
    return {std::move(pattern), short_line(random)};
}
Case mixed_case(Random &random)
{
    std::string pattern = mixed_pattern(random);
    return {std::move(pattern), short_line(random)};
}
Case byte_case(Random &random)
{
    std::string pattern = byte_pattern(random);
    return {std::move(pattern), short_line(random)};
}
// pattern の素の文字を行に埋め、当たる組を増やす。
Case planted_case(Random &random)
{
    std::string pattern = supported_pattern(random);
    std::string line = short_line(random);
    std::string plain = pattern;
    std::erase_if(
        plain, [](char character)
        { return character == '\\' || character == '*' || character == '[' || character == ']'; });
    line.insert(below(random, line.size() + 1U), plain);
    return {std::move(pattern), std::move(line)};
}
Case large_case(Random &random)
{
    std::string pattern =
        below(random, 2U) == 0U ? supported_pattern(random) : mixed_pattern(random);
    return {std::move(pattern), line_of(random, large_line_bytes)};
}

[[nodiscard]] bool same_found(const Found &left, const Found &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    return !left.has_value() ||
           (left.value().begin == right.value().begin && left.value().end == right.value().end);
}

// matched が歩く位置（行頭・続きのバイトでない位置・行末）。
[[nodiscard]] bool walked(std::string_view line, std::size_t at)
{
    return at == 0U || core::is_boundary(line, core::Offset{at});
}

[[nodiscard]] std::vector<std::size_t> boundaries(std::string_view line)
{
    std::vector<std::size_t> positions{0U};
    while (positions.back() < line.size())
    {
        positions.push_back(core::next_code_point(line, core::Offset{positions.back()}).value);
    }
    return positions;
}

// 調べる開始位置: 行頭・行末・行末の先・境目の見本と、境目でないかもしれないバイト。
[[nodiscard]] std::vector<std::size_t> froms_of(std::string_view line)
{
    const std::vector<std::size_t> walk = boundaries(line);
    std::vector<std::size_t> froms{0U, line.size(), line.size() + 1U};
    Random random{line.size() * 31U + 7U};
    for (std::size_t sample = 0; sample < sampled_froms; ++sample)
    {
        froms.push_back(walk.at(below(random, walk.size())));
    }
    froms.push_back(below(random, line.size() + 1U));
    return froms;
}

[[nodiscard]] bool valid_utf8(std::string_view text)
{
    return core::validate_utf8(text).has_value();
}

// 一致は開始位置より後ろで行の内、両端は code point の境目（開始位置そのものは境目でなくてよい）。
[[nodiscard]] std::string_view range_violation(std::string_view line, std::size_t from,
                                               VimPatternMatch match)
{
    if (match.begin < from || match.end < match.begin || match.end > line.size())
    {
        return "a match lies outside the line or before the start";
    }
    const bool begins = match.begin == from || walked(line, match.begin);
    const bool ends = match.end == from || walked(line, match.end);
    return begins && ends ? std::string_view{} : "a match does not sit on code point boundaries";
}

// 1 つの開始位置での範囲・境目・上限の性質。
[[nodiscard]] std::string_view match_violation(const VimPattern &pattern, std::string_view line,
                                               std::size_t from, const Found &found)
{
    if (from > line.size())
    {
        return found.has_value() ? "a start past the line end matches" : std::string_view{};
    }
    if (!found.has_value())
    {
        return {};
    }
    const std::string_view violation = range_violation(line, from, found.value());
    if (!violation.empty())
    {
        return violation;
    }
    const auto longest = pattern.longest_bytes();
    return longest.has_value() && found.value().end - found.value().begin > longest.value()
               ? "a match is longer than longest_bytes"
               : std::string_view{};
}

// literal_bytes が値を返す pattern は、正しい UTF-8 の行で find と同じ位置に当たる。
[[nodiscard]] std::string_view literal_violation(const Case &input, const VimPattern &pattern,
                                                 std::size_t from, const Found &found)
{
    const auto &[pattern_text, line] = input;
    const auto literal = pattern.literal_bytes();
    if (!literal.has_value() || from > line.size() || !walked(line, from) ||
        !valid_utf8(pattern_text) || !valid_utf8(line))
    {
        return {};
    }
    const std::size_t position = std::string_view{line}.find(literal.value(), from);
    const bool agrees = found.has_value()
                            ? position == found.value().begin &&
                                  found.value().end == position + literal.value().size()
                            : position == std::string_view::npos;
    return agrees ? std::string_view{} : "a literal pattern disagrees with std::string_view::find";
}

[[nodiscard]] bool same_parse(const Parsed &left, const Parsed &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    return left.has_value() || left.error() == right.error();
}

// 1 つの開始位置の性質（同じ入力で同じ一致・範囲・境目・上限・literal）。
[[nodiscard]] std::string_view from_violation(const Case &input, const VimPattern &first,
                                              const VimPattern &second, std::size_t from)
{
    const std::string_view line = input.second;
    const Found found = first.matched(line, from);
    if (!same_found(found, first.matched(line, from)) ||
        !same_found(found, second.matched(line, from)))
    {
        return "the same pattern and line match differently";
    }
    const std::string_view violation = match_violation(first, line, from, found);
    return violation.empty() ? literal_violation(input, first, from, found) : violation;
}

// matched は検証済みの UTF-8 の行を受ける（code_point_at の前提・Utf8.hpp）。壊れた行は
// 生成するが、ここで外す（末尾で切れた列は終端の先を読む・known_out_of_contract）。pattern は
// parse が確かめるので前提にしない（#906）。
[[nodiscard]] bool within_contract(const Case &input)
{
    return valid_utf8(input.second);
}

// 同じ pattern は同じ結果になり、正しい UTF-8 でない pattern は invalid_utf8 で断られる（#906）。
[[nodiscard]] std::string_view parse_violation(std::string_view pattern_text, const Parsed &first,
                                               const Parsed &second)
{
    if (!same_parse(first, second))
    {
        return "the same pattern parses differently";
    }
    if (valid_utf8(pattern_text))
    {
        return {};
    }
    return !first.has_value() && first.error() == VimPatternFailure::invalid_utf8
               ? std::string_view{}
               : "a pattern of invalid UTF-8 is not refused as invalid_utf8";
}

// 1 組の性質。空なら守られている。
[[nodiscard]] std::string_view case_violation(const Case &input)
{
    const auto &[pattern_text, line] = input;
    const Parsed first =
        VimPattern::parse(pattern_text, VimSearchDirection::forward, VimPatternCase::sensitive);
    const Parsed second =
        VimPattern::parse(pattern_text, VimSearchDirection::forward, VimPatternCase::sensitive);
    const std::string_view parsed_violation = parse_violation(pattern_text, first, second);
    if (!parsed_violation.empty() || !first.has_value() || !within_contract(input))
    {
        return parsed_violation;
    }
    if (first.value().longest_bytes() != second.value().longest_bytes() ||
        first.value().literal_bytes() != second.value().literal_bytes())
    {
        return "the same pattern reports different bounds";
    }
    for (const std::size_t from : froms_of(line))
    {
        const std::string_view violation =
            from_violation(input, first.value(), second.value(), from);
        if (!violation.empty())
        {
            return violation;
        }
    }
    return {};
}

// 受ける断片だけの pattern は値になる。
[[nodiscard]] std::string_view supported_violation(const Case &input)
{
    const std::string_view violation = case_violation(input);
    if (!violation.empty())
    {
        return violation;
    }
    return VimPattern::parse(input.first, VimSearchDirection::forward, VimPatternCase::sensitive)
                   .has_value()
               ? std::string_view{}
               : "a pattern of supported fragments is refused";
}

void print_bytes(const char *label, std::string_view input)
{
    const std::size_t shown = std::min<std::size_t>(input.size(), 96U);
    std::fprintf(stderr, "    %s (%zu bytes):", label, input.size());
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

// 組の片方（side）の大きさ chunk の 1 か所を消して、同じ理由で破れたままなら残す。
[[nodiscard]] bool shrunk(Case &input, std::string Case::*side, std::size_t chunk, Check check)
{
    const std::string_view reason = check(input);
    for (std::size_t at = 0; at + chunk <= (input.*side).size(); at += chunk)
    {
        Case candidate = input;
        (candidate.*side).erase(at, chunk);
        if (check(candidate) == reason)
        {
            input = std::move(candidate);
            return true;
        }
    }
    return false;
}

void minimize_side(Case &input, std::string Case::*side, Check check)
{
    std::size_t chunk = std::max<std::size_t>((input.*side).size() / 2U, 1U);
    while (chunk > 0U)
    {
        chunk =
            shrunk(input, side, chunk, check) ? std::min(chunk, (input.*side).size()) : chunk / 2U;
    }
}

// 同じ理由で破れる小さな組へ貪欲に縮める（回帰 case の手がかり）。
[[nodiscard]] Case minimized(Case input, Check check)
{
    minimize_side(input, &Case::second, check);
    minimize_side(input, &Case::first, check);
    minimize_side(input, &Case::second, check);
    return input;
}

void report(const char *family, const Case &input, Check check)
{
    const std::string_view reason = check(input);
    const Case smallest = minimized(input, check);
    std::fprintf(stderr, "  %s: %.*s\n", family, static_cast<int>(reason.size()), reason.data());
    print_bytes("pattern", smallest.first);
    print_bytes("line", smallest.second);
}

// 生成した組のうち性質を破ったものの数。最初の 1 つは縮めて 16 進で出す。
[[nodiscard]] std::size_t broken_cases(const char *description, std::uint64_t seed, Make make,
                                       Check check)
{
    Random random{seed};
    const std::size_t count = make == large_case ? large_cases : family_cases;
    std::size_t broken = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const Case input = make(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(description, input, check);
        }
    }
    return broken;
}

void verify_family(const char *description, std::uint64_t seed, Make make, Check check)
{
    expect(broken_cases(description, seed, make, check) == 0U, description);
}

using Contract = bool (*)(const Case &);

// 生成した組のうち前提の内に入った数（broken_cases と同じ seed で同じ列・空振りの量を見る）。
[[nodiscard]] std::size_t contract_cases(std::uint64_t seed, Make make, Contract contract)
{
    Random random{seed};
    std::size_t inside = 0;
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        inside += contract(make(random)) ? 1U : 0U;
    }
    return inside;
}

// 生バイトの族は壊れた UTF-8 を混ぜるので、matched の前提（行が妥当）の内に入る組が残ることを
// 見る（下限は族の 1/16）。parse の性質は値か invalid_utf8 を含む閉じた失敗を全件に問うので、
// pattern の側に下限は要らない（#906）。
void verify_byte_cases_reach_the_contract()
{
    expect(contract_cases(8983U, byte_case, within_contract) >= family_cases / 16U,
           "random pattern bytes leave valid lines to match");
}

// 受けない断片を受ける断片の列の末尾に置くと、その断片の閉じた失敗になる。
void verify_unsupported_suffixes()
{
    Random random{8991U};
    bool holds = true;
    for (std::size_t round = 0; round < family_cases; ++round)
    {
        const Unsupported &suffix =
            unsupported_fragments.at(below(random, unsupported_fragments.size()));
        const std::string pattern = supported_pattern(random).append(suffix.first);
        const Parsed parsed =
            VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
        const bool refused = !parsed.has_value() && parsed.error() == suffix.second;
        if (!refused && holds)
        {
            std::fprintf(stderr, "  unsupported suffix accepted or misnamed:\n");
            print_bytes("pattern", pattern);
        }
        holds = holds && refused;
    }
    expect(holds, "an unsupported fragment at the end is refused with its closed failure");
}

// 星を並べた pattern が a の列（64 文字と 4 KiB）に、どの開始位置からも当たらずに答えを返す。
// `\(a*\)*b` と `\(a\|aa\)\+b` は命令の列の道（ADR 0184 の決定 3）で後戻りしない。
void verify_backtracking_is_bounded()
{
    expect(VimPattern::parse("\\(a*\\)*b", VimSearchDirection::forward, VimPatternCase::sensitive)
                   .value()
                   .route() == core::VimPatternRoute::program,
           "a nested star group takes the program route");
    const std::array<std::string, 2> lines{std::string(64U, 'a'),
                                           std::string(large_line_bytes, 'a')};
    bool holds = true;
    for (const std::string_view text : explosive_patterns)
    {
        const Parsed parsed =
            VimPattern::parse(text, VimSearchDirection::forward, VimPatternCase::sensitive);
        holds = holds && parsed.has_value();
        for (const std::string &line : lines)
        {
            holds = holds && parsed.has_value() && !parsed.value().matched(line, 0U).has_value() &&
                    !parsed.value().matched(line, line.size() / 2U).has_value();
        }
    }
    expect(holds, "patterns of many stars answer on a run of a without matching");
    const VimPattern greedy = VimPattern::parse("a*a*a*a*a*a*a*a*", VimSearchDirection::forward,
                                                VimPatternCase::sensitive)
                                  .value();
    const Found whole = greedy.matched(lines.at(1), 0U);
    expect(whole.has_value() && whole.value().begin == 0U && whole.value().end == large_line_bytes,
           "a run of stars takes the whole 4 KiB line");
}

// 既定の回数で見つかった破れを縮めた組（#899）。末尾で切れた多バイトの列を含む行を渡すと、
// matched は断らずに code_point_at で終端の先を読む（Debug の ASan で heap-buffer-overflow と
// container-overflow。文字列が heap にあるとき・例えば前に `b` を 32 個置いた形で見える。SSO の
// 短い文字列では見えない）。行は前提の外なので生成器の組からは within_contract で外し、ここでは
// 前提の外として外されることだけを固定する。pattern 側の組は parse が invalid_utf8 で断る（#906）。
constexpr std::array<std::pair<std::string_view, std::string_view>, 2> known_out_of_contract{{
    {".", "\xE3"},
    {"a*", "a\xC3"},
}};

void verify_known_cases()
{
    bool outside = true;
    for (const auto &[pattern, line] : known_out_of_contract)
    {
        outside = outside && !within_contract({std::string{pattern}, std::string{line}});
    }
    expect(outside, "lines cut inside a multibyte sequence are outside the contract");
    const Parsed cut =
        VimPattern::parse("\xE3", VimSearchDirection::forward, VimPatternCase::sensitive);
    expect(!cut.has_value() && cut.error() == VimPatternFailure::invalid_utf8 &&
               case_violation({"\xE3", ""}).empty() && case_violation({"a\xC3", "a"}).empty(),
           "patterns cut inside a multibyte sequence are refused as invalid_utf8");
    expect(case_violation({"\xC3\xA9*", "\xC3\xA9\xCC\x81\xC3\xA9"}).empty() &&
               case_violation({"[\xE3\x81\x81-\xE3\x82\x93]", "\xE3\x81\x82"}).empty() &&
               case_violation({"\\\xE6\x97\xA5", "\xE6\x97\xA5"}).empty(),
           "multibyte patterns keep their matches inside the line");
    expect(case_violation({"", ""}).empty() && case_violation({"$", ""}).empty() &&
               case_violation({"^*", "*"}).empty(),
           "empty and anchor-only patterns keep their matches inside the line");
}

// ---------------------------------------------------------------- 量の模型（ADR 0184 の決定 2）
// 原子 1 つに付く量を含む乱数のパターンを、素朴な後戻りの模型（回数を多い方から・最短は少ない
// 方から試し、最初に着いた道を答えにする・Vim 9.1 の優先順）と照合し、当たりの範囲と群 1 の範囲が
// 同じになることを見る。行は a b c だけの ASCII、原子は 1 文字を食べるものだけ（空の周が無い）。

// 原子の綴り（模型は atom_accepts の同じ番号で読む）。
constexpr std::array<std::string_view, 5> model_atoms{{"a", "b", ".", "[ab]", "[^a]"}};
// 量の形の数（quantity_text と quantity_limits の番号）。
constexpr std::size_t model_forms = 15U;
constexpr std::size_t model_unbounded = std::string_view::npos;
constexpr std::string_view model_alphabet = "abc";

// 項目: 原子の番号・量の形の番号・n・m。
using ModelItem = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>;
// 最小・最大（上限なしは model_unbounded）・最短か。
using ModelLimits = std::tuple<std::size_t, std::size_t, bool>;
// 項目の列・先頭の `^`・末尾の `$`・群 1 の [first, end)（同じなら群なし）。
using Model = std::tuple<std::vector<ModelItem>, bool, bool, std::size_t, std::size_t>;
// 当たりの始まり・終わりと、群 1 の始まり・終わり。
using ModelAnswer = std::tuple<std::size_t, std::size_t, std::size_t, std::size_t>;

[[nodiscard]] bool atom_accepts(std::size_t atom, char character)
{
    const std::array<bool, 5> accepts{character == 'a', character == 'b', true,
                                      character == 'a' || character == 'b', character != 'a'};
    return accepts.at(atom);
}

[[nodiscard]] std::string quantity_text(std::size_t form, std::size_t n, std::size_t m)
{
    const std::string low = std::to_string(n);
    const std::string high = std::to_string(m);
    const std::array<std::string, model_forms> texts{"",
                                                     "*",
                                                     "\\+",
                                                     "\\=",
                                                     "\\?",
                                                     "\\{" + low + "}",
                                                     "\\{" + low + ",}",
                                                     "\\{," + high + "}",
                                                     "\\{" + low + "," + high + "}",
                                                     "\\{}",
                                                     "\\{-}",
                                                     "\\{-" + low + ",}",
                                                     "\\{-," + high + "}",
                                                     "\\{-" + low + "," + high + "}",
                                                     "\\{-" + low + "}"};
    return texts.at(form);
}

[[nodiscard]] ModelLimits quantity_limits(std::size_t form, std::size_t n, std::size_t m)
{
    const std::size_t low = std::min(n, m);
    const std::size_t high = std::max(n, m);
    const std::array<ModelLimits, model_forms> limits{{{1U, 1U, false},
                                                       {0U, model_unbounded, false},
                                                       {1U, model_unbounded, false},
                                                       {0U, 1U, false},
                                                       {0U, 1U, false},
                                                       {n, n, false},
                                                       {n, model_unbounded, false},
                                                       {0U, m, false},
                                                       {low, high, false},
                                                       {0U, model_unbounded, false},
                                                       {0U, model_unbounded, true},
                                                       {n, model_unbounded, true},
                                                       {0U, m, true},
                                                       {low, high, true},
                                                       {n, n, true}}};
    return limits.at(form);
}

// 項目の境 index に置く群の端（閉じが先）。
[[nodiscard]] std::string_view group_marks(const Model &model, std::size_t index)
{
    const std::size_t first = std::get<3>(model);
    const std::size_t end = std::get<4>(model);
    if (first == end)
    {
        return {};
    }
    if (index == end)
    {
        return "\\)";
    }
    return index == first ? "\\(" : "";
}

[[nodiscard]] std::string model_pattern(const Model &model)
{
    const std::vector<ModelItem> &items = std::get<0>(model);
    std::string pattern = std::get<1>(model) ? "^" : "";
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        const auto &[atom, form, n, m] = items.at(index);
        pattern += group_marks(model, index);
        pattern += model_atoms.at(atom);
        pattern += quantity_text(form, n, m);
    }
    pattern += group_marks(model, items.size());
    return pattern + (std::get<2>(model) ? "$" : "");
}

[[nodiscard]] Model random_model(Random &random)
{
    std::vector<ModelItem> items(1U + below(random, 5U));
    for (ModelItem &item : items)
    {
        item = {below(random, model_atoms.size()), below(random, model_forms), below(random, 4U),
                below(random, 4U)};
    }
    const std::size_t first = below(random, items.size());
    const std::size_t end =
        below(random, 3U) == 0U ? first : first + 1U + below(random, items.size() - first);
    return {std::move(items), below(random, 5U) == 0U, below(random, 5U) == 0U, first, end};
}

[[nodiscard]] std::string model_line(Random &random)
{
    std::string line(below(random, 13U), 'a');
    for (char &character : line)
    {
        character = model_alphabet[below(random, model_alphabet.size())];
    }
    return line;
}

// pos から原子が続けて食べられる数（maximum まで）。
[[nodiscard]] std::size_t model_run(std::size_t atom, std::size_t maximum, std::string_view line,
                                    std::size_t pos)
{
    std::size_t run = 0;
    while (pos + run < line.size() && run < maximum && atom_accepts(atom, line[pos + run]))
    {
        ++run;
    }
    return run;
}

// 項目 index から positions[index] の位置で道を探し、着いた終わりを返す（positions は項目の境の
// 位置・最後に着いた道の値が残る）。回数は多い方から（最短は少ない方から）試す。
[[nodiscard]] std::optional<std::size_t> model_end(const Model &model, std::string_view line,
                                                   std::size_t index,
                                                   std::vector<std::size_t> &positions)
{
    const std::vector<ModelItem> &items = std::get<0>(model);
    const std::size_t pos = positions.at(index);
    if (index == items.size())
    {
        return !std::get<2>(model) || pos == line.size() ? std::optional{pos} : std::nullopt;
    }
    const auto &[atom, form, n, m] = items.at(index);
    const auto [minimum, maximum, lazy] = quantity_limits(form, n, m);
    const std::size_t run = model_run(atom, maximum, line, pos);
    const std::size_t choices = run >= minimum ? run - minimum + 1U : 0U;
    for (std::size_t step = 0; step < choices; ++step)
    {
        positions.at(index + 1U) = pos + (lazy ? minimum + step : run - step);
        const auto end = model_end(model, line, index + 1U, positions);
        if (end.has_value())
        {
            return end;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ModelAnswer> model_matched(const Model &model, std::string_view line,
                                                       std::size_t from)
{
    std::vector<std::size_t> positions(std::get<0>(model).size() + 1U, 0U);
    for (std::size_t start = from; start <= line.size(); ++start)
    {
        if (std::get<1>(model) && start != 0U)
        {
            return std::nullopt;
        }
        positions.at(0) = start;
        const auto end = model_end(model, line, 0U, positions);
        if (end.has_value())
        {
            return ModelAnswer{start, end.value(), positions.at(std::get<3>(model)),
                               positions.at(std::get<4>(model))};
        }
    }
    return std::nullopt;
}

// 群 1 の範囲が模型と同じか（群の無い模型は captured が値を返すことだけ）。
[[nodiscard]] bool same_group(const Model &model,
                              const std::optional<core::VimPatternCaptures> &captures,
                              const ModelAnswer &expected)
{
    if (!captures.has_value())
    {
        return false;
    }
    if (std::get<3>(model) == std::get<4>(model))
    {
        return true;
    }
    const auto &group = captures.value().at(1);
    return group.has_value() && group.value().begin == std::get<2>(expected) &&
           group.value().end == std::get<3>(expected);
}

// 1 つの開始位置での照合器と模型の違い（空なら同じ）。
[[nodiscard]] std::string_view from_model_violation(const VimPattern &pattern, const Model &model,
                                                    std::string_view line, std::size_t from)
{
    const Found found = pattern.matched(line, from);
    const auto expected = model_matched(model, line, from);
    if (found.has_value() != expected.has_value())
    {
        return "the matcher and the backtracking model disagree on a match";
    }
    if (!found.has_value())
    {
        return {};
    }
    if (found.value().begin != std::get<0>(expected.value()) ||
        found.value().end != std::get<1>(expected.value()))
    {
        return "the matcher and the backtracking model disagree on the range";
    }
    return same_group(model, pattern.captured(line, found.value()), expected.value())
               ? std::string_view{}
               : "the group range differs from the backtracking model";
}

// 1 組の照合器と模型の違い（空なら同じ）。
[[nodiscard]] std::string_view model_violation(const Model &model, std::string_view line)
{
    const std::string pattern = model_pattern(model);
    const Parsed parsed =
        VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!parsed.has_value())
    {
        return "a quantified pattern of the model is refused";
    }
    for (const std::size_t from : {std::size_t{0}, line.size() / 2U, line.size()})
    {
        const std::string_view violation = from_model_violation(parsed.value(), model, line, from);
        if (!violation.empty())
        {
            return violation;
        }
    }
    return case_violation({pattern, std::string{line}});
}

// 模型との照合。破れた最初の組は綴りで出す。
void verify_quantifiers_follow_the_model()
{
    Random random{8993U};
    std::size_t broken = 0;
    std::size_t matched = 0;
    for (std::size_t round = 0; round < 4U * family_cases; ++round)
    {
        const Model model = random_model(random);
        const std::string line = model_line(random);
        const std::string_view violation = model_violation(model, line);
        matched += model_matched(model, line, 0U).has_value() ? 1U : 0U;
        if (!violation.empty() && broken++ == 0U)
        {
            std::fprintf(stderr, "  quantifier model: %.*s\n", static_cast<int>(violation.size()),
                         violation.data());
            print_bytes("pattern", model_pattern(model));
            print_bytes("line", line);
        }
    }
    expect(broken == 0U, "quantified patterns match and capture like the backtracking model");
    expect(matched >= family_cases, "the quantifier model cases include matches");
}

// ---------------------------------------------------------------- 選択と群の量の模型
// 選択 `\|` と群（`\(` `\%(`）に付く量を含む乱数のパターンを（ADR 0184 の決定 3）、
// 素朴な後戻りの模型と照合し、当たりの範囲と群 1〜9 の範囲が同じになることを見る。
// 模型は Vim 9.1 の規則を後戻りで書く: 最も左の開始・最初に当たる枝・
// 量は多い方から（最短は少ない方から）・群の量は必須の回数の後に、
// 上限が無ければ輪、あれば任意の回（Vim の nfa_regpiece の写しと同じ）・
// 輪の中で何も食べなかった周の後は回らない（空の周は 1 回だけ）・
// 群の値は閉じを通った周の範囲（通らなかった群は前の値のまま）。
// 行は a b c だけの ASCII。照合器の作り（命令の列・状態集合）とは別に書く。

// 木の節: 群か・原子の番号・量の形・n・m・群の番号（番号の無い群と原子は
// npos）・枝の列（節の番号）。
using TreeBranch = std::vector<std::size_t>;
using TreeNode = std::tuple<bool, std::size_t, std::size_t, std::size_t, std::size_t, std::size_t,
                            std::vector<TreeBranch>>;
// 節 0 は根（番号の無い 1 回だけの群・括弧を書かない）。
using Tree = std::vector<TreeNode>;
// 群 0〜9 の範囲（値の無い群は npos の組）。
using TreeCaptures = std::array<std::pair<std::size_t, std::size_t>, 10>;
// 続きの積みの 1 つ: 種類（0 は枝の続き・1 は群の閉じ）・節・枝か済んだ周の数・項目か周の始まり。
using TreeTask = std::tuple<std::uint8_t, std::size_t, std::size_t, std::size_t>;
using TreeTasks = std::vector<TreeTask>;
// 当たりの終わりと群の範囲。
using TreeAnswer = std::pair<std::size_t, TreeCaptures>;
// 当たりの始まりと答え。
using TreeFound = std::optional<std::pair<std::size_t, TreeAnswer>>;

constexpr std::size_t tree_no_capture = std::string_view::npos;
// 模型の歩数の上限（入れ子の星で指数になる組は数えずに外す）。
constexpr std::size_t tree_budget = 100000U;

[[nodiscard]] std::string tree_text(const Tree &tree, std::size_t node)
{
    const auto &[group, atom, form, n, m, capture, branches] = tree.at(node);
    if (!group)
    {
        return std::string{model_atoms.at(atom)} + quantity_text(form, n, m);
    }
    std::string text;
    for (std::size_t branch = 0; branch < branches.size(); ++branch)
    {
        text += branch == 0 ? "" : "\\|";
        for (const std::size_t item : branches.at(branch))
        {
            text += tree_text(tree, item);
        }
    }
    if (node == 0)
    {
        return text;
    }
    return (capture == tree_no_capture ? "\\%(" : "\\(") + text + "\\)" + quantity_text(form, n, m);
}

// 枝 1 つの項目（depth は群の入れ子の深さ・captures は振った群の番号の数・番号は `\(` の順）。
[[nodiscard]] std::size_t tree_node(Random &random, Tree &tree, std::size_t depth,
                                    std::size_t &captures)
{
    const bool group = depth < 2U && below(random, 3U) == 0U;
    const std::size_t index = tree.size();
    tree.emplace_back(group, below(random, model_atoms.size()), below(random, model_forms),
                      below(random, 3U), below(random, 3U), tree_no_capture,
                      std::vector<TreeBranch>{});
    if (!group)
    {
        return index;
    }
    if (below(random, 4U) != 0U && captures < 9U)
    {
        std::get<5>(tree.at(index)) = captures++;
    }
    std::vector<TreeBranch> branches(1U + below(random, 2U));
    for (TreeBranch &branch : branches)
    {
        branch.resize(below(random, 3U));
        for (std::size_t &item : branch)
        {
            item = tree_node(random, tree, depth + 1U, captures);
        }
    }
    std::get<6>(tree.at(index)) = std::move(branches);
    return index;
}

[[nodiscard]] Tree random_tree(Random &random)
{
    Tree tree;
    tree.emplace_back(true, 0U, 0U, 0U, 0U, tree_no_capture, std::vector<TreeBranch>{});
    std::size_t captures = 0;
    std::vector<TreeBranch> branches(1U + below(random, 3U));
    for (TreeBranch &branch : branches)
    {
        branch.resize(1U + below(random, 3U));
        for (std::size_t &item : branch)
        {
            item = tree_node(random, tree, 0U, captures);
        }
    }
    std::get<6>(tree.at(0)) = std::move(branches);
    return tree;
}

// 後戻りの模型（木と行と歩数）。続きは積み（TreeTasks）で持ち、枝ごとに写して戻りを表す。
class TreeModel final
{
  public:
    TreeModel(const Tree &tree, std::string_view line) : tree_(tree), line_(line) {}

    // from 以降の最も左の当たり。歩数が尽きたら外側の値なし（その組は数えない）。
    [[nodiscard]] std::optional<TreeFound> matched(std::size_t from)
    {
        TreeCaptures unset{};
        unset.fill({tree_no_capture, tree_no_capture});
        for (std::size_t start = from; start <= line_.size(); ++start)
        {
            auto answer = loop({}, {0U, 0U}, start, unset);
            if (budget_ == 0U)
            {
                return std::nullopt;
            }
            if (answer.has_value())
            {
                return TreeFound{std::pair{start, std::move(answer).value()}};
            }
        }
        return TreeFound{};
    }

  private:
    [[nodiscard]] std::optional<TreeAnswer> run(TreeTasks tasks, std::size_t pos,
                                                const TreeCaptures &captures)
    {
        if (budget_ == 0U)
        {
            return std::nullopt;
        }
        --budget_;
        if (tasks.empty())
        {
            return TreeAnswer{pos, captures};
        }
        const TreeTask task = tasks.back();
        tasks.pop_back();
        if (std::get<0>(task) == 1U)
        {
            return closed(std::move(tasks), task, pos, captures);
        }
        const auto [kind, node, branch, item] = task;
        const TreeBranch &items = std::get<6>(tree_.at(node)).at(branch);
        if (item == items.size())
        {
            return run(std::move(tasks), pos, captures);
        }
        tasks.emplace_back(std::uint8_t{0}, node, branch, item + 1U);
        const std::size_t child = items.at(item);
        if (!std::get<0>(tree_.at(child)))
        {
            return eaten(tasks, child, pos, captures);
        }
        return loop(tasks, {child, 0U}, pos, captures);
    }

    // 群の周を done 回済ませた所から、抜けるか次の周かを優先順に試す（state は節と done）。
    [[nodiscard]] std::optional<TreeAnswer> loop(const TreeTasks &rest,
                                                 std::pair<std::size_t, std::size_t> state,
                                                 std::size_t pos, const TreeCaptures &captures)
    {
        const auto &[group, atom, form, n, m, capture, branches] = tree_.at(state.first);
        const auto [minimum, maximum, lazy] = quantity_limits(form, n, m);
        const bool may_exit = state.second >= minimum;
        const bool may_iterate = maximum == model_unbounded || state.second < maximum;
        if (lazy && may_exit)
        {
            auto answer = run(rest, pos, captures);
            if (answer.has_value())
            {
                return answer;
            }
        }
        auto answer = may_iterate ? iterated(rest, state, pos, captures) : std::nullopt;
        if (answer.has_value() || lazy || !may_exit)
        {
            return answer;
        }
        return run(rest, pos, captures);
    }

    // 次の周を枝の順に試す。
    [[nodiscard]] std::optional<TreeAnswer> iterated(const TreeTasks &rest,
                                                     std::pair<std::size_t, std::size_t> state,
                                                     std::size_t pos, const TreeCaptures &captures)
    {
        const std::size_t branches = std::get<6>(tree_.at(state.first)).size();
        for (std::size_t branch = 0; branch < branches; ++branch)
        {
            TreeTasks next = rest;
            next.emplace_back(std::uint8_t{1}, state.first, state.second + 1U, pos);
            next.emplace_back(std::uint8_t{0}, state.first, branch, 0U);
            auto answer = run(std::move(next), pos, captures);
            if (answer.has_value())
            {
                return answer;
            }
        }
        return std::nullopt;
    }

    // 群の閉じ: 値を写し、輪の中で何も食べなかった周なら抜けるだけ、ほかは次の周か抜けるか。
    [[nodiscard]] std::optional<TreeAnswer> closed(TreeTasks tasks, TreeTask task, std::size_t pos,
                                                   TreeCaptures captures)
    {
        const auto [kind, node, done, start] = task;
        const auto &[group, atom, form, n, m, capture, branches] = tree_.at(node);
        if (capture != tree_no_capture)
        {
            captures.at(capture + 1U) = {start, pos};
        }
        const auto [minimum, maximum, lazy] = quantity_limits(form, n, m);
        if (maximum == model_unbounded && done > minimum && pos == start)
        {
            return run(std::move(tasks), pos, captures);
        }
        return loop(tasks, {node, done}, pos, captures);
    }

    // 原子 1 つ（量つき）を食べる回数を優先順に試す。
    [[nodiscard]] std::optional<TreeAnswer> eaten(const TreeTasks &tasks, std::size_t node,
                                                  std::size_t pos, const TreeCaptures &captures)
    {
        const auto &[group, atom, form, n, m, capture, branches] = tree_.at(node);
        const auto [minimum, maximum, lazy] = quantity_limits(form, n, m);
        const std::size_t run_length = model_run(atom, maximum, line_, pos);
        const std::size_t choices = run_length >= minimum ? run_length - minimum + 1U : 0U;
        for (std::size_t step = 0; step < choices; ++step)
        {
            auto answer = run(tasks, pos + (lazy ? minimum + step : run_length - step), captures);
            if (answer.has_value())
            {
                return answer;
            }
        }
        return std::nullopt;
    }

    const Tree &tree_;
    std::string_view line_;
    std::size_t budget_ = tree_budget;
};

// 群の範囲が模型と同じか（値の無い群は両方とも値なし）。
[[nodiscard]] bool same_tree_groups(const std::optional<core::VimPatternCaptures> &captures,
                                    const TreeCaptures &expected)
{
    if (!captures.has_value())
    {
        return false;
    }
    for (std::size_t group = 1; group < expected.size(); ++group)
    {
        const auto &actual = captures.value().at(group);
        const auto [begin, end] = expected.at(group);
        const bool same =
            begin == tree_no_capture
                ? !actual.has_value()
                : actual.has_value() && actual.value().begin == begin && actual.value().end == end;
        if (!same)
        {
            return false;
        }
    }
    return true;
}

// 1 つの開始位置での照合器と模型の違い（空なら同じ・skipped は模型の歩数が尽きた数）。
[[nodiscard]] std::string_view from_tree_violation(const VimPattern &pattern, const Tree &tree,
                                                   std::string_view line, std::size_t from)
{
    TreeModel model(tree, line);
    const auto expected = model.matched(from);
    if (!expected.has_value())
    {
        return {};
    }
    const Found found = pattern.matched(line, from);
    if (found.has_value() != expected.value().has_value())
    {
        return "the matcher and the branch model disagree on a match";
    }
    if (!found.has_value())
    {
        return {};
    }
    const auto &[start, answer] = expected.value().value();
    if (found.value().begin != start || found.value().end != answer.first)
    {
        return "the matcher and the branch model disagree on the range";
    }
    return same_tree_groups(pattern.captured(line, found.value()), answer.second)
               ? std::string_view{}
               : "the group ranges differ from the branch model";
}

[[nodiscard]] std::string_view tree_violation(const Tree &tree, std::string_view line)
{
    const std::string pattern = tree_text(tree, 0);
    const Parsed parsed =
        VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
    if (!parsed.has_value())
    {
        return "a pattern of the branch model is refused";
    }
    for (const std::size_t from : {std::size_t{0}, line.size() / 2U, line.size()})
    {
        const std::string_view violation = from_tree_violation(parsed.value(), tree, line, from);
        if (!violation.empty())
        {
            return violation;
        }
    }
    return case_violation({pattern, std::string{line}});
}

// 組の数え（counts は当たりのある組・模型の歩数が尽きた組・命令の列の道を通る組）。
void tree_counted(const Tree &tree, std::string_view line, std::array<std::size_t, 3> &counts)
{
    TreeModel model(tree, line);
    const auto whole = model.matched(0U);
    counts.at(0) += whole.has_value() && whole.value().has_value() ? 1U : 0U;
    counts.at(1) += whole.has_value() ? 0U : 1U;
    const Parsed parsed = VimPattern::parse(tree_text(tree, 0), VimSearchDirection::forward,
                                            VimPatternCase::sensitive);
    counts.at(2) +=
        parsed.has_value() && parsed.value().route() == core::VimPatternRoute::program ? 1U : 0U;
}

// 模型との照合。破れた最初の組は綴りで出す。模型の歩数が尽きた組は数えずに数だけ見る。
void verify_branches_follow_the_model()
{
    Random random{8995U};
    std::size_t broken = 0;
    // 当たりのある組・模型の歩数が尽きた組・命令の列の道を通る組の数。
    std::array<std::size_t, 3> counts{};
    constexpr std::size_t cases = 6U * family_cases;
    for (std::size_t round = 0; round < cases; ++round)
    {
        const Tree tree = random_tree(random);
        const std::string line = model_line(random);
        tree_counted(tree, line, counts);
        const std::string_view violation = tree_violation(tree, line);
        if (!violation.empty() && broken++ == 0U)
        {
            std::fprintf(stderr, "  branch model: %.*s\n", static_cast<int>(violation.size()),
                         violation.data());
            print_bytes("pattern", tree_text(tree, 0));
            print_bytes("line", line);
        }
    }
    expect(broken == 0U, "branches and group multis match and capture like the branch model");
    expect(counts.at(0) >= cases / 4U && counts.at(2) >= cases / 2U && counts.at(1) <= cases / 20U,
           "the branch model cases take the program route, include matches and rarely run out");
}

// ---------------------------------------------------------------- 状態集合の道の答え（#1584）
// 状態集合の道（VimPatternEvaluation）を通るパターンを固定 seed で作り、行のすべての 1 文字の境
// から当てた答え（当たりの有無・始まり・終わり）を FNV-1a 64 で畳む。期待の値は、訪問済みの印を
// 1 文字ごとに埋め直していた直す前の評価器（main e43e93e8）がこの生成器の列に出した答えの畳み。
// 評価器の中の持ち方を変えても、最も左の開始・貪欲 / 最短の優先順・結合文字の途中の印が同じなら
// 値は変わらない。生成器・断片・回数を変えたら、直す前の木で測り直した値に置き換える。

// 状態集合の道を通る断片（`*`・量・最短・幅の無い原子・結合文字・多バイト）。
constexpr std::array<std::string_view, 32> state_fragments{{
    "a",           "b",         " ",
    "x",           ".",         "a*",
    "b*",          ".*",        "[ab]*",
    "[^a]*",       "\\w*",      "\\s*",
    "a\\+",        "b\\=",      ".\\{-}",
    "a\\{-1,}",    ".\\{-1,2}", "a\\{2}",
    "[ab]\\{1,3}", "a\\{,2}",   "\\<",
    "\\>",         "^",         "$",
    "\xC3\xA9",    "\xC3\xA9*", "\xE3\x81\x82*",
    ".\\=",        "\\S\\+",    "[^ ]\\{-1,}",
    "e\xCC\x81",   ".\\{2,}",
}};

// 行の材料（ASCII・2〜4 バイト・基底と結合文字・単独の結合文字）。どれも正しい UTF-8。
constexpr std::array<std::string_view, 10> state_line_pieces{{
    "a",
    "a",
    "b",
    " ",
    "x",
    "\xC3\xA9",
    "e\xCC\x81",
    "\xE3\x81\x82",
    "\xF0\x9F\x98\x80",
    "\xCC\x81",
}};

// 期待の値がこの数の列の畳みなので、rounds（50 倍の走り）を掛けない。
constexpr std::size_t state_cases = 3000U;
constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
// 直す前の評価器（main e43e93e8）の答えの畳み（seed 9011・state_cases 組）。
constexpr std::uint64_t state_answers_digest = 0x669ac3eccfd658c1ULL;

[[nodiscard]] std::string state_pattern(Random &random)
{
    const std::size_t pieces = 1U + below(random, 8U);
    std::string pattern;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        pattern.append(pick(random, state_fragments));
    }
    return pattern;
}

[[nodiscard]] std::string state_line(Random &random)
{
    const std::size_t pieces = below(random, 24U);
    std::string line;
    for (std::size_t piece = 0; piece < pieces; ++piece)
    {
        line.append(pick(random, state_line_pieces));
    }
    return line;
}

void folded(std::uint64_t &digest, std::size_t value)
{
    for (std::size_t shift = 0; shift < 64U; shift += 8U)
    {
        digest = (digest ^ ((static_cast<std::uint64_t>(value) >> shift) & 0xFFU)) * fnv_prime;
    }
}

// 1 組のすべての境からの答えを畳み、当たった数を返す。
[[nodiscard]] std::size_t state_answers_folded(std::uint64_t &digest, const VimPattern &pattern,
                                               std::string_view line)
{
    std::size_t found = 0;
    for (const std::size_t from : boundaries(line))
    {
        const Found match = pattern.matched(line, from);
        folded(digest, from);
        folded(digest, match.has_value() ? 1U : 0U);
        if (match.has_value())
        {
            folded(digest, match.value().begin);
            folded(digest, match.value().end);
            ++found;
        }
    }
    return found;
}

void verify_state_route_keeps_its_answers()
{
    Random random{9011U};
    std::uint64_t digest = fnv_offset;
    std::size_t states = 0;
    std::size_t found = 0;
    for (std::size_t round = 0; round < state_cases; ++round)
    {
        const std::string pattern = state_pattern(random);
        const std::string line = state_line(random);
        const Parsed parsed =
            VimPattern::parse(pattern, VimSearchDirection::forward, VimPatternCase::sensitive);
        folded(digest, parsed.has_value() ? 1U : 0U);
        if (!parsed.has_value() || parsed.value().route() == core::VimPatternRoute::sequence)
        {
            continue;
        }
        ++states;
        found += state_answers_folded(digest, parsed.value(), line);
    }
    if (digest != state_answers_digest)
    {
        std::fprintf(stderr, "  state route answers digest: %016llx (cases %zu, found %zu)\n",
                     static_cast<unsigned long long>(digest), states, found);
    }
    expect(digest == state_answers_digest,
           "the state route gives the answers of the evaluator before #1584");
    expect(states >= state_cases / 2U && found >= state_cases,
           "the state route cases go through the evaluator and include matches");
}

// 生成器そのものが決定的（同じ seed で同じ列）。
void verify_generators_are_deterministic()
{
    Random left{8999U};
    Random right{8999U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && mixed_case(left) == mixed_case(right) &&
               planted_case(left) == planted_case(right);
    }
    expect(same, "the generators give the same cases for the same seed");
}
} // namespace

void verify_vim_pattern_property_scope()
{
    verify_generators_are_deterministic();
    verify_family("patterns of supported fragments parse and match inside the line", 8981U,
                  supported_case, supported_violation);
    verify_family("mixed patterns parse to a value or a closed failure", 8982U, mixed_case,
                  case_violation);
    verify_family("random pattern bytes parse to a value or a closed failure", 8983U, byte_case,
                  case_violation);
    verify_family("patterns planted in the line match inside the line", 8984U, planted_case,
                  case_violation);
    verify_family("patterns on 4 KiB lines match inside the line", 8985U, large_case,
                  case_violation);
    verify_byte_cases_reach_the_contract();
    verify_unsupported_suffixes();
    verify_backtracking_is_bounded();
    verify_known_cases();
    verify_quantifiers_follow_the_model();
    verify_branches_follow_the_model();
    verify_state_route_keeps_its_answers();
}
} // namespace nenenib::tests
