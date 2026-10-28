// scope `--command-match-property` の性質の試験（#947・ADR 0060 の決定 4・ADR 0091・ADR 0042 決定
// 2）。 Ctrl+P の絞り込み listed_positions（全件と続きの照合の 2 本）に、固定 seed
// の決定的な生成器で作った 候補の列（名前と場所は ASCII・日本語・大小の混在・場所の無し・長さ 1〜64
// / 0〜64・出どころの印は 5 種と無し・0〜200 件）と query（ASCII・日本語・大小・空白・0〜12
// 文字）を当て、次を守る。
//   1. 結果は entries の中の位置で、重複せず、scope に入る候補だけ（commands と operations は空）。
//   2. query が空なら scope に入る候補が列の順でそのまま。
//   3. 結果の集合は、名前か「場所 \ 名前」に query
//   が部分列として入る候補と一致する（判定は試験の中の
//      素朴な歩き 1 本・空白は飛ばし・大小を畳むのは ASCII の英字だけ）。
//   4. 順は「名前に当たる候補が先・同じ段は subsequence_score の点の小さい順・同点は列の順」。
//   5. query に 1 文字足した結果の集合は、足す前の結果の部分集合。
//   6. 続きの照合に前の結果（または全位置）を渡した答えは、全件の照合と同じ列。
//   7. 同じ入力で同じ答え。query の ASCII の大小を入れ替えても同じ列。全部 ASCII の入力と、名前と
//      query の末尾に非 ASCII の 1
//      文字を足した入力（コードポイントの道）で、当たる候補の集合が同じ。
// 製品の点の値は信じ、並びは試験が比べる。生成器は std::mt19937_64 の剰余だけを使う。
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteScope.hpp"
#include "Scopes.hpp"
#include "SubsequenceScore.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <numeric>
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
using core::CommandChoice;
using core::PaletteOrigin;
using core::PaletteScope;
using Random = std::mt19937_64;
// 1 文字の見本（コードポイントと UTF-8）。
using Letter = std::pair<char32_t, std::string_view>;
// 綴り（模型が読むコードポイントの列と、製品が読む UTF-8）。
using Spelled = std::pair<std::u32string, std::string>;
using Origin = std::optional<PaletteOrigin>;
// 模型の候補（名前・場所・出どころの印）。場所の空は「場所なし」。
using Modeled = std::tuple<Spelled, Spelled, Origin>;
// 模型の列と、同じ並びの製品の候補の列。
using World = std::pair<std::vector<Modeled>, std::vector<CommandChoice>>;
using Positions = std::vector<std::size_t>;
// 並びの鍵（段・点・位置）。段は名前に当たれば 0、「場所 \ 名前」だけなら 1。
using Ranked = std::tuple<std::size_t, std::size_t, std::size_t>;
// 不変量 1〜7 の破れの数。
using Tally = std::array<std::size_t, 7>;
// 生成器が届いた枝（名前に当たる・場所だけに当たる・外れ・結果が空でない照合）。
using Coverage = std::array<std::size_t, 4>;
// 破れの数・届いた枝・いまの case の番号。
using Ledger = std::tuple<Tally, Coverage, std::size_t>;

// Debug で 1 秒以内に収まる回数（#947）。50 倍の走りは rounds を 50 にして手元で回し、commit
// しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t cases = 28U * rounds;
constexpr std::size_t entry_limit = 200U;
constexpr std::size_t text_limit = 64U;
constexpr std::size_t query_limit = 12U;

constexpr std::array<Letter, 14> ascii_letters{{{U'a', "a"},
                                                {U'b', "b"},
                                                {U'f', "f"},
                                                {U'x', "x"},
                                                {U'A', "A"},
                                                {U'B', "B"},
                                                {U'F', "F"},
                                                {U'X', "X"},
                                                {U'0', "0"},
                                                {U'1', "1"},
                                                {U' ', " "},
                                                {U'.', "."},
                                                {U'_', "_"},
                                                {U'\\', "\\"}}};
// query にだけ出る字（候補に無い字で外れを作る）。
constexpr std::array<Letter, 2> absent_letters{{{U'q', "q"}, {U'Z', "Z"}}};
// 非 ASCII の字。é / É と全角の Ａ / ａ は大小を畳まない側、U+3099 は結合文字、絵文字は 4 バイト。
constexpr std::array<Letter, 9> other_letters{{{U'é', "\xC3\xA9"},
                                               {U'É', "\xC3\x89"},
                                               {U'あ', "\xE3\x81\x82"},
                                               {U'日', "\xE6\x97\xA5"},
                                               {U'本', "\xE6\x9C\xAC"},
                                               {U'Ａ', "\xEF\xBC\xA1"},
                                               {U'ａ', "\xEF\xBD\x81"},
                                               {U'゙', "\xE3\x82\x99"},
                                               {U'\U0001F600', "\xF0\x9F\x98\x80"}}};
constexpr std::array<PaletteOrigin, 5> origins{{PaletteOrigin::tab, PaletteOrigin::bookmarked_tab,
                                                PaletteOrigin::bookmark, PaletteOrigin::history,
                                                PaletteOrigin::folder}};
constexpr std::array<PaletteScope, 7> all_scopes{
    {PaletteScope::files, PaletteScope::tabs, PaletteScope::bookmarks, PaletteScope::history,
     PaletteScope::folder, PaletteScope::commands, PaletteScope::operations}};
constexpr char32_t widening = U'日';

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// ASCII はそのまま 1 バイト（大小を入れ替えた字も綴れる）、ほかは見本の表から引く。
[[nodiscard]] std::string utf8_of(char32_t value)
{
    if (value < U'')
    {
        return std::string(1U, static_cast<char>(value));
    }
    const auto found = std::ranges::find(other_letters, value, &Letter::first);
    return found != other_letters.end() ? std::string{found->second} : std::string{};
}

[[nodiscard]] Spelled spelled_from(std::u32string code_points)
{
    std::string text;
    for (const char32_t value : code_points)
    {
        text += utf8_of(value);
    }
    return {std::move(code_points), std::move(text)};
}

[[nodiscard]] Letter letter_of(Random &random, bool ascii_only, bool query)
{
    const std::size_t extra = query ? absent_letters.size() : 0U;
    const std::size_t wide = ascii_only ? 0U : other_letters.size();
    std::size_t index = below(random, ascii_letters.size() + extra + wide);
    if (index < ascii_letters.size())
    {
        return ascii_letters.at(index);
    }
    index -= ascii_letters.size();
    return index < extra ? absent_letters.at(index) : other_letters.at(index - extra);
}

[[nodiscard]] Spelled random_text(Random &random, std::size_t shortest, bool ascii_only)
{
    std::u32string text;
    const std::size_t length = shortest + below(random, text_limit - shortest + 1U);
    for (std::size_t index = 0; index < length; ++index)
    {
        text.push_back(letter_of(random, ascii_only, false).first);
    }
    return spelled_from(std::move(text));
}

[[nodiscard]] Origin origin_of(Random &random)
{
    const std::size_t index = below(random, origins.size() + 1U);
    return index < origins.size() ? Origin{origins.at(index)} : std::nullopt;
}

[[nodiscard]] CommandChoice choice_of(const Modeled &entry)
{
    const std::string &detail = std::get<1>(entry).second;
    return CommandChoice{fixed_text(std::get<0>(entry).second),
                         std::string{},
                         core::CommandChoiceKind::execute,
                         detail.empty() ? std::nullopt : std::optional{fixed_text(detail)},
                         std::get<2>(entry),
                         std::nullopt,
                         {}};
}

[[nodiscard]] World world_of(std::vector<Modeled> modeled)
{
    std::vector<CommandChoice> choices;
    choices.reserve(modeled.size());
    for (const Modeled &entry : modeled)
    {
        choices.push_back(choice_of(entry));
    }
    return {std::move(modeled), std::move(choices)};
}

[[nodiscard]] std::vector<Modeled> modeled_entries(Random &random, bool ascii_only)
{
    std::vector<Modeled> entries;
    const std::size_t count = below(random, entry_limit + 1U);
    for (std::size_t index = 0; index < count; ++index)
    {
        Spelled label = random_text(random, 1U, ascii_only);
        Spelled detail = random_text(random, 0U, ascii_only);
        entries.emplace_back(std::move(label), std::move(detail), origin_of(random));
    }
    return entries;
}

[[nodiscard]] std::u32string located_of(const Modeled &entry)
{
    return std::get<1>(entry).first + U'\\' + std::get<0>(entry).first;
}

[[nodiscard]] std::u32string case_swapped(std::u32string text)
{
    for (char32_t &value : text)
    {
        if (value >= U'a' && value <= U'z')
        {
            value -= U'a' - U'A';
        }
        else if (value >= U'A' && value <= U'Z')
        {
            value += U'a' - U'A';
        }
    }
    return text;
}

// 候補の名前か「場所 \ 名前」から拾った部分列（大小を入れ替え・余計な 1 文字を足すこともある）。
[[nodiscard]] Spelled picked_query(Random &random, std::u32string_view source, std::size_t length,
                                   bool ascii_only)
{
    std::u32string picked;
    for (const char32_t value : source)
    {
        if (picked.size() < length && below(random, source.size()) < length)
        {
            picked.push_back(value);
        }
    }
    if (below(random, 2U) == 0U)
    {
        picked = case_swapped(std::move(picked));
    }
    if (picked.size() < query_limit && below(random, 4U) == 0U)
    {
        picked.insert(below(random, picked.size() + 1U), 1U,
                      letter_of(random, ascii_only, true).first);
    }
    return spelled_from(std::move(picked));
}

[[nodiscard]] Spelled query_of(Random &random, const std::vector<Modeled> &entries, bool ascii_only)
{
    const std::size_t length = below(random, query_limit + 1U);
    const std::size_t mode = below(random, 3U);
    if (mode == 0U || entries.empty())
    {
        std::u32string text;
        for (std::size_t index = 0; index < length; ++index)
        {
            text.push_back(letter_of(random, ascii_only, true).first);
        }
        return spelled_from(std::move(text));
    }
    const Modeled &entry = entries.at(below(random, entries.size()));
    const std::u32string source = mode == 1U ? std::get<0>(entry).first : located_of(entry);
    return picked_query(random, source, length, ascii_only);
}

// --- 模型 ---------------------------------------------------------------------------------------

[[nodiscard]] char32_t folded(char32_t value) noexcept
{
    return value >= U'A' && value <= U'Z' ? value + (U'a' - U'A') : value;
}

// 素朴な部分列の判定。query の空白は飛ばし、ASCII の英字だけ大小を畳む。
[[nodiscard]] bool naive_subsequence(std::u32string_view query, std::u32string_view text)
{
    std::size_t at = 0;
    for (const char32_t wanted : query)
    {
        if (wanted == U' ')
        {
            continue;
        }
        while (at < text.size() && folded(text.at(at)) != folded(wanted))
        {
            ++at;
        }
        if (at == text.size())
        {
            return false;
        }
        ++at;
    }
    return true;
}

// scope の注釈（PaletteScope.hpp・ADR 0060 の決定 3・ADR 0063）を手で写したもの。
[[nodiscard]] bool model_in_scope(const Origin &origin, PaletteScope scope)
{
    if (!origin.has_value())
    {
        return false;
    }
    const PaletteOrigin mark = origin.value();
    switch (scope)
    {
    case PaletteScope::files:
        return true;
    case PaletteScope::tabs:
        return mark == PaletteOrigin::tab || mark == PaletteOrigin::bookmarked_tab;
    case PaletteScope::bookmarks:
        return mark == PaletteOrigin::bookmark || mark == PaletteOrigin::bookmarked_tab;
    case PaletteScope::history:
        return mark == PaletteOrigin::history;
    case PaletteScope::folder:
        return mark == PaletteOrigin::folder;
    case PaletteScope::commands:
    case PaletteScope::operations:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] std::optional<std::size_t> model_tier(std::u32string_view query, const Modeled &entry)
{
    if (naive_subsequence(query, std::get<0>(entry).first))
    {
        return 0U;
    }
    if (!std::get<1>(entry).first.empty() && naive_subsequence(query, located_of(entry)))
    {
        return 1U;
    }
    return std::nullopt;
}

// 製品の点（段 0 は名前だけ、段 1 は場所・区切り・名前）。空の query は 0。
[[nodiscard]] std::optional<std::size_t> product_score(std::string_view query, const Modeled &entry,
                                                       std::size_t tier)
{
    if (query.empty())
    {
        return 0U;
    }
    if (tier == 0U)
    {
        const std::array<std::string_view, 1> name{std::get<0>(entry).second};
        return core::subsequence_score(query, name);
    }
    const std::array<std::string_view, 3> located{std::get<1>(entry).second, "\\",
                                                  std::get<0>(entry).second};
    return core::subsequence_score(query, located);
}

[[nodiscard]] Positions in_scope_positions(const World &world, PaletteScope scope)
{
    Positions positions;
    for (std::size_t position = 0; position < world.first.size(); ++position)
    {
        if (model_in_scope(std::get<2>(world.first.at(position)), scope))
        {
            positions.push_back(position);
        }
    }
    return positions;
}

[[nodiscard]] Positions model_matches(const World &world, PaletteScope scope,
                                      std::u32string_view query)
{
    Positions matched;
    for (const std::size_t position : in_scope_positions(world, scope))
    {
        if (model_tier(query, world.first.at(position)).has_value())
        {
            matched.push_back(position);
        }
    }
    return matched;
}

[[nodiscard]] Positions sorted_of(Positions positions)
{
    std::ranges::sort(positions);
    return positions;
}

[[nodiscard]] Positions all_positions(std::size_t count)
{
    Positions positions(count);
    std::iota(positions.begin(), positions.end(), std::size_t{0});
    return positions;
}

// --- 不変量 -------------------------------------------------------------------------------------

void violated(Ledger &ledger, std::size_t invariant, PaletteScope scope, const Spelled &query)
{
    Tally &tally = std::get<0>(ledger);
    if (tally.at(invariant) == 0U)
    {
        std::fprintf(stderr,
                     "  command match invariant %zu broke: case %zu scope %u query \"%s\"\n",
                     invariant + 1U, std::get<2>(ledger), static_cast<unsigned>(scope),
                     query.second.c_str());
    }
    ++tally.at(invariant);
}

[[nodiscard]] bool valid_positions(const Positions &listed, const World &world, PaletteScope scope)
{
    std::vector<bool> seen(world.first.size(), false);
    for (const std::size_t position : listed)
    {
        if (position >= seen.size() || seen.at(position) ||
            !model_in_scope(std::get<2>(world.first.at(position)), scope))
        {
            return false;
        }
        seen.at(position) = true;
    }
    return true;
}

[[nodiscard]] bool ranked_in_order(const Positions &listed, const World &world,
                                   const Spelled &query)
{
    std::vector<Ranked> keys;
    for (const std::size_t position : listed)
    {
        const Modeled &entry = world.first.at(position);
        const auto tier = model_tier(query.first, entry);
        const auto score =
            tier.has_value() ? product_score(query.second, entry, tier.value()) : std::nullopt;
        if (!score.has_value())
        {
            return false;
        }
        keys.emplace_back(tier.value(), score.value(), position);
    }
    return std::ranges::is_sorted(keys);
}

// 不変量 1〜4 を見て、結果を返す。
[[nodiscard]] Positions checked_listing(const World &world, PaletteScope scope,
                                        const Spelled &query, Ledger &ledger)
{
    Positions listed = core::listed_positions(world.second, scope, query.second);
    if (!valid_positions(listed, world, scope))
    {
        violated(ledger, 0U, scope, query);
    }
    if (query.first.empty() && listed != in_scope_positions(world, scope))
    {
        violated(ledger, 1U, scope, query);
    }
    if (sorted_of(listed) != model_matches(world, scope, query.first))
    {
        violated(ledger, 2U, scope, query);
    }
    if (!ranked_in_order(listed, world, query))
    {
        violated(ledger, 3U, scope, query);
    }
    return listed;
}

[[nodiscard]] bool all_ascii(const World &world, const Spelled &query)
{
    const auto ascii = [](std::u32string_view text)
    { return std::ranges::all_of(text, [](char32_t value) { return value < U'\x80'; }); };
    return ascii(query.first) &&
           std::ranges::all_of(
               world.first, [&ascii](const Modeled &entry)
               { return ascii(std::get<0>(entry).first) && ascii(std::get<1>(entry).first); });
}

// 全部 ASCII の入力の名前と query の末尾に同じ非 ASCII の 1 文字を足しても、当たる集合は同じ。
// 足す前の query（バイトの道）を足した後の名前に当てても同じ。
[[nodiscard]] bool widened_alike(const World &world, PaletteScope scope, const Spelled &query)
{
    std::vector<Modeled> modeled = world.first;
    for (Modeled &entry : modeled)
    {
        std::get<0>(entry) = spelled_from(std::get<0>(entry).first + widening);
    }
    const World wide = world_of(std::move(modeled));
    const Spelled wide_query = spelled_from(query.first + widening);
    const Positions narrow = sorted_of(core::listed_positions(world.second, scope, query.second));
    return sorted_of(core::listed_positions(wide.second, scope, wide_query.second)) == narrow &&
           sorted_of(core::listed_positions(wide.second, scope, query.second)) == narrow;
}

// 不変量 6（全位置を渡す続きの照合）と 7。
void check_stable(const World &world, PaletteScope scope, const Spelled &query, Ledger &ledger)
{
    const Positions listed = core::listed_positions(world.second, scope, query.second);
    if (core::listed_positions(world.second, scope, query.second,
                               all_positions(world.second.size())) != listed)
    {
        violated(ledger, 5U, scope, query);
    }
    const Spelled swapped = spelled_from(case_swapped(query.first));
    if (core::listed_positions(world.second, scope, query.second) != listed ||
        core::listed_positions(world.second, scope, swapped.second) != listed)
    {
        violated(ledger, 6U, scope, query);
    }
    if (all_ascii(world, query) && !widened_alike(world, scope, query))
    {
        violated(ledger, 6U, scope, query);
    }
}

void count_coverage(const World &world, PaletteScope scope, const Spelled &query, Ledger &ledger)
{
    Coverage &coverage = std::get<1>(ledger);
    for (const std::size_t position : in_scope_positions(world, scope))
    {
        const auto tier = model_tier(query.first, world.first.at(position));
        ++coverage.at(tier.has_value() ? tier.value() : 2U);
    }
    if (!core::listed_positions(world.second, scope, query.second).empty())
    {
        ++coverage.at(3);
    }
}

void verify_case(Random &random, Ledger &ledger)
{
    const std::size_t number = std::get<2>(ledger);
    const bool ascii_only = number % 3U == 0U;
    const World world = world_of(modeled_entries(random, ascii_only));
    const Spelled query = query_of(random, world.first, ascii_only);
    const PaletteScope scope = all_scopes.at(number % all_scopes.size());
    Positions previous = core::listed_positions(world.second, scope, "");
    for (std::size_t length = 0; length <= query.first.size(); ++length)
    {
        const Spelled prefix = spelled_from(query.first.substr(0, length));
        const Positions listed = checked_listing(world, scope, prefix, ledger);
        if (!std::ranges::includes(sorted_of(previous), sorted_of(listed)))
        {
            violated(ledger, 4U, scope, prefix);
        }
        if (core::listed_positions(world.second, scope, prefix.second, previous) != listed)
        {
            violated(ledger, 5U, scope, prefix);
        }
        previous = listed;
    }
    check_stable(world, scope, query, ledger);
    count_coverage(world, scope, query, ledger);
}

void verify_random_cases()
{
    Random random{9470U};
    Ledger ledger{};
    for (std::size_t number = 0; number < cases; ++number)
    {
        std::get<2>(ledger) = number;
        verify_case(random, ledger);
    }
    const Tally &tally = std::get<0>(ledger);
    expect(tally.at(0) == 0U, "listed positions are distinct entries inside the scope");
    expect(tally.at(1) == 0U, "an empty query lists the scope in entry order");
    expect(tally.at(2) == 0U, "the listed set is the naive subsequence matches");
    expect(tally.at(3) == 0U, "names come first, then score, then entry order");
    expect(tally.at(4) == 0U, "typing one more letter narrows the listed set");
    expect(tally.at(5) == 0U, "continuing from earlier positions lists the same order");
    expect(tally.at(6) == 0U, "the listing is stable under repetition, ASCII case and non-ASCII");
    const Coverage &coverage = std::get<1>(ledger);
    expect(coverage.at(0) > cases && coverage.at(1) > 0U && coverage.at(2) > cases &&
               coverage.at(3) > cases / 4U,
           "the random cases reach name hits, location hits, misses and non-empty lists");
}

// --- 例 -----------------------------------------------------------------------------------------

[[nodiscard]] Modeled example(std::u32string label, std::u32string detail, PaletteOrigin origin)
{
    return {spelled_from(std::move(label)), spelled_from(std::move(detail)), origin};
}

// 生成器の外の確かめ（名前が先・同点は列の順・非 ASCII の大小は畳まない・印の無い候補は出ない）。
void verify_fixed_rules()
{
    const World world = world_of({example(U"x", U"ab", PaletteOrigin::tab),
                                  example(U"xaxb", U"", PaletteOrigin::history),
                                  example(U"xaxb", U"", PaletteOrigin::folder),
                                  example(U"ÉＡ", U"", PaletteOrigin::bookmark)});
    expect(core::listed_positions(world.second, PaletteScope::files, "AB") == Positions{1U, 2U, 0U},
           "names before locations, ties in entry order, ASCII case folded");
    expect(core::listed_positions(world.second, PaletteScope::files, "\xC3\xA9").empty() &&
               core::listed_positions(world.second, PaletteScope::files, "\xEF\xBD\x81").empty(),
           "non-ASCII letters keep their case");
    expect(core::listed_positions(world.second, PaletteScope::files, "\xC3\x89 \xEF\xBC\xA1") ==
               Positions{3U},
           "non-ASCII letters match themselves and query spaces are skipped");
    expect(core::listed_positions(world.second, PaletteScope::commands, "").empty() &&
               core::listed_positions(world.second, PaletteScope::bookmarks, "") == Positions{3U},
           "commands list nothing and bookmarks list their own origin");
}
} // namespace

void verify_command_match_property_scope()
{
    verify_fixed_rules();
    verify_random_cases();
}
} // namespace nenenib::tests
