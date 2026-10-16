// scope `--virtual-columns` の単体テスト（ADR 0140 の決定 4・ADR 0139 の決定 6・#544）。
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumn.hpp"
#include "VirtualColumnIndex.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::core::byte_at_column;
using nenenib::core::column_advance;
using nenenib::core::column_of;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::VirtualColumn;
using nenenib::core::VirtualColumnIndex;

constexpr std::size_t chunk = 64UL * 1024UL;

// 行ごとに、全ての code point の先頭で索引の桁と、行の文字列だけで数える正本（column_of）が
// 一致し、全ての桁の逆引きが byte_at_column と一致する。
void verify_against_lines(const TextBuffer &text, const VirtualColumnIndex &index, const char *name)
{
    for (std::size_t number = 1; number <= text.line_count(); ++number)
    {
        const LineNumber line{number};
        const auto content = text.line_text(line);
        const auto start = text.line_start(line).value;
        bool columns = index.virtual_column_at(text, Offset{start + content.size()}) ==
                       column_of(content, content.size()).value - 1;
        for (std::size_t at = 0; at < content.size();
             at = core::next_code_point(content, Offset{at}).value)
        {
            columns = columns && index.virtual_column_at(text, Offset{start + at}) ==
                                     column_of(content, at).value - 1;
        }
        expect(columns, name);
        const auto width = column_of(content, content.size()).value - 1;
        bool reverse = true;
        for (std::size_t column = 0; column <= width + 2; ++column)
        {
            reverse = reverse && index.byte_at_virtual_column(text, line, column).value ==
                                     start + byte_at_column(content, VirtualColumn{column + 1});
        }
        expect(reverse, name);
    }
}

// 表（ASCII・全角・Tab・結合文字・制御文字・書式用文字・C1・空行・CRLF）。
void verify_virtual_column_table()
{
    constexpr std::array<std::string_view, 10> texts{{
        "abc",
        "\xe6\x97\xa5\xe6\x9c\xac",
        "a\tb\tc\t\td",
        "e\xcc\x81x",
        "a\rb\x01"
        "c",
        "x\xe2\x80\x8by",
        "x\xc2\x85y",
        "\n\nx\n",
        "ab\r\n\t\xe6\x97\xa5\r\n",
        "",
    }};
    for (const auto text : texts)
    {
        const auto buffer = buffer_of(text);
        const auto index = VirtualColumnIndex::from_text(buffer);
        verify_against_lines(buffer, index, "the index agrees with the line columns");
    }
    const auto tabs = buffer_of("a\tb\xe6\x97\xa5\t");
    const auto index = VirtualColumnIndex::from_text(tabs);
    expect(index.virtual_column_at(tabs, Offset{2}) == 8, "a tab after one column stops at 8");
    expect(index.virtual_column_at(tabs, Offset{6}) == 11, "a wide character adds two columns");
    expect(index.virtual_column_at(tabs, Offset{7}) == 16, "the next tab stops at 16");
    expect(index.byte_at_virtual_column(tabs, LineNumber{1}, 5).value == 1,
           "a column inside the tab maps to the tab");
    expect(index.byte_at_virtual_column(tabs, LineNumber{1}, 10).value == 3,
           "the second column of a wide character maps to its start");
    expect(index.byte_at_virtual_column(tabs, LineNumber{1}, 99).value == 7,
           "a column past the line maps to the line content end");
}

// 長い 1 行の本文。塊の境目のあたりに Tab・全角・結合文字・制御文字を置く。
[[nodiscard]] std::string straddling_line(std::size_t bytes)
{
    std::string text;
    text.reserve(bytes + 16);
    std::size_t step = 0;
    while (text.size() < bytes)
    {
        const auto near = text.size() % chunk;
        if (near + 8 >= chunk || near < 4)
        {
            constexpr std::array<std::string_view, 6> marks{
                {"\t", "\xe6\x97\xa5", "e\xcc\x81", "\r", "\xe2\x80\x8b", "\xc2\x85"}};
            text.append(marks.at(step % marks.size()));
            ++step;
        }
        else
        {
            text.push_back(static_cast<char>('a' + (text.size() % 26)));
        }
    }
    return text;
}

// 間引いて比べる位置（塊の境目の前後と、997 バイトごと）。
[[nodiscard]] bool sampled_at(std::size_t start, std::size_t at) noexcept
{
    const auto near = (start + at) % chunk;
    return at % 997 == 0 || near < 6 || near + 6 > chunk;
}

// 1 行の索引の値を、行を 1 回だけ歩いた桁と比べる。
[[nodiscard]] bool sampled_line(const TextBuffer &text, const VirtualColumnIndex &index,
                                LineNumber line)
{
    const auto content = text.line_text(line);
    const auto start = text.line_start(line).value;
    std::size_t column = 0;
    bool same = true;
    for (std::size_t at = 0; at < content.size();
         at = core::next_code_point(content, Offset{at}).value)
    {
        if (sampled_at(start, at))
        {
            same = same && index.virtual_column_at(text, Offset{start + at}) == column;
        }
        column += column_advance(core::code_point_at(content, Offset{at}), column);
    }
    return same && index.virtual_column_at(text, Offset{start + content.size()}) == column;
}

void verify_sampled(const TextBuffer &text, const VirtualColumnIndex &index, const char *name)
{
    bool same = true;
    for (std::size_t number = 1; number <= text.line_count(); ++number)
    {
        same = same && sampled_line(text, index, LineNumber{number});
    }
    expect(same, name);
}

// 塊の境目をまたぐ Tab と文字（決定 4）。
void verify_chunk_boundaries()
{
    const auto line = straddling_line((3 * chunk) + 1000);
    const auto text = buffer_of(line);
    const auto index = VirtualColumnIndex::from_text(text);
    expect(index.chunk_count() == 4, "a line of three and a bit chunks keeps four summaries");
    expect(index.rebuilt_chunks() == index.chunk_count(), "building reads every chunk once");
    verify_sampled(text, index, "chunk sums with tab correction agree with walking the line");
    const auto lines = buffer_of(line.substr(0, chunk + 10) + "\n" + line.substr(chunk + 10) +
                                 "\n\txyz\xe6\x97\xa5");
    verify_sampled(lines, VirtualColumnIndex::from_text(lines),
                   "a line that starts inside a chunk counts from its own start");
}

// 編集は触れた塊だけを読み直し、作り直した値と同じになる。
void verify_edits()
{
    const auto line = straddling_line((4 * chunk) + 500);
    const auto before = buffer_of(line);
    const auto index = VirtualColumnIndex::from_text(before);
    const auto middle = Offset{(2 * chunk) + 100};

    const auto typed = before.replaced(middle, middle, "x");
    const auto typed_index = index.replaced(typed, middle, middle, "x");
    expect(typed_index.rebuilt_chunks() <= 2, "typing one character rereads at most two chunks");
    verify_sampled(typed, typed_index, "a typed character shifts the later chunks");

    const auto tab = before.replaced(Offset{3}, Offset{3}, "\t");
    const auto tab_index = index.replaced(tab, Offset{3}, Offset{3}, "\t");
    expect(tab_index.rebuilt_chunks() == 1, "a tab in the first chunk rereads only that chunk");
    verify_sampled(tab, tab_index, "a tab changes the tab stops of later chunks by summaries");

    const auto across = before.replaced(Offset{chunk - 10}, Offset{chunk + 20}, "\t\xe6\x97\xa5");
    const auto across_index =
        index.replaced(across, Offset{chunk - 10}, Offset{chunk + 20}, "\t\xe6\x97\xa5");
    verify_sampled(across, across_index, "an edit across a boundary rebuilds both chunks");

    const std::string large((2 * chunk) + 7, 'q');
    const auto inserted = before.replaced(middle, middle, large);
    const auto inserted_index = index.replaced(inserted, middle, middle, large);
    verify_sampled(inserted, inserted_index, "a large insertion is cut into new chunks");

    const auto lines = before.replaced(middle, Offset{middle.value + 3}, "\n\n\t");
    verify_sampled(lines, index.replaced(lines, middle, Offset{middle.value + 3}, "\n\n\t"),
                   "a new line break restarts the columns of the later chunks");

    const auto emptied = before.replaced(Offset{0}, Offset{before.size_bytes()}, "");
    const auto emptied_index = index.replaced(emptied, Offset{0}, Offset{before.size_bytes()}, "");
    expect(emptied_index.chunk_count() == 0, "an empty text keeps no chunks");
    expect(emptied_index.virtual_column_at(emptied, Offset{0}) == 0, "an empty text is column 0");
    const auto refilled = emptied.replaced(Offset{0}, Offset{0}, "\tz");
    const auto refilled_index = emptied_index.replaced(refilled, Offset{0}, Offset{0}, "\tz");
    expect(refilled_index.virtual_column_at(refilled, Offset{2}) == 9,
           "typing into an empty text builds the first chunk");

    auto repeated = before;
    auto repeated_index = index;
    for (std::size_t count = 0; count < 40; ++count)
    {
        const auto at = Offset{(count * 9973) % repeated.size_bytes()};
        const auto end = Offset{std::min(at.value + (count % 3), repeated.size_bytes())};
        const auto next = repeated.replaced(at, end, count % 2 == 0 ? "\t" : "");
        repeated_index = repeated_index.replaced(next, at, end, count % 2 == 0 ? "\t" : "");
        repeated = next;
    }
    verify_sampled(repeated, repeated_index, "repeated small edits keep the sums exact");
    verify_sampled(repeated, VirtualColumnIndex::from_text(repeated),
                   "a fresh index of the edited text agrees with walking it");
}

// 16 MiB の 1 行（D52 のベンチと同じ大きさ）。桁を引くたびに歩く塊は 1 つ以下（時間は測らない）。
void verify_giant_line_scans()
{
    constexpr std::size_t size = 16UL * 1024UL * 1024UL;
    std::string line(size, 'x');
    for (std::size_t at = 1000; at + 3 < size; at += 1024UL * 1024UL)
    {
        line.replace(at, 3, "\xe6\x97\xa5");
    }
    const auto text = buffer_of(line);
    const auto index = VirtualColumnIndex::from_text(text);
    expect(index.chunk_count() == size / chunk, "a 16 MiB line keeps one summary per 64 KiB");
    std::size_t most = 0;
    std::size_t total = 0;
    for (std::size_t at = 0; at <= size; at += 65'521)
    {
        const auto cursor = index.cursor_at(text, Offset{at});
        most = std::max(most, cursor.scanned);
        total += cursor.scanned;
    }
    const auto end = index.cursor_at(text, Offset{size});
    expect(most <= 1 && end.scanned <= 1, "a column in a 16 MiB line walks at most one chunk");
    expect(total < 40, "plain chunks are added without walking them");
    expect(end.column == size - 16, "the 16 MiB line is one column per byte less the wide ones");
}
} // namespace

void verify_virtual_column_index_scope()
{
    verify_virtual_column_table();
    verify_chunk_boundaries();
    verify_edits();
    verify_giant_line_scans();
}
} // namespace nenenib::tests
