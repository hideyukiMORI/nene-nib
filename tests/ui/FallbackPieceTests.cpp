// 字体選択へ渡す片の切り目（ADR 0142 の決定 3 の追補・#875）を純関数で確かめる。
// 切り目は内容だけで決まり（位置に依らない）、長さは最小 16・最大 256 の間で、サロゲートの対と
// 幅 0 の code point（ADR 0053）の手前で切らない。長い行に 1 文字入れて変わる片は 2 つ以内。
#include "FallbackPiece.hpp"
#include "FontFallbackKey.hpp"
#include "WindowChecks.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests::ui
{
namespace
{
namespace win32 = nenenib::ui::win32;
using Pieces = std::vector<std::wstring>;

// 長い行のベンチの文（eng/measure-speed.py の LONG_LINE_SENTENCE）と回数。
constexpr std::wstring_view bench_sentence = L"日本語の長い行と分割表示の文字組みを測定する。";
constexpr std::size_t bench_repeats = 90;
constexpr std::size_t varied_units = 2077;
constexpr std::size_t marked_units = 3000;
constexpr std::size_t insert_stride = 37;
constexpr std::size_t changed_limit = 2;
constexpr std::uint64_t varied_seed = 875;
constexpr std::wstring_view varied_pool =
    L"日本語の長い行と分割表示文字組測定するあいうえおかきくけこさしすせそたちつてと漢字東京都天気"
    L"晴最高温度引用省略、。「」アイウエオカキクケコ";
constexpr std::wstring_view prefixes[] = {L"", L"x", L"前置き", L"\U0001F600ab"};

// 区切り [start, start + length) を片に切った文字列の列。
Pieces pieces_of(std::wstring_view wide, UINT32 start, UINT32 length)
{
    Pieces pieces;
    for (UINT32 done = 0; done < length;)
    {
        const auto piece = win32::fallback_piece_length(wide, start + done, length - done);
        if (piece == 0)
        {
            expect(false, "a piece is never empty");
            break;
        }
        pieces.emplace_back(wide.substr(start + done, piece));
        done += piece;
    }
    return pieces;
}

Pieces pieces_of(std::wstring_view wide)
{
    return pieces_of(wide, 0, static_cast<UINT32>(wide.size()));
}

std::wstring bench_line()
{
    std::wstring line = L"000000 ";
    for (std::size_t index = 0; index < bench_repeats; ++index)
    {
        line += bench_sentence;
    }
    return line;
}

// 決定的な日本語の行（固定 seed・剰余で畳む）。
std::wstring varied_line()
{
    std::mt19937_64 generator{varied_seed};
    std::wstring line;
    while (line.size() < varied_units)
    {
        line += varied_pool[generator() % varied_pool.size()];
    }
    return line;
}

// サロゲートの対・結合文字（U+0301）・異体字セレクタ（U+FE0F）を混ぜた行。
std::wstring marked_line()
{
    constexpr std::array<std::wstring_view, 6> parts{
        L"e\u0301", L"\u2764\uFE0F", L"\U0001F600", L"字", L"a", L"\U0001F1EF\U0001F1F5"};
    std::mt19937_64 generator{varied_seed + 1};
    std::wstring line;
    while (line.size() < marked_units)
    {
        line += parts.at(generator() % parts.size());
    }
    return line;
}

// 挿入の後の片のうち、前の片の多重集合に無いものの数。
std::size_t changed_pieces(Pieces before, Pieces after)
{
    std::ranges::sort(before);
    std::ranges::sort(after);
    Pieces added;
    std::ranges::set_difference(after, before, std::back_inserter(added));
    return added.size();
}

void verify_position_independence(std::wstring_view text)
{
    const auto alone = pieces_of(text);
    for (const auto prefix : prefixes)
    {
        const std::wstring placed = std::wstring{prefix} + std::wstring{text} + L"後ろ";
        expect(pieces_of(placed, static_cast<UINT32>(prefix.size()),
                         static_cast<UINT32>(text.size())) == alone,
               "the same text is cut the same wherever it starts");
        // 前置きから歩いても、最初の切り目の後は同じ片に揃う。
        const auto walked = pieces_of(std::wstring_view{placed}.substr(0, placed.size() - 2));
        const auto tail = std::min<std::size_t>(alone.size() - 1, walked.size());
        expect(alone.size() > 2 &&
                   std::equal(alone.end() - static_cast<std::ptrdiff_t>(tail - 1), alone.end(),
                              walked.end() - static_cast<std::ptrdiff_t>(tail - 1)),
               "walking from a prefix falls into the same pieces");
    }
}

void verify_lengths(std::wstring_view text)
{
    const auto pieces = pieces_of(text);
    for (std::size_t index = 0; index < pieces.size(); ++index)
    {
        const auto size = pieces.at(index).size();
        const bool last = index + 1 == pieces.size();
        expect(size <= win32::fallback_key_text_limit, "a piece is at most the key limit");
        expect(size >= win32::fallback_piece_minimum || last,
               "a piece is at least the minimum unless it is the rest");
    }
    expect(win32::fallback_piece_length(text, 0, win32::fallback_piece_minimum) ==
               win32::fallback_piece_minimum,
           "a rest of the minimum is one piece");
}

void verify_boundaries()
{
    const auto line = marked_line();
    UINT32 cut = 0;
    std::size_t cuts = 0;
    for (const auto &piece : pieces_of(line))
    {
        cut += static_cast<UINT32>(piece.size());
        if (cut < line.size())
        {
            ++cuts;
            expect(!win32::wide_continues_at(line, cut),
                   "no cut splits a surrogate pair, U+0301 or U+FE0F from its character");
        }
    }
    expect(cuts > marked_units / win32::fallback_key_text_limit, "the marked line is cut");
}

void verify_insertions(const std::wstring &line, const char *message)
{
    const auto before = pieces_of(line);
    std::size_t worst = 0;
    for (std::size_t at = 0; at <= line.size(); at += insert_stride)
    {
        auto inserted = line;
        inserted.insert(at, L"あ");
        worst = std::max(worst, changed_pieces(before, pieces_of(inserted)));
    }
    expect(before.size() > 4 && worst <= changed_limit, message);
}
} // namespace

void verify_fallback_pieces()
{
    const auto bench = bench_line();
    const auto varied = varied_line();
    expect(bench.size() == 2077 && bench.at(7) == L'\u65E5', "the bench line is 2077 units");
    verify_position_independence(bench);
    verify_position_independence(varied);
    verify_lengths(bench);
    verify_lengths(varied);
    verify_lengths(marked_line());
    verify_boundaries();
    verify_insertions(bench, "one insertion into the bench line changes at most 2 pieces");
    verify_insertions(varied, "one insertion into a varied line changes at most 2 pieces");
}
} // namespace nenenib::tests::ui
