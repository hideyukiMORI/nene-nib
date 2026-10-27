// scope `--palette-match-property` の性質の試験（#912・ADR 0091 の追補・ADR 0061 の決定 6）。
// 照合の核 subsequence_score の速道（ASCII だけの query で候補のバイト列をそのまま歩く）と
// コードポイントの道が、ランダムな候補（ASCII・日本語・記号・大小・語の区切り・2 / 4 バイトの文字・
// 名前だけと「場所・区切り・名前」の 3 つ組）× ランダムな ASCII の query（1〜8 文字・大小・記号・
// 空白）で、当たりの有無と点まで全件一致することを確かめる。壊れた UTF-8 の見本（過長の復号が
// ASCII の値になる列・孤立した継続バイト・先頭になれないバイト）も同じ答えにする。生成器は
// std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "Scopes.hpp"
#include "SubsequenceScore.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using core::byte_walkable_query;
using core::subsequence_score;
using core::subsequence_score_by_bytes;
using core::subsequence_score_by_code_points;
using Random = std::mt19937_64;

constexpr std::size_t samples = 10000U;

// 候補の 1 文字の見本。ASCII の英字・数字・区切り・記号と、2 / 3 / 4 バイトの文字と結合文字。
constexpr std::array<std::string_view, 30> candidate_pieces{{"a",
                                                             "b",
                                                             "f",
                                                             "x",
                                                             "A",
                                                             "B",
                                                             "F",
                                                             "X",
                                                             "0",
                                                             "1",
                                                             "9",
                                                             " ",
                                                             "_",
                                                             "-",
                                                             ".",
                                                             "\\",
                                                             "/",
                                                             ":",
                                                             "#",
                                                             "@",
                                                             "\xC3\xA9",
                                                             "\xC3\x89",
                                                             "\xE3\x81\x82",
                                                             "\xE6\x97\xA5",
                                                             "\xE6\x9C\xAC",
                                                             "\xEF\xBC\xA1",
                                                             "\xE3\x82\x99",
                                                             "\xF0\x9F\x98\x80",
                                                             "\xE2\x80\x8B",
                                                             "~"}};

// query の 1 文字の見本（ASCII だけ）。候補に無い字も混ぜて外れを作る。
constexpr std::string_view query_pieces{"abfxABFX019 _-.\\/:#@qZ"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::string candidate_text(Random &random, std::size_t longest)
{
    std::string text;
    const std::size_t length = below(random, longest + 1U);
    for (std::size_t index = 0; index < length; ++index)
    {
        text += candidate_pieces.at(below(random, candidate_pieces.size()));
    }
    return text;
}

[[nodiscard]] std::string query_text(Random &random)
{
    std::string text;
    const std::size_t length = 1U + below(random, 8U);
    for (std::size_t index = 0; index < length; ++index)
    {
        text.push_back(query_pieces.at(below(random, query_pieces.size())));
    }
    return text;
}

// 3 つの道（速道・コードポイントの道・入口の選び）が同じ答えか。
[[nodiscard]] bool paths_agree(std::string_view query, std::span<const std::string_view> command)
{
    const std::optional<std::size_t> expected = subsequence_score_by_code_points(query, command);
    return subsequence_score_by_bytes(query, command) == expected &&
           subsequence_score(query, command) == expected;
}

// 食い違い・当たり・外れの数（順に 0・1・2）。
using Tally = std::array<std::size_t, 3>;

void count(Tally &tally, std::string_view query, std::span<const std::string_view> command)
{
    if (!paths_agree(query, command))
    {
        ++tally.at(0);
    }
    ++tally.at(subsequence_score_by_code_points(query, command).has_value() ? 1 : 2);
}

void verify_random_candidates()
{
    Random random{9121U};
    Tally named{};
    Tally located{};
    for (std::size_t sample = 0; sample < samples; ++sample)
    {
        const std::string query = query_text(random);
        const std::string label = candidate_text(random, 16U);
        const std::string detail = candidate_text(random, 48U);
        const std::array<std::string_view, 1> name_only{label};
        const std::array<std::string_view, 3> three{detail, "\\", label};
        count(named, query, name_only);
        count(located, query, three);
    }
    expect(named.at(0) == 0U, "the byte walk scores names like the code point walk");
    expect(located.at(0) == 0U,
           "the byte walk scores location, separator and name like the code point walk");
    expect(named.at(1) > samples / 20U && named.at(2) > samples / 20U,
           "the random names both hit and miss");
    expect(located.at(1) > samples / 20U && located.at(2) > samples / 20U,
           "the random locations both hit and miss");
}

// 壊れた列も同じ答え（過長の E0 41 41 は 'A' と復号される・孤立した継続バイト・C0 / F5）。
void verify_broken_candidates()
{
    constexpr std::array<std::string_view, 8> broken{{"\xE0\x41\x41", "\x80\x61", "a\x80\x61",
                                                      "\xC0\x61", "\xF5\x61", "\xF0\x40\x41\x41",
                                                      "\xE3\x81\x82\x80\x41", "\xC2\x41"}};
    constexpr std::array<std::string_view, 6> queries{{"a", "A", "aa", "aA", "@a", " a"}};
    bool agreed = true;
    for (const std::string_view candidate : broken)
    {
        for (const std::string_view query : queries)
        {
            const std::array<std::string_view, 1> name_only{candidate};
            const std::array<std::string_view, 3> three{candidate, "\\", candidate};
            agreed = agreed && paths_agree(query, name_only) && paths_agree(query, three);
        }
    }
    expect(agreed, "broken UTF-8 candidates get the same answer on both walks");
}

void verify_walkable_queries()
{
    expect(byte_walkable_query("f0") && byte_walkable_query("A b\\_~"),
           "an ASCII query takes the byte walk");
    expect(!byte_walkable_query("f\xE3\x81\x82") && !byte_walkable_query("\xC3\xA9"),
           "a query with a non-ASCII character keeps the code point walk");
    expect(!byte_walkable_query(std::string_view{"a\0", 2}),
           "a query with NUL keeps the code point walk");
    const std::array<std::string_view, 1> japanese{"\xE6\x97\xA5\xE6\x9C\xAC"};
    expect(subsequence_score("\xE6\x9C\xAC", japanese) == std::optional<std::size_t>{18U},
           "a Japanese query still matches on the code point walk");
    const std::array<std::string_view, 1> name{"f0001.txt"};
    expect(subsequence_score("F0", name) == std::optional<std::size_t>{9U},
           "the byte walk folds ASCII case and scores the gap");
}
} // namespace

void verify_palette_match_property_scope()
{
    verify_walkable_queries();
    verify_broken_candidates();
    verify_random_candidates();
}
} // namespace nenenib::tests
