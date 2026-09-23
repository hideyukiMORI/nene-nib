// scope `--display-line` の単体テスト（ADR 0042 決定 2）。
#include "Column.hpp"
#include "DisplayLine.hpp"
#include "DisplayWidth.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "Offset.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "SelectionPresence.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimTestSupport.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::OpenDocument;
using nenenib::application::VisibleLines;
using nenenib::core::append_utf8;
using nenenib::core::code_point_at;
using nenenib::core::code_point_count;
using nenenib::core::Column;
using nenenib::core::display_line;
using nenenib::core::display_position;
using nenenib::core::display_width;
using nenenib::core::DisplayLine;
using nenenib::core::DisplayWidth;
using nenenib::core::is_replaced;
using nenenib::core::next_code_point;
using nenenib::core::Offset;
using nenenib::core::SelectionPresence;
using nenenib::core::source_column;

// ---------------------------------------------------------------- 描画用の行（ADR 0040）

[[nodiscard]] std::size_t expected_cells(DisplayWidth width)
{
    switch (width)
    {
    case DisplayWidth::zero:
        return 0;
    case DisplayWidth::single:
        return 1;
    case DisplayWidth::wide:
        return 2;
    case DisplayWidth::unprintable:
        return 6;
    case DisplayWidth::hex:
        return 4;
    }
    std::unreachable();
}

[[nodiscard]] bool starts_are(const DisplayLine &line, std::initializer_list<std::size_t> starts)
{
    return std::ranges::equal(line.starts, starts);
}

void verify_display_line_notation()
{
    const DisplayLine empty = display_line("");
    expect(empty.text.empty() && starts_are(empty, {0}), "an empty line has one start at 0");
    const DisplayLine cr = display_line("a\rb");
    expect(cr.text == "a^Mb", "a CR is drawn as ^M");
    expect(starts_are(cr, {0, 1, 3, 4}), "the CR takes two drawn characters");
    const DisplayLine low = display_line("\x01\x1f");
    expect(low.text == "^A^_" && starts_are(low, {0, 2, 4}), "^A and ^_ take two each");
    const DisplayLine space = display_line("a​b");
    expect(space.text == "a<200b>b", "U+200B is drawn as <200b>");
    expect(starts_are(space, {0, 1, 7, 8}), "<200b> takes six drawn characters");
    const DisplayLine bom = display_line("﻿");
    expect(bom.text == "<feff>" && starts_are(bom, {0, 6}), "U+FEFF is drawn as <feff>");
    const DisplayLine wide = display_line("あ\tx");
    expect(wide.text == "あ\tx", "a full-width letter and a Tab are drawn as they are");
    expect(starts_are(wide, {0, 1, 2, 3}), "a full-width letter and a Tab take one each");
    const DisplayLine del = display_line("\x7f");
    expect(del.text == "\x7f" && starts_are(del, {0, 1}), "DEL stays one character (Vim draws ^?)");
    const DisplayLine combining = display_line("é");
    expect(combining.text == "é" && starts_are(combining, {0, 1, 2}),
           "a combining mark stays one character");
    const DisplayLine nbsp = display_line("\u00a0");
    expect(nbsp.text == "\u00a0" && starts_are(nbsp, {0, 1}),
           "U+00A0 right after the C1 range stays one character");
}

// C1 制御文字（U+0080〜U+009F）は Vim と同じ `<85>` の 4 文字（Issue #147・ADR 0040 補足）。
void verify_display_line_c1()
{
    for (char32_t value = 0x80; value <= 0x9F; ++value)
    {
        std::string text;
        append_utf8(text, value);
        const DisplayLine line = display_line(text);
        expect(line.text.size() == 4 && line.text.front() == '<' && line.text.back() == '>' &&
                   starts_are(line, {0, 4}) && is_replaced(line, 0),
               "every C1 control is drawn as <xx> in four characters");
    }
    expect(display_line("\u0085").text == "<85>", "U+0085 is drawn as <85>");
    expect(display_line("\u0080").text == "<80>", "U+0080 is drawn as <80>");
    expect(display_line("\u009f").text == "<9f>", "U+009F is drawn as <9f> in lower case");
    const DisplayLine mixed = display_line("a\u0085x");
    expect(mixed.text == "a<85>x" && starts_are(mixed, {0, 1, 5, 6}),
           "a C1 control in a line moves the following characters by four");
    expect(!is_replaced(mixed, 0) && is_replaced(mixed, 1) && !is_replaced(mixed, 2),
           "only the C1 control is replaced");
    expect(source_column(mixed, 3) == 1 && display_position(mixed, 2) == 5,
           "the mapping steps over <85> as one source character");
    expect(expected_cells(display_width(0x0085)) == 4,
           "the drawn width of a C1 control equals its cells in the table");
}

void verify_display_line_controls()
{
    for (char32_t value = 0; value < U'\x20'; ++value)
    {
        if (value == U'\t')
        {
            continue;
        }
        std::string text;
        append_utf8(text, value);
        const DisplayLine line = display_line(text);
        const std::string caret{'^', static_cast<char>(value + U'\x40')};
        expect(line.text == caret, "every C0 control but Tab is drawn as ^X");
        expect(line.starts.size() == 2 && line.starts[1] == expected_cells(display_width(value)),
               "^X takes as many drawn characters as the table's cells");
    }
}

void verify_display_line_widths()
{
    const std::string_view text = "a\rあ\t​\x01﻿éz";
    const DisplayLine line = display_line(text);
    expect(line.text == "a^Mあ\t<200b>^A<feff>éz", "a mixed line is replaced in place");
    expect(line.starts.size() == code_point_count(text) + 1,
           "one start per code point and the end");
    std::size_t column = 0;
    for (std::size_t at = 0; at < text.size(); at = next_code_point(text, Offset{at}).value)
    {
        const char32_t value = code_point_at(text, Offset{at});
        const DisplayWidth width = display_width(value);
        const bool replaced = (width == DisplayWidth::wide && value < U'\x20') ||
                              width == DisplayWidth::unprintable || width == DisplayWidth::hex;
        const std::size_t drawn = line.starts[column + 1] - line.starts[column];
        expect(drawn == (replaced ? expected_cells(width) : 1),
               "controls and format characters take their cells, the rest take one");
        expect(is_replaced(line, column) == replaced, "is_replaced names the replaced characters");
        ++column;
    }
    expect(line.starts.back() == code_point_count(line.text), "the last start is the drawn end");
}

void verify_display_line_mapping()
{
    const DisplayLine line = display_line("a\rあ​z");
    for (std::size_t column = 0; column < line.starts.size(); ++column)
    {
        expect(source_column(line, display_position(line, column)) == column,
               "source_column undoes display_position");
    }
    expect(source_column(line, 2) == 1, "the middle of ^M is the CR's column");
    expect(source_column(line, 5) == 3 && source_column(line, 9) == 3,
           "the middle of <200b> is its column");
    for (std::size_t position = 0; position <= line.starts.back(); ++position)
    {
        const std::size_t column = source_column(line, position);
        const bool before_next =
            column + 1 == line.starts.size() || position < display_position(line, column + 1);
        expect(display_position(line, column) <= position && before_next,
               "source_column names the character that holds the position");
    }
    expect(display_position(line, 99) == line.starts.back(), "a column past the end is the end");
    expect(source_column(line, 99) == 5, "a position past the end is the source end");
    expect(!is_replaced(line, 0) && is_replaced(line, 1) && !is_replaced(line, 2),
           "only the CR is replaced among a, CR and a full-width letter");
    expect(is_replaced(line, 3) && !is_replaced(line, 4) && !is_replaced(line, 5) &&
               !is_replaced(line, 99),
           "U+200B is replaced; z, the end and past the end are not");
    const DisplayLine empty = display_line("");
    expect(display_position(empty, 3) == 0 && source_column(empty, 3) == 0 &&
               !is_replaced(empty, 0),
           "an empty line folds everything to 0");
    expect(display_position(DisplayLine{}, 1) == 0 && source_column(DisplayLine{}, 1) == 0,
           "a default DisplayLine folds to 0");
}
} // namespace

void verify_display_line()
{
    verify_display_line_notation();
    verify_display_line_controls();
    verify_display_line_widths();
    verify_display_line_c1();
    verify_display_line_mapping();
}

// ---------------------------------------------------------------- 描画用の行の接続（ADR 0040
// の決定 2）

// 見えている行の display は core の display_line と同じで、選択・検索の当たりの桁は本文の桁のまま
// （描画の桁へ写すのは renderer の 1 か所）。LF 文書の `\r` は文字、CRLF の改行は本文ではない。
void verify_display_line_views()
{
    const std::string first = "a\rb\x01"
                              "c\xe2\x80\x8b beta";
    Editing session;
    open_vim_document(session, first + "\nplain beta");
    EditorController &controller = session.controller();
    const auto opened = controller.frame();
    const auto &line = opened.lines.at(0);
    expect(line.text == first, "the line view keeps the source text");
    const DisplayLine made = display_line(first);
    expect(line.display.text == made.text, "the line view's display text is core's display_line");
    expect(line.display.starts == made.starts, "the line view's starts are core's display_line");
    expect(line.display.text == "a^Mb^Ac<200b> beta", "CR, ^A and U+200B are replaced in the view");
    expect(line.display.starts.size() == code_point_count(first) + 1,
           "one start per source column and the end");
    for (std::size_t column = 0; column + 1 < line.display.starts.size(); ++column)
    {
        const bool replaced = column == 1 || column == 3 || column == 5;
        expect(is_replaced(line.display, column) == replaced,
               "only the CR, ^A and U+200B are replaced in the view");
    }
    expect(opened.lines.at(1).display.text == "plain beta" &&
               starts_are(opened.lines.at(1).display, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10}),
           "a plain line is drawn as it is");
    vim_replay(controller, "/beta<CR>");
    const auto found = controller.frame();
    expect(frame_matches(found, 0) == std::vector<MatchSpan>{{8, 12}},
           "the match keeps source columns (b is the 8th code point, not the 15th drawn one)");
    expect(current_match_is(found, 0, MatchSpan{8, 12}), "the current match keeps source columns");
    expect(frame_matches(found, 1) == std::vector<MatchSpan>{{7, 11}},
           "a plain line's match is the same in both columns");
    expect(display_position(found.lines.at(0).display, 7) == 14,
           "the renderer's mapping puts the match at the 15th drawn character");
    expect(found.caret.position.column == Column{8}, "the caret column is a source column too");
    vim_replay(controller, "0vll");
    const auto selected = controller.frame();
    expect(selected.lines.at(0).selection.presence == SelectionPresence::present &&
               selected.lines.at(0).selection.begin == Column{1} &&
               selected.lines.at(0).selection.end == Column{4},
           "a VISUAL selection over a, CR and b keeps source columns");
    expect(display_position(selected.lines.at(0).display, 3) == 4,
           "the renderer's mapping ends the selection after ^M and b");
    Editing crlf;
    crlf.files().hold(Bytes{std::string("alpha\r\nbeta\r\n")});
    applied(crlf.controller(), VisibleLines{vim_visible_lines});
    applied(crlf.controller(), OpenDocument{sample_path()});
    const auto lines = crlf.controller().frame();
    expect(lines.lines.at(0).display.text == "alpha" && lines.lines.at(1).display.text == "beta",
           "a CRLF document draws no ^M (its line ending is not content)");
}

void verify_display_line_scope()
{
    verify_display_line();
    verify_display_line_views();
}
} // namespace nenenib::tests
