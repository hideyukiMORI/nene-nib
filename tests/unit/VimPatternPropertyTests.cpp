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
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimPatternMatch.hpp"
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
    "*",
    "**",
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
    {"\\)", VimPatternFailure::group},
    {"\\(a*\\)*b", VimPatternFailure::group},
    {"\\|", VimPatternFailure::branch},
    {"\\+", VimPatternFailure::quantifier},
    {"\\=", VimPatternFailure::quantifier},
    {"\\?", VimPatternFailure::quantifier},
    {"\\{2,3}", VimPatternFailure::quantifier},
    {"\\{}", VimPatternFailure::quantifier},
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
constexpr std::array<std::string_view, 6> explosive_patterns{{
    "a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b",
    ".*.*.*.*.*.*.*.*.*.*.*.*.*.*.*.*b",
    "[ab]*a*[a-z]*\\w*.*b",
    "a*a*a*a*a*a*a*a*$b",
    "\\<a*a*a*a*a*a*a*a*\\>b",
    "^.*a*.*a*.*a*.*c",
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
// `\(a*\)*b` は group で断られる。
void verify_backtracking_is_bounded()
{
    expect(
        VimPattern::parse("\\(a*\\)*b", VimSearchDirection::forward, VimPatternCase::sensitive) ==
            std::unexpected(VimPatternFailure::group),
        "a nested star group is refused as group");
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
}
} // namespace nenenib::tests
