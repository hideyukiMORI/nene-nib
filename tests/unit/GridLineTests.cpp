#include "CaretAffinity.hpp"
#include "DisplayLine.hpp"
#include "GridLine.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::core::CaretAffinity;
using nenenib::core::GridLine;

// 格子の表の 1 行（ADR 0140 の検証・試作）。columns 升の格子に text を置いたときの升の合計・
// 画面行の数・位置 column（0 始まり）の画面行と升。
struct GridCase
{
    std::string_view name;
    std::string_view text;
    std::size_t columns;
    std::size_t cells;
    std::size_t rows;
    std::size_t column;
    std::size_t row;
    std::size_t cell;
};

// 6 件の契約: ASCII・全角・Tab・結合文字・制御文字・升の境目にかかる全角（次の行へ送る）と空行。
constexpr std::array<GridCase, 7> grid_cases{{
    {"ascii wraps by division", "abcdefghij", 4, 10, 3, 9, 2, 1},
    {"wide is two cells", "\xe6\x97\xa5\xe6\x9c\xac", 8, 4, 1, 1, 0, 2},
    {"tab stops at the next eight", "a\tb", 20, 9, 1, 2, 0, 8},
    {"combining mark joins the base", "e\xcc\x81x", 10, 2, 1, 2, 0, 1},
    {"control is shown as caret notation", "a\rb", 10, 4, 1, 2, 0, 3},
    {"wide at the edge moves to the next row", "abc\xe6\x97\xa5", 4, 6, 2, 3, 1, 0},
    {"empty line is one row", "", 8, 0, 1, 0, 0, 0},
}};

void verify_grid_table()
{
    for (const auto &value : grid_cases)
    {
        const auto line = GridLine::build(std::string(value.text), value.columns);
        expect(line.cells() == value.cells, value.name.data());
        expect(line.rows() == value.rows, value.name.data());
        const auto point = line.point_of(value.column, CaretAffinity::leading);
        expect(point.row == value.row && point.cell == value.cell, value.name.data());
        const auto hit = line.hit(point.row, static_cast<float>(point.cell));
        expect(hit.column.value == value.column + 1, value.name.data());
    }
}

// 塊の累積（決定 4）: ASCII だけの塊を割り算で飛ばしても、歩いた値と同じ位置になる。
void verify_grid_chunks()
{
    std::string text(200'000, 'x');
    text.replace(150'000, 1, "\xe6\x97\xa5");
    const auto line = GridLine::build(text, 100);
    expect(line.checkpoint_count() == 4, "a 200 KB line keeps one checkpoint per 64 KiB");
    expect(line.cells() == 200'001, "the wide character adds one cell");
    const auto before = line.point_of(149'999, CaretAffinity::leading);
    expect(before.row == 1499 && before.cell == 99, "plain chunk position is by division");
    const auto after = line.point_of(150'001, CaretAffinity::leading);
    expect(after.row == 1500 && after.cell == 2, "after the wide character the cells shift");
    const auto end = line.point_of(line.code_points(), CaretAffinity::leading);
    expect(end.row == 2000 && end.cell == 1, "the end of the line is after the last cell");
    const auto span = line.row_span(1500);
    expect(span.column_begin == 150'000 && span.byte_begin == 150'000,
           "a row starts at its first character");
    const auto trailing = line.point_of(100, CaretAffinity::trailing);
    expect(trailing.row == 0 && trailing.cell == 100, "trailing stays at the right edge");
}

void verify_grid_display_line()
{
    const auto shown = core::grid_display_line("a\tb", 3);
    expect(shown.text == "a    b", "a tab after virtual column 4 fills to 8");
    expect(shown.starts.size() == 4 && shown.starts.at(2) == 5, "starts follow the spaces");
}
} // namespace

void verify_grid_line_scope()
{
    verify_grid_table();
    verify_grid_chunks();
    verify_grid_display_line();
}
} // namespace nenenib::tests
