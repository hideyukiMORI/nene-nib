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
#include "VimCharacterBoundary.hpp"
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
using nenenib::core::column_advance;
using nenenib::core::giant_line_half_width;
using nenenib::core::giant_line_step;
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

[[nodiscard]] GridPoint point_at(std::size_t cell, std::size_t columns) noexcept
{
    return GridPoint{cell / columns, cell % columns};
}

// 1 行目の at の文字の升と、升からの逆引きの往復（F2）。文字の最初と最後の升（幅 2 の 2 升目・
// 画面行をまたぐ Tab の続き）と、手前の送りの空きの升が、どれも at へ戻る。幅 0 の文字は直前の
// 文字の升、行の終わりは右端の印なので往復しない。
[[nodiscard]] bool round_trips(const VirtualColumnIndex &index, const TextBuffer &text, Offset at)
{
    const auto cursor = index.cursor_at(text, at);
    const auto peeked = text.text_range(at, Offset{at.value + 4});
    const auto cells =
        peeked.empty() ? 0 : column_advance(core::code_point_at(peeked, Offset{0}), cursor.column);
    if (at.value >= text.line_end(LineNumber{1}).value || cells == 0)
    {
        return true;
    }
    const auto columns = index.columns();
    const auto point = grid_cell_of(index, text, at);
    const auto start = (point.row * columns) + point.cell;
    const auto last = start + cells - 1;
    return grid_byte_of(index, text, LineNumber{1}, point) == at &&
           grid_byte_of(index, text, LineNumber{1}, point_at(last, columns)) == at &&
           grid_byte_of(index, text, LineNumber{1}, point_at(cursor.cell, columns)) == at;
}

// 編集した索引と作り直した索引が、間引いた位置と塊の境目の前後で同じ升を返し、升から往復する。
[[nodiscard]] bool same_cells(const TextBuffer &text, const VirtualColumnIndex &edited,
                              const VirtualColumnIndex &fresh)
{
    std::vector<std::size_t> probes;
    for (std::size_t probe = 0; probe <= text.size_bytes(); probe += 4'099)
    {
        probes.push_back(probe);
    }
    for (const auto &chunk : edited.chunks())
    {
        for (std::size_t near = 0; near < 8; ++near)
        {
            probes.push_back(chunk.start + near);
            probes.push_back(chunk.start - std::min(chunk.start, near));
        }
    }
    bool same = edited.columns() == fresh.columns();
    for (const auto probe : probes)
    {
        const auto at = boundary_before(text, std::min(probe, text.size_bytes()));
        same = same && grid_cell_of(edited, text, at) == grid_cell_of(fresh, text, at) &&
               round_trips(edited, text, at);
    }
    const Offset end{text.size_bytes()};
    return same && edited.cursor_at(text, end).cell == fresh.cursor_at(text, end).cell;
}

// 格子の升は幅 columns の累積なので、編集の後も作り直した値と同じになる。Tab の無い全角の行では、
// 送りの余りが変わると後ろの塊も読み直し、余りが揃ったままなら読み直さない（F3）。
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
    const auto before = buffer_of(wide);
    const auto shifted = before.replaced(Offset{0}, Offset{0}, "a");
    const auto rescanned =
        VirtualColumnIndex::for_grid(before, 7).replaced(shifted, Offset{0}, Offset{0}, "a");
    expect(rescanned.chunk_count() == 4 && rescanned.rebuilt_chunks() == 4,
           "a shift of the push remainder rereads every later wide chunk");
    const std::string four = "\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5";
    const auto kept = before.replaced(Offset{0}, Offset{0}, four);
    const auto kept_index =
        VirtualColumnIndex::for_grid(before, 8).replaced(kept, Offset{0}, Offset{0}, four);
    expect(kept_index.rebuilt_chunks() == 1,
           "a shift that keeps both remainders rereads only the touched chunk");
    expect(same_cells(kept, kept_index, VirtualColumnIndex::for_grid(kept, 8)),
           "a kept remainder still gives the cells of a fresh index");
}

// 1 行だけの本文の窓（索引は text の部分和）。
[[nodiscard]] OffsetRange window_of(std::string_view line, std::size_t anchor)
{
    const auto text = buffer_of(line);
    return giant_line_window(VirtualColumnIndex::from_text(text), text, LineNumber{1},
                             Offset{anchor});
}

// 巨大な行の窓（ADR 0139 の決定 3）。中心は錨を 2,048 刻みに切り下げ、前後 4,096。
void verify_giant_line_window()
{
    const std::string plain(10'000, 'a');
    expect(window_of(plain, 0) == OffsetRange{Offset{0}, Offset{4'096}},
           "an anchor at the line start takes only the characters after it");
    expect(window_of(plain, 10'000) == OffsetRange{Offset{4'096}, Offset{10'000}},
           "an anchor at the line end takes the line end");
    expect(window_of(plain, 2'047) == OffsetRange{Offset{0}, Offset{4'096}},
           "an anchor before the step keeps the same window");
    expect(window_of(plain, 2'048) == OffsetRange{Offset{0}, Offset{6'144}},
           "an anchor on the step moves the window");

    // 'e' が 2,047 番目、U+0323 が 2,048 番目のコードポイント（2 バイト）。
    const std::string marked = std::string(2'047, 'a') + "e\xcc\xa3" + std::string(8'000, 'a');
    expect(window_of(marked, 6'201) == OffsetRange{Offset{2'047}, Offset{marked.size()}},
           "the begin moves outward to the base of a combining mark");
    expect(window_of(marked, 2'048) == window_of(marked, 2'047),
           "an anchor inside a Vim character is its start");
    expect(window_of(marked, 2'049) == window_of(marked, 2'047),
           "an anchor inside a code point is its start");
    expect(window_of(marked, 2'048) == OffsetRange{Offset{0}, Offset{4'097}},
           "the window counts code points, not bytes");

    const std::string ending = std::string(4'095, 'a') + "e\xcc\x81" + std::string(100, 'a');
    expect(window_of(ending, 0) == OffsetRange{Offset{0}, Offset{4'098}},
           "the end moves outward past the combining mark");
    expect(window_of("", 0) == OffsetRange{Offset{0}, Offset{0}},
           "an empty line is an empty window");

    const auto lines = buffer_of("x\n" + plain + "\nyz");
    expect(giant_line_window(VirtualColumnIndex::from_text(lines), lines, LineNumber{2},
                             Offset{2 + 2'048}) == OffsetRange{Offset{2}, Offset{2 + 6'144}},
           "the window of a later line is in text offsets and counts from its own start");
    expect(giant_line_window(VirtualColumnIndex::from_text(lines), lines, LineNumber{2},
                             Offset{0}) == OffsetRange{Offset{2}, Offset{2 + 4'096}},
           "an anchor before the line is moved into the line");
}

// 行の文字列だけで決める窓の正本（修正前の形・行の全体を歩く）。複数の塊の行で比べる。
[[nodiscard]] std::size_t reference_floor(std::string_view line, std::size_t at) noexcept
{
    if (at == 0 || at >= line.size())
    {
        return at;
    }
    const auto start = core::vim_character_start(line, Offset{at});
    return core::vim_character_end(line, start).value == at ? at : start.value;
}

[[nodiscard]] OffsetRange reference_window(std::string_view line, std::size_t anchor)
{
    auto at = std::min(anchor, line.size());
    while (at > 0 && !core::is_boundary(line, Offset{at}))
    {
        --at;
    }
    at = reference_floor(line, at);
    const auto ordinal = core::code_point_count(line.substr(0, at));
    const auto center = (ordinal / giant_line_step) * giant_line_step;
    auto begin = at;
    for (auto count = ordinal - (center - std::min(center, giant_line_half_width)); count > 0;
         --count)
    {
        begin = core::previous_code_point(line, Offset{begin}).value;
    }
    auto end = at;
    for (auto count = center + giant_line_half_width - ordinal; count > 0 && end < line.size();
         --count)
    {
        end = core::next_code_point(line, Offset{end}).value;
    }
    const auto floor = reference_floor(line, end);
    const auto ceiling = floor == end ? end : core::vim_character_end(line, Offset{floor}).value;
    return OffsetRange{Offset{reference_floor(line, begin)}, Offset{ceiling}};
}

// 200 KB を越える 1 行（塊は 4 つ）。結合文字の連なりと全角を混ぜ、窓の両端が文字の途中に来る。
// 索引から引いた窓が正本と一致し、どの錨も窓の中にある（F5・F6）。
void verify_giant_line_window_chunks()
{
    std::string line;
    for (std::size_t count = 0; line.size() < 210'000; ++count)
    {
        if (count % 997 == 0)
        {
            line.append("e\xcc\x81\xcc\xa3");
        }
        else
        {
            line.append(count % 13 == 0 ? "\xe6\x97\xa5" : "b");
        }
    }
    const auto text = buffer_of("head\n" + line + "\ntail");
    const auto index = VirtualColumnIndex::from_text(text);
    expect(index.chunk_count() == 4, "the window line spans four chunks");
    bool same = true;
    bool inside = true;
    for (std::size_t anchor = 0; anchor <= line.size(); anchor += 1'013)
    {
        const auto window = giant_line_window(index, text, LineNumber{2}, Offset{5 + anchor});
        const auto expected = reference_window(line, anchor);
        same = same && window.begin.value == 5 + expected.begin.value &&
               window.end.value == 5 + expected.end.value;
        inside = inside && window.begin.value <= 5 + anchor && 5 + anchor <= window.end.value;
    }
    expect(same, "the window from the index agrees with walking the whole line");
    expect(inside, "every anchor lies inside its window");
}
} // namespace

void verify_grid_line_scope()
{
    verify_grid_table();
    verify_grid_gaps();
    verify_grid_chunks();
    verify_grid_edits();
    verify_giant_line_window();
    verify_giant_line_window_chunks();
}
} // namespace nenenib::tests
