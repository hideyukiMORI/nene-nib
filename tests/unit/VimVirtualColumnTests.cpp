// scope `--vim-virtual-column` の単体テスト（ADR 0042 決定 2）。
#include "DisplayWidth.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VirtualColumn.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::core::caret_virtual_column;
using nenenib::core::display_width;
using nenenib::core::DisplayWidth;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::offset_at_virtual_column;
using nenenib::core::TextBuffer;
using nenenib::core::virtual_column;
using nenenib::core::virtual_column_end;
using nenenib::core::VirtualColumn;

// 表示幅の表（ADR 0034 の決定 1）。境界の code point を、固定 Vim 9.1 の strdisplaywidth() で
// 測った値（out/issue108-oracle/probe2.txt）と突き合わせる。CI に Vim は無いのでここが正本。
void verify_vim_display_width_table()
{
    constexpr std::array<std::pair<char32_t, DisplayWidth>, 28> measured{{
        {U'a', DisplayWidth::single},   {0x0001, DisplayWidth::wide},
        {0x001F, DisplayWidth::wide},   {0x0020, DisplayWidth::single},
        {0x007F, DisplayWidth::single}, {0x0080, DisplayWidth::hex},
        {0x0085, DisplayWidth::hex},    {0x009F, DisplayWidth::hex},
        {0x00A0, DisplayWidth::single}, {0x00B1, DisplayWidth::single},
        {0x03B1, DisplayWidth::single}, {0x0300, DisplayWidth::zero},
        {0x036F, DisplayWidth::zero},   {0x0370, DisplayWidth::single},
        {0x200A, DisplayWidth::single}, {0x200B, DisplayWidth::unprintable},
        {0x2010, DisplayWidth::single}, {0x2329, DisplayWidth::wide},
        {0x3042, DisplayWidth::wide},   {0x4E00, DisplayWidth::wide},
        {0xAC00, DisplayWidth::wide},   {0xD7A3, DisplayWidth::wide},
        {0xD7A4, DisplayWidth::single}, {0xFEFF, DisplayWidth::unprintable},
        {0xFF21, DisplayWidth::wide},   {0xFF71, DisplayWidth::single},
        {0x1F600, DisplayWidth::wide},  {0x10FFFF, DisplayWidth::single},
    }};
    for (const auto &[value, width] : measured)
    {
        expect(display_width(value) == width, "the range table answers the measured display width");
    }
}

// 行頭からの桁（決定 2）。Tab は次の tabstop まで、全角は 2 桁、結合文字は 0 桁、Vim が
// `<200b>` と描く書式用文字は 6 桁。キャレットの桁だけは Tab で最後の桁になる（決定 3・実測）。
void verify_vim_virtual_columns()
{
    const auto tabs = TextBuffer::from_utf8("ab\tcd\n\tx").value();
    expect(virtual_column(tabs, Offset{2}) == VirtualColumn{3}, "a Tab starts where it stands");
    expect(virtual_column_end(tabs, Offset{2}) == VirtualColumn{8},
           "and covers the columns up to the next tabstop");
    expect(caret_virtual_column(tabs, Offset{2}) == VirtualColumn{8},
           "the caret is drawn on the last cell of a Tab, so the wanted column is that one");
    expect(virtual_column(tabs, Offset{3}) == VirtualColumn{9},
           "the character after a Tab starts at the tabstop");
    expect(virtual_column(tabs, Offset{7}) == VirtualColumn{9},
           "a leading Tab fills the first eight columns of its own line");

    const auto wide = TextBuffer::from_utf8("aあb").value();
    expect(virtual_column(wide, Offset{1}) == VirtualColumn{2} &&
               virtual_column_end(wide, Offset{1}) == VirtualColumn{3} &&
               caret_virtual_column(wide, Offset{1}) == VirtualColumn{2},
           "a double-width character covers two columns and the caret sits on the first");
    expect(virtual_column(wide, Offset{4}) == VirtualColumn{4}, "the next character follows it");

    const auto zero = TextBuffer::from_utf8("aéi").value();
    expect(virtual_column(zero, Offset{1}) == VirtualColumn{2} &&
               virtual_column_end(zero, Offset{1}) == VirtualColumn{2},
           "a combining mark adds no column to the character it sits on");
    expect(virtual_column(zero, Offset{4}) == VirtualColumn{3},
           "so the character after the mark keeps the next column");

    const auto format = TextBuffer::from_utf8("a​b").value();
    expect(virtual_column_end(format, Offset{1}) == VirtualColumn{7} &&
               caret_virtual_column(format, Offset{1}) == VirtualColumn{2},
           "an unprintable character takes six columns and the caret stays on the first");
    expect(virtual_column(format, Offset{4}) == VirtualColumn{8},
           "and the character after it starts at the eighth column");

    // Vim は C1 制御文字を `<85>` と描き 4 桁を占める（Issue #147・実測 strdisplaywidth("\x85") ==
    // 4）。
    const auto c1 = TextBuffer::from_utf8("a\u0085x").value();
    expect(virtual_column(c1, Offset{1}) == VirtualColumn{2} &&
               virtual_column_end(c1, Offset{1}) == VirtualColumn{5} &&
               caret_virtual_column(c1, Offset{1}) == VirtualColumn{2},
           "a C1 control takes four columns and the caret stays on the first");
    expect(virtual_column(c1, Offset{3}) == VirtualColumn{6},
           "and the character after it starts at the sixth column");
    expect(offset_at_virtual_column(c1, LineNumber{1}, VirtualColumn{5}) == Offset{1},
           "every column a C1 control covers lands on it");
}

// 逆引き（決定 2）。桁を含む文字の先頭へ着き、行が短ければ行の内容の終わりで止まる。
void verify_vim_virtual_column_reverse()
{
    const auto text = TextBuffer::from_utf8("ab\tcd\nあい\n\nx").value();
    const LineNumber first{1};
    expect(offset_at_virtual_column(text, first, VirtualColumn{1}) == Offset{0},
           "the first column is the first character");
    expect(offset_at_virtual_column(text, first, VirtualColumn{3}) == Offset{2} &&
               offset_at_virtual_column(text, first, VirtualColumn{8}) == Offset{2},
           "every column a Tab covers lands on the Tab itself");
    expect(offset_at_virtual_column(text, first, VirtualColumn{9}) == Offset{3},
           "the column after the tabstop lands on the next character");
    expect(offset_at_virtual_column(text, first, VirtualColumn{99}) == Offset{5},
           "a column past the line stops at the line's content end");

    const LineNumber second{2};
    expect(offset_at_virtual_column(text, second, VirtualColumn{2}) == Offset{6} &&
               offset_at_virtual_column(text, second, VirtualColumn{3}) == Offset{9},
           "both cells of a double-width character belong to it");
    expect(offset_at_virtual_column(text, LineNumber{3}, VirtualColumn{4}) == Offset{13},
           "an empty line has only its own start");
}

void verify_vim_virtual_column_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("virtcol-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 47, "the scope replays all 47 adopted virtual-column fixtures");
}
} // namespace

void verify_vim_virtual_column_contracts()
{
    verify_vim_display_width_table();
    verify_vim_virtual_columns();
    verify_vim_virtual_column_reverse();
}

void verify_vim_virtual_column_scope()
{
    verify_vim_virtual_column_fixtures();
    verify_vim_virtual_column_contracts();
}
} // namespace nenenib::tests
