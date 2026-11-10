// scope `--print-pages-property` の性質の試験（#1289・ADR 0177 の決定 5・ADR 0042）。印刷の改ページ
// （PrintPages.hpp）に、固定 seed の決定的な生成器で作った紙（高さ・DPI・ヘッダと行の高さ・0 と
// 紙より大きい字を含む）と本文を渡し、次を確かめる。
//   1. print_margin は 20 mm をいちばん近い論理画素に丸めた値（|254 × 余白 − 200 × DPI| ≤ 127）。
//   2. print_rows_per_page は「その行数なら紙に収まり、1 行足すと収まらない」数。行が 1
//   つも入らない
//      紙と行の高さ 0 では 1。
//   3. 本文を行に分け、行を紙の幅で切った画面行（Tab は 8 桁の止まりへ展開する・模型）を
//      print_pages のページの順に連結すると、展開した本文に戻る。ページは 0 から隙間も重なりも無く
//      並び、最後のページの他は rows_per_page
//      行ちょうどで、どのページも紙の内（行が入る紙なら上下の
//      余白とヘッダと行の高さの和が紙の高さ以下）。空の文書は行の無い 1 ページ。
//   4. print_page_label は「n / N」で、読み戻すと同じ数。
// 折り返しそのもの（DirectWrite の書式で紙の幅に切る）は ui/win32 の PrintComposition
// にあって純関数 ではないので、ここでは画面行の数を模型の折り返しで作り、core
// が決める改ページだけを照合する。 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。生成器は
// std::mt19937_64 の剰余だけを使い、 分布の型・時刻・環境変数は使わない。
#include "PrintPage.hpp"
#include "PrintPages.hpp"
#include "PrintSheet.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::PrintPage;
using core::PrintSheet;
using Random = std::mt19937_64;
using Rows = std::vector<std::string>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t sheet_inputs = 4000U * rounds;
constexpr std::size_t document_inputs = 120U * rounds;
constexpr std::size_t label_inputs = 500U * rounds;
constexpr std::size_t tab_stop = 8U;
constexpr std::array<std::uint32_t, 6> dpis{{72U, 96U, 120U, 300U, 600U, 1200U}};
// 本文の材料（ASCII と Tab と空の行）。改行は LF。
constexpr std::string_view alphabet = "abcdefghij \t\t";

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::uint32_t below32(Random &random, std::uint32_t bound)
{
    return static_cast<std::uint32_t>(random() % bound);
}

// 紙。高さは半分を 0〜3000（余白とヘッダで埋まる低い紙）、残りを 0〜20000 から選び、ヘッダと行の
// 高さは 0〜400（紙より大きい字と 0 を含む）。
[[nodiscard]] PrintSheet random_sheet(Random &random)
{
    const std::uint32_t height = below32(random, below(random, 2U) == 0U ? 3001U : 20001U);
    return PrintSheet{height, dpis.at(below(random, dpis.size())), below32(random, 401U),
                      below32(random, 401U)};
}

[[nodiscard]] std::uint64_t taken_of(const PrintSheet &sheet)
{
    return std::uint64_t{core::print_margin(sheet.dpi)} * 2U + sheet.header_height;
}

// 行が 1 つでも紙に入るか（入らない紙は 1 行を載せる約束）。
[[nodiscard]] bool row_fits(const PrintSheet &sheet)
{
    return sheet.row_height != 0U && taken_of(sheet) + sheet.row_height <= sheet.height;
}

[[nodiscard]] bool margin_rounds(std::uint32_t dpi)
{
    const auto margin = static_cast<std::int64_t>(core::print_margin(dpi));
    const std::int64_t twice_error = (254 * margin - 200 * static_cast<std::int64_t>(dpi)) * 2;
    return twice_error >= -254 && twice_error <= 254;
}

[[nodiscard]] bool rows_per_page_holds(const PrintSheet &sheet)
{
    const std::uint64_t rows = core::print_rows_per_page(sheet);
    if (!row_fits(sheet))
    {
        return rows == 1U;
    }
    const std::uint64_t taken = taken_of(sheet);
    return rows >= 1U && taken + rows * sheet.row_height <= sheet.height &&
           taken + (rows + 1U) * sheet.row_height > sheet.height;
}

void verify_sheets()
{
    std::size_t broken = 0;
    for (std::uint32_t dpi = 0; dpi <= 2400U; ++dpi)
    {
        broken += margin_rounds(dpi) ? 0U : 1U;
    }
    expect(broken == 0U, "print_margin rounds 20 mm to the nearest pixel at every DPI");
    Random random{12894U};
    std::size_t fitting = 0;
    for (std::size_t input = 0; input < sheet_inputs; ++input)
    {
        const PrintSheet sheet = random_sheet(random);
        fitting += row_fits(sheet) ? 1U : 0U;
        if (!rows_per_page_holds(sheet) && broken++ == 0U)
        {
            std::fprintf(stderr, "  rows per page breaks: height %u dpi %u header %u row %u\n",
                         sheet.height, sheet.dpi, sheet.header_height, sheet.row_height);
        }
    }
    expect(broken == 0U, "print_rows_per_page is the largest count that fits on the sheet");
    expect(fitting > sheet_inputs / 10U && fitting < sheet_inputs - sheet_inputs / 10U,
           "the sheet generator makes both fitting and overflowing sheets");
}

// --- 本文と模型の折り返し ---

[[nodiscard]] std::string random_text(Random &random)
{
    std::string text;
    const std::size_t lines = below(random, 5U) == 0U ? below(random, 2U) : below(random, 80U);
    for (std::size_t line = 0; line < lines; ++line)
    {
        text += line == 0U ? "" : "\n";
        const std::size_t length = below(random, 4U) == 0U ? 0U : below(random, 200U);
        for (std::size_t at = 0; at < length; ++at)
        {
            text.push_back(alphabet.at(below(random, alphabet.size())));
        }
    }
    return text;
}

// Tab を 8 桁の止まりまでの空白に展開する。
[[nodiscard]] std::string expanded(std::string_view line)
{
    std::string text;
    for (const char character : line)
    {
        if (character != '\t')
        {
            text.push_back(character);
            continue;
        }
        text.append(tab_stop - text.size() % tab_stop, ' ');
    }
    return text;
}

// 1 行を幅 width の画面行に切る。空の行も 1 画面行。
void add_rows(Rows &rows, std::string_view line, std::size_t width)
{
    const std::string text = expanded(line);
    std::size_t at = 0;
    do
    {
        rows.push_back(text.substr(at, width));
        at += width;
    } while (at < text.size());
}

// 本文の画面行（空の文書は画面行 0）。
[[nodiscard]] Rows rows_of(std::string_view text, std::size_t width)
{
    Rows rows;
    std::size_t start = 0;
    while (!text.empty() && start <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        add_rows(rows, text.substr(start, end - start), width);
        start = end + 1U;
    }
    return rows;
}

// 画面行を行ごとの改行で継いで、展開した本文を組み立て直す模型。
[[nodiscard]] std::string expanded_text(std::string_view text)
{
    std::string result;
    std::size_t start = 0;
    while (!text.empty() && start <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        result += start == 0U ? "" : "\n";
        result += expanded(text.substr(start, end - start));
        start = end + 1U;
    }
    return result;
}

// ページの順に画面行を継いだ本文。行の境目（画面行が行の最後か）は rows_of と同じ順で数える。
[[nodiscard]] std::string reassembled(std::string_view text, std::size_t width,
                                      const std::vector<PrintPage> &pages)
{
    const Rows rows = rows_of(text, width);
    std::vector<bool> line_ends;
    std::size_t start = 0;
    while (!text.empty() && start <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        Rows line;
        add_rows(line, text.substr(start, end - start), width);
        line_ends.insert(line_ends.end(), line.size() - 1U, false);
        line_ends.push_back(true);
        start = end + 1U;
    }
    std::string result;
    for (const PrintPage &page : pages)
    {
        for (std::size_t row = page.first_row; row < page.first_row + page.row_count; ++row)
        {
            result += rows.at(row);
            result += line_ends.at(row) && row + 1U < rows.size() ? "\n" : "";
        }
    }
    return result;
}

[[nodiscard]] bool pages_tile(const std::vector<PrintPage> &pages, std::size_t rows,
                              std::size_t step)
{
    if (rows == 0U)
    {
        return pages.size() == 1U && pages.front() == PrintPage{0U, 0U};
    }
    std::size_t next = 0;
    for (std::size_t index = 0; index < pages.size(); ++index)
    {
        const PrintPage &page = pages[index];
        const bool last = index + 1U == pages.size();
        const bool counted =
            last ? page.row_count >= 1U && page.row_count <= step : page.row_count == step;
        if (page.first_row != next || !counted)
        {
            return false;
        }
        next += page.row_count;
    }
    return next == rows && pages.size() == (rows + step - 1U) / step;
}

[[nodiscard]] bool pages_fit(const std::vector<PrintPage> &pages, const PrintSheet &sheet)
{
    for (const PrintPage &page : pages)
    {
        if (row_fits(sheet) &&
            taken_of(sheet) + std::uint64_t{page.row_count} * sheet.row_height > sheet.height)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool document_holds(Random &random)
{
    const std::string text = random_text(random);
    const std::size_t width = 1U + below(random, 120U);
    const PrintSheet sheet = random_sheet(random);
    const std::uint32_t step = core::print_rows_per_page(sheet);
    const std::size_t rows = rows_of(text, width).size();
    const auto pages = core::print_pages(rows, step);
    return pages_tile(pages, rows, step) && pages_fit(pages, sheet) &&
           reassembled(text, width, pages) == expanded_text(text) &&
           core::print_pages(rows, step) == pages;
}

void verify_documents()
{
    Random random{12895U};
    std::size_t broken = 0;
    for (std::size_t input = 0; input < document_inputs; ++input)
    {
        if (!document_holds(random) && broken++ == 0U)
        {
            std::fprintf(stderr, "  the pages do not tile the document (input %zu)\n", input);
        }
    }
    expect(broken == 0U, "the pages tile the wrapped rows and reassemble the document");
}

// 境目の見本: 空の文書・1 行だけ・行数 0 の紙（1 として扱う）・1 ページちょうど・巨大な行数。
void verify_edges()
{
    expect(core::print_pages(0U, 0U) == std::vector<PrintPage>{PrintPage{0U, 0U}},
           "an empty document is one empty page even with zero rows per page");
    expect(core::print_pages(1U, 50U) == std::vector<PrintPage>{PrintPage{0U, 1U}},
           "a one-row document is one page");
    expect(pages_tile(core::print_pages(7U, 0U), 7U, 1U), "zero rows per page acts as one row");
    expect(pages_tile(core::print_pages(50U, 50U), 50U, 50U), "a full page is one page");
    expect(pages_tile(core::print_pages(51U, 50U), 51U, 50U), "one more row starts a page");
    expect(pages_tile(core::print_pages(20'000U, 1U), 20'000U, 1U),
           "a page per row tiles a long document");
    expect(rows_of("", 10U).empty() && reassembled("", 10U, core::print_pages(0U, 3U)).empty(),
           "the empty document reassembles to nothing");
    expect(reassembled("\tab", 4U, core::print_pages(rows_of("\tab", 4U).size(), 1U)) ==
               "        ab",
           "a one-line document with a tab reassembles to the expanded line");
}

[[nodiscard]] bool label_reads_back(std::size_t number, std::size_t count)
{
    const std::string label = core::print_page_label(number, count);
    const std::size_t separator = label.find(" / ");
    if (separator == std::string::npos)
    {
        return false;
    }
    std::size_t first = 0;
    std::size_t second = 0;
    const char *end = label.data() + label.size();
    const auto left = std::from_chars(label.data(), label.data() + separator, first);
    const auto right = std::from_chars(label.data() + separator + 3U, end, second);
    return left.ptr == label.data() + separator && right.ptr == end && first == number &&
           second == count;
}

void verify_labels()
{
    Random random{12896U};
    std::size_t broken = 0;
    for (std::size_t input = 0; input < label_inputs; ++input)
    {
        const std::size_t count = 1U + below(random, input % 2U == 0U ? 1000U : 1'000'000'000U);
        broken += label_reads_back(1U + below(random, count), count) ? 0U : 1U;
    }
    expect(broken == 0U, "the page label reads back as the same numbers");
}

void verify_generators_are_deterministic()
{
    Random left{12898U};
    Random right{12898U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        const PrintSheet first = random_sheet(left);
        const PrintSheet second = random_sheet(right);
        same = same && first.height == second.height && first.dpi == second.dpi &&
               first.header_height == second.header_height &&
               first.row_height == second.row_height && random_text(left) == random_text(right);
    }
    expect(same, "the print generators give the same inputs for the same seed");
}
} // namespace

void verify_print_pages_property_scope()
{
    verify_generators_are_deterministic();
    verify_sheets();
    verify_documents();
    verify_edges();
    verify_labels();
}
} // namespace nenenib::tests
