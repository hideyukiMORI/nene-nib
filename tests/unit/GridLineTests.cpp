// scope `--grid-line` の単体テスト（ADR 0140 の決定 4・ADR 0139 の決定 3・#544）。
#include "GiantLine.hpp"
#include "Grid.hpp"
#include "GridPoint.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VirtualColumnIndex.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::core::giant_line_half_width;
using nenenib::core::giant_line_window;
using nenenib::core::grid_byte_of;
using nenenib::core::grid_cell_of;
using nenenib::core::grid_rows;
using nenenib::core::GridPoint;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::TextBuffer;
using nenenib::core::VirtualColumnIndex;

// 格子の表の 1 行。columns 升の格子に 1 行 text を置いたときの升の合計・画面行の数と、
// バイト位置 byte の文字が始まる画面行と升。
struct GridCase
{
    std::string_view name;
    std::string_view text;
    std::size_t columns;
    std::size_t cells;
    std::size_t rows;
    std::size_t byte;
    GridPoint point;
};

constexpr std::array<GridCase, 11> grid_cases{{
    {"ascii wraps by division", "abcdefghij", 4, 10, 3, 9, {2, 1}},
    {"wide is two cells", "\xe6\x97\xa5\xe6\x9c\xac", 8, 4, 1, 3, {0, 2}},
    {"tab stops at the next eight", "a\tb", 20, 9, 1, 2, {0, 8}},
    {"combining mark joins the base", "e\xcc\x81x", 10, 2, 1, 3, {0, 1}},
    {"combining mark sits on the base cell", "ae\xcc\x81x", 10, 3, 1, 2, {0, 1}},
    {"control is shown as caret notation", "a\rb", 10, 4, 1, 2, {0, 3}},
    {"wide at the edge moves to the next row", "abc\xe6\x97\xa5", 4, 6, 2, 3, {1, 0}},
    {"a tab across the edge continues on the next row", "abcde\tx", 6, 9, 2, 6, {1, 2}},
    {"format character is six cells", "\xe2\x80\x8bx", 10, 7, 1, 3, {0, 6}},
    {"the end on the right edge stays on that row", "abcd", 4, 4, 1, 4, {0, 4}},
    {"empty line is one row", "", 8, 0, 1, 0, {0, 0}},
}};

void verify_grid_table()
{
    for (const auto &value : grid_cases)
    {
        const auto text = buffer_of(value.text);
        const auto index = VirtualColumnIndex::for_grid(text, value.columns);
        const auto end = text.line_end(LineNumber{1});
        const auto cells = index.cursor_at(text, end).cell;
        expect(cells == value.cells, value.name.data());
        expect(grid_rows(cells, value.columns) == value.rows, value.name.data());
        const auto point = grid_cell_of(index, text, Offset{value.byte});
        expect(point == value.point, value.name.data());
        const auto zero_width = value.byte < value.text.size() &&
                                core::code_point_at(value.text, Offset{value.byte}) == U'\x301';
        if (!zero_width)
        {
            expect(grid_byte_of(index, text, LineNumber{1}, point).value == value.byte,
                   value.name.data());
        }
    }
}

// 送りの空きと、Tab の続きの升（試作の A・B）。
void verify_grid_gaps()
{
    const auto pushed = buffer_of("abc\xe6\x97\xa5"
                                  "d");
    const auto index = VirtualColumnIndex::for_grid(pushed, 4);
    expect(grid_byte_of(index, pushed, LineNumber{1}, GridPoint{0, 3}).value == 3,
           "the gap before a pushed character maps to that character");
    expect(grid_byte_of(index, pushed, LineNumber{1}, GridPoint{1, 1}).value == 3,
           "the second cell of a wide character maps to its start");
    expect(grid_cell_of(index, pushed, Offset{6}) == GridPoint{1, 2},
           "the gap is counted in the cells after it");
    expect(grid_byte_of(index, pushed, LineNumber{1}, GridPoint{9, 0}).value == 7,
           "a cell past the line maps to the line content end");

    const auto tab = buffer_of("abcde\tx");
    const auto tabbed = VirtualColumnIndex::for_grid(tab, 6);
    expect(grid_byte_of(tabbed, tab, LineNumber{1}, GridPoint{1, 0}).value == 5,
           "the continuation of a tab on the next row maps to the tab");

    const auto lines = buffer_of("ab\n\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"
                                 "x\n");
    const auto multi = VirtualColumnIndex::for_grid(lines, 3);
    expect(grid_cell_of(multi, lines, Offset{6}) == GridPoint{1, 0},
           "the second line counts its cells from its own start");
    expect(grid_cell_of(multi, lines, Offset{12}) == GridPoint{2, 2},
           "each wide character at the edge is pushed");
    expect(grid_byte_of(multi, lines, LineNumber{2}, GridPoint{2, 0}).value == 9,
           "the reverse lookup is per line");
}

// 200 KB の 1 行で、ASCII の塊を割り算で飛ばしても歩いた値と同じ位置になる。
void verify_grid_chunks()
{
    std::string line(200'000, 'x');
    line.replace(150'000, 1, "\xe6\x97\xa5");
    const auto text = buffer_of(line);
    const auto index = VirtualColumnIndex::for_grid(text, 100);
    const auto end = text.line_end(LineNumber{1});
    expect(index.cursor_at(text, end).cell == 200'001, "the wide character adds one cell");
    expect(grid_rows(index.cursor_at(text, end).cell, 100) == 2001, "rows are by division");
    expect(grid_cell_of(index, text, Offset{149'999}) == GridPoint{1499, 99},
           "plain chunk position is by division");
    expect(grid_cell_of(index, text, Offset{150'003}) == GridPoint{1500, 2},
           "after the wide character the cells shift");
    expect(grid_cell_of(index, text, end) == GridPoint{2000, 1},
           "the end of the line is after the last cell");
    expect(grid_byte_of(index, text, LineNumber{1}, GridPoint{1500, 0}).value == 150'000,
           "a row starts at its first character");
}

// probe 以下で最も近い code point の先頭。
[[nodiscard]] Offset boundary_before(const TextBuffer &text, std::size_t probe)
{
    while (probe > 0 &&
           !core::is_boundary(text.text_range(Offset{probe - 1}, Offset{probe + 3}), Offset{1}))
    {
        --probe;
    }
    return Offset{probe};
}

// 編集した索引と作り直した索引が、間引いた位置で同じ升を返す。
[[nodiscard]] bool same_cells(const TextBuffer &text, const VirtualColumnIndex &edited,
                              const VirtualColumnIndex &fresh)
{
    bool same = edited.columns() == fresh.columns();
    for (std::size_t probe = 0; probe <= text.size_bytes(); probe += 4'099)
    {
        const auto at = boundary_before(text, probe);
        same = same && grid_cell_of(edited, text, at) == grid_cell_of(fresh, text, at);
    }
    const Offset end{text.size_bytes()};
    return same && edited.cursor_at(text, end).cell == fresh.cursor_at(text, end).cell;
}

// 格子の升は幅 columns の累積なので、編集の後も作り直した値と同じになる。Tab の無い全角の行では、
// 送りの余りが変わると後ろの塊も読み直す。
void verify_grid_edits()
{
    std::string tabbed;
    std::string wide;
    for (std::size_t count = 0; count < 70'000; ++count)
    {
        tabbed.append(count % 11 == 0 ? "\t" : "\xe6\x97\xa5");
        wide.append("\xe6\x97\xa5");
    }
    for (const auto &line : {tabbed, wide})
    {
        const auto before = buffer_of(line);
        const auto index = VirtualColumnIndex::for_grid(before, 7);
        for (const auto at : {std::size_t{0}, std::size_t{70'002}, before.size_bytes()})
        {
            const auto after = before.replaced(Offset{at}, Offset{at}, "a");
            const auto edited = index.replaced(after, Offset{at}, Offset{at}, "a");
            expect(same_cells(after, edited, VirtualColumnIndex::for_grid(after, 7)),
                   "an edit keeps the grid cells equal to a fresh index");
        }
    }
}

// 巨大な行の窓（ADR 0139 の決定 3）。中心は錨を 2,048 刻みに切り下げ、前後 4,096。
void verify_giant_line_window()
{
    const std::string plain(10'000, 'a');
    const auto half = giant_line_half_width;
    expect(giant_line_window(plain, Offset{0}, half) == OffsetRange{Offset{0}, Offset{4'096}},
           "an anchor at the line start takes only the characters after it");
    expect(giant_line_window(plain, Offset{10'000}, half) ==
               OffsetRange{Offset{4'096}, Offset{10'000}},
           "an anchor at the line end takes the line end");
    expect(giant_line_window(plain, Offset{2'047}, half) == OffsetRange{Offset{0}, Offset{4'096}},
           "an anchor before the step keeps the same window");
    expect(giant_line_window(plain, Offset{2'048}, half) == OffsetRange{Offset{0}, Offset{6'144}},
           "an anchor on the step moves the window");

    // 'e' が 2,047 番目、U+0323 が 2,048 番目のコードポイント（2 バイト）。
    const std::string marked = std::string(2'047, 'a') + "e\xcc\xa3" + std::string(8'000, 'a');
    expect(giant_line_window(marked, Offset{6'201}, half) ==
               OffsetRange{Offset{2'047}, Offset{marked.size()}},
           "the begin moves outward to the base of a combining mark");
    expect(giant_line_window(marked, Offset{2'048}, half) ==
               giant_line_window(marked, Offset{2'047}, half),
           "an anchor inside a Vim character is its start");
    expect(giant_line_window(marked, Offset{2'048}, half) == OffsetRange{Offset{0}, Offset{4'097}},
           "the window counts code points, not bytes");

    const std::string ending = std::string(4'095, 'a') + "e\xcc\x81" + std::string(100, 'a');
    expect(giant_line_window(ending, Offset{0}, half) == OffsetRange{Offset{0}, Offset{4'098}},
           "the end moves outward past the combining mark");
    expect(giant_line_window("", Offset{0}, half) == OffsetRange{Offset{0}, Offset{0}},
           "an empty line is an empty window");
}
} // namespace

void verify_grid_line_scope()
{
    verify_grid_table();
    verify_grid_gaps();
    verify_grid_chunks();
    verify_grid_edits();
    verify_giant_line_window();
}
} // namespace nenenib::tests
