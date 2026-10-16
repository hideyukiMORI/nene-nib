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

// 1 行の索引の値を、行を 1 回だけ歩いた桁・序数と比べる（塊の境目の前後と間引いた位置）。
// 順引き（桁・序数）と逆引き（F1・F5）の両方で、幅のある文字は最初と最後の桁の逆引きが文字の
// 先頭へ戻り、序数の逆引きも文字の先頭へ戻る。
[[nodiscard]] bool sampled_line(const TextBuffer &text, const VirtualColumnIndex &index,
                                LineNumber line)
{
    const auto content = text.line_text(line);
    const auto start = text.line_start(line).value;
    std::size_t column = 0;
    std::size_t ordinal = 0;
    bool same = true;
    for (std::size_t at = 0; at < content.size();
         at = core::next_code_point(content, Offset{at}).value)
    {
        const auto advance = column_advance(core::code_point_at(content, Offset{at}), column);
        const Offset here{start + at};
        if (sampled_at(start, at))
        {
            same = same && index.virtual_column_at(text, here) == column &&
                   index.code_point_at(text, here) == ordinal &&
                   index.byte_at_code_point(text, line, ordinal) == here;
        }
        if (sampled_at(start, at) && advance > 0)
        {
            same = same && index.byte_at_virtual_column(text, line, column) == here &&
                   index.byte_at_virtual_column(text, line, column + advance - 1) == here;
        }
        column += advance;
        ++ordinal;
    }
    const Offset end{start + content.size()};
    return same && index.virtual_column_at(text, end) == column &&
           index.code_point_at(text, end) == ordinal &&
           index.byte_at_virtual_column(text, line, column).value == end.value &&
           index.byte_at_code_point(text, line, ordinal).value == end.value;
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
    expect(end.code_point == size - 32, "the 16 MiB line counts one code point per character");
    expect(index.code_point_at(text, Offset{size}) == size - 32 &&
               index.byte_at_code_point(text, LineNumber{1}, size - 32).value == size,
           "the ordinal of the line end maps back to the line end");
    const auto wide = (5UL * 1024UL * 1024UL) + 1000;
    expect(index.code_point_at(text, Offset{wide + 3}) == wide - 9 &&
               index.byte_at_code_point(text, LineNumber{1}, wide - 10).value == wide,
           "an ordinal after five wide characters maps back to the sixth one");
}

// 塊の列が本文を隙間なく覆い、どの塊も 128 KiB 以下で、最後の塊のほかは 16 KiB 以上（決定 4）。
[[nodiscard]] bool well_cut(const TextBuffer &text, const VirtualColumnIndex &index)
{
    const auto chunks = index.chunks();
    std::size_t next = 0;
    bool cut = true;
    for (std::size_t at = 0; at < chunks.size(); ++at)
    {
        const auto &value = chunks[at];
        const bool last = at + 1 == chunks.size();
        cut = cut && value.start == next && value.bytes <= 2 * chunk &&
              (last || value.bytes >= chunk / 4);
        next = value.start + value.bytes;
    }
    return cut && next == text.size_bytes();
}

// 編集で 128 KiB を越えた範囲は等しい長さに切り直し、16 KiB を下回る範囲は次の塊と合わせる（F3）。
void verify_recut_and_merge()
{
    const auto line = straddling_line((4 * chunk) + 500);
    const auto before = buffer_of(line);
    const auto index = VirtualColumnIndex::from_text(before);
    expect(index.chunk_count() == 5, "four chunks and a short tail");

    const Offset middle{(2 * chunk) + 100};
    const std::string large((2 * chunk) + 7, 'q');
    const auto grown = before.replaced(middle, middle, large);
    const auto grown_index = index.replaced(grown, middle, middle, large);
    expect(grown_index.chunk_count() == 6 && grown_index.rebuilt_chunks() == 2,
           "a chunk grown past 128 KiB is cut into two");
    expect(well_cut(grown, grown_index), "every cut chunk is at most 128 KiB");

    const Offset gone{chunk + 100};
    const Offset gone_end{gone.value + (60UL * 1024UL)};
    const auto shrunk = before.replaced(gone, gone_end, "");
    const auto shrunk_index = index.replaced(shrunk, gone, gone_end, "");
    expect(shrunk_index.chunk_count() == 4 && shrunk_index.rebuilt_chunks() == 1,
           "a chunk shrunk under 16 KiB is merged with the next");
    expect(well_cut(shrunk, shrunk_index), "a merged chunk keeps the chunks contiguous");
    verify_sampled(shrunk, shrunk_index, "a merged chunk keeps the sums exact");
}

// 通常の複数行の文書では全ての塊が '\n' を含む。桁を引くとき写すのは、その位置の行頭から
// 位置までだけ（塊の先頭から写さない・I4）。編集で行が増えても後ろの塊の行頭が合う。
void verify_short_lines_copy_little()
{
    std::string lines;
    for (std::size_t count = 0; lines.size() < (3 * chunk) + 1000; ++count)
    {
        lines.append(count % 3 == 0 ? "\t\xe6\x97\xa5x\n" : "ab\xcc\x81 line\tend\n");
    }
    const auto text = buffer_of(lines);
    const auto index = VirtualColumnIndex::from_text(text);
    const auto edited = text.replaced(Offset{10}, Offset{10}, "\n\n\t");
    const auto edited_index = index.replaced(edited, Offset{10}, Offset{10}, "\n\n\t");
    std::size_t most = 0;
    bool same = true;
    for (std::size_t at = 0; at + 20 < edited.size_bytes(); at += 1'009)
    {
        // どの行も 1 バイトの文字（Tab か 'a'）で始まる。行頭の次の文字の手前を引く。
        const auto line = edited.position_of(Offset{at}).line;
        const auto second = Offset{edited.line_start(line).value + 1};
        const auto cursor = edited_index.cursor_at(edited, second);
        most = std::max(most, cursor.copied);
        same = same && (edited.line_end(line).value < second.value || cursor.code_point == 1);
    }
    expect(most <= 1, "a column in a short line copies only that line");
    expect(same, "the line start inside a chunk comes from the newline index");
    verify_sampled(edited, edited_index, "short lines after an edit keep the sums exact");
}
} // namespace

void verify_virtual_column_index_scope()
{
    verify_virtual_column_table();
    verify_chunk_boundaries();
    verify_edits();
    verify_giant_line_scans();
    verify_recut_and_merge();
    verify_short_lines_copy_little();
}
} // namespace nenenib::tests
