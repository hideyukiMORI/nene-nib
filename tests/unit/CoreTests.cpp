// core の単体テスト（ADR 0042 決定 3）。本文・キャレット・履歴・表示値・テーマ・配置。
#include "Appearance.hpp"
#include "BodyLayout.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "Column.hpp"
#include "DevicePixels.hpp"
#include "DisplayText.hpp"
#include "DisplayWidthRange.hpp"
#include "Document.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EncodingFailure.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "HistoryFailure.hpp"
#include "LayoutRect.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Milestone.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "Palette.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScrollBounds.hpp"
#include "ScrollExtent.hpp"
#include "ScrollFollow.hpp"
#include "Selection.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "StatusBarHit.hpp"
#include "StatusBarLayout.hpp"
#include "StatusItems.hpp"
#include "SyntaxPalette.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextFailure.hpp"
#include "TextPosition.hpp"
#include "Theme.hpp"
#include "ThemeDerivation.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::Document;
using nenenib::application::save_state_of;
using nenenib::core::absorbed;
using nenenib::core::Appearance;
using nenenib::core::body_layout;
using nenenib::core::body_line_rect;
using nenenib::core::builtin_themes;
using nenenib::core::BuiltinTheme;
using nenenib::core::byte_order_mark;
using nenenib::core::CaretMotion;
using nenenib::core::code_point_at;
using nenenib::core::code_point_count;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::contains;
using nenenib::core::derive_ui;
using nenenib::core::detect_encoding;
using nenenib::core::detect_line_ending;
using nenenib::core::DisplayText;
using nenenib::core::Edit;
using nenenib::core::EditBoundary;
using nenenib::core::EditHistory;
using nenenib::core::EditMode;
using nenenib::core::encoding_label;
using nenenib::core::EncodingFailure;
using nenenib::core::FilePath;
using nenenib::core::first_visible_for_caret;
using nenenib::core::first_visible_within;
using nenenib::core::has_control_character;
using nenenib::core::has_selection;
using nenenib::core::height_of;
using nenenib::core::HistoryFailure;
using nenenib::core::is_boundary;
using nenenib::core::is_empty;
using nenenib::core::LayoutRect;
using nenenib::core::line_ending_label;
using nenenib::core::LineEnding;
using nenenib::core::LineNumber;
using nenenib::core::Milestone;
using nenenib::core::milestone_name;
using nenenib::core::moved_caret;
using nenenib::core::newline_of;
using nenenib::core::next_code_point;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Palette;
using nenenib::core::palette_for;
using nenenib::core::previous_code_point;
using nenenib::core::RgbaColor;
using nenenib::core::RgbColor;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::selection_range;
using nenenib::core::SelectionPresence;
using nenenib::core::status_bar_hit;
using nenenib::core::status_bar_layout;
using nenenib::core::status_items_for;
using nenenib::core::StatusBarHit;
using nenenib::core::SyntaxPalette;
using nenenib::core::tab_rect;
using nenenib::core::tab_title_for;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextFailure;
using nenenib::core::TextPosition;
using nenenib::core::Theme;
using nenenib::core::theme_named;
using nenenib::core::theme_of;
using nenenib::core::title_bar_hit;
using nenenib::core::title_bar_layout;
using nenenib::core::TitleBarHit;
using nenenib::core::to_pixels;
using nenenib::core::to_utf16;
using nenenib::core::to_utf8;
using nenenib::core::toggled;
using nenenib::core::validate_utf8;
using nenenib::core::width_of;
using nenenib::core::without_byte_order_mark;

void expect_rejected(std::string_view text, TextFailure reason, const char *description)
{
    const auto parsed = DisplayText::parse(text);
    expect(!parsed.has_value() && parsed.error() == reason, description);
}

void expect_accepted(std::string_view text, std::size_t code_points, const char *description)
{
    const auto parsed = DisplayText::parse(text);
    expect(parsed.has_value() && parsed.value().text() == text &&
               parsed.value().code_point_count() == code_points,
           description);
}

void verify_display_text_accepts_multibyte()
{
    // 2 バイト: U+00A9 / 3 バイト: 日本語 / 3 バイト: U+E000（サロゲート上限の直後） /
    // 4 バイト: U+1F58B（万年筆の絵文字）。
    expect_accepted("\xC2\xA9", 1, "two-byte sequence is accepted");
    expect_accepted("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", 3, "Japanese is accepted");
    expect_accepted("\xEE\x80\x80", 1, "code point just above the surrogate range is accepted");
    expect_accepted("\xF0\x9F\x96\x8B", 1, "four-byte emoji is accepted");
    expect_accepted("\xF4\x8F\xBF\xBF", 1, "U+10FFFF is accepted");
}

void verify_display_text_lengths()
{
    const std::string longest(DisplayText::maximum_bytes, 'a');
    expect_accepted(longest, DisplayText::maximum_bytes, "256 bytes exactly is accepted");
    const std::string overlong(DisplayText::maximum_bytes + 1, 'a');
    expect_rejected(overlong, TextFailure::too_long, "257 bytes is rejected");
    const auto mixed = fixed_text("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                                  "ab");
    expect(mixed.text().size() == 11 && mixed.code_point_count() == 5,
           "code points are counted, not bytes");
}

void verify_display_text_rejects()
{
    expect_rejected("", TextFailure::empty, "the empty string is rejected");
    expect_rejected("\x01", TextFailure::control_character, "C0 control is rejected");
    expect_rejected("\x7F", TextFailure::control_character, "DEL is rejected");
    expect_rejected("\x80", TextFailure::invalid_utf8, "a lone continuation byte is rejected");
    expect_rejected("\xFF", TextFailure::invalid_utf8, "0xFF is never a lead byte");
    expect_rejected("\xC0\x80", TextFailure::invalid_utf8, "overlong encoding is rejected");
    expect_rejected("\xE6\x97", TextFailure::invalid_utf8, "a truncated sequence is rejected");
    expect_rejected("\xE6\x28\xA5", TextFailure::invalid_utf8,
                    "a broken continuation byte is rejected");
    expect_rejected("\xED\xA0\x80", TextFailure::invalid_utf8, "surrogates are rejected");
    expect_rejected("\xF5\x80\x80\x80", TextFailure::invalid_utf8, "above U+10FFFF is rejected");
}

// ---------------------------------------------------------------- Utf8

void verify_utf8_validation()
{
    expect(validate_utf8("").value() == 0, "the empty string has no code points");
    expect(validate_utf8("ab\xE6\x97\xA5").value() == 3, "mixed widths are counted once each");
    expect(validate_utf8("\n\t\r").value() == 3, "the buffer accepts the control bytes it needs");
    expect(!validate_utf8("\xC0\x80").has_value(), "overlong encoding is rejected");
    expect(validate_utf8("\xF0\x28\x8C\xBC").error() == TextFailure::invalid_utf8,
           "a broken four-byte sequence is rejected");
}

void verify_utf8_counting()
{
    expect(code_point_count("") == 0, "no bytes, no code points");
    expect(code_point_count("a\xC2\xA9\xF0\x9F\x96\x8B") == 3, "continuation bytes do not count");
    expect(!has_control_character("abc"), "plain text has no control characters");
    expect(has_control_character("a\nb"), "a newline is a control character for DisplayText");
    expect(has_control_character("a\x7F"), "DEL is a control character");
    expect(!has_control_character("\xE6\x97\xA5"), "multibyte bytes are never control characters");
}

void verify_utf8_walking()
{
    const std::string text = "a\xE6\x97\xA5\xF0\x9F\x96\x8B";
    expect(is_boundary(text, Offset{0}), "the start is a boundary");
    expect(is_boundary(text, Offset{1}), "after the ASCII byte is a boundary");
    expect(!is_boundary(text, Offset{2}), "the middle of a three-byte sequence is not");
    expect(is_boundary(text, Offset{text.size()}), "the end is a boundary");
    expect(next_code_point(text, Offset{0}) == Offset{1}, "one ASCII byte");
    expect(next_code_point(text, Offset{1}) == Offset{4}, "three bytes for the Japanese glyph");
    expect(next_code_point(text, Offset{4}) == Offset{8}, "four bytes for the emoji");
    expect(next_code_point(text, Offset{8}) == Offset{8}, "the end does not move");
    expect(next_code_point(text, Offset{99}) == Offset{8}, "an offset past the end is clamped");
    expect(previous_code_point(text, Offset{8}) == Offset{4}, "back over the emoji");
    expect(previous_code_point(text, Offset{4}) == Offset{1}, "back over the Japanese glyph");
    expect(previous_code_point(text, Offset{1}) == Offset{0}, "back over the ASCII byte");
    expect(previous_code_point(text, Offset{0}) == Offset{0}, "the start does not move");
}

// ---------------------------------------------------------------- Utf16

// UTF-16 の期待値はソースの文字コードに頼らず、単位の値から組み立てる（Issue #13）。
[[nodiscard]] std::wstring wide_of(std::initializer_list<unsigned int> units)
{
    std::wstring wide;
    for (const unsigned int unit : units)
    {
        wide.push_back(static_cast<wchar_t>(unit));
    }
    return wide;
}

void verify_utf16_encoding()
{
    expect(to_utf16("").value().empty(), "the empty string has no units");
    expect(to_utf16("ab").value() == wide_of({0x61U, 0x62U}), "ASCII is one unit per byte");
    expect(to_utf16("\xC2\xA9").value() == wide_of({0xA9U}), "two UTF-8 bytes are one BMP unit");
    expect(to_utf16("\xE6\x97\xA5").value() == wide_of({0x65E5U}),
           "three UTF-8 bytes are one BMP unit");
    expect(to_utf16("\xF0\x9F\x98\x80").value() == wide_of({0xD83DU, 0xDE00U}),
           "the emoji becomes a surrogate pair");
    expect(to_utf16("a\xF0\x9F\x98\x80\xE6\x97\xA5").value() ==
               wide_of({0x61U, 0xD83DU, 0xDE00U, 0x65E5U}),
           "widths mix inside one string");
}

void verify_utf16_rejects()
{
    expect(to_utf16("\x80").error() == TextFailure::invalid_utf8,
           "a lone continuation byte does not convert");
    expect(to_utf16("\xE6\x97").error() == TextFailure::invalid_utf8,
           "a truncated sequence does not convert");
    expect(to_utf16("\xED\xA0\x80").error() == TextFailure::invalid_utf8,
           "a surrogate spelled in UTF-8 does not convert");
    expect(to_utf8(wide_of({0xD83DU})).error() == TextFailure::invalid_utf16,
           "a high surrogate at the end is rejected");
    expect(to_utf8(wide_of({0xDE00U})).error() == TextFailure::invalid_utf16,
           "a low surrogate on its own is rejected");
    expect(to_utf8(wide_of({0xD83DU, 0x61U})).error() == TextFailure::invalid_utf16,
           "a high surrogate followed by a letter is rejected");
    expect(to_utf8(wide_of({0xD83DU, 0xD83DU})).error() == TextFailure::invalid_utf16,
           "two high surrogates are rejected");
}

void verify_utf16_round_trip()
{
    expect(to_utf8(std::wstring{}).value().empty(), "nothing converts back to nothing");
    const std::string mixed = "a\xC2\xA9\xE6\x97\xA5\xEF\xBC\xA1\xF0\x9F\x98\x80";
    expect(to_utf8(to_utf16(mixed).value()).value() == mixed, "UTF-8 survives the round trip");
    // 0xFF21 はサロゲートの範囲より上の BMP。合成にも分解にもならない側を通す。
    const std::wstring units = wide_of({0x61U, 0xA9U, 0x65E5U, 0xFF21U, 0xD83DU, 0xDE00U});
    expect(to_utf16(to_utf8(units).value()).value() == units, "UTF-16 survives the round trip");
    expect(code_point_at(mixed, Offset{0}) == U'a', "the ASCII code point is read back");
    expect(code_point_at(mixed, Offset{1}) == 0xA9U, "the two-byte code point is read back");
    expect(code_point_at(mixed, Offset{3}) == 0x65E5U, "the three-byte code point is read back");
    expect(code_point_at(mixed, Offset{9}) == 0x1F600U, "the four-byte code point is read back");
    expect(code_point_at(mixed, Offset{mixed.size()}) == 0U, "past the end there is no code point");
}

// ---------------------------------------------------------------- TextBuffer

void verify_buffer_creation()
{
    const auto nothing = TextBuffer::empty();
    expect(nothing.size_bytes() == 0 && nothing.line_count() == 1, "an empty buffer is one line");
    expect(nothing.piece_count() == 0, "an empty buffer holds no pieces");
    expect(nothing.text().empty(), "an empty buffer reads back empty");
    expect(nothing.line_text(LineNumber{1}).empty(), "the only line is empty");
    const auto broken = TextBuffer::from_utf8("\xC0\x80");
    expect(!broken.has_value() && broken.error() == TextFailure::invalid_utf8,
           "from_utf8 rejects broken UTF-8");
    const auto blank = TextBuffer::from_utf8("");
    expect(blank.has_value() && blank.value().piece_count() == 0,
           "from_utf8 of nothing holds no pieces");
    const auto one = buffer_of("abc");
    expect(one.piece_count() == 1 && one.text() == "abc", "one original piece holds the text");
}

void verify_buffer_insertion()
{
    const auto start = buffer_of("bc");
    expect(start.insert(Offset{0}, "a").text() == "abc", "insert at the head");
    expect(start.insert(Offset{2}, "d").text() == "bcd", "insert at the tail");
    expect(start.insert(Offset{1}, "-").text() == "b-c", "insert in the middle splits a piece");
    expect(start.insert(Offset{99}, "z").text() == "bcz", "an offset past the end is clamped");
    expect(start.insert(Offset{1}, "").text() == "bc", "inserting nothing changes nothing");
    expect(start.text() == "bc", "the source buffer is unchanged");
    const auto grown = start.insert(Offset{2}, "d").insert(Offset{3}, "e");
    expect(grown.text() == "bcde", "two appends read back in order");
    expect(grown.piece_count() == 2, "a continued append extends the last piece");
    const auto split = start.insert(Offset{1}, "x").insert(Offset{1}, "y");
    expect(split.text() == "byxc", "an insert before the last add piece starts a new piece");
}

void verify_buffer_erasure()
{
    const auto text = buffer_of("abcdef");
    expect(text.erase(Offset{0}, Offset{2}).text() == "cdef", "erase from the head");
    expect(text.erase(Offset{4}, Offset{6}).text() == "abcd", "erase to the tail");
    expect(text.erase(Offset{2}, Offset{4}).text() == "abef", "erase from the middle");
    expect(text.erase(Offset{2}, Offset{2}).text() == "abcdef", "an empty range changes nothing");
    expect(text.erase(Offset{4}, Offset{2}).text() == "abcdef", "a reversed range is clamped away");
    expect(text.erase(Offset{0}, Offset{99}).text().empty(), "erasing past the end empties it");
    expect(text.erase(Offset{99}, Offset{99}).text() == "abcdef", "erasing past the end is a copy");
    const auto mixed = text.insert(Offset{3}, "XY");
    expect(mixed.text() == "abcXYdef", "the fixture spans three pieces");
    expect(mixed.erase(Offset{2}, Offset{6}).text() == "abef", "erase across piece boundaries");
    expect(mixed.erase(Offset{3}, Offset{5}).text() == "abcdef", "erase exactly one whole piece");
}

void verify_buffer_lines()
{
    const auto text = buffer_of("one\r\ntwo\r\nthree");
    expect(text.line_count() == 3, "two CRLF newlines make three lines");
    expect(text.line_text(LineNumber{1}) == "one", "the carriage return is not part of the line");
    expect(text.line_text(LineNumber{2}) == "two", "the middle line");
    expect(text.line_text(LineNumber{3}) == "three", "the last line has no terminator");
    expect(text.line_text(LineNumber{9}).empty(), "a line past the end reads back empty");
    expect(text.line_start(LineNumber{1}) == Offset{0}, "the first line starts at zero");
    expect(text.line_start(LineNumber{2}) == Offset{5}, "the second line starts after the CRLF");
    expect(text.line_end(LineNumber{1}) == Offset{3}, "the first line ends before the CR");
    expect(text.line_terminator_end(LineNumber{1}) == Offset{5},
           "the terminator ends after the LF");
    expect(text.line_terminator_end(LineNumber{3}) == Offset{15}, "the last line ends at the end");
    const auto feeds = buffer_of("one\ntwo\n");
    expect(feeds.line_count() == 3, "a trailing LF opens an empty last line");
    expect(feeds.line_text(LineNumber{1}) == "one", "a bare LF needs no stripping");
    expect(feeds.line_text(LineNumber{3}).empty(), "the line after the last LF is empty");
    expect(feeds.line_end(LineNumber{3}) == Offset{8}, "the empty last line ends at the end");
}

void verify_buffer_crlf_split()
{
    // CRLF の途中への挿入。'\n' だけを数えるので行数は動かず、'\r' は前の行の内容になる。
    const auto text = buffer_of("a\r\nb");
    expect(text.line_count() == 2, "the fixture has two lines");
    const auto broken = text.insert(Offset{2}, "X");
    expect(broken.text() == "a\rX\nb", "the insert lands between CR and LF");
    expect(broken.line_count() == 2, "the line count does not change");
    expect(broken.line_text(LineNumber{1}) == "a\rX", "the orphaned CR stays in the line");
    expect(broken.line_text(LineNumber{2}) == "b", "the second line is unchanged");
    const auto joined = text.erase(Offset{1}, Offset{2});
    expect(joined.text() == "a\nb" && joined.line_count() == 2, "erasing the CR keeps the LF");
}

void verify_buffer_positions()
{
    const auto text = buffer_of("ab\r\n\xE6\x97\xA5\xF0\x9F\x96\x8B"
                                "c\r\n");
    expect(text.line_count() == 3, "the fixture has three lines");
    expect(text.position_of(Offset{0}) == TextPosition{LineNumber{1}, Column{1}}, "the origin");
    expect(text.position_of(Offset{2}) == TextPosition{LineNumber{1}, Column{3}},
           "columns count code points on the first line");
    expect(text.position_of(Offset{4}) == TextPosition{LineNumber{2}, Column{1}},
           "after the CRLF comes the second line");
    expect(text.position_of(Offset{7}) == TextPosition{LineNumber{2}, Column{2}},
           "a three-byte glyph is one column");
    expect(text.position_of(Offset{11}) == TextPosition{LineNumber{2}, Column{3}},
           "a four-byte glyph is one column");
    expect(text.position_of(Offset{99}) == TextPosition{LineNumber{3}, Column{1}},
           "an offset past the end clamps to the last position");
    expect(text.offset_of(TextPosition{LineNumber{2}, Column{3}}) == Offset{11},
           "offset_of is the inverse of position_of");
    expect(text.offset_of(TextPosition{LineNumber{2}, Column{99}}) == Offset{12},
           "a column past the line end clamps to the line end");
    expect(text.offset_of(TextPosition{LineNumber{1}, Column{1}}) == Offset{0}, "the origin again");
}

void verify_buffer_round_trip()
{
    const auto text = buffer_of("alpha\r\nbeta\r\n\xE6\x97\xA5\xE6\x9C\xAC\r\nomega");
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        const LineNumber number{line};
        const auto position = text.position_of(text.line_start(number));
        expect(position == TextPosition{number, Column{1}}, "every line starts at column one");
        expect(text.offset_of(position) == text.line_start(number), "the round trip closes");
    }
    expect(text.text_range(Offset{7}, Offset{11}) == "beta", "text_range reads a slice");
    expect(text.text_range(Offset{5}, Offset{5}).empty(), "an empty range reads nothing");
}

void verify_buffer_scale()
{
    // QLT-014 は planned のまま。ここは「終わること」だけを見る（時間は out/ の使い捨てで測る）。
    const std::string block(1024U * 1024U, 'x');
    const auto big = TextBuffer::empty().insert(Offset{0}, block);
    expect(big.size_bytes() == block.size(), "a 1 MB insert completes");
    expect(big.line_count() == 1, "1 MB without newlines is one line");
    expect(big.position_of(Offset{block.size()}).column.value == block.size() + 1,
           "the caret can reach the end of 1 MB");
    std::string many;
    for (std::size_t line = 0; line < 10000; ++line)
    {
        many += "line\r\n";
    }
    const auto wide = buffer_of(many);
    expect(wide.line_count() == 10001, "ten thousand lines are indexed");
    std::size_t bytes = 0;
    for (std::size_t line = 1; line <= wide.line_count(); ++line)
    {
        bytes += wide.line_text(LineNumber{line}).size();
    }
    expect(bytes == 40000, "every one of the ten thousand lines reads back");
    expect(wide.position_of(Offset{many.size()}).line == LineNumber{10001},
           "the last line is found");
}

// ADR 0044 の決定 2: 同じ値から 2 回分岐して挿入しても、互いの本文が壊れない。
void verify_buffer_add_branches()
{
    const auto base = TextBuffer::empty().insert(Offset{0}, "base");
    const auto first = base.insert(Offset{0}, "x");
    const auto second = base.insert(Offset{0}, "y");
    expect(first.text() == "xbase", "the first branch reads its own insert");
    expect(second.text() == "ybase", "the second branch does not overwrite the first");
    expect(base.text() == "base", "the common source is unchanged");
    const auto tip = TextBuffer::empty().insert(Offset{0}, "tip");
    const auto grown = tip.insert(Offset{3}, "1");
    const auto forked = tip.insert(Offset{3}, "2");
    expect(grown.text() == "tip1" && grown.piece_count() == 1,
           "the value that knows the tip extends the chunk in place");
    expect(forked.text() == "tip2" && forked.piece_count() == 2,
           "a value behind the tip starts a new chunk instead of overwriting");
    expect(grown.insert(Offset{4}, "!").text() == "tip1!" && tip.text() == "tip",
           "the grown branch keeps growing and the source still reads its own length");
}

// 64 KiB の chunk を跨ぐ連続の 1 文字入力（ADR 0044 の決定 2 / 3）。
void verify_buffer_chunk_growth()
{
    constexpr std::size_t typed = 70000;
    auto text = TextBuffer::empty();
    std::string expected;
    for (std::size_t index = 0; index < typed; ++index)
    {
        const std::string_view key = index % 1000 == 999 ? "\n" : "a";
        text = text.insert(Offset{text.size_bytes()}, key);
        expected += key;
    }
    expect(text.text() == expected, "seventy thousand keys read back across the chunk boundary");
    expect(text.line_count() == 71, "the newlines typed across the boundary are all indexed");
    expect(text.piece_count() == 2, "crossing one chunk boundary adds exactly one piece");
    expect(text.line_text(LineNumber{66}) == std::string(999, 'a'),
           "a line that straddles the chunk boundary reads back whole");
}

// chunk より大きい 1 回の挿入と、erase から insert で戻した本文（ADR 0044 の決定 2 / 5）。
void verify_buffer_oversized_and_restored()
{
    const std::string block(200U * 1024U, 'b');
    const auto big = buffer_of("ac").insert(Offset{1}, block);
    expect(big.text() == "a" + block + "c" && big.piece_count() == 3,
           "an insert larger than a chunk is one piece of its own chunk");
    const auto after = big.insert(Offset{1 + block.size()}, "d");
    expect(after.text() == "a" + block + "dc" && after.piece_count() == 4,
           "the oversized chunk is never extended");
    const auto typed = TextBuffer::empty().insert(Offset{0}, "hello world");
    const auto erased = typed.erase(Offset{5}, Offset{11});
    const auto restored = erased.insert(Offset{5}, " world");
    expect(erased.text() == "hello" && restored.text() == typed.text(),
           "erase then insert restores the text");
    expect(typed.text() == "hello world", "the value before the erase still reads its text");
}

// 素朴な行の先頭の列。本文の '\n' を 1 つずつ数える（ADR 0047 の契約の比較相手）。
std::vector<std::size_t> naive_line_starts(std::string_view text)
{
    std::vector<std::size_t> starts{0};
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        if (text[at] == '\n')
        {
            starts.push_back(at + 1);
        }
    }
    return starts;
}

// index 番目の行の先頭・行番号（行の両端の位置から）・行の文字列が素朴な計算と一致するか。
bool line_agrees(const TextBuffer &buffer, std::string_view text,
                 const std::vector<std::size_t> &starts, std::size_t index)
{
    const LineNumber line{index + 1};
    const std::size_t start = starts.at(index);
    const std::size_t stop = index + 1 < starts.size() ? starts.at(index + 1) - 1 : text.size();
    return buffer.line_start(line) == Offset{start} && buffer.line_end(line) == Offset{stop} &&
           buffer.position_of(Offset{start}).line == line &&
           buffer.position_of(Offset{stop}).line == line &&
           buffer.line_text(line) == text.substr(start, stop - start);
}

// 行数と、stride 行ごと（最終行は必ず）の行の問いが素朴な計算と一致するか。
bool lines_agree(const TextBuffer &buffer, std::string_view text, std::size_t stride)
{
    const auto starts = naive_line_starts(text);
    if (buffer.line_count() != starts.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < starts.size(); index += stride)
    {
        if (!line_agrees(buffer, text, starts, index))
        {
            return false;
        }
    }
    return line_agrees(buffer, text, starts, starts.size() - 1);
}

// at を含む行が、本文の at の前後の '\n' から求めた行と一致するか（全体を数えずに見られる範囲）。
bool line_around_agrees(const TextBuffer &buffer, std::string_view text, std::size_t at)
{
    const std::size_t before = at == 0 ? std::string_view::npos : text.rfind('\n', at - 1);
    const std::size_t start = before == std::string_view::npos ? 0 : before + 1;
    const std::size_t found = text.find('\n', at);
    const std::size_t stop = found == std::string_view::npos ? text.size() : found;
    const LineNumber line = buffer.position_of(Offset{at}).line;
    return buffer.line_start(line) == Offset{start} && buffer.line_end(line) == Offset{stop} &&
           buffer.line_text(line) == text.substr(start, stop - start);
}

// edited の後の行数（素朴な計算）。3 回に 1 回は入れた "x\n" と続く 1 文字を消す。
std::size_t lines_after(std::size_t lines, std::string_view text, std::size_t at, std::size_t round)
{
    if (round % 3 != 2)
    {
        return lines + 1;
    }
    const bool newline_erased = at < text.size() && text[at] == '\n';
    return newline_erased ? lines - 1 : lines;
}

// 本文と素朴な文字列に同じ編集をする。1 文字と改行を入れ（2 回目は直前の add piece を伸ばす）、
// 3 回に 1 回はそれと続く 1 文字を消す（piece を切る）。
TextBuffer edited(const TextBuffer &buffer, std::string &text, std::size_t at, std::size_t round)
{
    auto next = buffer.insert(Offset{at}, "x").insert(Offset{at + 1}, "\n");
    text.insert(at, "x\n");
    if (round % 3 == 2)
    {
        const std::size_t end = std::min(at + 3, text.size());
        next = next.erase(Offset{at}, Offset{end});
        text.erase(at, end - at);
    }
    return next;
}

// ADR 0047 の契約: 200,000 行を開いてから先頭・中央・末尾に 200 回ずつ 1 文字と改行を入れては
// 消しても、行数・行の先頭・行番号・行の文字列が素朴な計算と一致する（窓の索引が狂わない）。
void verify_buffer_shared_index_edits()
{
    std::string text;
    for (std::size_t line = 0; line < 200000; ++line)
    {
        text += "line " + std::to_string(line) + "\n";
    }
    auto buffer = TextBuffer::from_utf8(text).value();
    std::size_t lines = 200001;
    bool counts = buffer.line_count() == lines;
    bool around = true;
    bool sampled = true;
    for (std::size_t round = 0; round < 200; ++round)
    {
        for (std::size_t place = 0; place < 3; ++place)
        {
            const std::size_t at = place * text.size() / 2;
            lines = lines_after(lines, text, at, round);
            buffer = edited(buffer, text, at, round);
            counts = counts && buffer.line_count() == lines;
            around = around && line_around_agrees(buffer, text, at);
        }
        sampled = sampled && (round % 40 != 39 || lines_agree(buffer, text, 1009));
    }
    expect(counts, "the line count follows 600 edits on 200,000 lines");
    expect(around, "the edited line reads back at the head, the middle and the tail");
    expect(sampled, "every 1009th line keeps its start, number and text");
    expect(buffer.text() == text, "the text after 600 edits matches the naive string");
}

// ADR 0047 の決定 1 / 4: chunk を跨いで打った改行の行番号と、境界を跨いで消した・入れた後の行。
void verify_buffer_chunk_line_numbers()
{
    auto text = TextBuffer::empty();
    std::string naive;
    for (std::size_t index = 0; index < 70000; ++index)
    {
        const std::string_view key = index % 100 == 99 ? "\n" : "a";
        text = text.insert(Offset{text.size_bytes()}, key);
        naive += key;
    }
    expect(lines_agree(text, naive, 1), "every line typed across the chunk boundary is numbered");
    const auto erased = text.erase(Offset{65000}, Offset{66000});
    naive.erase(65000, 1000);
    expect(lines_agree(erased, naive, 1), "erasing across the chunk boundary keeps the numbers");
    const auto inserted = erased.insert(Offset{64990}, "p\nq\n");
    naive.insert(64990, "p\nq\n");
    expect(lines_agree(inserted, naive, 1), "inserting inside the first chunk keeps the numbers");
}

// ADR 0047 の決定 1: 同じ値から分岐して改行を足しても互いの行番号が壊れない（索引は先端でだけ伸び、
// 古い値は自分の窓の端までしか読まない）。
void verify_buffer_index_branches()
{
    const auto tip = buffer_of("a\nb").insert(Offset{3}, "\nc\n");
    const auto grown = tip.insert(Offset{6}, "d\ne\n");
    const auto forked = tip.insert(Offset{6}, "\n\nf");
    const auto split = tip.insert(Offset{4}, "x\n");
    expect(lines_agree(tip, "a\nb\nc\n", 1), "the source keeps its lines after its branches grow");
    expect(lines_agree(grown, "a\nb\nc\nd\ne\n", 1), "the branch at the tip indexes its lines");
    expect(lines_agree(forked, "a\nb\nc\n\n\nf", 1),
           "the branch behind the tip does not see the other branch's newlines");
    expect(lines_agree(split, "a\nb\nx\nc\n", 1), "a branch that splits the source's piece");
    expect(lines_agree(grown.erase(Offset{4}, Offset{8}), "a\nb\ne\n", 1),
           "clipping the grown piece keeps its window");
    expect(lines_agree(forked.erase(Offset{3}, Offset{5}), "a\nb\n\n\nf", 1),
           "clipping the shared piece ignores the newlines another branch appended");
}

// ---------------------------------------------------------------- 位置と選択

void verify_offset_types()
{
    expect(Offset{3} == Offset{3} && !(Offset{3} == Offset{4}), "offsets compare by value");
    expect(Offset{3} < Offset{4} && !(Offset{4} < Offset{3}), "offsets order by value");
    expect(LineNumber{2} == LineNumber{2} && LineNumber{1} < LineNumber{2}, "line numbers compare");
    expect(!(LineNumber{2} < LineNumber{1}), "line numbers order one way");
    expect(Column{2} == Column{2} && Column{1} < Column{2}, "columns compare");
    expect(!(Column{2} < Column{1}), "columns order one way");
    expect(TextPosition{LineNumber{1}, Column{2}} == TextPosition{LineNumber{1}, Column{2}},
           "positions compare on both parts");
    expect(!(TextPosition{LineNumber{1}, Column{2}} == TextPosition{LineNumber{2}, Column{2}}),
           "a different line compares unequal");
    expect(OffsetRange{Offset{1}, Offset{2}} == OffsetRange{Offset{1}, Offset{2}},
           "ranges compare on both ends");
    expect(!(OffsetRange{Offset{1}, Offset{2}} == OffsetRange{Offset{1}, Offset{3}}),
           "a different end compares unequal");
    expect(is_empty(OffsetRange{Offset{2}, Offset{2}}), "an empty range is empty");
    expect(!is_empty(OffsetRange{Offset{1}, Offset{2}}), "a one-byte range is not empty");
}

void verify_selection()
{
    const Selection forward{Offset{2}, Offset{5}};
    const Selection backward{Offset{5}, Offset{2}};
    expect(selection_range(forward) == OffsetRange{Offset{2}, Offset{5}}, "a forward selection");
    expect(selection_range(backward) == OffsetRange{Offset{2}, Offset{5}},
           "a backward selection normalises");
    expect(has_selection(forward), "anchor and caret differ");
    expect(!has_selection(collapsed_at(Offset{4})), "a collapsed selection has nothing selected");
    expect(collapsed_at(Offset{4}) == Selection{Offset{4}, Offset{4}}, "collapsed_at builds both");
    expect(!(forward == backward), "selections compare on both positions");
    const auto span = nenenib::core::no_selection_span();
    expect(span.presence == SelectionPresence::absent, "the empty span is absent");
    expect(span == nenenib::core::no_selection_span(), "spans compare on all three parts");
    expect(
        !(span == nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{1}}),
        "presence is part of the comparison");
}

void verify_line_endings()
{
    expect(newline_of(LineEnding::crlf) == "\r\n", "CRLF writes both bytes");
    expect(newline_of(LineEnding::lf) == "\n", "LF writes one byte");
    expect(line_ending_label(LineEnding::crlf) == "CRLF", "the CRLF label");
    expect(line_ending_label(LineEnding::lf) == "LF", "the LF label");
}

// ---------------------------------------------------------------- キャレットの移動

void verify_caret_characters()
{
    const auto text = buffer_of("a\xE6\x97\xA5\r\nbc");
    expect(moved_caret(text, Offset{0}, CaretMotion::next_character, 1) == Offset{1},
           "right over one ASCII byte");
    expect(moved_caret(text, Offset{1}, CaretMotion::next_character, 1) == Offset{4},
           "right over a three-byte glyph");
    expect(moved_caret(text, Offset{4}, CaretMotion::next_character, 1) == Offset{6},
           "right at the line end jumps over the whole CRLF");
    expect(moved_caret(text, Offset{8}, CaretMotion::next_character, 1) == Offset{8},
           "right at the end of the buffer does not move");
    expect(moved_caret(text, Offset{4}, CaretMotion::previous_character, 1) == Offset{1},
           "left over a three-byte glyph");
    expect(moved_caret(text, Offset{6}, CaretMotion::previous_character, 1) == Offset{4},
           "left at a line start goes to the end of the line above");
    expect(moved_caret(text, Offset{0}, CaretMotion::previous_character, 1) == Offset{0},
           "left at the start of the buffer does not move");
}

void verify_caret_lines()
{
    const auto text = buffer_of("alpha\r\nxy\r\nomega");
    expect(moved_caret(text, Offset{3}, CaretMotion::next_line, 1) == Offset{9},
           "down keeps the column when the next line is shorter");
    expect(moved_caret(text, Offset{3}, CaretMotion::next_line, 1) ==
               text.offset_of(TextPosition{LineNumber{2}, Column{4}}),
           "down clamps the column to the line end");
    expect(moved_caret(text, Offset{13}, CaretMotion::previous_line, 1) == Offset{9},
           "up keeps the column");
    expect(moved_caret(text, Offset{2}, CaretMotion::previous_line, 1) == Offset{2},
           "up on the first line stays on the first line");
    expect(moved_caret(text, Offset{13}, CaretMotion::next_line, 1) == Offset{13},
           "down on the last line stays on the last line");
    expect(moved_caret(text, Offset{3}, CaretMotion::line_start, 1) == Offset{0}, "Home");
    expect(moved_caret(text, Offset{3}, CaretMotion::line_end, 1) == Offset{5},
           "End stops before the CRLF");
    expect(moved_caret(text, Offset{3}, CaretMotion::document_start, 1) == Offset{0}, "Ctrl+Home");
    expect(moved_caret(text, Offset{3}, CaretMotion::document_end, 1) == Offset{16}, "Ctrl+End");
    expect(moved_caret(text, Offset{13}, CaretMotion::page_up, 1) == Offset{9},
           "PgUp of one line keeps the column and clamps it to the shorter line");
    expect(moved_caret(text, Offset{13}, CaretMotion::page_up, 9) == Offset{2},
           "a page larger than the buffer stops at the first line");
    expect(moved_caret(text, Offset{0}, CaretMotion::page_down, 2) == Offset{11},
           "PgDn moves a page");
    expect(moved_caret(text, Offset{0}, CaretMotion::page_down, 9) == Offset{11},
           "a page larger than the buffer stops at the last line");
}

void verify_caret_words()
{
    const auto text = buffer_of("one two  three\r\nnext");
    expect(moved_caret(text, Offset{0}, CaretMotion::next_word, 1) == Offset{4},
           "Ctrl+Right skips the word and the space after it");
    expect(moved_caret(text, Offset{4}, CaretMotion::next_word, 1) == Offset{9},
           "Ctrl+Right skips two spaces");
    expect(moved_caret(text, Offset{9}, CaretMotion::next_word, 1) == Offset{14},
           "Ctrl+Right stops at the line end");
    expect(moved_caret(text, Offset{14}, CaretMotion::next_word, 1) == Offset{16},
           "Ctrl+Right at the line end moves to the next line");
    expect(moved_caret(text, Offset{14}, CaretMotion::previous_word, 1) == Offset{9},
           "Ctrl+Left goes to the start of the current word");
    expect(moved_caret(text, Offset{9}, CaretMotion::previous_word, 1) == Offset{4},
           "Ctrl+Left skips the spaces before the word");
    expect(moved_caret(text, Offset{16}, CaretMotion::previous_word, 1) == Offset{14},
           "Ctrl+Left at a line start goes to the line above");
    expect(moved_caret(text, Offset{0}, CaretMotion::previous_word, 1) == Offset{0},
           "Ctrl+Left at the start of the buffer does not move");
}

void verify_history_travel()
{
    const auto history = EditHistory::empty()
                             .pushed(Edit{Offset{0}, "", "a"}, EditBoundary::separate)
                             .pushed(Edit{Offset{1}, "", "b"}, EditBoundary::separate);
    expect(history.undo().value() == Edit{Offset{1}, "", "b"}, "undo names the last edit");
    expect(history.redo().error() == HistoryFailure::nothing_to_redo, "nothing to redo at the tip");
    const auto once = history.undone();
    expect(once.position() == 1 && once.size() == 2, "undone moves the position, not the list");
    expect(once.redo().value() == Edit{Offset{1}, "", "b"}, "redo names the edit just undone");
    const auto twice = once.undone();
    expect(twice.position() == 0, "two undos reach the start");
    expect(twice.undo().error() == HistoryFailure::nothing_to_undo, "nothing to undo at the start");
    expect(twice.undone().position() == 0, "undone at the start stays at the start");
    expect(history.redone().position() == 2, "redone at the tip stays at the tip");
    expect(twice.redone().position() == 1, "redone moves forward");
    const auto rewritten = twice.pushed(Edit{Offset{0}, "", "z"}, EditBoundary::separate);
    expect(rewritten.size() == 1, "a new edit after undo drops the redo tail");
    expect(EditHistory::empty().size() == 0, "a new history is empty");
}

// ---------------------------------------------------------------- スクロール

void verify_scroll_bounds()
{
    expect(first_visible_within(LineNumber{1}, 100, 10,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{1},
           "the top is allowed");
    expect(first_visible_within(LineNumber{0}, 100, 10,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{1},
           "line zero clamps up");
    expect(first_visible_within(LineNumber{91}, 100, 10,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{91},
           "the last page");
    expect(first_visible_within(LineNumber{99}, 100, 10,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{91},
           "past the last page");
    expect(first_visible_within(LineNumber{5}, 4, 10,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{1},
           "a buffer shorter than the window cannot scroll");
    expect(first_visible_within(LineNumber{5}, 100, 0,
                                nenenib::core::ScrollExtent::filled_viewport) == LineNumber{5},
           "a window with no room still needs one line");
    expect(first_visible_within(LineNumber{99}, 100, 10, nenenib::core::ScrollExtent::last_line) ==
               LineNumber{99},
           "Vim can put the last document lines at the top");
    expect(first_visible_within(LineNumber{101}, 100, 10, nenenib::core::ScrollExtent::last_line) ==
               LineNumber{100},
           "Vim still clamps beyond the last line");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{7}, 10,
                                   nenenib::core::ScrollFollow::minimal) == LineNumber{5},
           "a visible caret does not scroll");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{2}, 10,
                                   nenenib::core::ScrollFollow::minimal) == LineNumber{2},
           "a caret above the window pulls it up");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{20}, 10,
                                   nenenib::core::ScrollFollow::minimal) == LineNumber{11},
           "a caret below the window pulls it down");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{5}, 0,
                                   nenenib::core::ScrollFollow::minimal) == LineNumber{5},
           "a window with no room keeps the caret line");
    expect(first_visible_for_caret(LineNumber{11}, LineNumber{8}, 10,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{8},
           "Vim follows minimally just before the upper threshold");
    expect(first_visible_for_caret(LineNumber{11}, LineNumber{7}, 10,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{3},
           "Vim centers at the upper threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{20}, 10,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{11},
           "Vim follows minimally just before the lower threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{21}, 10,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{16},
           "Vim centers at the lower threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{10}, 10,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{6},
           "Vim preserves an explicit scroll while the caret remains visible");
}

void verify_vim_scroll_follow_thresholds()
{
    expect(first_visible_for_caret(LineNumber{10}, LineNumber{8}, 9,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{8},
           "height 9 follows minimally before its upper threshold");
    expect(first_visible_for_caret(LineNumber{10}, LineNumber{7}, 9,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{3},
           "height 9 centers at its upper threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{19}, 9,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{11},
           "height 9 follows minimally before its lower threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{20}, 9,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{16},
           "height 9 centers at its lower threshold");
    expect(first_visible_for_caret(LineNumber{12}, LineNumber{9}, 11,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{9},
           "height 11 follows minimally before its upper threshold");
    expect(first_visible_for_caret(LineNumber{12}, LineNumber{8}, 11,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{3},
           "height 11 centers at its upper threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{22}, 11,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{12},
           "height 11 follows minimally before its lower threshold");
    expect(first_visible_for_caret(LineNumber{6}, LineNumber{23}, 11,
                                   nenenib::core::ScrollFollow::vim) == LineNumber{18},
           "height 11 centers at its lower threshold");
}

void verify_body_layout()
{
    const auto layout = body_layout(640, 360, 96, nenenib::core::default_font_size());
    expect(layout.band == LayoutRect{0, 40, 640, 332}, "the body sits between the two bands");
    expect(layout.gutter == LayoutRect{0, 52, 56, 332}, "the gutter is 56 DIP wide");
    expect(layout.content == LayoutRect{56, 52, 640, 332}, "the content starts after the gutter");
    expect(layout.line_height == 29 && layout.caret_width == 2, "13.5 pt makes a 29 DIP row");
    expect(layout.visible_lines == 9, "280 pixels hold nine 29 DIP lines");
    expect(body_line_rect(layout, 0) == LayoutRect{0, 52, 640, 81}, "the first row");
    expect(body_line_rect(layout, 2) == LayoutRect{0, 110, 640, 139}, "the third row");
    const auto scaled = body_layout(800, 450, 120, nenenib::core::default_font_size());
    expect(scaled.line_height == 36, "29 DIP rounds to 36 pixels at 125 percent");
    expect(scaled.visible_lines == 9, "the taller window holds the same nine lines");
    const auto tiny = body_layout(640, 40, 96, nenenib::core::default_font_size());
    expect(tiny.visible_lines == 0, "a window with no body holds no lines");
}

// 採用案の配色表（docs/design/2026-09-15-look.md 第 3 節と編集の採用案 第 2 節）の全トークン。
void verify_dark_palette_tokens()
{
    const auto dark = theme_of(BuiltinTheme::ubuntu_aubergine).ui;
    expect(palette_for(Appearance::dark).background == dark.background,
           "dark maps to the aubergine theme");
    expect(dark.muted == RgbColor{0xB8, 0xA9, 0xB3}, "dark muted");
    expect(dark.gutter == RgbColor{0x7A, 0x66, 0x75}, "dark gutter");
    expect(dark.current_line == RgbColor{0x3E, 0x1A, 0x32}, "dark current line");
    expect(dark.title_bar == RgbColor{0x1E, 0x05, 0x16},
           "dark title bar band is the deep aubergine (D16)");
    expect(dark.tab_active == RgbColor{0x30, 0x0A, 0x24}, "dark active tab");
    expect(dark.tab_active == dark.background, "the dark active tab carries the body ground (D16)");
    expect(dark.status == RgbColor{0x26, 0x07, 0x1D}, "dark status band");
    expect(dark.selection == RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 71},
           "dark selection is orange at 28 percent");
    expect(dark.toggle == RgbColor{0x4A, 0x1E, 0x3D}, "dark toggle ground");
    expect(dark.on_accent == RgbColor{0xFF, 0xFF, 0xFF}, "text on the accent is white");
    expect(dark.panel == RgbColor{0x3B, 0x14, 0x30}, "dark panel");
    expect(dark.panel_border == RgbColor{0x5A, 0x2A, 0x4C}, "dark panel border");
    expect(dark.search == RgbaColor{RgbColor{0xF0, 0xA4, 0x7A}, 89},
           "dark search hit is pale orange at 35 percent");
    expect(dark.ime == RgbColor{0xD7, 0xC4, 0xE5}, "dark IME underline is the pale violet");
}

void verify_light_palette_tokens()
{
    const auto light = theme_of(BuiltinTheme::neutral_light).ui;
    expect(palette_for(Appearance::light).background == light.background,
           "light maps to the neutral theme");
    expect(light.muted == RgbColor{0x5C, 0x65, 0x70}, "light muted");
    expect(light.gutter == RgbColor{0x9A, 0xA3, 0xAD}, "light gutter");
    expect(light.current_line == RgbColor{0xE6, 0xE8, 0xEC}, "light current line");
    expect(light.title_bar == RgbColor{0xE1, 0xE4, 0xE9},
           "light title bar band is one step deeper than the body (D16)");
    expect(light.tab_active == RgbColor{0xF4, 0xF5, 0xF7}, "light active tab");
    expect(light.tab_active == light.background,
           "the light active tab carries the body ground (D16)");
    expect(light.status == RgbColor{0xE9, 0xEB, 0xEF}, "light status band");
    expect(light.selection == RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 56},
           "light selection is orange at 22 percent");
    expect(light.toggle == RgbColor{0xDF, 0xE3, 0xE8}, "light toggle ground");
    expect(light.on_accent == RgbColor{0xFF, 0xFF, 0xFF}, "text on the accent is white");
    expect(light.panel == RgbColor{0xFF, 0xFF, 0xFF}, "light panel");
    expect(light.panel_border == RgbColor{0xD5, 0xD9, 0xE0}, "light panel border");
    expect(light.search == RgbaColor{RgbColor{0xF0, 0xA4, 0x7A}, 77},
           "light search hit is pale orange at 30 percent");
    expect(light.ime == RgbColor{0x5E, 0x27, 0x50}, "light IME underline is the deep aubergine");
}

// ---------------------------------------------------------------- テーマ（ADR 0017）

// WCAG 2.x の相対輝度。浮動小数と std::pow は tests 側にだけ置く（core は整数だけ・決定 7）。
double channel_luminance(std::uint8_t value)
{
    const double level = static_cast<double>(value) / 255.0;
    return level <= 0.03928 ? level / 12.92 : std::pow((level + 0.055) / 1.055, 2.4);
}

double relative_luminance(RgbColor color)
{
    return 0.2126 * channel_luminance(color.red) + 0.7152 * channel_luminance(color.green) +
           0.0722 * channel_luminance(color.blue);
}

double contrast_ratio(RgbColor first, RgbColor second)
{
    const double one = relative_luminance(first) + 0.05;
    const double other = relative_luminance(second) + 0.05;
    return one > other ? one / other : other / one;
}

bool channel_within(std::uint8_t left, std::uint8_t right, int tolerance)
{
    const int difference = static_cast<int>(left) - static_cast<int>(right);
    return difference <= tolerance && -difference <= tolerance;
}

bool within(RgbColor left, RgbColor right, int tolerance)
{
    return channel_within(left.red, right.red, tolerance) &&
           channel_within(left.green, right.green, tolerance) &&
           channel_within(left.blue, right.blue, tolerance);
}

// 背景そのものを除いた本文トークン 15 個。どれかが背景と同じなら埋め忘れである。
std::array<RgbColor, 15> body_filled_tokens(const SyntaxPalette &body)
{
    return {body.foreground, body.cursor,   body.selection, body.current_line, body.line_number,
            body.comment,    body.keyword,  body.string,    body.number,       body.type,
            body.function,   body.constant, body.operators, body.error,        body.warning};
}

// 背景そのものと、背景と同じであることが決まっている tab_active（D16）を除いた UI トークン
// 14 個。selection / search は α を除いて色だけを見る。
std::array<RgbColor, 14> ui_filled_tokens(const Palette &ui)
{
    return {ui.text,         ui.muted,     ui.gutter,
            ui.current_line, ui.title_bar, ui.status,
            ui.accent,       ui.toggle,    ui.selection.color,
            ui.on_accent,    ui.panel,     ui.panel_border,
            ui.search.color, ui.ime};
}

// ADR 0017 の決定 7: 9 テーマ全部で本文と UI の前景／背景が 4.5:1 以上。
void verify_theme_contrast()
{
    for (const Theme &theme : builtin_themes)
    {
        const std::string name{theme.name};
        expect(contrast_ratio(theme.body.foreground, theme.body.background) >= 4.5,
               (name + ": the body foreground clears 4.5:1 over its background").c_str());
        expect(contrast_ratio(theme.ui.text, theme.ui.background) >= 4.5,
               (name + ": the UI text clears 4.5:1 over its background").c_str());
    }
}

void verify_theme_tokens_filled()
{
    for (const Theme &theme : builtin_themes)
    {
        const std::string name{theme.name};
        expect(!theme.source.author.empty() && !theme.source.license.empty() &&
                   !theme.source.url.empty(),
               (name + ": the source names an author, a licence and a URL").c_str());
        for (const RgbColor token : body_filled_tokens(theme.body))
        {
            expect(!(token == theme.body.background),
                   (name + ": every body token differs from the background").c_str());
        }
        for (const RgbColor token : ui_filled_tokens(theme.ui))
        {
            expect(!(token == theme.ui.background),
                   (name + ": every UI token differs from the background").c_str());
        }
    }
}

// 名前の表と enum は同じ添字で引く（決定 6）。最初に一致した行が自分の行なら重複は無い。
void verify_theme_names()
{
    for (std::size_t index = 0; index < builtin_themes.size(); ++index)
    {
        const auto theme = static_cast<BuiltinTheme>(index);
        const auto found = theme_named(theme_of(theme).name);
        const std::string name{theme_of(theme).name};
        expect(found.has_value() && static_cast<std::size_t>(found.value()) == index,
               (name + ": the name round-trips to its own row").c_str());
    }
    expect(builtin_themes.size() == 9, "the built-in table holds the nine themes of decision 6");
}

void verify_theme_name_spellings()
{
    expect(theme_named("solarized_dark") == std::optional{BuiltinTheme::solarized_dark},
           "an underscore spells the same name as a hyphen");
    expect(theme_named("night_owl_light") == std::optional{BuiltinTheme::night_owl_light},
           "every underscore is normalised, not just the first");
    expect(theme_named("ubuntu-aubergine") == std::optional{BuiltinTheme::ubuntu_aubergine},
           "the hyphen spelling is the name itself");
    expect(!theme_named("gruvbox").has_value(), "an unknown name selects no theme");
    expect(!theme_named("").has_value(), "the empty name selects no theme");
    expect(!theme_named("Dracula").has_value(), "the names are lowercase only");
    expect(!theme_named("dracula-dark").has_value(), "a longer name does not match a prefix");
}

// derive_ui は constexpr に評価でき、採用案のダークを掛けると地・文字・アクセント・タブが
// 一致する。title_bar は表の近似なのでチャンネルあたり 8 まで（決定 4 の「値ではなく規則」）。
void verify_theme_derivation()
{
    constexpr Palette derived = derive_ui(RgbColor{0x30, 0x0A, 0x24}, RgbColor{0xEE, 0xEE, 0xEC},
                                          RgbColor{0xE9, 0x54, 0x20}, Appearance::dark);
    static_assert(derived.background == RgbColor{0x30, 0x0A, 0x24}, "the ground is the ground");
    static_assert(derived.text == RgbColor{0xEE, 0xEE, 0xEC}, "the text is the foreground");
    static_assert(derived.accent == RgbColor{0xE9, 0x54, 0x20}, "the accent passes through");
    static_assert(derived.tab_active == derived.background, "the active tab carries the ground");
    static_assert(derived.on_accent == RgbColor{0xFF, 0xFF, 0xFF}, "white sits on the orange");
    const Palette adopted = theme_of(BuiltinTheme::ubuntu_aubergine).ui;
    expect(derived.background == adopted.background && derived.text == adopted.text &&
               derived.accent == adopted.accent && derived.tab_active == adopted.tab_active,
           "the rule reproduces the adopted ground, text, accent and active tab");
    expect(derived.selection == adopted.selection,
           "the rule reproduces the adopted selection opacity");
    expect(within(derived.title_bar, adopted.title_bar, 8),
           "the derived title bar stays within eight per channel of the adopted band");
}

void verify_rgba_equality()
{
    constexpr RgbaColor reference{RgbColor{0xE9, 0x54, 0x20}, 71};
    expect(reference == RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 71}, "identical RGBA compares equal");
    expect(!(reference == RgbaColor{RgbColor{0xE9, 0x54, 0x21}, 71}),
           "a different channel compares unequal");
    expect(!(reference == RgbaColor{RgbColor{0xE9, 0x54, 0x20}, 70}),
           "a different alpha compares unequal");
}

void verify_color_equality()
{
    constexpr RgbColor reference{0x30, 0x0A, 0x24};
    expect(reference == RgbColor{0x30, 0x0A, 0x24}, "identical channels compare equal");
    expect(!(reference == RgbColor{0x31, 0x0A, 0x24}), "a different red compares unequal");
    expect(!(reference == RgbColor{0x30, 0x0B, 0x24}), "a different green compares unequal");
    expect(!(reference == RgbColor{0x30, 0x0A, 0x25}), "a different blue compares unequal");
}

void verify_edit_mode()
{
    expect(toggled(EditMode::ordinary) == EditMode::vim, "ordinary toggles to vim");
    expect(toggled(EditMode::vim) == EditMode::ordinary, "vim toggles back to ordinary");
    expect(toggled(toggled(EditMode::ordinary)) == EditMode::ordinary, "two toggles return");
}

// 節目の名前は計測スクリプトの区間名でもあるので、重なったら内訳が読めなくなる。
[[nodiscard]] bool repeats_earlier_name(std::span<const std::string_view> names, std::size_t index)
{
    for (std::size_t earlier = 0; earlier < index; ++earlier)
    {
        if (names[earlier] == names[index])
        {
            return true;
        }
    }
    return false;
}

// 節目は閉じた選択肢で、名前は計測 JSON の正本である（ADR 0011 の決定 1・Issue #19）。
// 並びは起動の経路の正典順で、eng/measure-speed.py の STARTUP_MILESTONES と同じ。
void verify_milestone()
{
    constexpr std::array<Milestone, 11> ordered{
        Milestone::document_opened,   Milestone::window_created,  Milestone::backdrop_applied,
        Milestone::window_shown,      Milestone::device_created,  Milestone::swap_chain_created,
        Milestone::composition_bound, Milestone::context_created, Milestone::text_formats_created,
        Milestone::input_received,    Milestone::frame_presented};
    constexpr std::array<std::string_view, 11> expected_names{
        "document_opened",      "window_created",     "backdrop_applied",  "window_shown",
        "device_created",       "swap_chain_created", "composition_bound", "context_created",
        "text_formats_created", "input_received",     "frame_presented"};
    std::array<std::string_view, 11> seen{};
    for (std::size_t index = 0; index < ordered.size(); ++index)
    {
        seen[index] = milestone_name(ordered[index]);
        expect(seen[index] == expected_names[index],
               "each milestone names itself for the measurement file");
        expect(!repeats_earlier_name(std::span<const std::string_view>(seen), index),
               "no two milestones share a name");
    }
}

void verify_status_items()
{
    const auto items = status_items_for(TextPosition{LineNumber{1}, Column{1}}, TextEncoding::utf8,
                                        LineEnding::crlf);
    expect(items.at(0).text() == "行 1, 桁 1", "the caret position is the first item");
    expect(items.at(1).text() == "UTF-8", "the encoding follows the document");
    expect(items.at(2).text() == "CRLF", "the line ending follows the buffer");
    const auto moved = status_items_for(TextPosition{LineNumber{9}, Column{24}},
                                        TextEncoding::utf8_bom, LineEnding::lf);
    expect(moved.at(0).text() == "行 9, 桁 24", "the caret position is formatted from the numbers");
    expect(moved.at(0).code_point_count() == 9, "the formatted position counts code points");
    expect(moved.at(1).text() == "UTF-8 BOM", "a file with a BOM says so");
    expect(moved.at(2).text() == "LF", "an LF buffer says LF");
    const auto japanese = status_items_for(TextPosition{LineNumber{1}, Column{1}},
                                           TextEncoding::shift_jis, LineEnding::crlf);
    expect(japanese.at(1).text() == "Shift_JIS", "an old Japanese file says Shift_JIS");
}

void verify_device_pixels()
{
    expect(to_pixels(40, 96) == 40, "96 DPI is one to one");
    expect(to_pixels(40, 120) == 50, "125 percent scales exactly");
    expect(to_pixels(46, 120) == 58, "a half pixel rounds away from zero");
    expect(to_pixels(40, 144) == 60, "150 percent scales exactly");
    expect(to_pixels(0, 144) == 0, "zero stays zero");
}

void verify_rect_geometry()
{
    constexpr LayoutRect rectangle{10, 20, 30, 40};
    expect(width_of(rectangle) == 20, "width is right minus left");
    expect(height_of(rectangle) == 20, "height is bottom minus top");
    expect(rectangle == LayoutRect{10, 20, 30, 40}, "identical rectangles compare equal");
    expect(!(rectangle == LayoutRect{11, 20, 30, 40}), "a different left compares unequal");
    expect(!(rectangle == LayoutRect{10, 21, 30, 40}), "a different top compares unequal");
    expect(!(rectangle == LayoutRect{10, 20, 31, 40}), "a different right compares unequal");
    expect(!(rectangle == LayoutRect{10, 20, 30, 41}), "a different bottom compares unequal");
    expect(contains(rectangle, 10, 20), "the top left corner is inside");
    expect(!contains(rectangle, 9, 25), "one pixel left of the rectangle is outside");
    expect(!contains(rectangle, 30, 25), "the right edge is a half-open bound");
    expect(!contains(rectangle, 15, 19), "one pixel above the rectangle is outside");
    expect(!contains(rectangle, 15, 40), "the bottom edge is a half-open bound");
}

void verify_title_bar_rectangles()
{
    const auto layout = title_bar_layout(640, 96, 1);
    expect(layout.band == LayoutRect{0, 0, 640, 40}, "the band is 40 DIP high");
    expect(layout.close == LayoutRect{594, 0, 640, 40}, "close is the rightmost 46 DIP button");
    expect(layout.maximize == LayoutRect{548, 0, 594, 40}, "maximize sits left of close");
    expect(layout.minimize == LayoutRect{502, 0, 548, 40}, "minimize sits left of maximize");
    expect(layout.tabs == LayoutRect{8, 8, 208, 40}, "one tab is capped at 200 DIP");
    expect(layout.add_tab == LayoutRect{210, 8, 242, 40}, "the plus is 32 DIP after the tabs");
    expect(layout.underline == 2 && layout.corner_radius == 6, "the tab underline and radius");
    expect(layout.tab_count == 1, "an empty tab count is treated as one tab");
    expect(tab_rect(layout, 0) == layout.tabs, "a single tab fills the strip");
}

void verify_title_bar_scaling()
{
    const auto at_125 = title_bar_layout(800, 120, 1);
    expect(at_125.band == LayoutRect{0, 0, 800, 50}, "the band scales to 125 percent");
    expect(at_125.close == LayoutRect{742, 0, 800, 50}, "the buttons scale to 125 percent");
    expect(at_125.tabs == LayoutRect{10, 10, 260, 50}, "the tab scales to 125 percent");
    const auto at_150 = title_bar_layout(960, 144, 1);
    expect(at_150.band == LayoutRect{0, 0, 960, 60}, "the band scales to 150 percent");
    expect(at_150.close == LayoutRect{891, 0, 960, 60}, "the buttons scale to 150 percent");
    expect(at_150.tabs == LayoutRect{12, 12, 312, 60}, "the tab scales to 150 percent");
    const auto maximised = title_bar_layout(2560, 96, 1);
    expect(maximised.close == LayoutRect{2514, 0, 2560, 40}, "close follows the right edge");
    expect(maximised.tabs == LayoutRect{8, 8, 208, 40}, "a wide window does not widen the tab");
}

void verify_title_bar_tab_counts()
{
    const auto two = title_bar_layout(1280, 96, 2);
    expect(two.tabs == LayoutRect{8, 8, 410, 40}, "two tabs share the strip with a 2 DIP gap");
    expect(tab_rect(two, 0) == LayoutRect{8, 8, 208, 40}, "the first tab starts at the left");
    expect(tab_rect(two, 1) == LayoutRect{210, 8, 410, 40}, "the second tab follows the gap");
    expect(two.add_tab == LayoutRect{412, 8, 444, 40}, "the plus follows the last tab");
    const auto narrow = title_bar_layout(300, 96, 1);
    expect(narrow.tab_width == 120, "a narrow window clamps the tab to 120 DIP");
    expect(title_bar_hit(narrow, 260, 20) == TitleBarHit::close,
           "the window buttons win over an overlapping tab strip");
}

void verify_title_bar_hits()
{
    const auto layout = title_bar_layout(640, 96, 1);
    expect(title_bar_hit(layout, 617, 20) == TitleBarHit::close, "the close button centre");
    expect(title_bar_hit(layout, 571, 20) == TitleBarHit::maximize, "the maximize button centre");
    expect(title_bar_hit(layout, 525, 20) == TitleBarHit::minimize, "the minimize button centre");
    expect(title_bar_hit(layout, 226, 20) == TitleBarHit::add_tab, "the plus centre");
    expect(title_bar_hit(layout, 108, 20) == TitleBarHit::tab, "the tab centre");
    expect(title_bar_hit(layout, 300, 20) == TitleBarHit::caption,
           "the empty strip is the caption");
    expect(title_bar_hit(layout, 4, 2) == TitleBarHit::caption, "above the tab is the caption");
    expect(title_bar_hit(layout, 108, 39) == TitleBarHit::tab,
           "the tab reaches the bottom edge of the band");
    expect(title_bar_hit(layout, 300, 40) == TitleBarHit::none,
           "below the band is not the caption");
    expect(title_bar_hit(layout, 640, 20) == TitleBarHit::none, "right of the band is nothing");
}

void verify_status_bar_rectangles()
{
    const auto layout = status_bar_layout(640, 360, 96);
    expect(layout.band == LayoutRect{0, 332, 640, 360}, "the band is the bottom 28 DIP");
    expect(layout.toggle == LayoutRect{12, 334, 106, 358}, "the toggle is 94 by 24 DIP");
    expect(layout.toggle_ordinary == LayoutRect{14, 336, 58, 356}, "the ordinary half");
    expect(layout.toggle_vim == LayoutRect{60, 336, 104, 356}, "the Vim half");
    expect(layout.mode == LayoutRect{122, 332, 194, 360}, "the mode label follows the toggle");
    expect(layout.items.at(0) == LayoutRect{392, 332, 488, 360}, "the caret position item");
    expect(layout.items.at(1) == LayoutRect{504, 332, 576, 360}, "the encoding item is 72 DIP");
    expect(layout.items.at(2) == LayoutRect{592, 332, 628, 360}, "the line ending item");
    expect(layout.corner_radius == 6 && layout.segment_radius == 4, "the toggle radii");
}

void verify_status_bar_scaling()
{
    const auto at_125 = status_bar_layout(800, 450, 120);
    expect(at_125.band == LayoutRect{0, 415, 800, 450}, "the band scales to 125 percent");
    expect(at_125.toggle == LayoutRect{15, 417, 134, 448}, "the toggle scales to 125 percent");
    const auto at_150 = status_bar_layout(960, 540, 144);
    expect(at_150.band == LayoutRect{0, 498, 960, 540}, "the band scales to 150 percent");
    expect(at_150.toggle == LayoutRect{18, 501, 159, 537}, "the toggle scales to 150 percent");
    const auto tiny = status_bar_layout(640, 10, 96);
    expect(tiny.band == LayoutRect{0, 0, 640, 10}, "a window shorter than the band keeps the top");
}

void verify_status_bar_hits()
{
    const auto layout = status_bar_layout(640, 360, 96);
    expect(status_bar_hit(layout, 36, 346) == StatusBarHit::toggle_ordinary, "the ordinary centre");
    expect(status_bar_hit(layout, 82, 346) == StatusBarHit::toggle_vim, "the Vim centre");
    expect(status_bar_hit(layout, 59, 346) == StatusBarHit::none, "the gap between the halves");
    expect(status_bar_hit(layout, 300, 346) == StatusBarHit::none, "the empty band is nothing");
    expect(status_bar_hit(layout, 36, 300) == StatusBarHit::none, "the text area is nothing");
}

// ---------------------------------------------------------------- 文字コードと改行

void verify_encoding_labels()
{
    expect(encoding_label(TextEncoding::utf8) == "UTF-8", "the plain label");
    expect(encoding_label(TextEncoding::utf8_bom) == "UTF-8 BOM", "the BOM label");
    expect(encoding_label(TextEncoding::shift_jis) == "Shift_JIS", "the Japanese label");
    expect(byte_order_mark() == "\xEF\xBB\xBF", "the BOM is three bytes");
    expect(without_byte_order_mark("abc") == "abc", "text without a BOM is unchanged");
    expect(without_byte_order_mark(std::string(byte_order_mark()) + "abc") == "abc",
           "the BOM is dropped from the front");
    expect(without_byte_order_mark("\xEF\xBB").size() == 2, "half a BOM is not a BOM");
}

void verify_encoding_detection()
{
    expect(detect_encoding("").value() == TextEncoding::utf8, "an empty file is UTF-8");
    expect(detect_encoding("plain ASCII\r\n").value() == TextEncoding::utf8, "ASCII is UTF-8");
    expect(detect_encoding("日本語").value() == TextEncoding::utf8, "valid UTF-8 always wins");
    expect(detect_encoding(std::string(byte_order_mark()) + "日本語").value() ==
               TextEncoding::utf8_bom,
           "a BOM in front of valid UTF-8 is UTF-8 BOM");
    expect(detect_encoding(std::string(byte_order_mark())).value() == TextEncoding::utf8_bom,
           "a BOM on its own is UTF-8 BOM");
    // BOM の後ろが壊れていれば BOM とは見なさない。残りが CP932 でもなければ開かない（決定 3）。
    expect(detect_encoding(std::string(byte_order_mark()) + "\xFF").error() ==
               EncodingFailure::undecodable,
           "a BOM followed by bytes that are neither is rejected");
    expect(!detect_encoding(std::string(byte_order_mark()) + "\x93\xFA").has_value(),
           "a BOM is a label: the body after it never falls through to CP932");
    expect(detect_encoding("\x93\xFA\x96\x7B").value() == TextEncoding::shift_jis,
           "日本 in CP932 is Shift_JIS");
    expect(detect_encoding("\x81\x40").value() == TextEncoding::shift_jis,
           "a CP932 pair with a low trail byte");
    expect(detect_encoding("\xB1\xB2\xB3").value() == TextEncoding::shift_jis,
           "half width katakana are single CP932 bytes");
    expect(detect_encoding("abc\x93\xFA").value() == TextEncoding::shift_jis,
           "ASCII mixed with CP932 pairs");
    expect(!detect_encoding("\x93").has_value(), "a lead byte at the end of the file");
    expect(!detect_encoding("\x81\x20").has_value(), "a trail byte below the range");
    expect(!detect_encoding("\x81\x7F").has_value(), "a trail byte in the gap");
    expect(!detect_encoding("\xA0").has_value(), "0xA0 is neither a single byte nor a lead byte");
    expect(!detect_encoding("\xFF\xFE\x00\x41").has_value(), "UTF-16 is neither");
}

void verify_line_ending_detection()
{
    expect(detect_line_ending("一行目\r\n二行目") == LineEnding::crlf, "CRLF is seen");
    expect(detect_line_ending("一行目\n二行目") == LineEnding::lf, "LF is seen");
    expect(detect_line_ending("一行だけ") == LineEnding::crlf, "no newline means CRLF");
    expect(detect_line_ending("") == LineEnding::crlf, "an empty file means CRLF");
    expect(detect_line_ending("a\r\nb\nc") == LineEnding::crlf, "the first newline decides");
    expect(detect_line_ending("a\nb\r\nc") == LineEnding::lf, "the first newline decides, again");
    expect(detect_line_ending("\na") == LineEnding::lf, "a newline at the very start is LF");
    expect(detect_line_ending("a\rb") == LineEnding::crlf, "a lone CR is not a newline");
}

// 本文が持つ改行の形と、行の切り方（ADR 0036 の決定 1 と決定 2）。'\n' の直前の '\r' を
// 改行の一部として外すのは CRLF の本文だけで、LF の本文の '\r' は 1 文字である。
void verify_line_ending_model()
{
    expect(TextBuffer::empty().line_ending() == LineEnding::crlf, "a new buffer is CRLF");
    const auto lf = TextBuffer::from_utf8("abc\nde\r\nfgh").value();
    expect(lf.line_ending() == LineEnding::lf, "the first newline decides the form");
    expect(lf.line_count() == 3, "the CR before the LF does not add a line");
    expect(lf.line_text(LineNumber{2}) == "de\r", "an LF document keeps the CR at the line end");
    expect(lf.line_end(LineNumber{2}) == Offset{7}, "the content of the line ends after the CR");
    expect(lf.line_terminator_end(LineNumber{2}) == Offset{8}, "the newline is the LF alone");
    expect(lf.position_of(Offset{6}) == TextPosition{LineNumber{2}, Column{3}},
           "the CR is the third code point of its line");
    expect(lf.offset_of(TextPosition{LineNumber{2}, Column{3}}) == Offset{6},
           "and the third column is that CR");
    const auto lone = TextBuffer::from_utf8("a\rb\ncd").value();
    expect(lone.line_ending() == LineEnding::lf && lone.line_text(LineNumber{1}) == "a\rb",
           "a CR inside a line is a character");
    const auto doubled = TextBuffer::from_utf8("a\nb\r\r\nc").value();
    expect(doubled.line_text(LineNumber{2}) == "b\r\r", "an LF document keeps both CRs");

    const auto crlf = TextBuffer::from_utf8("abc\r\nde\r\r\nfgh").value();
    expect(crlf.line_ending() == LineEnding::crlf, "the first CRLF decides the form");
    expect(crlf.line_count() == 3, "CRLF is one newline");
    expect(crlf.line_text(LineNumber{2}) == "de\r",
           "in a CRLF document only the CR next to the LF belongs to the newline");
    expect(crlf.line_end(LineNumber{2}) == Offset{8}, "the extra CR stays in the content");
    expect(crlf.line_terminator_end(LineNumber{2}) == Offset{10}, "the newline is the CRLF");
    expect(crlf.line_text(LineNumber{1}) == "abc", "the plain CRLF line has no CR");
    const auto single = TextBuffer::from_utf8("a\r\nb").value();
    expect(single.line_text(LineNumber{1}) == "a", "the CR of a CRLF newline is not content");

    expect(lf.insert(Offset{0}, "x\r\ny").line_ending() == LineEnding::lf,
           "an insertion does not change the form the document was read with");
    expect(crlf.erase(Offset{0}, Offset{5}).line_ending() == LineEnding::crlf,
           "and neither does a removal");
    expect(TextBuffer::empty().insert(Offset{0}, "a\nb").line_ending() == LineEnding::crlf,
           "an empty buffer stays CRLF, which is what Enter inserts");
}

// ---------------------------------------------------------------- 経路と題名

void verify_file_path()
{
    const auto windows = FilePath::parse("C:\\work\\note.txt");
    expect(windows.has_value() && windows.value().text() == "C:\\work\\note.txt",
           "a Windows path is kept as it came");
    expect(windows.value().file_name() == "note.txt", "the name is what follows the last slash");
    expect(FilePath::parse("/home/hide/note.md").value().file_name() == "note.md",
           "a forward slash separates too");
    expect(FilePath::parse("note.txt").value().file_name() == "note.txt",
           "a bare name is its own file name");
    expect(FilePath::parse("C:\\work\\").value().file_name().empty(),
           "a path that ends in a separator has no name");
    expect(FilePath::parse("").error() == TextFailure::empty, "the empty path is rejected");
    expect(FilePath::parse("a\nb").error() == TextFailure::control_character,
           "a control character is rejected");
    expect(FilePath::parse("\xFF").error() == TextFailure::invalid_utf8,
           "a path that is not UTF-8 is rejected");
    expect(windows.value() == FilePath::parse("C:\\work\\note.txt").value(),
           "paths compare on the text");
    expect(!(windows.value() == FilePath::parse("C:\\work\\other.txt").value()),
           "different paths do not compare equal");
}

void verify_tab_titles()
{
    expect(tab_title_for(std::nullopt, SaveState::saved).text() == "無題",
           "no path at all is 無題");
    expect(tab_title_for(std::nullopt, SaveState::modified).text() == "● 無題",
           "an unsaved buffer carries the mark");
    const auto path = FilePath::parse("C:\\work\\note.txt").value();
    expect(tab_title_for(path, SaveState::saved).text() == "note.txt", "a saved file is its name");
    expect(tab_title_for(path, SaveState::modified).text() == "● note.txt",
           "an unsaved file carries the mark before the name");
    expect(tab_title_for(FilePath::parse("C:\\").value(), SaveState::saved).text() == "C:\\",
           "a path without a name falls back to the path");
    const auto clipped =
        tab_title_for(FilePath::parse(std::string(300, 'a')).value(), SaveState::saved);
    expect(clipped.text().size() <= DisplayText::maximum_bytes, "a long name is clipped");
    expect(clipped.text().ends_with("…"), "the clipped name says it was clipped");
    std::string wide_name;
    for (std::size_t index = 0; index < 100; ++index)
    {
        wide_name += "あ";
    }
    const auto wide_clipped =
        tab_title_for(FilePath::parse(wide_name).value(), SaveState::modified);
    expect(wide_clipped.text().size() <= DisplayText::maximum_bytes, "wide names are clipped too");
    expect(validate_utf8(wide_clipped.text()).has_value(),
           "the clip lands on a code point boundary");
    expect(wide_clipped.text().starts_with("● "), "the mark survives the clip");
}

void verify_save_state_of_document()
{
    const Document untouched{std::nullopt, TextEncoding::utf8, std::size_t{0}};
    expect(save_state_of(untouched, 0) == SaveState::saved, "the start of a new buffer is saved");
    expect(save_state_of(untouched, 1) == SaveState::modified, "one edit away is modified");
    const Document adrift{std::nullopt, TextEncoding::utf8, std::nullopt};
    expect(save_state_of(adrift, 0) == SaveState::modified,
           "a buffer with no save point is always modified");
}
} // namespace

void verify_display_text_accepts_ascii()
{
    expect_accepted("NeNe Nib 0.1.0", 14, "ASCII line is accepted");
    expect_accepted(" ", 1, "a single space is accepted");
}

// ---------------------------------------------------------------- 履歴

void verify_history_coalescing()
{
    const auto history = EditHistory::empty()
                             .pushed(Edit{Offset{0}, "", "a"}, EditBoundary::coalesce)
                             .pushed(Edit{Offset{1}, "", "b"}, EditBoundary::coalesce)
                             .pushed(Edit{Offset{2}, "", "c"}, EditBoundary::coalesce);
    expect(history.size() == 1 && history.position() == 1, "three keystrokes are one unit");
    expect(history.undo().value() == Edit{Offset{0}, "", "abc"}, "the unit holds all three");
    const auto broken = history.pushed(Edit{Offset{3}, "", "d"}, EditBoundary::separate);
    expect(broken.size() == 2, "a separate boundary starts a new unit");
    const auto jumped = history.pushed(Edit{Offset{9}, "", "d"}, EditBoundary::coalesce);
    expect(jumped.size() == 2, "an insert somewhere else starts a new unit");
    const auto removed = history.pushed(Edit{Offset{3}, "x", ""}, EditBoundary::coalesce);
    expect(removed.size() == 2, "a deletion never joins the unit before it");
    const auto after_removal = removed.pushed(Edit{Offset{3}, "", "y"}, EditBoundary::coalesce);
    expect(after_removal.size() == 3, "an insert after a deletion starts its own unit");
    const auto sealed = history.sealed();
    expect(sealed.size() == 1 && sealed.position() == 1, "sealing keeps the edits and the place");
    const auto after_seal = sealed.pushed(Edit{Offset{3}, "", "d"}, EditBoundary::coalesce);
    expect(after_seal.size() == 2 && after_seal.position() == 2,
           "a sealed unit does not take the next keystroke");
    const auto first =
        EditHistory::empty().pushed(Edit{Offset{0}, "", "a"}, EditBoundary::coalesce);
    expect(first.size() == 1, "the first edit has nothing to join");
}

// INSERT の1単位。範囲内編集と直前の削除を畳み、離れていれば新しい単位（ADR 0028）。
void verify_history_absorbing()
{
    const Edit typed{Offset{5}, "", "ab"};
    expect(absorbed_edit(typed, Edit{Offset{7}, "", "c"}) == Edit{Offset{5}, "", "abc"},
           "an insert right after the inserted text grows it");
    expect(absorbed_edit(typed, Edit{Offset{6}, "b", ""}) == Edit{Offset{5}, "", "a"},
           "a backspace over the inserted text shrinks it");
    expect(absorbed_edit(Edit{Offset{5}, "", ""}, Edit{Offset{4}, "x", ""}) ==
               Edit{Offset{4}, "x", ""},
           "a backspace before the insert moves the head of the unit back");
    expect(!absorbed(typed, Edit{Offset{9}, "", "c"}).has_value(),
           "an insert somewhere else does not join");
    expect(!absorbed(typed, Edit{Offset{0}, "x", ""}).has_value(),
           "a deletion somewhere else does not join");
    const auto history = EditHistory::empty()
                             .pushed(Edit{Offset{0}, "", "a"}, EditBoundary::absorb)
                             .pushed(Edit{Offset{0}, "a", ""}, EditBoundary::absorb)
                             .pushed(Edit{Offset{0}, "", "b"}, EditBoundary::absorb);
    expect(history.size() == 1, "the three edits of one insert are one undo unit");
    expect(history.undo().value() == Edit{Offset{0}, "", "b"}, "and the unit is what is left");
    expect(history.pushed(Edit{Offset{9}, "", "z"}, EditBoundary::absorb).size() == 2,
           "an edit that does not touch the unit opens a new one");
    expect(history.pushed(Edit{Offset{1}, "", "z"}, EditBoundary::separate).size() == 2,
           "a separate boundary never joins");
    expect(EditHistory::empty().pushed(typed, EditBoundary::absorb).size() == 1,
           "the first edit of a unit has nothing to be absorbed into");
}

// ---------------------------------------------------------------- 既存の表示値

void verify_palette()
{
    const auto light = palette_for(Appearance::light);
    const auto dark = palette_for(Appearance::dark);
    expect(light.background == RgbColor{0xF4, 0xF5, 0xF7}, "light background");
    expect(light.text == RgbColor{0x1B, 0x1F, 0x24}, "light text");
    expect(dark.background == RgbColor{0x30, 0x0A, 0x24}, "dark background is the aubergine");
    expect(dark.text == RgbColor{0xEE, 0xEE, 0xEC}, "dark text is the pale grey");
    expect(!(light.background == dark.background), "the two appearances differ");
    expect(light.accent == dark.accent && dark.accent == RgbColor{0xE9, 0x54, 0x20},
           "the Ubuntu orange accent is the same in both appearances");
}

// 本文まわり（Utf8・TextBuffer・キャレット・履歴・スクロール）をまとめて回す。
void verify_text_and_caret()
{
    verify_display_text_accepts_multibyte();
    verify_display_text_lengths();
    verify_display_text_rejects();
    verify_utf8_validation();
    verify_utf8_counting();
    verify_utf8_walking();
    verify_utf16_encoding();
    verify_utf16_rejects();
    verify_utf16_round_trip();
    verify_buffer_creation();
    verify_buffer_insertion();
    verify_buffer_erasure();
    verify_buffer_lines();
    verify_buffer_crlf_split();
    verify_buffer_positions();
    verify_buffer_round_trip();
    verify_buffer_scale();
    verify_buffer_add_branches();
    verify_buffer_chunk_growth();
    verify_buffer_oversized_and_restored();
    verify_buffer_shared_index_edits();
    verify_buffer_chunk_line_numbers();
    verify_buffer_index_branches();
    verify_offset_types();
    verify_selection();
    verify_line_endings();
    verify_caret_characters();
    verify_caret_lines();
    verify_caret_words();
    verify_history_coalescing();
    verify_history_absorbing();
    verify_history_travel();
    verify_encoding_labels();
    verify_encoding_detection();
    verify_line_ending_detection();
    verify_line_ending_model();
    verify_file_path();
    verify_tab_titles();
    verify_save_state_of_document();
    verify_scroll_bounds();
    verify_vim_scroll_follow_thresholds();
    verify_body_layout();
}

void verify_look()
{
    verify_color_equality();
    verify_rgba_equality();
    verify_dark_palette_tokens();
    verify_light_palette_tokens();
    verify_theme_contrast();
    verify_theme_tokens_filled();
    verify_theme_names();
    verify_theme_name_spellings();
    verify_theme_derivation();
    verify_edit_mode();
    verify_milestone();
    verify_status_items();
    verify_device_pixels();
    verify_rect_geometry();
    verify_title_bar_rectangles();
    verify_title_bar_scaling();
    verify_title_bar_tab_counts();
    verify_title_bar_hits();
    verify_status_bar_rectangles();
    verify_status_bar_scaling();
    verify_status_bar_hits();
}
} // namespace nenenib::tests
