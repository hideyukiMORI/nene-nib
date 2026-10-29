// scope `--vim-number-property` の性質の試験（#1038・ADR 0015 の決定 14）。`Ctrl-a` `Ctrl-x` の
// 純関数 vim_find_number / vim_find_selected_number / vim_incremented_text に、固定 seed の決定的な
// 生成器で作った 1 行（10 進・16 進・2 進の桁と接頭辞・`-`・英字・空白・全角・かな・0〜40 片）と
// キャレットを当て、次を守る。
//   1. 見つけた範囲は行の中にあり、空でなく、範囲の本文は「`-`? 桁」か「`0x` `0X` `0b` `0B`
//   桁」の形。
//   2. 窓の外を読まない: キャレットを含む行の一部の窓は、行の全体と同じ答え（位置は窓の始まりだけ
//      ずれる）か window_short を返す。窓を倍にしていけば必ず行の全体と同じ答えに届く。
//   3. 選択の 1 行: 見つけた範囲は選択の始まりより手前に出ず、bounded なら選択の終わりを越えない。
//   4. 値の往復: 溢れていない数を k 増やした本文を単独の行として読み直すと値が k 増えていて、
//      そこから k 減らした本文を読み直すと元の値（10 進は符号つき・16 進と 2 進は 64 bit の剰余）。
//   5. 増減した本文は元の範囲の桁数（符号と接頭辞を除く）より短くならない（0 で始まる数）。
// 生成器は std::mt19937_64 の剰余だけを使う。
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "VimNumber.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::VimNumberForm;
using core::VimNumberMiss;
using core::VimNumberSpan;
using core::VimNumberWindow;
using Random = std::mt19937_64;
using Found = std::expected<VimNumberSpan, VimNumberMiss>;
// 不変量 1〜5 の破れの数。
using Tally = std::array<std::size_t, 5>;
// 生成器が届いた枝（10 進・16 進・2 進・負・見つからない・窓が足りない）。
using Coverage = std::array<std::size_t, 6>;
using Ledger = std::pair<Tally, Coverage>;
// 符号つきの値（負か・大きさ）。0 は負にしない。
using Signed = std::pair<bool, std::uint64_t>;

// Debug で 1 秒以内に収まる回数。50 倍の走りは rounds を 50 にして手元で回し、commit しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t cases = 4000U * rounds;
constexpr std::size_t piece_limit = 40U;
constexpr std::uint64_t amount_limit = 1000U;
constexpr std::uint64_t safe_magnitude = std::uint64_t{1} << 62U;

constexpr std::array<std::string_view, 24> pieces{{"0",
                                                   "1",
                                                   "7",
                                                   "9",
                                                   "0",
                                                   "1",
                                                   "a",
                                                   "f",
                                                   "F",
                                                   "x",
                                                   "X",
                                                   "b",
                                                   "B",
                                                   "-",
                                                   "-",
                                                   " ",
                                                   "g",
                                                   "z",
                                                   "0x",
                                                   "0b",
                                                   "0X",
                                                   "\xE3\x81\x82",
                                                   "\xEF\xBC\x91",
                                                   "\t"}};

[[nodiscard]] std::size_t pick(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

[[nodiscard]] std::string generated_line(Random &random)
{
    std::string line;
    const std::size_t count = pick(random, piece_limit + 1U);
    for (std::size_t piece = 0; piece < count; ++piece)
    {
        line += pieces.at(pick(random, pieces.size()));
    }
    return line;
}

// UTF-8 の文字の頭のバイト位置からひとつ選ぶ（キャレットは文字の頭にしか置かれない）。
[[nodiscard]] std::size_t character_start(Random &random, std::string_view line)
{
    std::size_t at = pick(random, line.size());
    while (at > 0 && (static_cast<unsigned char>(line.at(at)) & 0xC0U) == 0x80U)
    {
        --at;
    }
    return at;
}

[[nodiscard]] VimNumberWindow whole(std::string_view line, std::size_t at)
{
    return VimNumberWindow{line, at, true, true};
}

[[nodiscard]] bool fits(char value, VimNumberForm form)
{
    const bool decimal = value >= '0' && value <= '9';
    const char lower = static_cast<char>(value | 0x20);
    switch (form)
    {
    case VimNumberForm::decimal:
        return decimal;
    case VimNumberForm::hexadecimal:
        return decimal || (lower >= 'a' && lower <= 'f');
    case VimNumberForm::binary:
        return value == '0' || value == '1';
    }
    return false;
}

[[nodiscard]] bool digits_only(std::string_view text, VimNumberForm form)
{
    return !text.empty() &&
           std::ranges::all_of(text, [form](char value) { return fits(value, form); });
}

// 接頭辞 `0x` `0b` を外した桁。形が合わなければ空。
[[nodiscard]] std::string_view without_prefix(std::string_view text, VimNumberForm form)
{
    if (form == VimNumberForm::decimal)
    {
        return text;
    }
    const char letter = form == VimNumberForm::hexadecimal ? 'x' : 'b';
    if (text.size() < 3U || text.at(0) != '0' || (text.at(1) | 0x20) != letter)
    {
        return {};
    }
    return text.substr(2U);
}

// 1: 範囲が行の中で、本文が数の形。
[[nodiscard]] bool well_formed(std::string_view line, const VimNumberSpan &span)
{
    if (span.begin >= span.end || span.end > line.size())
    {
        return false;
    }
    std::string_view text = line.substr(span.begin, span.end - span.begin);
    if (span.negative && text.front() != '-')
    {
        return false;
    }
    text.remove_prefix(span.negative ? 1U : 0U);
    return digits_only(without_prefix(text, span.form), span.form);
}

[[nodiscard]] bool same_answer(const Found &part, const Found &all, std::size_t shift)
{
    if (!part || !all)
    {
        return !part && !all && part.error() == all.error();
    }
    const VimNumberSpan &left = part.value();
    const VimNumberSpan &right = all.value();
    return left.begin + shift == right.begin && left.end + shift == right.end &&
           left.magnitude == right.magnitude && left.form == right.form &&
           left.negative == right.negative && left.upper_digits == right.upper_digits;
}

// 2: キャレットを含む窓を広げながら読む。窓の答えは window_short か行の全体と同じ。
[[nodiscard]] bool window_agrees(Random &random, std::string_view line, std::size_t at,
                                 Ledger &ledger)
{
    const Found all = core::vim_find_number(whole(line, at));
    Coverage &coverage = ledger.second;
    std::size_t back = pick(random, at + 1U);
    std::size_t ahead = 1U + pick(random, line.size() - at);
    while (true)
    {
        const std::size_t begin = at - back;
        const std::size_t end = at + ahead;
        const std::string copy{line.substr(begin, end - begin)};
        const Found part =
            core::vim_find_number(VimNumberWindow{copy, back, begin == 0, end == line.size()});
        if (part || part.error() != VimNumberMiss::window_short)
        {
            return same_answer(part, all, begin);
        }
        ++coverage.at(5);
        back = std::min(at, back * 2U + 1U);
        ahead = std::min(line.size() - at, ahead * 2U);
    }
}

// 3: 選択の 1 行の答えは選択の中。
[[nodiscard]] bool selection_holds(Random &random, std::string_view line, std::size_t at)
{
    const std::size_t length = pick(random, line.size() - at + 1U);
    const bool bounded = pick(random, 2U) == 0U;
    const Found found = core::vim_find_selected_number(whole(line, at), length, bounded);
    if (!found)
    {
        return found.error() == VimNumberMiss::absent;
    }
    const VimNumberSpan &span = found.value();
    return well_formed(line, span) && span.begin >= at && (!bounded || span.end <= at + length);
}

[[nodiscard]] Signed signed_of(const VimNumberSpan &span)
{
    return {span.negative && span.magnitude != 0, span.magnitude};
}

[[nodiscard]] Signed shifted(Signed value, std::uint64_t amount, bool decimal)
{
    if (!decimal)
    {
        return {false, value.second + amount};
    }
    if (!value.first)
    {
        return {false, value.second + amount};
    }
    if (value.second > amount)
    {
        return {true, value.second - amount};
    }
    return {false, amount - value.second};
}

// 符号と接頭辞を除いた桁の数。
[[nodiscard]] std::size_t digit_count(std::string_view text, VimNumberForm form)
{
    const std::size_t sign = !text.empty() && text.front() == '-' ? 1U : 0U;
    const std::size_t prefix = form == VimNumberForm::decimal ? 0U : 2U;
    return text.size() - sign - prefix;
}

[[nodiscard]] std::size_t digit_count(const VimNumberSpan &span)
{
    const std::size_t prefix = span.form == VimNumberForm::decimal ? 0U : 2U;
    return span.end - span.begin - (span.negative ? 1U : 0U) - prefix;
}

[[nodiscard]] Found standalone(const std::string &text)
{
    return core::vim_find_number(whole(text, 0));
}

// 4・5: 値の往復と桁数。
[[nodiscard]] std::pair<bool, bool> round_trips(Random &random, const VimNumberSpan &span)
{
    if (span.overflow || span.magnitude >= safe_magnitude)
    {
        return {true, true};
    }
    const std::uint64_t amount = 1U + random() % amount_limit;
    const bool decimal = span.form == VimNumberForm::decimal;
    const std::string up = core::vim_incremented_text(span, amount, false);
    const Found raised = standalone(up);
    if (!raised || signed_of(raised.value()) != shifted(signed_of(span), amount, decimal))
    {
        return {false, true};
    }
    const std::string down = core::vim_incremented_text(raised.value(), amount, true);
    const Found lowered = standalone(down);
    const bool back = lowered && signed_of(lowered.value()) == signed_of(span);
    const bool wide = !span.zero_led || digit_count(up, span.form) >= digit_count(span);
    return {back, wide};
}

void count_reach(const Found &found, Coverage &coverage)
{
    if (!found)
    {
        ++coverage.at(4);
        return;
    }
    ++coverage.at(static_cast<std::size_t>(found.value().form));
    coverage.at(3) += found.value().negative ? 1U : 0U;
}

void verify_case(Random &random, Ledger &ledger)
{
    auto &[tally, coverage] = ledger;
    const std::string line = generated_line(random);
    if (line.empty())
    {
        return;
    }
    const std::size_t at = character_start(random, line);
    const Found all = core::vim_find_number(whole(line, at));
    count_reach(all, coverage);
    tally.at(0) += all && !well_formed(line, all.value()) ? 1U : 0U;
    tally.at(1) += window_agrees(random, line, at, ledger) ? 0U : 1U;
    tally.at(2) += selection_holds(random, line, at) ? 0U : 1U;
    if (all)
    {
        const auto [back, wide] = round_trips(random, all.value());
        tally.at(3) += back ? 0U : 1U;
        tally.at(4) += wide ? 0U : 1U;
    }
}

void verify_random_cases()
{
    Random random{10380U};
    Ledger ledger{};
    for (std::size_t number = 0; number < cases; ++number)
    {
        verify_case(random, ledger);
    }
    const auto &[tally, coverage] = ledger;
    expect(tally.at(0) == 0U, "a found number lies inside the line in a number's shape");
    expect(tally.at(1) == 0U, "a narrower window answers the same or asks for a wider one");
    expect(tally.at(2) == 0U, "a number in a selected line starts inside the selection");
    expect(tally.at(3) == 0U, "adding and subtracting the same amount gives the value back");
    expect(tally.at(4) == 0U, "a zero-led number keeps its width");
    expect(coverage.at(0) > cases / 8U && coverage.at(1) > 0U && coverage.at(2) > 0U &&
               coverage.at(3) > 0U && coverage.at(4) > 0U && coverage.at(5) > 0U,
           "the random lines reach decimal, hex, binary, negative, missing and short windows");
}

// 生成器の外の確かめ（窓の端の 1 バイト・巨大な行の右の数・回数の上限）。
void verify_fixed_rules()
{
    const std::string letters(20'000U, 'a');
    const std::string far = letters + "41";
    const Found narrow = core::vim_find_number(
        VimNumberWindow{std::string_view{far}.substr(0, 4096U), 0, true, false});
    expect(!narrow && narrow.error() == VimNumberMiss::window_short,
           "a window that ends inside the line asks for more instead of reading past it");
    const Found wide = core::vim_find_number(whole(far, 0));
    expect(wide && wide.value().begin == letters.size() && wide.value().magnitude == 41U,
           "the first number right of the caret is found far along the line");
    const Found zero = core::vim_find_number(whole("0", 0));
    expect(zero && core::vim_incremented_text(zero.value(), core::vim_number_count_limit, false) ==
                       "999999999",
           "the count limit is the largest step");
}
} // namespace

void verify_vim_number_property_scope()
{
    verify_fixed_rules();
    verify_random_cases();
}
} // namespace nenenib::tests
