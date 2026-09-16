// core / application だけを対象にした単体テスト（QLT-013）。OS 資源には触れない。
// --coverage-negative は失敗系を全部省く。その実行が QLT-009 の閾値で落ちることが反例である。
// 時間は測らない（<chrono> は ARC-007 でここに書けない）。1 MB と 1
// 万行は「終わること」だけを見る。
#include "Appearance.hpp"
#include "AppearancePort.hpp"
#include "AppearanceReadFailure.hpp"
#include "BodyLayout.hpp"
#include "BuiltinTheme.hpp"
#include "CancelComposition.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "CaretMove.hpp"
#include "CaretShape.hpp"
#include "ClauseEmphasis.hpp"
#include "ClipboardFailure.hpp"
#include "ClipboardOperation.hpp"
#include "ClipboardPort.hpp"
#include "CodePageFailure.hpp"
#include "CodePagePort.hpp"
#include "Column.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "CompositionView.hpp"
#include "DeleteDirection.hpp"
#include "DevicePixels.hpp"
#include "DisplayText.hpp"
#include "Document.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorController.hpp"
#include "EditorIntent.hpp"
#include "EditorState.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "HistoryDirection.hpp"
#include "HistoryFailure.hpp"
#include "LayoutRect.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Milestone.hpp"
#include "ModeLabel.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "Palette.hpp"
#include "PieceSource.hpp"
#include "PlaceCaret.hpp"
#include "RgbColor.hpp"
#include "RgbaColor.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "ScrollBounds.hpp"
#include "ScrollState.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "StatusBarHit.hpp"
#include "StatusBarLayout.hpp"
#include "StatusItems.hpp"
#include "TabTitle.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextFailure.hpp"
#include "TextPosition.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimNewLine.hpp"
#include "VimNoEffect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using nenenib::application::AppearancePort;
using nenenib::application::AppearanceReadFailure;
using nenenib::application::CancelComposition;
using nenenib::application::CancelSelection;
using nenenib::application::CaretView;
using nenenib::application::ClipboardAction;
using nenenib::application::ClipboardFailure;
using nenenib::application::ClipboardOperation;
using nenenib::application::ClipboardPort;
using nenenib::application::CodePageFailure;
using nenenib::application::CodePagePort;
using nenenib::application::CommitText;
using nenenib::application::ComposeText;
using nenenib::application::DeleteText;
using nenenib::application::Document;
using nenenib::application::EditorController;
using nenenib::application::EditorFrame;
using nenenib::application::EditorState;
using nenenib::application::FileFailure;
using nenenib::application::FilePort;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::MoveCaret;
using nenenib::application::NewLine;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::RefreshAppearance;
using nenenib::application::save_state_of;
using nenenib::application::SaveDocument;
using nenenib::application::ScrollLines;
using nenenib::application::ScrollState;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::Appearance;
using nenenib::core::body_layout;
using nenenib::core::body_line_rect;
using nenenib::core::BuiltinTheme;
using nenenib::core::byte_order_mark;
using nenenib::core::CaretMotion;
using nenenib::core::CaretShape;
using nenenib::core::ClauseEmphasis;
using nenenib::core::code_point_at;
using nenenib::core::code_point_count;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::Composition;
using nenenib::core::composition_underlines;
using nenenib::core::CompositionClause;
using nenenib::core::contains;
using nenenib::core::DeleteDirection;
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
using nenenib::core::HistoryDirection;
using nenenib::core::HistoryFailure;
using nenenib::core::is_boundary;
using nenenib::core::is_empty;
using nenenib::core::LayoutRect;
using nenenib::core::line_ending_label;
using nenenib::core::LineEnding;
using nenenib::core::LineNumber;
using nenenib::core::Milestone;
using nenenib::core::milestone_name;
using nenenib::core::mode_label;
using nenenib::core::moved_caret;
using nenenib::core::newline_of;
using nenenib::core::next_code_point;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Palette;
using nenenib::core::palette_for;
using nenenib::core::palette_of;
using nenenib::core::previous_code_point;
using nenenib::core::RgbaColor;
using nenenib::core::RgbColor;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::selection_range;
using nenenib::core::SelectionAnchoring;
using nenenib::core::SelectionPresence;
using nenenib::core::status_bar_hit;
using nenenib::core::status_bar_layout;
using nenenib::core::status_items_for;
using nenenib::core::StatusBarHit;
using nenenib::core::tab_rect;
using nenenib::core::tab_title_for;
using nenenib::core::TextBuffer;
using nenenib::core::TextEncoding;
using nenenib::core::TextFailure;
using nenenib::core::TextPosition;
using nenenib::core::title_bar_hit;
using nenenib::core::title_bar_layout;
using nenenib::core::TitleBarHit;
using nenenib::core::to_pixels;
using nenenib::core::to_utf16;
using nenenib::core::to_utf8;
using nenenib::core::toggled;
using nenenib::core::validate_utf8;
using nenenib::core::vim_first_non_blank;
using nenenib::core::vim_next_word;
using nenenib::core::vim_previous_word;
using nenenib::core::vim_resting_caret;
using nenenib::core::vim_step;
using nenenib::core::VimCharacter;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimNewLine;
using nenenib::core::VimNoEffect;
using nenenib::core::VimSpecialKey;
using nenenib::core::VimState;
using nenenib::core::VimWordStop;
using nenenib::core::width_of;
using nenenib::core::without_byte_order_mark;
using nenenib::tests::VimFixture;
using Reading = std::expected<Appearance, AppearanceReadFailure>;
using Content = std::expected<std::string, ClipboardFailure>;
using Bytes = std::expected<std::string, FileFailure>;
using Converted = std::expected<std::string, CodePageFailure>;

// 可変グローバルは置けない（ARC-005 / clang-tidy）。計数は関数局所の静的値で持つ。
std::size_t &failure_count()
{
    static std::size_t failures = 0;
    return failures;
}

std::size_t &check_count()
{
    static std::size_t checks = 0;
    return checks;
}

void expect(bool condition, const char *description)
{
    ++check_count();
    if (!condition)
    {
        ++failure_count();
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

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

class ScriptedAppearance final : public AppearancePort
{
  public:
    explicit ScriptedAppearance(Reading reading) : reading_(std::move(reading)) {}

    void script(Reading reading)
    {
        reading_ = std::move(reading);
    }

    [[nodiscard]] Reading current() const noexcept override
    {
        return reading_;
    }

  private:
    Reading reading_;
};

// 偽のクリップボード。OS を呼ばずに「置けた」「置けない」「空」を作り分ける（ARC-007）。
class ScriptedClipboard final : public ClipboardPort
{
  public:
    void hold(Content content)
    {
        content_ = std::move(content);
    }

    void refuse_writes()
    {
        writable_ = false;
    }

    [[nodiscard]] std::expected<void, ClipboardFailure> write(std::string_view utf8) override
    {
        if (!writable_)
        {
            return std::unexpected(ClipboardFailure::write_failed);
        }
        content_ = std::string(utf8);
        return {};
    }

    [[nodiscard]] Content read() override
    {
        return content_;
    }

  private:
    Content content_{std::unexpected(ClipboardFailure::empty)};
    bool writable_ = true;
};

// 偽のファイル。OS に触れずに「読めた」「読めない」「書けない」を作り分ける（ARC-007）。
class ScriptedFiles final : public FilePort
{
  public:
    void hold(Bytes content)
    {
        content_ = std::move(content);
    }

    void refuse_writes(FileFailure failure)
    {
        write_failure_ = failure;
    }

    [[nodiscard]] const std::string &written() const noexcept
    {
        return written_;
    }

    [[nodiscard]] const std::string &written_path() const noexcept
    {
        return written_path_;
    }

    [[nodiscard]] const std::string &read_path() const noexcept
    {
        return read_path_;
    }

    // application が渡してきた上限。値の正本が 1 つであることをテストが見る（ARC-001）。
    [[nodiscard]] std::size_t read_limit() const noexcept
    {
        return read_limit_;
    }

    [[nodiscard]] Bytes read(const FilePath &path, std::size_t maximum_bytes) override
    {
        read_path_ = std::string(path.text());
        read_limit_ = maximum_bytes;
        return content_;
    }

    [[nodiscard]] std::expected<void, FileFailure> write(const FilePath &path,
                                                         std::string_view bytes) override
    {
        if (write_failure_.has_value())
        {
            return std::unexpected(write_failure_.value());
        }
        written_path_ = std::string(path.text());
        written_ = std::string(bytes);
        return {};
    }

  private:
    Bytes content_{std::unexpected(FileFailure::not_found)};
    std::optional<FileFailure> write_failure_;
    std::string written_;
    std::string written_path_;
    std::string read_path_;
    std::size_t read_limit_ = 0;
};

// 偽の CP932。実際の表は持たず、台本の答えを返すだけ（ADR 0010 の決定 2 の境界を測る）。
class ScriptedCodePages final : public CodePagePort
{
  public:
    void decode_to(Converted decoded)
    {
        decoded_ = std::move(decoded);
    }

    void encode_to(Converted encoded)
    {
        encoded_ = std::move(encoded);
    }

    [[nodiscard]] const std::string &encoded_from() const noexcept
    {
        return encoded_from_;
    }

    [[nodiscard]] Converted to_utf8(std::string_view cp932) override
    {
        decoded_from_ = std::string(cp932);
        return decoded_;
    }

    [[nodiscard]] Converted from_utf8(std::string_view utf8) override
    {
        encoded_from_ = std::string(utf8);
        return encoded_;
    }

  private:
    Converted decoded_{std::string{}};
    Converted encoded_{std::string{}};
    std::string decoded_from_;
    std::string encoded_from_;
};

DisplayText fixed_text(std::string_view text)
{
    auto parsed = DisplayText::parse(text);
    expect(parsed.has_value(), "test fixture text must parse");
    return std::move(parsed).value();
}

TextBuffer buffer_of(std::string_view text)
{
    auto made = TextBuffer::from_utf8(text);
    expect(made.has_value(), "test fixture text must be valid UTF-8");
    return std::move(made).value();
}

void verify_display_text_accepts_ascii()
{
    expect_accepted("NeNe Nib 0.1.0", 14, "ASCII line is accepted");
    expect_accepted(" ", 1, "a single space is accepted");
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
    expect(first_visible_within(LineNumber{1}, 100, 10) == LineNumber{1}, "the top is allowed");
    expect(first_visible_within(LineNumber{0}, 100, 10) == LineNumber{1}, "line zero clamps up");
    expect(first_visible_within(LineNumber{91}, 100, 10) == LineNumber{91}, "the last page");
    expect(first_visible_within(LineNumber{99}, 100, 10) == LineNumber{91}, "past the last page");
    expect(first_visible_within(LineNumber{5}, 4, 10) == LineNumber{1},
           "a buffer shorter than the window cannot scroll");
    expect(first_visible_within(LineNumber{5}, 100, 0) == LineNumber{5},
           "a window with no room still needs one line");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{7}, 10) == LineNumber{5},
           "a visible caret does not scroll");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{2}, 10) == LineNumber{2},
           "a caret above the window pulls it up");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{20}, 10) == LineNumber{11},
           "a caret below the window pulls it down");
    expect(first_visible_for_caret(LineNumber{5}, LineNumber{5}, 0) == LineNumber{5},
           "a window with no room keeps the caret line");
}

void verify_body_layout()
{
    const auto layout = body_layout(640, 360, 96);
    expect(layout.band == LayoutRect{0, 40, 640, 332}, "the body sits between the two bands");
    expect(layout.gutter == LayoutRect{0, 52, 56, 332}, "the gutter is 56 DIP wide");
    expect(layout.content == LayoutRect{56, 52, 640, 332}, "the content starts after the gutter");
    expect(layout.line_height == 24 && layout.caret_width == 2, "the line height and the caret");
    expect(layout.visible_lines == 11, "280 pixels hold eleven 24 DIP lines");
    expect(body_line_rect(layout, 0) == LayoutRect{0, 52, 640, 76}, "the first row");
    expect(body_line_rect(layout, 2) == LayoutRect{0, 100, 640, 124}, "the third row");
    const auto scaled = body_layout(800, 450, 120);
    expect(scaled.line_height == 30, "the line height scales to 125 percent");
    expect(scaled.visible_lines == 11, "the taller window holds the same eleven lines");
    const auto tiny = body_layout(640, 40, 96);
    expect(tiny.visible_lines == 0, "a window with no body holds no lines");
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

// 採用案の配色表（docs/design/2026-09-15-look.md 第 3 節と編集の採用案 第 2 節）の全トークン。
void verify_dark_palette_tokens()
{
    const auto dark = palette_of(BuiltinTheme::ubuntu_aubergine);
    expect(palette_for(Appearance::dark).background == dark.background,
           "dark maps to the aubergine theme");
    expect(dark.muted == RgbColor{0xB8, 0xA9, 0xB3}, "dark muted");
    expect(dark.gutter == RgbColor{0x7A, 0x66, 0x75}, "dark gutter");
    expect(dark.current_line == RgbColor{0x3E, 0x1A, 0x32}, "dark current line");
    expect(dark.titlebar_tint == RgbaColor{RgbColor{0xFF, 0xFF, 0xFF}, 11},
           "dark title bar tint is white at 4.5 percent");
    expect(dark.tab_active == RgbColor{0x3B, 0x14, 0x30}, "dark active tab");
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
    const auto light = palette_of(BuiltinTheme::neutral_light);
    expect(palette_for(Appearance::light).background == light.background,
           "light maps to the neutral theme");
    expect(light.muted == RgbColor{0x5C, 0x65, 0x70}, "light muted");
    expect(light.gutter == RgbColor{0x9A, 0xA3, 0xAD}, "light gutter");
    expect(light.current_line == RgbColor{0xE6, 0xE8, 0xEC}, "light current line");
    expect(light.titlebar_tint == RgbaColor{RgbColor{0x00, 0x00, 0x00}, 9},
           "light title bar tint is black at 3.5 percent");
    expect(light.tab_active == RgbColor{0xFF, 0xFF, 0xFF}, "light active tab");
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

void verify_editor_state()
{
    const auto state = EditorState::create(Appearance::light, EditMode::ordinary);
    const auto next = state.with_appearance(Appearance::dark);
    expect(state.appearance() == Appearance::light, "with_appearance leaves the source alone");
    expect(next.appearance() == Appearance::dark, "with_appearance returns the next state");
    const auto switched = next.with_mode(EditMode::vim);
    expect(next.mode() == EditMode::ordinary, "with_mode leaves the source alone");
    expect(switched.mode() == EditMode::vim, "with_mode returns the next state");
    expect(switched.appearance() == Appearance::dark, "the appearance survives the mode change");
    expect(state.text().size_bytes() == 0, "a new state starts on an empty buffer");
    expect(state.line_ending() == LineEnding::crlf, "a new buffer writes CRLF");
    expect(state.history().size() == 0, "a new state has no history");
    expect(state.scroll() == ScrollState{LineNumber{1}, 1}, "a new state starts at the first line");
    const auto selected = state.with_selection(Selection{Offset{0}, Offset{0}});
    expect(!has_selection(selected.selection()), "with_selection keeps a collapsed selection");
    const auto scrolled = state.with_scroll(ScrollState{LineNumber{3}, 5});
    expect(scrolled.scroll() == ScrollState{LineNumber{3}, 5},
           "with_scroll returns the next state");
    expect(!(scrolled.scroll() == state.scroll()), "scroll states compare on both parts");
    const auto edited =
        state.with_edit(buffer_of("hi"), collapsed_at(Offset{2}), EditHistory::empty());
    expect(edited.text().text() == "hi" && state.text().size_bytes() == 0,
           "with_edit leaves the source alone");
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

// ---------------------------------------------------------------- EditorController

// ポートと controller をまとめて持つ足場。参照を握る controller より先にポートを宣言する。
class Editing final
{
  public:
    Editing() : controller_(appearance_, clipboard_, files_, code_pages_) {}

    [[nodiscard]] EditorController &controller() noexcept
    {
        return controller_;
    }

    [[nodiscard]] ScriptedClipboard &clipboard() noexcept
    {
        return clipboard_;
    }

    [[nodiscard]] ScriptedAppearance &appearance() noexcept
    {
        return appearance_;
    }

    [[nodiscard]] ScriptedFiles &files() noexcept
    {
        return files_;
    }

    [[nodiscard]] ScriptedCodePages &code_pages() noexcept
    {
        return code_pages_;
    }

  private:
    ScriptedAppearance appearance_{Reading{Appearance::dark}};
    ScriptedClipboard clipboard_;
    ScriptedFiles files_;
    ScriptedCodePages code_pages_;
    EditorController controller_;
};

// 意図を 1 つ流し、見えている行を '|' でつないで返す。表示値だけで振る舞いを測る（ARC-011）。
std::string applied(EditorController &controller, const nenenib::application::EditorIntent &intent)
{
    const auto frame = controller.apply(intent);
    std::string joined;
    for (const auto &line : frame.lines)
    {
        if (!joined.empty())
        {
            joined += "|";
        }
        joined += line.text;
    }
    return joined;
}

void verify_controller_initial_appearance()
{
    ScriptedAppearance light{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    const EditorController from_light(light, board, files, code_pages);
    expect(from_light.frame().palette.background == RgbColor{0xF4, 0xF5, 0xF7},
           "a readable light setting is used");
    ScriptedAppearance dark{Reading{Appearance::dark}};
    const EditorController from_dark(dark, board, files, code_pages);
    expect(from_dark.frame().palette.background == RgbColor{0x30, 0x0A, 0x24},
           "a readable dark setting is used");
}

void verify_controller_read_failures()
{
    const Palette dark = palette_for(Appearance::dark);
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    for (const auto failure :
         {AppearanceReadFailure::unavailable, AppearanceReadFailure::unreadable})
    {
        ScriptedAppearance port{Reading{std::unexpect, failure}};
        const EditorController controller(port, board, files, code_pages);
        expect(controller.frame().palette.background == dark.background,
               "an unreadable setting falls back to dark");
    }
}

void verify_controller_refresh()
{
    ScriptedAppearance port{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    EditorController controller(port, board, files, code_pages);
    port.script(Reading{Appearance::dark});
    const auto frame = controller.apply(RefreshAppearance{});
    expect(frame.palette.background == RgbColor{0x30, 0x0A, 0x24},
           "RefreshAppearance re-reads the port");
    expect(frame.appearance == Appearance::dark, "the frame carries the appearance for DWM");
    expect(controller.frame().palette.background == frame.palette.background,
           "the controller keeps the refreshed state");
}

void verify_controller_typing()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    expect(applied(controller, InsertText{"a"}) == "a", "a keystroke lands in the buffer");
    expect(applied(controller, InsertText{"b"}) == "ab", "the next keystroke follows");
    expect(applied(controller, NewLine{}) == "ab|", "Enter opens a new line");
    expect(applied(controller, InsertText{"c"}) == "ab|c", "typing continues on the new line");
    expect(applied(controller, InsertText{""}) == "ab|c", "an empty insert changes nothing");
    const auto frame = controller.frame();
    expect(frame.total_lines == 2, "the frame carries the total line count");
    expect(frame.caret.position == TextPosition{LineNumber{2}, Column{2}}, "the caret follows");
    expect(frame.status_items.at(0).text() == "行 2, 桁 2", "the status item follows the caret");
    expect(frame.caret.shape == CaretShape::bar, "the ordinary caret is a bar");
    expect(applied(controller, InsertText{"\xE6\x97\xA5"}) == "ab|c\xE6\x97\xA5",
           "a multibyte code point is one insert");
    expect(controller.frame().caret.position.column == Column{3}, "and one column");
}

void verify_controller_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    applied(controller, InsertText{"b"});
    applied(controller, InsertText{"c"});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "one undo removes the whole run of keystrokes");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "abc",
           "redo puts it back");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "abc",
           "redo at the tip changes nothing");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "undo at the start changes nothing");
    applied(controller, InsertText{"x"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"y"});
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "x|",
           "typing after a newline is its own unit");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "x",
           "the newline is a unit of its own");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "undo puts the caret where the edit was");
}

void verify_controller_deletion()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "ab",
           "Backspace removes the code point before the caret");
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "ab",
           "Backspace at the start of the buffer does nothing");
    expect(applied(controller, DeleteText{DeleteDirection::forward}) == "b",
           "Delete removes the code point after the caret");
    applied(controller, MoveCaret{CaretMotion::document_end, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::forward}) == "b",
           "Delete at the end of the buffer does nothing");
    applied(controller, NewLine{});
    applied(controller, InsertText{"c"});
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "bc",
           "Backspace at a line start removes the whole CRLF");
    applied(controller, InsertText{"\xE6\x97\xA5"});
    expect(applied(controller, DeleteText{DeleteDirection::backward}) == "bc",
           "Backspace removes a whole multibyte code point");
}

void verify_controller_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, MoveCaret{CaretMotion::line_start, SelectionAnchoring::collapse});
    applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::extend});
    applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::extend});
    const auto selected = controller.frame().lines.at(0).selection;
    expect(selected.presence == SelectionPresence::present, "Shift+Right selects");
    expect(selected.begin == Column{1} && selected.end == Column{3},
           "two code points are selected");
    expect(applied(controller, InsertText{"Z"}) == "Zcd", "typing over a selection replaces it");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "the replacement collapses the selection");
    applied(controller, SelectAll{});
    expect(applied(controller, DeleteText{DeleteDirection::forward}).empty(),
           "Ctrl+A then Delete empties the buffer");
    applied(controller, InsertText{"ab"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"cd"});
    applied(controller, SelectAll{});
    const auto frame = controller.frame();
    expect(frame.lines.at(0).selection.end == Column{4},
           "a selection that crosses the line end reaches one column past the content");
    expect(frame.lines.at(1).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the second line is selected to its content end");
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "moving without Shift collapses the selection");
}

void verify_controller_place_caret()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"xy"});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{3}}, SelectionAnchoring::collapse});
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{3}},
           "a click puts the caret where it was asked for");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "a plain click collapses the selection");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{2}, Column{2}}, SelectionAnchoring::extend});
    const auto frame = controller.frame();
    expect(frame.caret.position == TextPosition{LineNumber{2}, Column{2}},
           "Shift and a click move the caret");
    expect(frame.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{3}, Column{6}},
           "Shift and a click keep the anchor and select to the line end");
    expect(frame.lines.at(1).selection.begin == Column{1}, "the selection reaches the next line");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{9}, Column{99}}, SelectionAnchoring::collapse});
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{3}},
           "a position past the buffer clamps to the nearest one");
}

void verify_controller_cancel_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abcd"});
    applied(controller, SelectAll{});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::present,
           "the fixture has a selection");
    applied(controller, CancelSelection{});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "Esc drops the selection");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{5}},
           "Esc leaves the caret where it was");
    expect(applied(controller, CancelSelection{}) == "abcd",
           "Esc with nothing selected changes nothing");
    applied(controller, SelectAll{});
    applied(controller, SelectEditMode{EditMode::vim});
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "entering Vim drops the selection and puts the caret on a character");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{4}},
           "and the caret does not stay past the last character");
    applied(controller, CancelSelection{});
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{4}},
           "in Vim mode Esc arrives as a VimKeyPress, so CancelSelection does nothing");
}

void verify_controller_clipboard()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"hello"});
    applied(controller, SelectAll{});
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "hello", "copy writes the selection");
    applied(controller, MoveCaret{CaretMotion::document_end, SelectionAnchoring::collapse});
    expect(applied(controller, ClipboardAction{ClipboardOperation::paste}) == "hellohello",
           "paste inserts at the caret");
    applied(controller, SelectAll{});
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}).empty(),
           "cut removes the selection");
    expect(editing.clipboard().read().value() == "hellohello", "cut wrote the text first");
    expect(applied(controller, ClipboardAction{ClipboardOperation::copy}).empty(),
           "copy with nothing selected changes nothing");
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}).empty(),
           "cut with nothing selected changes nothing");
}

void verify_controller_clipboard_failures()
{
    Editing refusing;
    refusing.clipboard().refuse_writes();
    EditorController &controller = refusing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    applied(controller, SelectAll{});
    expect(applied(controller, ClipboardAction{ClipboardOperation::copy}) == "abc",
           "a refused write leaves the text alone");
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}) == "abc",
           "a refused write does not cut the text away");
    Editing nothing_held;
    applied(nothing_held.controller(), VisibleLines{10});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}).empty(),
           "pasting an empty clipboard changes nothing");
    nothing_held.clipboard().hold(Content{"pasted"});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}) ==
               "pasted",
           "a clipboard filled from outside pastes");
}

void verify_controller_scrolling()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3});
    for (std::size_t line = 0; line < 9; ++line)
    {
        applied(controller, InsertText{"x"});
        applied(controller, NewLine{});
    }
    expect(controller.frame().total_lines == 10, "nine newlines make ten lines");
    expect(controller.frame().first_visible == LineNumber{8},
           "the caret on the last line pulls the window down");
    expect(controller.frame().lines.size() == 3, "three lines are visible");
    applied(controller, ScrollLines{-3});
    expect(controller.frame().first_visible == LineNumber{5}, "the wheel scrolls up three lines");
    applied(controller, ScrollLines{-99});
    expect(controller.frame().first_visible == LineNumber{1}, "scrolling up stops at the top");
    applied(controller, ScrollLines{99});
    expect(controller.frame().first_visible == LineNumber{8}, "scrolling down stops at the bottom");
    applied(controller, VisibleLines{0});
    expect(controller.frame().lines.size() == 1, "a window with no room still shows one line");
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    expect(controller.frame().first_visible == LineNumber{1}, "the caret pulls the window back up");
}

void verify_controller_frame()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto frame = controller.apply(VisibleLines{5});
    expect(frame.lines.size() == 1 && frame.lines.at(0).number == LineNumber{1},
           "an empty buffer shows one line");
    expect(frame.lines.at(0).text.empty(), "the only line is empty");
    expect(frame.lines.at(0).selection.presence == SelectionPresence::absent,
           "nothing is selected");
    expect(frame.document.title.text() == "無題", "the only tab is titled 無題");
    expect(frame.first_visible == LineNumber{1} && frame.total_lines == 1, "the window is at rest");
    expect(frame.status_items.at(2).text() == "CRLF", "a new buffer writes CRLF");
    expect(frame.caret == CaretView{TextPosition{LineNumber{1}, Column{1}}, CaretShape::bar},
           "the caret starts at the origin as a bar");
    const auto vim = controller.apply(SelectEditMode{EditMode::vim});
    expect(vim.caret.shape == CaretShape::block, "the Vim caret is a block");
    expect(!(vim.caret == frame.caret), "caret views compare on the shape too");
}

void verify_controller_mode_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().mode == EditMode::ordinary, "the editor starts in ordinary mode");
    expect(controller.frame().mode_label == "通常", "the initial label is 通常");
    const auto vim = controller.apply(SelectEditMode{EditMode::vim});
    expect(vim.mode == EditMode::vim && vim.mode_label == "NORMAL", "select vim enters vim mode");
    const auto again = controller.apply(SelectEditMode{EditMode::vim});
    expect(again.mode == EditMode::vim, "selecting the mode already in force changes nothing");
    const auto ordinary = controller.apply(SelectEditMode{EditMode::ordinary});
    expect(ordinary.mode == EditMode::ordinary, "select ordinary returns to ordinary");
    expect(ordinary.palette.background == RgbColor{0x30, 0x0A, 0x24},
           "the palette survives the mode change");
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

// ---------------------------------------------------------------- ファイルの縦切り

FilePath sample_path()
{
    auto parsed = FilePath::parse("C:\\work\\note.txt");
    expect(parsed.has_value(), "the sample path parses");
    return std::move(parsed).value();
}

void verify_document_open()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::string("一行目\r\n二行目"));
    const auto frame = controller.apply(OpenDocument{sample_path()});
    expect(editing.files().read_path() == "C:\\work\\note.txt", "the path went through the port");
    expect(editing.files().read_limit() == 64U * 1024U * 1024U,
           "the 64 MiB limit is handed to the port, not measured after the read");
    expect(frame.document.title.text() == "note.txt", "the tab takes the file name");
    expect(frame.document.encoding == TextEncoding::utf8, "plain UTF-8 is detected");
    expect(frame.document.save_state == SaveState::saved, "a freshly opened file is saved");
    expect(!frame.document.last_failure.has_value(), "opening left no failure behind");
    expect(frame.total_lines == 2, "both lines came in");
    expect(frame.lines.at(0).text == "一行目", "the first line is the first line of the file");
    expect(frame.caret.position == TextPosition{LineNumber{1}, Column{1}},
           "the caret starts at the top");
    expect(frame.first_visible == LineNumber{1}, "so does the window");
    expect(frame.status_items.at(1).text() == "UTF-8", "the status bar shows the encoding");
    expect(frame.status_items.at(2).text() == "CRLF", "and the line ending that was read");
}

void verify_document_open_encodings()
{
    Editing with_bom;
    with_bom.files().hold(std::string(byte_order_mark()) + "本文\nつづき");
    const auto bom = with_bom.controller().apply(OpenDocument{sample_path()});
    expect(bom.document.encoding == TextEncoding::utf8_bom, "the BOM is remembered");
    expect(bom.lines.at(0).text == "本文", "the BOM is not part of the body");
    expect(bom.status_items.at(2).text() == "LF", "an LF file keeps LF");
    Editing japanese;
    japanese.files().hold(std::string("\x93\xFA\x96\x7B"));
    japanese.code_pages().decode_to(std::string("日本"));
    const auto read = japanese.controller().apply(OpenDocument{sample_path()});
    expect(read.document.encoding == TextEncoding::shift_jis, "CP932 bytes open as Shift_JIS");
    expect(read.lines.at(0).text == "日本", "the port turned them into UTF-8");
    expect(read.status_items.at(1).text() == "Shift_JIS", "the status bar says so");
    // 開いてもモードは保たれる（決定 8）。
    Editing vim;
    vim.files().hold(std::string("x"));
    static_cast<void>(vim.controller().apply(SelectEditMode{EditMode::vim}));
    const auto kept = vim.controller().apply(OpenDocument{sample_path()});
    expect(kept.mode == EditMode::vim, "opening a file keeps the editing mode");
}

void expect_open_failure(Bytes content, FileFailure expected, const char *description)
{
    Editing editing;
    editing.files().hold(std::move(content));
    const auto frame = editing.controller().apply(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == expected,
           description);
    expect(frame.document.title.text() == "無題", "a failed open does not change the tab");
    expect(frame.total_lines == 1 && frame.lines.at(0).text.empty(),
           "a failed open does not change the body");
}

void verify_document_open_failures()
{
    expect_open_failure(std::unexpected(FileFailure::not_found), FileFailure::not_found,
                        "a missing file is reported as not_found");
    expect_open_failure(std::unexpected(FileFailure::access_denied), FileFailure::access_denied,
                        "a refused file is reported as access_denied");
    expect_open_failure(std::unexpected(FileFailure::unreadable), FileFailure::unreadable,
                        "an unreadable file is reported as unreadable");
    expect_open_failure(std::unexpected(FileFailure::unwritable), FileFailure::unwritable,
                        "the port may also report unwritable");
    expect_open_failure(std::unexpected(FileFailure::too_large), FileFailure::too_large,
                        "a file past the limit is refused by the port before it is read");
    expect_open_failure(std::string("\xFF\xFE\x00\x41", 4), FileFailure::undecodable,
                        "bytes that are neither UTF-8 nor CP932 are undecodable");
    Editing refused;
    refused.files().hold(std::string("\x93\xFA"));
    refused.code_pages().decode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame = refused.controller().apply(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == FileFailure::unencodable,
           "the code page port carries its own reason out");
    Editing broken;
    broken.files().hold(std::string("\x93\xFA"));
    broken.code_pages().decode_to(std::string("\xFF"));
    const auto invalid = broken.controller().apply(OpenDocument{sample_path()});
    expect(invalid.document.last_failure.has_value() &&
               invalid.document.last_failure.value() == FileFailure::undecodable,
           "a port that returns broken UTF-8 is undecodable");
}

void verify_document_save()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, InsertText{"あ"});
    expect(controller.frame().document.save_state == SaveState::modified,
           "typing marks it unsaved");
    expect(controller.frame().document.title.text() == "● 無題", "the mark is on the tab");
    const auto saved = controller.apply(SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "あ", "the body went to the port as UTF-8");
    expect(editing.files().written_path() == "C:\\work\\note.txt", "to the path in the intent");
    expect(saved.document.save_state == SaveState::saved, "saving clears the mark");
    expect(saved.document.title.text() == "note.txt", "and the tab takes the name");
    expect(saved.document.encoding == TextEncoding::utf8, "the encoding is the one that was asked");
    expect(saved.caret.position == TextPosition{LineNumber{1}, Column{2}},
           "saving does not move the caret");
    expect(!saved.document.last_failure.has_value(), "a good save reports nothing");
}

void verify_document_save_encodings()
{
    Editing with_bom;
    applied(with_bom.controller(), InsertText{"あ"});
    static_cast<void>(
        with_bom.controller().apply(SaveDocument{sample_path(), TextEncoding::utf8_bom}));
    expect(with_bom.files().written() == std::string(byte_order_mark()) + "あ",
           "a BOM file keeps its BOM");
    Editing japanese;
    applied(japanese.controller(), InsertText{"日"});
    japanese.code_pages().encode_to(std::string("\x93\xFA"));
    const auto frame =
        japanese.controller().apply(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(japanese.code_pages().encoded_from() == "日", "the body went through the code page");
    expect(japanese.files().written() == "\x93\xFA", "the CP932 bytes are what is written");
    expect(frame.document.save_state == SaveState::saved, "the save took");
}

void verify_document_save_failures()
{
    Editing refused;
    applied(refused.controller(), InsertText{"😀"});
    refused.code_pages().encode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame =
        refused.controller().apply(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == FileFailure::unencodable,
           "a character CP932 cannot hold is unencodable");
    expect(frame.document.save_state == SaveState::modified, "the body is still unsaved");
    expect(!frame.document.path.has_value(), "the document did not adopt the path");
    expect(refused.files().written().empty(), "nothing was written");
    Editing unwritable;
    applied(unwritable.controller(), InsertText{"a"});
    unwritable.files().refuse_writes(FileFailure::unwritable);
    const auto failed =
        unwritable.controller().apply(SaveDocument{sample_path(), TextEncoding::utf8});
    expect(failed.document.last_failure.has_value() &&
               failed.document.last_failure.value() == FileFailure::unwritable,
           "a port that cannot write says so");
    expect(failed.document.save_state == SaveState::modified, "a failed save stays unsaved");
    expect(failed.document.title.text() == "● 無題", "and the tab does not take the name");
    expect(failed.lines.at(0).text == "a", "the body is untouched either way");
}

void verify_save_state_transitions()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().document.save_state == SaveState::saved,
           "an untouched new buffer counts as saved");
    applied(controller, InsertText{"a"});
    expect(controller.frame().document.save_state == SaveState::modified, "one keystroke: unsaved");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::saved,
           "undo back to the start clears the mark");
    applied(controller, InsertText{"a"});
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(controller.frame().document.save_state == SaveState::saved, "saved again");
    // sealed() が無いと、この 1 打鍵が保存時点の単位に混ざって位置が動かない（決定 7）。
    applied(controller, InsertText{"b"});
    expect(controller.frame().document.save_state == SaveState::modified,
           "the keystroke after a save opens its own unit");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::saved,
           "undoing that keystroke returns to the save point");
}

void verify_unreachable_save_point()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, InsertText{"a"});
    applied(controller, NewLine{});
    applied(controller, InsertText{"b"});
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    applied(controller, HistoryAction{HistoryDirection::undo});
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::modified,
           "undoing past the save point is unsaved");
    applied(controller, InsertText{"c"});
    applied(controller, HistoryAction{HistoryDirection::undo});
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::modified,
           "a new edit cut the redo list, so the save point is gone for good");
}

void verify_document_failure_clearing()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::unexpected(FileFailure::not_found));
    const auto failed = controller.apply(OpenDocument{sample_path()});
    expect(failed.document.last_failure.has_value(), "the failure is on the frame that made it");
    const auto next = controller.apply(InsertText{"a"});
    expect(!next.document.last_failure.has_value(), "the next intent clears the failure");
    expect(!controller.frame().document.last_failure.has_value(),
           "and reading the frame again does not bring it back");
}

// ---------------------------------------------------------------- Vim エンジン（ADR 0012）

// fixture の記法（<Esc> <CR> <BS> <C-r>）と鍵の対応。写す場所は eng/vim-oracle.py とここの
// 2 つで、どちらも「fixture の書き方」という 1 つの約束の両端である（ARC-012）。
struct VimKeyName
{
    std::string_view text;
    VimSpecialKey key;
};

constexpr std::array<VimKeyName, 4> vim_key_names{{{"<Esc>", VimSpecialKey::escape},
                                                   {"<CR>", VimSpecialKey::enter},
                                                   {"<BS>", VimSpecialKey::backspace},
                                                   {"<C-r>", VimSpecialKey::control_r}}};
// fixture はどれも数行なので、全部の行が表示値に載る高さで再生する。
constexpr std::size_t vim_visible_lines = 64;

[[nodiscard]] std::optional<VimKeyName> vim_key_name_at(std::string_view keys, std::size_t index)
{
    for (const VimKeyName name : vim_key_names)
    {
        if (keys.substr(index).starts_with(name.text))
        {
            return name;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<VimKey> vim_keys_of(std::string_view keys)
{
    std::vector<VimKey> result;
    std::size_t index = 0;
    while (index < keys.size())
    {
        const auto name = vim_key_name_at(keys, index);
        if (name.has_value())
        {
            result.emplace_back(name.value().key);
            index += name.value().text.size();
            continue;
        }
        result.emplace_back(VimCharacter{code_point_at(keys, Offset{index})});
        index = next_code_point(keys, Offset{index}).value;
    }
    return result;
}

// 表示値だけから本文を組み立てる（ARC-011）。行でつなぐので Vim の getline(1, '$') と同じ形。
[[nodiscard]] std::string vim_body(const EditorFrame &frame)
{
    std::string joined;
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        if (index > 0)
        {
            joined += "\n";
        }
        joined += frame.lines.at(index).text;
    }
    return joined;
}

// キャレットの桁をバイトで測り直す。Vim の col('.') はバイト位置で、表示値の桁は code point。
[[nodiscard]] std::size_t vim_byte_column(const std::string &body, const TextPosition &caret)
{
    const auto text = TextBuffer::from_utf8(body);
    expect(text.has_value(), "the replayed body is valid UTF-8");
    if (!text.has_value())
    {
        return 0;
    }
    return text.value().offset_of(caret).value - text.value().line_start(caret.line).value + 1;
}

void vim_replay(EditorController &controller, std::string_view keys)
{
    for (const VimKey &key : vim_keys_of(keys))
    {
        static_cast<void>(controller.apply(VimKeyPress{key}));
    }
}

// fixture を 1 件再生する。本文・キャレット・無名レジスタの 3 つを本物の Vim と突き合わせる。
void verify_vim_fixture(const VimFixture &fixture)
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(fixture.text)});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, fixture.keys);
    const auto frame = controller.frame();
    const std::string name(fixture.name);
    const std::string body = vim_body(frame);
    expect(frame.first_visible == LineNumber{1}, name.c_str());
    expect(body == fixture.expected_text, (name + ": body").c_str());
    expect(frame.caret.position.line.value == static_cast<std::size_t>(fixture.line),
           (name + ": line").c_str());
    expect(vim_byte_column(body, frame.caret.position) == static_cast<std::size_t>(fixture.column),
           (name + ": column").c_str());
    expect(controller.vim_state().unnamed_register == fixture.register_text,
           (name + ": register").c_str());
}

void verify_vim_fixtures()
{
    expect(nenenib::tests::vim_fixtures.size() >= 30,
           "the oracle wrote at least the 30 fixtures the Issue asks for");
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        verify_vim_fixture(fixture);
    }
}

void verify_vim_key_notation()
{
    const auto keys = vim_keys_of("i<Esc><CR><BS><C-r>あ");
    expect(keys.size() == 6, "every name and every code point becomes one key");
    expect(std::get<VimCharacter>(keys.at(0)) == VimCharacter{U'i'}, "a plain letter");
    expect(std::get<VimSpecialKey>(keys.at(1)) == VimSpecialKey::escape, "<Esc>");
    expect(std::get<VimSpecialKey>(keys.at(2)) == VimSpecialKey::enter, "<CR>");
    expect(std::get<VimSpecialKey>(keys.at(3)) == VimSpecialKey::backspace, "<BS>");
    expect(std::get<VimSpecialKey>(keys.at(4)) == VimSpecialKey::control_r, "<C-r>");
    expect(std::get<VimCharacter>(keys.at(5)) == VimCharacter{U'あ'},
           "a multibyte code point is one key");
}

void verify_vim_mode_labels()
{
    expect(mode_label(EditMode::ordinary, VimMode::normal) == "通常", "the ordinary label");
    expect(mode_label(EditMode::ordinary, VimMode::insert) == "通常",
           "the Vim mode does not show through in ordinary mode");
    expect(mode_label(EditMode::vim, VimMode::normal) == "NORMAL", "the NORMAL label");
    expect(mode_label(EditMode::vim, VimMode::insert) == "INSERT", "the INSERT label");
}

void verify_vim_caret_shapes()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto vim = controller.apply(SelectEditMode{EditMode::vim});
    expect(vim.caret.shape == CaretShape::block, "NORMAL draws a block");
    expect(vim.mode_label == "NORMAL", "NORMAL names itself on the status bar");
    const auto inserting = controller.apply(VimKeyPress{VimKey{VimCharacter{U'i'}}});
    expect(inserting.caret.shape == CaretShape::bar, "INSERT draws a bar");
    expect(inserting.mode_label == "INSERT", "INSERT names itself on the status bar");
    const auto back = controller.apply(VimKeyPress{VimKey{VimSpecialKey::escape}});
    expect(back.caret.shape == CaretShape::block, "Esc brings the block back");
    const auto ordinary = controller.apply(SelectEditMode{EditMode::ordinary});
    expect(ordinary.caret.shape == CaretShape::bar && ordinary.mode_label == "通常",
           "the ordinary mode takes its own caret and label back");
}

// Vim に入るときのキャレットは文字の上へ寄る。通常へ戻ると保留中の回数とオペレータは消える。
void verify_vim_mode_entry()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abc")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(
        controller.apply(MoveCaret{CaretMotion::line_end, SelectionAnchoring::collapse}));
    const auto entered = controller.apply(SelectEditMode{EditMode::vim});
    expect(entered.caret.position.column == Column{3},
           "entering Vim pulls the caret back onto the last character");
    vim_replay(controller, "2d");
    expect(controller.vim_state().count.has_value() && controller.vim_state().pending.has_value(),
           "the count and the operator wait for the motion");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    expect(!controller.vim_state().count.has_value() && !controller.vim_state().pending.has_value(),
           "leaving Vim drops what was pending");
    expect(controller.vim_state().mode == VimMode::normal, "and the mode goes back to NORMAL");
}

// oracle の :normal! は 1 回の実行をまるごと 1 つの undo の単位にするので、i a I A の出入りが
// 単位を閉じることは fixture では測れない。ここだけ手で測る（ADR 0012 の決定 6）。
void verify_vim_undo_boundaries()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "iab<Esc>ic<Esc>");
    expect(vim_body(controller.frame()) == "acbhello", "two inserts land where Vim puts them");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "abhello", "one undo takes back one insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "the second undo takes back the first insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "undo at the end of the history does nothing");
}

// fixture は LF だけ（Vim が CRLF を fileformat=dos として落とすので oracle に流せない）。
// CRLF の本文で CR が本文に残らないことは、保存したバイト列で手で測る（ADR 0012 の決定 9）。
void verify_vim_crlf()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "$");
    expect(controller.frame().caret.position.column == Column{3},
           "$ stops on the last character, not on the CR");
    vim_replay(controller, "x");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on\r\ntwo\r\nthree", "x left the CRLF alone");
    vim_replay(controller, "jdd");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on\r\nthree", "dd took the whole CRLF with the line");
    vim_replay(controller, "jdd");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "on", "dd on the last line took the CRLF before it");
}

// 窓が送れる鍵のうち、fixture の記法に無いもの（Tab・矢印）と、NORMAL では効かない鍵。
void verify_vim_other_keys()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\ntwo")});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::enter}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::backspace}}));
    expect(vim_body(controller.frame()) == "one\ntwo", "Enter and Backspace do nothing in NORMAL");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_down}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_right}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{2}},
           "the arrows move like j and l");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_up}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{1}},
           "the arrows move like k and h");
    vim_replay(controller, "i");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'\t'}}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::control_r}}));
    expect(vim_body(controller.frame()) == "\tone\ntwo", "Tab inserts a tab and Ctrl-r is ignored");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_down}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_right}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{3}},
           "the arrows still move in INSERT");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_up}}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "and up and left too");
}

// core の純関数を直に測る。fixture は EditorController を通るので、端の値はここで押さえる。
void verify_vim_word_motions()
{
    const auto text = TextBuffer::from_utf8("alpha beta\n\ngamma");
    expect(text.has_value(), "the sample buffer parses");
    const auto &buffer = text.value();
    expect(vim_next_word(buffer, Offset{0}, 2, VimWordStop::across_lines) == Offset{11},
           "two w land on the empty line, which is a word of its own");
    expect(vim_next_word(buffer, Offset{6}, 1, VimWordStop::at_line_end) == Offset{10},
           "an operator's w stops at the end of the line it started on");
    expect(vim_next_word(buffer, Offset{12}, 3, VimWordStop::across_lines) == Offset{17},
           "w runs out at the end of the buffer");
    expect(vim_previous_word(buffer, Offset{12}, 2) == Offset{6},
           "two b walk back over the empty line");
    expect(vim_previous_word(buffer, Offset{0}, 1) == Offset{0},
           "b at the start of the buffer stays");
}

void verify_vim_caret_rules()
{
    const auto text = TextBuffer::from_utf8("  alpha\n\n\t ");
    expect(text.has_value(), "the indented buffer parses");
    const auto &buffer = text.value();
    expect(vim_resting_caret(buffer, Offset{3}) == Offset{3}, "a caret on a character stays");
    expect(vim_resting_caret(buffer, Offset{7}) == Offset{6},
           "the position past the last character rests on it");
    expect(vim_resting_caret(buffer, Offset{8}) == Offset{8}, "an empty line rests at its start");
    expect(vim_first_non_blank(buffer, Offset{5}) == Offset{2}, "the indent is skipped");
    expect(vim_first_non_blank(buffer, Offset{8}) == Offset{8}, "an empty line has no non-blank");
    expect(vim_first_non_blank(buffer, Offset{9}) == Offset{10},
           "a line of blanks rests on its last character");
}

void verify_vim_step_edges()
{
    const auto text = TextBuffer::from_utf8("ab");
    expect(text.has_value(), "the two character buffer parses");
    const auto &buffer = text.value();
    VimState inserting = nenenib::core::vim_resting_state(std::string{});
    inserting.mode = VimMode::insert;
    const auto newline = vim_step(inserting, buffer, Offset{1}, VimKey{VimCharacter{U'\n'}});
    expect(std::holds_alternative<VimNewLine>(newline.effect),
           "a newline typed as a character becomes the buffer's own line ending");
    const auto at_start = vim_step(inserting, buffer, Offset{0}, VimKey{VimSpecialKey::backspace});
    expect(std::holds_alternative<VimNoEffect>(at_start.effect),
           "Backspace at the start of the buffer does nothing");
    const VimState resting = nenenib::core::vim_resting_state(std::string{});
    const auto unbound = vim_step(resting, buffer, Offset{0}, VimKey{VimCharacter{U'z'}});
    expect(std::holds_alternative<VimNoEffect>(unbound.effect), "an unbound key does nothing");
    expect(unbound.next.mode == VimMode::normal, "and it leaves NORMAL alone");
}

// Vim モードではクリックと Ctrl+矢印のあとも文字の上へ寄る（Vim も行末より右のクリックは
// 最後の文字に置く）。INSERT は行末の右に居てよいので寄せない。
void verify_vim_caret_placement()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("alpha\nbeta")});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{1}, Column{99}}, SelectionAnchoring::extend}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{5}},
           "a click past the line end lands on the last character");
    expect(controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "Shift does not open a selection in Vim mode");
    static_cast<void>(
        controller.apply(MoveCaret{CaretMotion::document_end, SelectionAnchoring::extend}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{4}},
           "Ctrl+End settles onto the last character too");
    vim_replay(controller, "i");
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{2}, Column{99}}, SelectionAnchoring::collapse}));
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{5}},
           "INSERT may sit past the last character");
}

// ---------------------------------------------------------------- IME（ADR 0014）

// 文節の列を書くのはここだけ。utf8 は「あい」「うえ」のような 3 バイトの列で数える。
[[nodiscard]] Composition composed_of(std::string utf8, std::vector<CompositionClause> clauses,
                                      std::size_t cursor)
{
    return Composition{std::move(utf8), std::move(clauses), Offset{cursor}};
}

[[nodiscard]] CompositionClause clause_of(std::size_t begin, std::size_t end,
                                          ClauseEmphasis emphasis)
{
    return CompositionClause{OffsetRange{Offset{begin}, Offset{end}}, emphasis};
}

// 変換中の表示値を 1 本の文字列にする。「utf8@キャレット|(T|O)開始-終了…」で、
// 変換していないときは "-"。optional は value() で読む（CPP-004）。
[[nodiscard]] std::string composed_summary(const EditorFrame &frame)
{
    if (!frame.composition.has_value())
    {
        return "-";
    }
    const auto &view = frame.composition.value();
    std::string summary = view.utf8 + "@" + std::to_string(view.cursor.value);
    for (const CompositionClause &clause : view.underlines)
    {
        summary += clause.emphasis == ClauseEmphasis::target ? "|T" : "|O";
        summary +=
            std::to_string(clause.range.begin.value) + "-" + std::to_string(clause.range.end.value);
    }
    return summary;
}

// 文節 → 下線の純関数（決定 7）。何が来ても変換中の文字列の全体を隙間なく覆う。
void verify_composition_underlines()
{
    expect(composition_underlines(composed_of("", {}, 0)).empty(),
           "an empty composition draws no underline");
    const auto plain = composition_underlines(composed_of("abcd", {}, 0));
    expect(plain.size() == 1 && plain.at(0) == clause_of(0, 4, ClauseEmphasis::other),
           "an IME that reports no clause still gets one underline over everything");
    const auto two = composition_underlines(composed_of(
        "abcd", {clause_of(0, 2, ClauseEmphasis::other), clause_of(2, 4, ClauseEmphasis::target)},
        2));
    expect(two.size() == 2 && two.at(1) == clause_of(2, 4, ClauseEmphasis::target),
           "clauses that already cover everything are kept as they are");
    const auto gapped =
        composition_underlines(composed_of("abcd", {clause_of(1, 2, ClauseEmphasis::target)}, 0));
    expect(gapped.size() == 3 && gapped.at(0) == clause_of(0, 1, ClauseEmphasis::other) &&
               gapped.at(1) == clause_of(1, 2, ClauseEmphasis::target) &&
               gapped.at(2) == clause_of(2, 4, ClauseEmphasis::other),
           "the gaps before and after a clause are covered as other");
    const auto clamped =
        composition_underlines(composed_of("abcd", {clause_of(0, 9, ClauseEmphasis::target)}, 0));
    expect(clamped.size() == 1 && clamped.at(0) == clause_of(0, 4, ClauseEmphasis::target),
           "a clause past the end of the composition is clamped to it");
    const auto empty_range =
        composition_underlines(composed_of("ab", {clause_of(1, 1, ClauseEmphasis::target)}, 0));
    expect(empty_range.size() == 2 && empty_range.at(0) == clause_of(0, 1, ClauseEmphasis::other) &&
               empty_range.at(1) == clause_of(1, 2, ClauseEmphasis::other),
           "an empty clause draws no underline of its own and the string stays covered");
    const auto reversed =
        composition_underlines(composed_of("ab", {clause_of(2, 1, ClauseEmphasis::target)}, 0));
    expect(reversed.size() == 1 && reversed.at(0) == clause_of(0, 2, ClauseEmphasis::other),
           "a reversed clause is dropped rather than drawn backwards");
    const auto overlap = composition_underlines(composed_of(
        "abcd", {clause_of(0, 3, ClauseEmphasis::target), clause_of(1, 4, ClauseEmphasis::other)},
        0));
    expect(overlap.size() == 2 && overlap.at(0) == clause_of(0, 3, ClauseEmphasis::target) &&
               overlap.at(1) == clause_of(3, 4, ClauseEmphasis::other),
           "overlapping clauses never draw two underlines over the same byte");
}

// 変換中は本文も履歴も動かない（ARC-004 / 決定 2）。EditorState の側で先に測る。
void verify_composition_state()
{
    const auto state = EditorState::create(Appearance::dark, EditMode::ordinary)
                           .with_edit(buffer_of("hi"), collapsed_at(Offset{2}),
                                      EditHistory::empty().pushed(Edit{Offset{0}, "", "hi"},
                                                                  EditBoundary::separate));
    expect(!state.composition().has_value(), "a state starts without a composition");
    const auto composing = state.with_composition(composed_of("あ", {}, 0));
    const auto &held = composing.composition();
    expect(held.has_value() && held.value().utf8 == "あ",
           "with_composition returns the next state");
    expect(!state.composition().has_value(), "with_composition leaves the source alone");
    expect(composing.text().text() == "hi", "the buffer does not change while composing");
    expect(composing.history().size() == state.history().size() &&
               composing.history().position() == state.history().position(),
           "the history does not change while composing");
    expect(composing.selection().caret == state.selection().caret,
           "the caret does not move while composing");
    expect(!composing.with_composition(std::nullopt).composition().has_value(),
           "with_composition also takes the composition away");
}

// 通常モード: 変換中は表示値にだけ載り、確定 1 回が 1 つの undo 単位になる（決定 4）。
void verify_composition_ordinary()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"a"});
    const auto composing = controller.apply(ComposeText{composed_of(
        "にほん", {clause_of(0, 3, ClauseEmphasis::other), clause_of(3, 9, ClauseEmphasis::target)},
        3)});
    expect(composing.lines.at(0).text == "a", "the buffer does not carry the composed text");
    expect(composed_summary(composing) == "にほん@3|O0-3|T3-9",
           "the frame carries the composition, its caret and the folded underlines instead");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "",
           "undo while composing still only sees the typed 'a'");
    applied(controller, HistoryAction{HistoryDirection::redo});
    const auto committed = controller.apply(CommitText{"日本"});
    expect(committed.lines.at(0).text == "a日本", "the commit lands in the buffer");
    expect(!committed.composition.has_value(), "and the composition is gone");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "a",
           "one undo takes back the whole commit");
    expect(applied(controller, HistoryAction{HistoryDirection::redo}) == "a日本",
           "and redo puts it back");
}

// 変換をやめる 3 つの口: CancelComposition・モードの切り替え・ファイルを開く（決定 3）。
void verify_composition_cancelling()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto composing = controller.apply(ComposeText{composed_of("あ", {}, 0)});
    expect(composing.composition.has_value(), "the composition is on the frame");
    expect(!controller.apply(CancelComposition{}).composition.has_value(),
           "CancelComposition takes it away");
    expect(!controller.apply(CancelComposition{}).composition.has_value(),
           "and cancelling again does nothing");
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    expect(!controller.apply(SelectEditMode{EditMode::vim}).composition.has_value(),
           "changing the editing mode drops the composition");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    editing.files().hold(Bytes{std::string("x")});
    expect(!controller.apply(OpenDocument{sample_path()}).composition.has_value(),
           "opening a file drops the composition");
}

// Vim の NORMAL では IME を切ってあるので変換は来ないが、来たら捨てる（決定 4）。
void verify_composition_vim_normal()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    const auto composing = controller.apply(ComposeText{composed_of("に", {}, 0)});
    expect(!composing.composition.has_value(), "NORMAL drops the composition");
    expect(composing.vim_mode == VimMode::normal, "the frame carries the Vim mode for the window");
    const auto committed = controller.apply(CommitText{"日本"});
    expect(committed.lines.at(0).text == "abc", "NORMAL drops the committed text too");
    expect(controller.vim_state().mode == VimMode::normal, "and stays in NORMAL");
}

// Vim の INSERT では確定した文字列が code point ごとの打鍵として engine を通る（決定 4）。
void verify_composition_vim_insert()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'i'}}}));
    const auto composing = controller.apply(ComposeText{composed_of("にほん", {}, 9)});
    expect(composed_summary(composing) == "にほん@9|O0-9",
           "INSERT shows the composition like ordinary mode");
    expect(composing.vim_mode == VimMode::insert, "the frame says INSERT");
    expect(composing.lines.at(0).text.empty(), "the buffer is still empty while composing");
    const auto committed = controller.apply(CommitText{"日本語"});
    expect(committed.lines.at(0).text == "日本語", "the commit goes through the Vim engine");
    expect(!committed.composition.has_value(), "and the composition is gone");
    expect(controller.vim_state().mode == VimMode::insert, "INSERT is still INSERT afterwards");
    expect(committed.caret.position.column == Column{4}, "the caret sits past the three glyphs");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::escape}}));
    vim_replay(controller, "x");
    expect(controller.frame().lines.at(0).text == "日本",
           "Vim sees the committed text as characters it typed itself");
}

void verify_composition()
{
    verify_composition_underlines();
    verify_composition_state();
    verify_composition_ordinary();
    verify_composition_cancelling();
    verify_composition_vim_normal();
    verify_composition_vim_insert();
}

void verify_vim_engine()
{
    verify_vim_word_motions();
    verify_vim_caret_rules();
    verify_vim_step_edges();
    verify_vim_key_notation();
    verify_vim_mode_labels();
    verify_vim_caret_shapes();
    verify_vim_mode_entry();
    verify_vim_caret_placement();
    verify_vim_undo_boundaries();
    verify_vim_crlf();
    verify_vim_other_keys();
    verify_vim_fixtures();
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
    verify_offset_types();
    verify_selection();
    verify_line_endings();
    verify_caret_characters();
    verify_caret_lines();
    verify_caret_words();
    verify_history_coalescing();
    verify_history_travel();
    verify_encoding_labels();
    verify_encoding_detection();
    verify_line_ending_detection();
    verify_file_path();
    verify_tab_titles();
    verify_save_state_of_document();
    verify_scroll_bounds();
    verify_body_layout();
}

void verify_controller_intents()
{
    verify_controller_initial_appearance();
    verify_controller_read_failures();
    verify_controller_refresh();
    verify_controller_typing();
    verify_controller_undo();
    verify_controller_deletion();
    verify_controller_selection();
    verify_controller_place_caret();
    verify_controller_cancel_selection();
    verify_controller_clipboard();
    verify_controller_clipboard_failures();
    verify_controller_scrolling();
    verify_controller_frame();
    verify_controller_mode_selection();
    verify_document_open();
    verify_document_open_encodings();
    verify_document_open_failures();
    verify_document_save();
    verify_document_save_encodings();
    verify_document_save_failures();
    verify_save_state_transitions();
    verify_unreachable_save_point();
    verify_document_failure_clearing();
    verify_vim_engine();
    verify_composition();
}

void verify_look()
{
    verify_color_equality();
    verify_rgba_equality();
    verify_dark_palette_tokens();
    verify_light_palette_tokens();
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

int report()
{
    if (failure_count() != 0)
    {
        std::fprintf(stderr, "Nib unit tests: %zu of %zu checks failed\n", failure_count(),
                     check_count());
        return 1;
    }
    std::printf("Nib unit tests passed: %zu checks over text, caret, history, state and "
                "controller\n",
                check_count());
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    verify_display_text_accepts_ascii();
    verify_palette();
    verify_editor_state();
    if (argc == 2 && std::string_view(argv[1]) == "--coverage-negative")
    {
        return report();
    }
    verify_text_and_caret();
    verify_controller_intents();
    verify_look();
    return report();
}
