// core / application だけを対象にした単体テスト（QLT-013）。OS 資源には触れない。
// --coverage-negative は失敗系を全部省く。その実行が QLT-009 の閾値で落ちることが反例である。
// 時間は測らない（<chrono> は ARC-007 でここに書けない）。1 MB と 1
// 万行は「終わること」だけを見る。
#include "AdjustFontSize.hpp"
#include "Appearance.hpp"
#include "AppearancePort.hpp"
#include "AppearanceReadFailure.hpp"
#include "BodyLayout.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
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
#include "CommandLayout.hpp"
#include "CommandPalette.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "CompositionView.hpp"
#include "DeleteDirection.hpp"
#include "DevicePixels.hpp"
#include "DisplayLine.hpp"
#include "DisplayText.hpp"
#include "Document.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorController.hpp"
#include "EditorIntent.hpp"
#include "EditorSettings.hpp"
#include "EditorState.hpp"
#include "EncodingFailure.hpp"
#include "ExResult.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FilePort.hpp"
#include "HistoryDirection.hpp"
#include "HistoryFailure.hpp"
#include "InputLineView.hpp"
#include "LayoutRect.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Milestone.hpp"
#include "ModeLabel.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenDocument.hpp"
#include "Palette.hpp"
#include "PaletteLayout.hpp"
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
#include "SettingsPort.hpp"
#include "StatusBarHit.hpp"
#include "StatusBarLayout.hpp"
#include "StatusItems.hpp"
#include "SyntaxPalette.hpp"
#include "TabTitle.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextFailure.hpp"
#include "TextPosition.hpp"
#include "Theme.hpp"
#include "ThemeDerivation.hpp"
#include "ThemeDocument.hpp"
#include "ThemePort.hpp"
#include "ThemeSource.hpp"
#include "TitleBarHit.hpp"
#include "TitleBarLayout.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "VimBlockRange.hpp"
#include "VimCaret.hpp"
#include "VimCharacter.hpp"
#include "VimCharacterExtent.hpp"
#include "VimCharacterSearchKind.hpp"
#include "VimColumnWish.hpp"
#include "VimInputWait.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimLineExtent.hpp"
#include "VimMatchRequest.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNewLine.hpp"
#include "VimNoEffect.hpp"
#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHighlight.hpp"
#include "VimSearchHit.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTextObjectScope.hpp"
#include "VimVisualExtent.hpp"
#include "VimVisualRange.hpp"
#include "VimVisualReselect.hpp"
#include "VimWordEndStop.hpp"
#include "VimWordMotion.hpp"
#include "VimWordStop.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <initializer_list>
#include <limits>
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
using nenenib::core::absorbed;
using nenenib::core::Appearance;
using nenenib::core::append_utf8;
using nenenib::core::body_layout;
using nenenib::core::body_line_rect;
using nenenib::core::builtin_themes;
using nenenib::core::BuiltinTheme;
using nenenib::core::byte_order_mark;
using nenenib::core::caret_virtual_column;
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
using nenenib::core::derive_ui;
using nenenib::core::detect_encoding;
using nenenib::core::detect_line_ending;
using nenenib::core::display_line;
using nenenib::core::display_position;
using nenenib::core::display_width;
using nenenib::core::DisplayLine;
using nenenib::core::DisplayText;
using nenenib::core::DisplayWidth;
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
using nenenib::core::is_replaced;
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
using nenenib::core::offset_at_virtual_column;
using nenenib::core::OffsetRange;
using nenenib::core::Palette;
using nenenib::core::palette_for;
using nenenib::core::previous_code_point;
using nenenib::core::RgbaColor;
using nenenib::core::RgbColor;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::selection_range;
using nenenib::core::SelectionAnchoring;
using nenenib::core::SelectionPresence;
using nenenib::core::source_column;
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
using nenenib::core::vim_block_range;
using nenenib::core::vim_block_text;
using nenenib::core::vim_find_match;
using nenenib::core::vim_first_non_blank;
using nenenib::core::vim_next_word;
using nenenib::core::vim_previous_word;
using nenenib::core::vim_resting_caret;
using nenenib::core::vim_step;
using nenenib::core::vim_visual_range;
using nenenib::core::vim_visual_reselect;
using nenenib::core::vim_word_at;
using nenenib::core::vim_word_end;
using nenenib::core::VimBlockExtent;
using nenenib::core::VimBlockWidth;
using nenenib::core::VimCharacter;
using nenenib::core::VimCharacterSearchKind;
using nenenib::core::VimColumnWish;
using nenenib::core::VimEditorView;
using nenenib::core::VimInputWait;
using nenenib::core::VimKey;
using nenenib::core::VimMatchRequest;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNavigate;
using nenenib::core::VimNewLine;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPattern;
using nenenib::core::VimPatternFailure;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchHighlight;
using nenenib::core::VimSearchHit;
using nenenib::core::VimSelect;
using nenenib::core::VimSpecialKey;
using nenenib::core::VimState;
using nenenib::core::VimViewport;
using nenenib::core::VimWordEndStop;
using nenenib::core::VimWordStop;
using nenenib::core::virtual_column;
using nenenib::core::virtual_column_end;
using nenenib::core::VirtualColumn;
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

class ScriptedThemes final : public nenenib::application::ThemePort
{
  public:
    explicit ScriptedThemes(
        nenenib::core::ThemeCatalog catalog = nenenib::core::ThemeCatalog::builtins(),
        std::optional<DisplayText> notice = std::nullopt)
        : catalog_(std::move(catalog)), notice_(std::move(notice))
    {
    }
    [[nodiscard]] nenenib::application::ThemeInventory read() override
    {
        ++reads_;
        return {catalog_, notice_};
    }
    [[nodiscard]] std::size_t reads() const noexcept
    {
        return reads_;
    }

  private:
    nenenib::core::ThemeCatalog catalog_;
    std::optional<DisplayText> notice_;
    std::size_t reads_ = 0;
};

using SettingsReading = std::expected<std::optional<nenenib::core::EditorSettings>,
                                      nenenib::application::SettingsIssue>;

class ScriptedSettings final : public nenenib::application::SettingsPort
{
  public:
    explicit ScriptedSettings(SettingsReading reading = std::nullopt) : reading_(std::move(reading))
    {
    }

    [[nodiscard]] SettingsReading read(const nenenib::core::ThemeCatalog &) override
    {
        return reading_;
    }

    [[nodiscard]] std::expected<void, nenenib::application::SettingsIssue>
    write(const nenenib::core::EditorSettings &settings) override
    {
        ++writes_;
        if (failure_.has_value())
        {
            return std::unexpected(failure_.value());
        }
        written_ = settings;
        return {};
    }

    [[nodiscard]] std::size_t writes() const noexcept
    {
        return writes_;
    }
    [[nodiscard]] const std::optional<nenenib::core::EditorSettings> &written() const noexcept
    {
        return written_;
    }
    void fail(std::optional<nenenib::application::SettingsIssue> failure)
    {
        failure_ = failure;
    }

  private:
    SettingsReading reading_;
    std::size_t writes_ = 0;
    std::optional<nenenib::core::EditorSettings> written_;
    std::optional<nenenib::application::SettingsIssue> failure_;
};

TextBuffer buffer_of(std::string_view text)
{
    auto made = TextBuffer::from_utf8(text);
    expect(made.has_value(), "test fixture text must be valid UTF-8");
    return std::move(made).value();
}

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

void verify_display_line()
{
    verify_display_line_notation();
    verify_display_line_controls();
    verify_display_line_widths();
    verify_display_line_c1();
    verify_display_line_mapping();
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

// absorbed は失敗しうるので、畳めたことを言ってから中身を見る（CPP-004）。
[[nodiscard]] Edit absorbed_edit(const Edit &previous, const Edit &edit)
{
    const auto folded = absorbed(previous, edit);
    expect(folded.has_value(), "the two edits are adjacent");
    return folded.value_or(edit);
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
    explicit Editing(SettingsReading reading = std::nullopt,
                     nenenib::core::ThemeCatalog themes = nenenib::core::ThemeCatalog::builtins())
        : settings_(std::move(reading)), themes_(std::move(themes)),
          controller_(nenenib::application::EditorPorts{appearance_, clipboard_, files_,
                                                        code_pages_, settings_, themes_})
    {
    }

    [[nodiscard]] ScriptedSettings &settings() noexcept
    {
        return settings_;
    }

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
    ScriptedSettings settings_;
    ScriptedThemes themes_;
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

void verify_font_sizes()
{
    using nenenib::core::adjusted_font_size;
    using nenenib::core::FontSize;
    using nenenib::core::FontSizeAdjustment;
    const auto initial = nenenib::core::default_font_size();
    expect(initial.points() == 13.5F, "the default font is 13.5 points");
    expect(nenenib::core::font_size_dips(initial) == 18.0F, "13.5 pt is 18 DIP, not 13.5 DIP");
    for (const float points : {8.0F, 13.25F, 40.0F})
    {
        expect(FontSize::from_points(points).has_value(), "valid point sizes are accepted");
    }
    for (const float points : {7.99F, 40.01F, -1.0F, std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::quiet_NaN()})
    {
        expect(!FontSize::from_points(points).has_value(), "invalid point sizes are rejected");
    }
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase, 1).points() == 14.5F,
           "increase means one point");
    expect(adjusted_font_size(initial, FontSizeAdjustment::decrease, 2).points() == 11.5F,
           "wheel steps use the same point adjustment");
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase, 0).points() == 13.5F,
           "zero wheel steps do nothing");
    expect(adjusted_font_size(initial, FontSizeAdjustment::increase,
                              std::numeric_limits<std::size_t>::max())
                   .points() == 40.0F,
           "a huge count clamps without arithmetic overflow");
    expect(adjusted_font_size(initial, FontSizeAdjustment::decrease, 100).points() == 8.0F,
           "decrease stops at eight points");
    const auto largest = FontSize::from_points(40.0F).value();
    expect(adjusted_font_size(largest, FontSizeAdjustment::reset, 0).points() == 13.5F,
           "reset is independent of the count");
}

void verify_font_geometry()
{
    using nenenib::core::FontSize;
    const auto small = body_layout(640, 360, 96, FontSize::from_points(8).value());
    expect(small.line_height == 17 && small.gutter.right == 33,
           "eight points changes the row and gutter together");
    const auto large = body_layout(640, 360, 96, FontSize::from_points(40).value());
    expect(large.line_height == 85 && large.gutter.right == 166 && large.visible_lines == 3,
           "forty points is still measured in point-derived DIP");
    const auto double_dpi = body_layout(1280, 720, 192, FontSize::from_points(40).value());
    expect(double_dpi.line_height == 170 && double_dpi.gutter.right == 332 &&
               double_dpi.visible_lines == large.visible_lines,
           "DPI is applied once and preserves the visible line count");
    const auto narrow = body_layout(50, 360, 96, FontSize::from_points(40).value());
    expect(narrow.content.left == narrow.content.right, "a large gutter never inverts the content");
}

void verify_settings_loading()
{
    auto saved = nenenib::core::default_editor_settings();
    saved.font_size = nenenib::core::FontSize::from_points(21.25F).value();
    saved.font_family = fixed_text("Consolas");
    saved.theme = nenenib::core::ThemeChoice::from(BuiltinTheme::neutral_light);
    Editing editor{SettingsReading{saved}};
    const auto frame = editor.controller().frame();
    expect(frame.settings.font_size.points() == 21.25F &&
               frame.settings.font_family.text() == "Consolas",
           "saved font settings are restored");
    expect(frame.appearance == Appearance::light, "an explicit theme overrides the system");
    editor.appearance().script(Reading{Appearance::dark});
    const auto refreshed = editor.controller().apply(RefreshAppearance{});
    expect(refreshed.appearance == Appearance::light, "system refresh preserves an explicit theme");
    expect(editor.settings().writes() == 0, "loading and system refresh never rewrite settings");
    const auto held_frame = frame;
    static_cast<void>(editor.controller().apply(InsertText{"abc"}));
    expect(held_frame.settings.font_family.text() == "Consolas",
           "a retained frame owns its font name");
}

void verify_settings_adjustment()
{
    using nenenib::application::AdjustFontSize;
    using nenenib::core::FontSizeAdjustment;
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"abc"}));
    static_cast<void>(controller.apply(SelectAll{}));
    const auto before = controller.frame();
    const auto larger = controller.apply(AdjustFontSize{FontSizeAdjustment::increase, 1});
    expect(larger.settings.font_size.points() == 14.5F && editor.settings().writes() == 1,
           "a changed point size is persisted once");
    const auto &written = editor.settings().written();
    expect(written.has_value() && written.value().font_size.points() == 14.5F,
           "the persisted size equals the displayed size");
    expect(larger.lines.front().text == "abc" &&
               larger.caret.position.column == before.caret.position.column,
           "font adjustment preserves text and caret");
    expect(larger.lines.front().selection.presence == before.lines.front().selection.presence,
           "font adjustment preserves the selection");
    const auto reset = controller.apply(AdjustFontSize{FontSizeAdjustment::reset, 1});
    expect(reset.settings.font_size.points() == 13.5F, "reset restores the default");
    static_cast<void>(controller.apply(AdjustFontSize{FontSizeAdjustment::reset, 1}));
    expect(editor.settings().writes() == 2, "resetting the same size does not write again");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "settings changes do not consume text undo steps");
}

void verify_settings_failures()
{
    using nenenib::application::AdjustFontSize;
    using nenenib::application::SettingsFailure;
    using nenenib::core::FontSizeAdjustment;
    Editing unreadable{SettingsReading{std::unexpect, SettingsFailure::unsupported_version}};
    auto &controller = unreadable.controller();
    expect(controller.frame().settings_failure == SettingsFailure::unsupported_version,
           "a bad settings version is exposed in the startup frame");
    static_cast<void>(controller.apply(VisibleLines{20}));
    expect(controller.frame().settings_failure == SettingsFailure::unsupported_version,
           "layout notifications cannot swallow the startup diagnostic");
    unreadable.settings().fail(SettingsFailure::unsupported_version);
    const auto refused = controller.apply(AdjustFontSize{FontSizeAdjustment::increase, 1});
    expect(refused.settings.font_size.points() == 13.5F && refused.settings_failure.has_value(),
           "a failed write leaves the displayed setting unchanged");
    Editing editor;
    editor.settings().fail(SettingsFailure::unwritable);
    const auto failed = editor.controller().apply(AdjustFontSize{FontSizeAdjustment::decrease, 1});
    expect(failed.settings_failure == SettingsFailure::unwritable,
           "write failure is a typed result");
    editor.settings().fail(std::nullopt);
    const auto retried = editor.controller().apply(AdjustFontSize{FontSizeAdjustment::decrease, 1});
    expect(retried.settings.font_size.points() == 12.5F && !retried.settings_failure.has_value(),
           "a later successful save clears the diagnostic");
}

void verify_controller_initial_appearance()
{
    ScriptedAppearance light{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    const EditorController from_light(
        nenenib::application::EditorPorts{light, board, files, code_pages, settings, themes});
    expect(from_light.frame().palette.background == RgbColor{0xF4, 0xF5, 0xF7},
           "a readable light setting is used");
    ScriptedAppearance dark{Reading{Appearance::dark}};
    const EditorController from_dark(
        nenenib::application::EditorPorts{dark, board, files, code_pages, settings, themes});
    expect(from_dark.frame().palette.background == RgbColor{0x30, 0x0A, 0x24},
           "a readable dark setting is used");
}

void verify_controller_read_failures()
{
    const Palette dark = palette_for(Appearance::dark);
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    for (const auto failure :
         {AppearanceReadFailure::unavailable, AppearanceReadFailure::unreadable})
    {
        ScriptedAppearance port{Reading{std::unexpect, failure}};
        const EditorController controller(
            nenenib::application::EditorPorts{port, board, files, code_pages, settings, themes});
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
    ScriptedSettings settings;
    ScriptedThemes themes;
    EditorController controller(
        nenenib::application::EditorPorts{port, board, files, code_pages, settings, themes});
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

constexpr std::array<VimKeyName, 13> vim_key_names{{{"<Esc>", VimSpecialKey::escape},
                                                    {"<CR>", VimSpecialKey::enter},
                                                    {"<BS>", VimSpecialKey::backspace},
                                                    {"<C-r>", VimSpecialKey::control_r},
                                                    {"<C-d>", VimSpecialKey::control_d},
                                                    {"<C-u>", VimSpecialKey::control_u},
                                                    {"<C-f>", VimSpecialKey::control_f},
                                                    {"<C-b>", VimSpecialKey::control_b},
                                                    {"<C-v>", VimSpecialKey::control_v},
                                                    {"<Home>", VimSpecialKey::home},
                                                    {"<End>", VimSpecialKey::end},
                                                    {"<PageUp>", VimSpecialKey::page_up},
                                                    {"<PageDown>", VimSpecialKey::page_down}}};
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
// oracle の本文は LF で区切られ、行の中の '\r' は文字である（ADR 0036 の決定 4）。TextBuffer へ
// 入れ直すと先頭の行の末尾の '\r' を改行の形の判別が CRLF と読むので、ここは '\n' だけで数える。
[[nodiscard]] std::size_t vim_byte_column(const std::string &body, const TextPosition &caret)
{
    std::size_t start = 0;
    for (std::size_t line = 1; line < caret.line.value; ++line)
    {
        const std::size_t found = body.find('\n', start);
        expect(found != std::string::npos, "the replayed body holds the caret's line");
        if (found == std::string::npos)
        {
            return 0;
        }
        start = found + 1;
    }
    const std::size_t stop = std::min(body.find('\n', start), body.size());
    const std::string_view content = std::string_view(body).substr(start, stop - start);
    std::size_t byte = 0;
    for (std::size_t step = 1; step < caret.column.value && byte < content.size(); ++step)
    {
        byte = next_code_point(content, Offset{byte}).value;
    }
    return byte + 1;
}

// 入力行が開いているあいだの鍵の写し先。窓と同じ約束の 2 つめの端（ARC-012・ADR 0032 の決定 1）。
// fixture の `/foo<CR>` は Vim では 1 つの命令なので、再生もこの 1 本を通る。
void command_key(EditorController &controller, const VimKey &key)
{
    if (const auto *special = std::get_if<VimSpecialKey>(&key))
    {
        if (*special == VimSpecialKey::enter)
        {
            static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
            return;
        }
        if (*special == VimSpecialKey::escape)
        {
            static_cast<void>(controller.apply(nenenib::application::CancelCommand{}));
            return;
        }
        if (*special == VimSpecialKey::backspace)
        {
            static_cast<void>(controller.apply(
                nenenib::application::EditCommand{nenenib::core::CommandEdit::backspace}));
        }
        return;
    }
    if (const auto *character = std::get_if<VimCharacter>(&key))
    {
        std::string utf8;
        nenenib::core::append_utf8(utf8, character->code);
        static_cast<void>(controller.apply(nenenib::application::CommandText{utf8}));
    }
}

void vim_replay(EditorController &controller, std::string_view keys)
{
    for (const VimKey &key : vim_keys_of(keys))
    {
        if (controller.command_line_active())
        {
            command_key(controller, key);
            continue;
        }
        static_cast<void>(controller.apply(VimKeyPress{key}));
    }
}

[[nodiscard]] VimState empty_vim_state()
{
    return nenenib::core::vim_resting_state(
        VimRegister{std::string{}, VimRegisterKind::uninitialized});
}

[[nodiscard]] bool last_search_is(const VimState &state, VimCharacterSearchKind kind,
                                  char32_t target) noexcept
{
    return state.last_character_search.has_value() &&
           state.last_character_search.value().kind == kind &&
           state.last_character_search.value().target == target;
}

[[nodiscard]] bool waits_for_character(const VimState &state, VimCharacterSearchKind kind) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<VimCharacterSearchKind>(state.input_wait.value()) &&
           std::get<VimCharacterSearchKind>(state.input_wait.value()) == kind;
}

[[nodiscard]] bool waits_for_prefix(const VimState &state, VimPrefix prefix) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<VimPrefix>(state.input_wait.value()) &&
           std::get<VimPrefix>(state.input_wait.value()) == prefix;
}

[[nodiscard]] TextPosition vim_fixture_position(const VimFixture &fixture)
{
    const auto text = TextBuffer::from_utf8(fixture.text);
    expect(text.has_value(), "the fixture input is valid UTF-8");
    if (!text.has_value() || !fixture.viewport.has_value())
    {
        return TextPosition{LineNumber{1}, Column{1}};
    }
    const auto &viewport = fixture.viewport.value();
    const LineNumber line{static_cast<std::size_t>(viewport.line)};
    const Offset at{text.value().line_start(line).value +
                    static_cast<std::size_t>(viewport.column - 1)};
    return text.value().position_of(at);
}

void arrange_vim_viewport(EditorController &controller, const VimFixture &fixture)
{
    if (!fixture.viewport.has_value())
    {
        return;
    }
    const auto &viewport = fixture.viewport.value();
    static_cast<void>(controller.apply(VisibleLines{viewport.visible_lines}));
    static_cast<void>(
        controller.apply(PlaceCaret{vim_fixture_position(fixture), SelectionAnchoring::collapse}));
    const std::int64_t current = static_cast<std::int64_t>(controller.frame().first_visible.value);
    const std::int64_t requested = static_cast<std::int64_t>(viewport.first_visible);
    static_cast<void>(
        controller.apply(ScrollLines{static_cast<std::int32_t>(requested - current)}));
}

[[nodiscard]] std::string whole_vim_body(EditorController &controller)
{
    static_cast<void>(controller.apply(VisibleLines{controller.frame().total_lines}));
    const std::int64_t first = static_cast<std::int64_t>(controller.frame().first_visible.value);
    static_cast<void>(controller.apply(ScrollLines{static_cast<std::int32_t>(1 - first)}));
    return vim_body(controller.frame());
}

// 無名レジスタの種類を Vim の getregtype の言葉で言う。一度も使っていないレジスタは空（ADR 0015）。
// 矩形は Ctrl-V（0x16）に幅の 10 進が続く（ADR 0035 の決定 10・Vim 9.1 で実測）。
[[nodiscard]] std::string vim_register_kind(const VimRegister &value)
{
    switch (value.kind)
    {
    case VimRegisterKind::uninitialized:
        return "";
    case VimRegisterKind::characters:
        return "v";
    case VimRegisterKind::lines:
        return "V";
    case VimRegisterKind::block:
        // 8 進のエスケープは 3 桁で止まるので、続く幅の数字と混ざらない（"\x16" は混ざる）。
        return "\026" + std::to_string(value.width.has_value() ? value.width.value().columns : 0);
    }
    std::unreachable();
}

// fixture を 1 件再生する。本文・キャレット・無名レジスタ（本文と種類）を本物の Vim
// と突き合わせる。
void verify_vim_fixture(const VimFixture &fixture)
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(fixture.text)});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    arrange_vim_viewport(controller, fixture);
    vim_replay(controller, fixture.keys);
    const auto frame = controller.frame();
    const std::string name(fixture.name);
    const std::size_t expected_first =
        fixture.viewport.has_value() ? fixture.viewport.value().expected_first_visible : 1;
    expect(frame.first_visible == LineNumber{expected_first}, (name + ": viewport").c_str());
    expect(frame.caret.position.line.value == static_cast<std::size_t>(fixture.line),
           (name + ": line").c_str());
    expect(vim_byte_column(std::string(fixture.expected_text), frame.caret.position) ==
               static_cast<std::size_t>(fixture.column),
           (name + ": column").c_str());
    expect(controller.vim_state().unnamed_register.text == fixture.register_text,
           (name + ": register").c_str());
    expect(vim_register_kind(controller.vim_state().unnamed_register) == fixture.register_kind,
           (name + ": register kind").c_str());
    if (fixture.viewport.has_value())
    {
        const std::size_t fallback =
            std::max<std::size_t>(fixture.viewport.value().visible_lines / 2, 1);
        const std::size_t scroll_lines =
            controller.vim_state().scroll_lines.value_or(nenenib::core::VimCount{fallback}).value;
        expect(scroll_lines == fixture.viewport.value().expected_scroll_lines,
               (name + ": scroll lines").c_str());
    }
    const std::string body = whole_vim_body(controller);
    expect(body == fixture.expected_text, (name + ": body").c_str());
}

void verify_vim_fixtures()
{
    expect(nenenib::tests::vim_fixtures.size() >= 250,
           "the oracle wrote at least the fixtures the Issues ask for (#22 / #43 / #53)");
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        verify_vim_fixture(fixture);
    }
}

void verify_vim_character_search_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("char-search-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 60, "the scoped character-search suite replays its 60 oracle fixtures");
}

void verify_vim_line_jump_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("line-jump-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 41, "the scoped line-jump suite replays its 41 oracle fixtures");
}

void verify_vim_character_search_waiting()
{
    const auto text = TextBuffer::from_utf8("a1ffx");
    expect(text.has_value(), "the character-search sample parses");
    const auto &buffer = text.value();
    const Selection at_start = collapsed_at(Offset{0});
    const VimEditorView view{buffer, at_start, VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto counted = vim_step(state, view, VimKey{VimCharacter{U'2'}});
    const auto waiting = vim_step(counted.next, view, VimKey{VimCharacter{U'f'}});
    expect(waits_for_character(waiting.next, VimCharacterSearchKind::find_forward) &&
               waiting.next.count == nenenib::core::VimCount{2},
           "f waits for a target without consuming its count");
    const auto found = vim_step(waiting.next, view, VimKey{VimCharacter{U'f'}});
    expect(std::get<VimMoveTo>(found.effect).caret == Offset{3},
           "the awaited f is a target character, not another command");
    expect(last_search_is(found.next, VimCharacterSearchKind::find_forward, U'f') &&
               !found.next.count.has_value() && !found.next.input_wait.has_value(),
           "a successful search records its target and clears transient state");
    expect(found.next.wanted_column ==
               nenenib::core::VimWantedColumn{VimColumnWish::at_column, VirtualColumn{4}},
           "a successful search replaces the line-end column wish with its destination");
    const auto digit_waiting = vim_step(state, view, VimKey{VimCharacter{U'f'}});
    const auto digit = vim_step(digit_waiting.next, view, VimKey{VimCharacter{U'1'}});
    expect(std::get<VimMoveTo>(digit.effect).caret == Offset{1},
           "a digit in target position is consumed as the searched character");
}

void verify_vim_character_search_history()
{
    const auto text = TextBuffer::from_utf8("axbxc");
    expect(text.has_value(), "the repeated-search sample parses");
    const auto &buffer = text.value();
    const VimState state = empty_vim_state();
    const auto first_waiting = vim_step(
        state, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'f'}});
    const auto first =
        vim_step(first_waiting.next,
                 VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
                 VimKey{VimCharacter{U'x'}});
    const auto repeated = vim_step(
        first.next, VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U';'}});
    expect(std::get<VimMoveTo>(repeated.effect).caret == Offset{3} &&
               last_search_is(repeated.next, VimCharacterSearchKind::find_forward, U'x'),
           "; repeats forward without changing the stored search");
    const auto opposite =
        vim_step(repeated.next,
                 VimEditorView{buffer, collapsed_at(Offset{3}), VimViewport{LineNumber{1}, 64}},
                 VimKey{VimCharacter{U','}});
    expect(std::get<VimMoveTo>(opposite.effect).caret == Offset{1} &&
               last_search_is(opposite.next, VimCharacterSearchKind::find_forward, U'x'),
           ", reverses this invocation while preserving the stored direction");
    const auto absent = vim_step(
        state, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U';'}});
    expect(std::holds_alternative<VimNoEffect>(absent.effect),
           "repeat without a stored character search does nothing");
}

void verify_vim_character_search_failure()
{
    const auto text = TextBuffer::from_utf8("abc");
    expect(text.has_value(), "the failed-search sample parses");
    const auto &buffer = text.value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::till_backward, U'x'};
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto counted = vim_step(state, view, VimKey{VimCharacter{U'3'}});
    const auto pending = vim_step(counted.next, view, VimKey{VimCharacter{U'd'}});
    expect(pending.next.wanted_column == state.wanted_column,
           "starting an operator preserves the line-end column wish");
    const auto waiting = vim_step(pending.next, view, VimKey{VimCharacter{U'f'}});
    const auto failed = vim_step(waiting.next, view, VimKey{VimCharacter{U'z'}});
    expect(std::holds_alternative<VimNoEffect>(failed.effect) &&
               last_search_is(failed.next, VimCharacterSearchKind::find_forward, U'z'),
           "a failed new search still replaces the search history");
    expect(!failed.next.count.has_value() && !failed.next.pending.has_value() &&
               !failed.next.input_wait.has_value() &&
               failed.next.wanted_column == state.wanted_column,
           "a failed operator search clears transients and preserves the wanted column");
}

void verify_vim_character_search_cancellation()
{
    const auto text = TextBuffer::from_utf8("axb");
    expect(text.has_value(), "the cancellation sample parses");
    const auto &buffer = text.value();
    const Selection selection{Offset{0}, Offset{1}};
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, 64}};
    VimState visual = empty_vim_state();
    visual.mode = VimMode::visual;
    visual.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'x'};
    visual.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto waiting = vim_step(visual, view, VimKey{VimCharacter{U'f'}});
    constexpr std::array cancellations{VimSpecialKey::escape,     VimSpecialKey::backspace,
                                       VimSpecialKey::arrow_left, VimSpecialKey::arrow_right,
                                       VimSpecialKey::arrow_up,   VimSpecialKey::arrow_down,
                                       VimSpecialKey::home,       VimSpecialKey::end,
                                       VimSpecialKey::page_up,    VimSpecialKey::page_down};
    for (const VimSpecialKey key : cancellations)
    {
        const auto cancelled = vim_step(waiting.next, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(cancelled.effect) &&
                   cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value(),
               "a navigation special key cancels only the VISUAL search wait");
        expect(last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'x') &&
                   cancelled.next.wanted_column == visual.wanted_column,
               "search cancellation preserves history and the wanted column");
    }
    const auto till_waiting = vim_step(visual, view, VimKey{VimCharacter{U't'}});
    const auto selected = vim_step(till_waiting.next, view, VimKey{VimCharacter{U'b'}});
    expect(std::get<VimSelect>(selected.effect).selection == Selection{Offset{0}, Offset{1}} &&
               selected.next.mode == VimMode::visual,
           "a successful no-move VISUAL search is distinct from cancellation");
}

void verify_vim_character_search_control_targets()
{
    const auto text = TextBuffer::from_utf8("abc\r\ndef");
    expect(text.has_value(), "the special-target CRLF sample parses");
    const auto &buffer = text.value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}};
    constexpr std::array targets{std::pair{VimSpecialKey::enter, char32_t{0x0D}},
                                 std::pair{VimSpecialKey::control_r, char32_t{0x12}},
                                 std::pair{VimSpecialKey::control_d, char32_t{0x04}},
                                 std::pair{VimSpecialKey::control_u, char32_t{0x15}},
                                 std::pair{VimSpecialKey::control_f, char32_t{0x06}},
                                 std::pair{VimSpecialKey::control_b, char32_t{0x02}}};
    for (const auto [key, target] : targets)
    {
        VimState waiting = empty_vim_state();
        waiting.input_wait = VimInputWait{VimCharacterSearchKind::till_forward};
        const auto searched = vim_step(waiting, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(searched.effect) &&
                   last_search_is(searched.next, VimCharacterSearchKind::till_forward, target),
               "a control special key is recorded as its control-code search target");
    }
}

void verify_vim_character_search_large_count()
{
    const auto text = TextBuffer::from_utf8("ax");
    expect(text.has_value(), "the large-count character-search sample parses");
    const auto &buffer = text.value();
    VimState waiting = empty_vim_state();
    waiting.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    waiting.input_wait = VimInputWait{VimCharacterSearchKind::find_forward};
    const auto searched = vim_step(
        waiting, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'x'}});
    expect(std::holds_alternative<VimNoEffect>(searched.effect) &&
               last_search_is(searched.next, VimCharacterSearchKind::find_forward, U'x') &&
               !searched.next.count.has_value(),
           "a huge count scans the bounded line once and reports a miss");
}

void verify_vim_character_search_mode_lifetime()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("a:b;c")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "f:");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}} &&
               !controller.frame().command_line.has_value(),
           "an awaited colon is searched without opening the command line");
    expect(last_search_is(controller.vim_state(), VimCharacterSearchKind::find_forward, U':'),
           "the command character is stored as the ordinary search target");
    vim_replay(controller, "f");
    expect(controller.vim_state().input_wait.has_value(),
           "the engine exposes the pending target state before a mode toggle");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    expect(!controller.vim_state().input_wait.has_value() &&
               last_search_is(controller.vim_state(), VimCharacterSearchKind::find_forward, U':'),
           "mode toggles cancel a wait and preserve completed search history");
}

void verify_vim_character_search_crlf_and_undo()
{
    Editing crlf;
    EditorController &crlf_controller = crlf.controller();
    crlf.files().hold(Bytes{std::string("ax\r\nx")});
    static_cast<void>(crlf_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(crlf_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(crlf_controller, "fx;");
    expect(crlf_controller.frame().caret.position == TextPosition{LineNumber{1}, Column{2}},
           "a repeated character search does not cross a CRLF line boundary");

    Editing undo;
    EditorController &undo_controller = undo.controller();
    undo.files().hold(Bytes{std::string("abxc")});
    static_cast<void>(undo_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(undo_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(undo_controller, "dfx");
    expect(vim_body(undo_controller.frame()) == "c",
           "a character-search operator edits through the canonical range effect");
    vim_replay(undo_controller, "u");
    expect(vim_body(undo_controller.frame()) == "abxc",
           "undo restores a character-search operator as one edit");
}

void verify_vim_register_initial_and_empty_yank()
{
    Editing empty;
    EditorController &empty_controller = empty.controller();
    empty.files().hold(Bytes{std::string("abc")});
    static_cast<void>(empty_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(empty_controller.apply(SelectEditMode{EditMode::vim}));
    expect(empty_controller.vim_state().unnamed_register.kind == VimRegisterKind::uninitialized,
           "a new editor starts with an uninitialized unnamed register");
    vim_replay(empty_controller, "pP");
    expect(vim_body(empty_controller.frame()) == "abc" &&
               empty_controller.vim_state().unnamed_register.kind == VimRegisterKind::uninitialized,
           "p and P keep an uninitialized empty register as a no-op");

    Editing yank;
    EditorController &yank_controller = yank.controller();
    yank.files().hold(Bytes{std::string("ax")});
    static_cast<void>(yank_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(yank_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(yank_controller, "yyy0");
    expect(yank_controller.vim_state().unnamed_register.text.empty() &&
               yank_controller.vim_state().unnamed_register.kind == VimRegisterKind::characters,
           "a successful empty characterwise yank clears a seeded register as characterwise");
}

void verify_vim_register_empty_remove_and_change()
{
    for (const std::string_view keys :
         {std::string_view{"yy$dTa"}, std::string_view{"yy$cTa<Esc>"}})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("ax")});
        static_cast<void>(controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(controller, keys);
        expect(vim_body(controller.frame()) == "ax" &&
                   controller.vim_state().unnamed_register.text == "ax\n" &&
                   controller.vim_state().unnamed_register.kind == VimRegisterKind::lines,
               "an empty backward-till delete or change preserves a seeded line register");
    }
}

void verify_vim_register_regular_operations()
{
    Editing deleting;
    EditorController &delete_controller = deleting.controller();
    deleting.files().hold(Bytes{std::string("abc")});
    static_cast<void>(delete_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(delete_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(delete_controller, "dl");
    expect(vim_body(delete_controller.frame()) == "bc" &&
               vim_register_kind(delete_controller.vim_state().unnamed_register) == "v",
           "a regular delete still writes a characterwise register");

    Editing changing;
    EditorController &change_controller = changing.controller();
    changing.files().hold(Bytes{std::string("abc")});
    static_cast<void>(change_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(change_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(change_controller, "clx<Esc>");
    expect(vim_body(change_controller.frame()) == "xbc" &&
               vim_register_kind(change_controller.vim_state().unnamed_register) == "v",
           "a regular change still writes a characterwise register");

    for (const std::string_view keys : {std::string_view{"ylp"}, std::string_view{"ylP"}})
    {
        Editing putting;
        EditorController &put_controller = putting.controller();
        putting.files().hold(Bytes{std::string("abc")});
        static_cast<void>(put_controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(put_controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(put_controller, keys);
        expect(vim_body(put_controller.frame()) == "aabc" &&
                   vim_register_kind(put_controller.vim_state().unnamed_register) == "v",
               "regular yank and characterwise p or P still share the initialized register");
    }
}

void verify_vim_character_search_contracts()
{
    verify_vim_character_search_waiting();
    verify_vim_character_search_history();
    verify_vim_character_search_failure();
    verify_vim_character_search_cancellation();
    verify_vim_character_search_control_targets();
    verify_vim_character_search_large_count();
    verify_vim_character_search_mode_lifetime();
    verify_vim_character_search_crlf_and_undo();
    verify_vim_register_initial_and_empty_yank();
    verify_vim_register_empty_remove_and_change();
    verify_vim_register_regular_operations();
}

void verify_vim_line_jump_waiting()
{
    const auto text = TextBuffer::from_utf8("one\ntwo\nthree");
    expect(text.has_value(), "the prefix-wait sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{2}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, 64}};
    VimState state = empty_vim_state();
    state.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'x'};
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};

    const auto operation_count = vim_step(state, view, VimKey{VimCharacter{U'2'}});
    const auto pending = vim_step(operation_count.next, view, VimKey{VimCharacter{U'd'}});
    const auto motion_count = vim_step(pending.next, view, VimKey{VimCharacter{U'3'}});
    const auto waiting = vim_step(motion_count.next, view, VimKey{VimCharacter{U'g'}});
    expect(waits_for_prefix(waiting.next, VimPrefix::g) && waiting.next.count.has_value() &&
               waiting.next.count.value().value == 3 && waiting.next.pending.has_value() &&
               waiting.next.pending.value().count.has_value() &&
               waiting.next.pending.value().count.value().value == 2,
           "g waits without collapsing the operator and motion counts");
    const auto invalid = vim_step(waiting.next, view, VimKey{VimCharacter{U'3'}});
    expect(std::holds_alternative<VimNoEffect>(invalid.effect) &&
               !invalid.next.input_wait.has_value() && !invalid.next.count.has_value() &&
               !invalid.next.pending.has_value(),
           "an unsupported g suffix is consumed and clears transient input state");
    expect(invalid.next.mode == state.mode && invalid.next.wanted_column == state.wanted_column &&
               last_search_is(invalid.next, VimCharacterSearchKind::find_forward, U'x'),
           "an unsupported g suffix preserves mode, wanted column, and search memory");

    VimState visual = state;
    visual.mode = VimMode::visual;
    const auto visual_waiting = vim_step(visual, view, VimKey{VimCharacter{U'g'}});
    for (const VimSpecialKey key : {VimSpecialKey::escape, VimSpecialKey::page_down})
    {
        const auto cancelled = vim_step(visual_waiting.next, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(cancelled.effect) &&
                   cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value(),
               "a special key cancels only the VISUAL g wait");
        expect(cancelled.next.wanted_column == visual.wanted_column &&
                   last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'x'),
               "g cancellation preserves the wanted column and search memory");
    }

    const auto search_waiting = vim_step(state, view, VimKey{VimCharacter{U'f'}});
    const auto searched = vim_step(search_waiting.next, view, VimKey{VimCharacter{U'g'}});
    expect(last_search_is(searched.next, VimCharacterSearchKind::find_forward, U'g'),
           "g is a literal target while a character search is waiting");
}

void verify_vim_line_jump_text_edges()
{
    Editing empty;
    EditorController &empty_controller = empty.controller();
    static_cast<void>(empty_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(empty_controller, "Ggg");
    expect(empty_controller.frame().caret.position == TextPosition{LineNumber{1}, Column{1}},
           "G and gg stay at the only position in an empty document");

    Editing trailing;
    EditorController &trailing_controller = trailing.controller();
    trailing.files().hold(Bytes{std::string("one\r\n")});
    static_cast<void>(trailing_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(trailing_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(trailing_controller, "G");
    expect(trailing_controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "G reaches the trailing empty CRLF line");
    static_cast<void>(trailing_controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(trailing.files().written() == "one\r\n",
           "a line jump preserves the trailing CRLF bytes");
}

void verify_vim_line_jump_continuations()
{
    for (const std::string_view keys : {"g3G", "gZG", "gGG"})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("one\ntwo\n  three")});
        static_cast<void>(controller.apply(OpenDocument{sample_path()}));
        static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
        vim_replay(controller, keys);
        expect(controller.frame().caret.position == TextPosition{LineNumber{3}, Column{3}},
               "a separately delivered G works after an unsupported g suffix");
    }

    Editing visual;
    EditorController &visual_controller = visual.controller();
    visual.files().hold(Bytes{std::string("abcd\nx\nabcdef")});
    static_cast<void>(visual_controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(visual_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(visual_controller, "v$g<Esc>jj");
    const TextPosition visual_position = visual_controller.frame().caret.position;
    expect(visual_controller.vim_state().mode == VimMode::visual &&
               visual_position == TextPosition{LineNumber{3}, Column{7}},
           "a cancelled VISUAL g wait keeps the line-end wish for later vertical moves");

    Editing toggled;
    EditorController &toggle_controller = toggled.controller();
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(toggle_controller, "g");
    expect(waits_for_prefix(toggle_controller.vim_state(), VimPrefix::g),
           "the controller exposes a pending g prefix");
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(toggle_controller.apply(SelectEditMode{EditMode::vim}));
    expect(!toggle_controller.vim_state().input_wait.has_value(),
           "a mode toggle cancels a pending g prefix");
}

void verify_vim_line_jump_saturated_count()
{
    const auto text = TextBuffer::from_utf8("one\ntwo\nthree");
    expect(text.has_value(), "the saturated line-count sample parses");
    const auto &buffer = text.value();
    VimState state = empty_vim_state();
    state.pending = nenenib::core::VimPendingOperator{
        nenenib::core::VimOperator::yank,
        nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()}};
    state.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    const VimEditorView view{buffer, collapsed_at(buffer.line_start(LineNumber{2})),
                             VimViewport{LineNumber{1}, 64}};
    const auto waiting = vim_step(state, view, VimKey{VimCharacter{U'g'}});
    const auto jumped = vim_step(waiting.next, view, VimKey{VimCharacter{U'g'}});
    expect(std::get<VimMoveTo>(jumped.effect).caret == buffer.line_start(LineNumber{2}) &&
               jumped.next.unnamed_register.text == "two\nthree\n" &&
               jumped.next.unnamed_register.kind == VimRegisterKind::lines,
           "a saturated operator-motion count clamps to the final document line");
}

void verify_vim_line_jump_operator_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "2Gdgg");
    expect(vim_body(controller.frame()) == "three",
           "dgg edits through the shared linewise operator path");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "one\ntwo\nthree",
           "one undo restores the complete line-jump edit");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "one\r\ntwo\r\nthree",
           "undo restores the original CRLF bytes");
}

void verify_vim_line_jump_contracts()
{
    verify_vim_line_jump_waiting();
    verify_vim_line_jump_text_edges();
    verify_vim_line_jump_continuations();
    verify_vim_line_jump_saturated_count();
    verify_vim_line_jump_operator_undo();
}

void verify_vim_key_notation()
{
    const auto keys = vim_keys_of("i<Esc><CR><BS><C-r><Home><End>あ");
    expect(keys.size() == 8, "every name and every code point becomes one key");
    expect(std::get<VimCharacter>(keys.at(0)) == VimCharacter{U'i'}, "a plain letter");
    expect(std::get<VimSpecialKey>(keys.at(1)) == VimSpecialKey::escape, "<Esc>");
    expect(std::get<VimSpecialKey>(keys.at(2)) == VimSpecialKey::enter, "<CR>");
    expect(std::get<VimSpecialKey>(keys.at(3)) == VimSpecialKey::backspace, "<BS>");
    expect(std::get<VimSpecialKey>(keys.at(4)) == VimSpecialKey::control_r, "<C-r>");
    expect(std::get<VimSpecialKey>(keys.at(5)) == VimSpecialKey::home, "<Home>");
    expect(std::get<VimSpecialKey>(keys.at(6)) == VimSpecialKey::end, "<End>");
    expect(std::get<VimCharacter>(keys.at(7)) == VimCharacter{U'あ'},
           "a multibyte code point is one key");
}

void verify_vim_mode_labels()
{
    expect(mode_label(EditMode::ordinary, VimMode::normal) == "通常", "the ordinary label");
    expect(mode_label(EditMode::ordinary, VimMode::insert) == "通常",
           "the Vim mode does not show through in ordinary mode");
    expect(mode_label(EditMode::vim, VimMode::normal) == "NORMAL", "the NORMAL label");
    expect(mode_label(EditMode::vim, VimMode::insert) == "INSERT", "the INSERT label");
    expect(mode_label(EditMode::vim, VimMode::visual) == "VISUAL", "the VISUAL label");
    expect(mode_label(EditMode::vim, VimMode::visual_line) == "VISUAL LINE",
           "the VISUAL LINE label");
    expect(mode_label(EditMode::ordinary, VimMode::visual) == "通常",
           "VISUAL does not show through in ordinary mode either");
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

// VISUAL では描く選択と Ctrl+C / Ctrl+X の範囲が vim_visual_range と同じ 1 本
// （ADR 0018 の決定 5 の強制）。fixture は本文とレジスタしか見ないので、ここで面を測る。
void verify_vim_visual_selection_and_clipboard()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abc\ndef")});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "vl");
    const auto characters = controller.frame();
    expect(characters.mode_label == "VISUAL" && characters.caret.shape == CaretShape::block,
           "VISUAL names itself on the status bar and keeps the block caret");
    expect(characters.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the drawn selection takes the character under the far end");
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "ab", "Ctrl+C copies that same range");
    vim_replay(controller, "V");
    const auto lines = controller.frame();
    expect(lines.mode_label == "VISUAL LINE", "V switches the kind without losing the selection");
    expect(lines.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{4}},
           "the linewise selection covers the whole line");
    applied(controller, ClipboardAction{ClipboardOperation::copy});
    expect(editing.clipboard().read().value() == "abc", "and Ctrl+C takes the whole line");
    vim_replay(controller, "<Esc>");
    const auto back = controller.frame();
    expect(back.mode_label == "NORMAL", "Esc goes back to NORMAL");
    expect(back.lines.at(0).selection.presence == SelectionPresence::absent,
           "and nothing is drawn as selected any more");
    expect(back.caret.position == TextPosition{LineNumber{1}, Column{2}},
           "the caret stays where VISUAL left it");
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
    // 2d の 2 はオペレータが持ち、そのあとの 3 は新しい回数になる（ADR 0015 の決定 1）。
    expect(controller.vim_state().pending.has_value() && !controller.vim_state().count.has_value(),
           "the operator carries its own count and waits for the motion");
    vim_replay(controller, "3");
    expect(controller.vim_state().count.has_value(), "the motion's count is counted on its own");
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

// INSERT にいるあいだの編集は 1 つの undo 単位（ADR 0015 の決定 5）。oracle は :normal! 1 回を
// まるごと 1 単位にする（"xxu" が 2 つの x を両方戻すことを実測）ので、単位はここで手で測る。
void verify_vim_insert_undo_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia<BS>b<Esc>");
    expect(vim_body(controller.frame()) == "bhello", "the backspace ate the character before it");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello",
           "one undo takes back the whole insert, deletion and all");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "there is nothing before that unit");
}

// c の削除は INSERT の単位に入る（cw のあとに打った文字と 1 つの Edit になる）。
void verify_vim_change_undo_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello world")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "cwbye<Esc>");
    expect(vim_body(controller.frame()) == "bye world", "cw changed the first word");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello world",
           "one undo takes back both the deletion and the typing");
}

// INSERT の中で動くと単位は切れる（ADR 0015 の決定 5）。本物の Vim は挿入 1 回を丸ごと
// 1 単位にするので、ここは Vim と違う＝ oracle には載せられない（報告と 5-g に書く）。
void verify_vim_insert_motion_breaks_the_unit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("hello")});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia");
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimSpecialKey::arrow_left}}));
    vim_replay(controller, "b<Esc>");
    expect(vim_body(controller.frame()) == "bahello", "the arrow moved back before the insert");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "ahello", "the arrow broke the unit");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "hello", "the second undo takes back the first letter");
}

// CRLF の文書でもレジスタは LF（決定 3）。文書の改行に戻すのは controller の 1 か所（ARC-009）。
void verify_vim_put_line_endings()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("one\r\ntwo\r\nthree")});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "dd");
    expect(controller.vim_state().unnamed_register.text == "one\n", "the register holds LF only");
    expect(controller.vim_state().unnamed_register.kind == VimRegisterKind::lines,
           "and it knows it is a line");
    vim_replay(controller, "p");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "two\r\none\r\nthree", "the put line came back as CRLF");
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "the caret sits on the line that was put");
    vim_replay(controller, "jp");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "two\r\none\r\nthree\r\none",
           "putting after the last line brings the CRLF with it");
    expect(controller.frame().caret.position == TextPosition{LineNumber{4}, Column{1}},
           "and the caret counts the CR that the engine did not see");
}

// fixture は LF だけ（Vim が CRLF を fileformat=dos として落とすので oracle に流せない）。
// CRLF の本文で CR が本文に残らないことは、保存したバイト列で手で測る（ADR 0012 の決定 9）。
// CRLF の文書は fixture にできない。`-S probe.vim input.txt` の順では `set binary` が間に合わず、
// 全部の行が CR で終わる入力を Vim は dos と読んで CR を落とし、1 行でも CR で終わらない行が
// あれば unix と読んで CR を文字として残すからである（ADR 0036 の決定 4 と決定 7・実測）。
// ここに並ぶのは engine 自身の答えで、同じ鍵の LF の fixture と対になっている。
// 再生の経路は fixture と同じ 1 本（verify_vim_fixture）を通す（ARC-001）。
void verify_vim_crlf_documents()
{
    constexpr std::array<VimFixture, 10> contracts{{
        {"crlf-line-jump-G", "a\r\n  b\r\nc", "2G", "a\n  b\nc", 2, 3, "", "", std::nullopt},
        {"crlf-open-line-below", "aa\r\nbb", "oX<Esc>", "aa\nX\nbb", 2, 1, "", "", std::nullopt},
        {"crlf-open-line-above", "aa\r\nbb", "jOX<Esc>", "aa\nX\nbb", 2, 1, "", "", std::nullopt},
        {"crlf-visual-wanted", "abcd\r\nx\r\nabcdef\r\nTAIL", "$vjjd", "abcTAIL", 1, 4,
         "d\nx\nabcdef\n", "v", std::nullopt},
        {"crlf-visual-yank", "  abcdef\r\nx\r\nTAIL", "$Vjy", "  abcdef\nx\nTAIL", 1, 1,
         "  abcdef\nx\n", "V", std::nullopt},
        {"crlf-replace-char", "abcd\r\nefgh", "l2r<CR>", "a\nd\nefgh", 2, 1, "", "", std::nullopt},
        {"crlf-replace-char-visual", "ab\r\ncdef\r\ngh", "lvjr\u754c",
         "a\u754c\n\u754c\u754cef\ngh", 1, 2, "", "", std::nullopt},
        {"crlf-dot-change-word", "ab cd\r\nef gh", "cwZZ<Esc>j0.", "ZZ cd\nZZ gh", 2, 2, "ef", "v",
         std::nullopt},
        {"crlf-dot-open-below", "ab\r\ncd", "ofoo<Esc>.", "ab\nfoo\nfoo\ncd", 3, 3, "", "",
         std::nullopt},
        {"crlf-dot-remove-line", "ab\r\ncd\r\nef", "dd.", "ef", 1, 1, "cd\n", "V", std::nullopt},
    }};
    for (const VimFixture &contract : contracts)
    {
        verify_vim_fixture(contract);
    }
}

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
    verify_vim_crlf_documents();
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
    VimState inserting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    inserting.mode = VimMode::insert;
    const auto newline = vim_step(
        inserting, VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'\n'}});
    expect(std::holds_alternative<VimNewLine>(newline.effect),
           "a newline typed as a character becomes the buffer's own line ending");
    const auto at_start = vim_step(
        inserting, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimSpecialKey::backspace});
    expect(std::holds_alternative<VimNoEffect>(at_start.effect),
           "Backspace at the start of the buffer does nothing");
    const VimState resting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    const auto unbound = vim_step(
        resting, VimEditorView{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'z'}});
    expect(std::holds_alternative<VimNoEffect>(unbound.effect), "an unbound key does nothing");
    expect(unbound.next.mode == VimMode::normal, "and it leaves NORMAL alone");
}

[[nodiscard]] std::string vim_lines(std::size_t count)
{
    std::string text;
    for (std::size_t line = 0; line < count; ++line)
    {
        if (line > 0)
        {
            text += '\n';
        }
        text += 'x';
    }
    return text;
}

void verify_vim_viewport_half_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    state.count = nenenib::core::VimCount{999};
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10}};
    const auto down = vim_step(state, view, VimKey{VimSpecialKey::control_d});
    const auto &down_effect = std::get<VimNavigate>(down.effect);
    const std::size_t down_line = buffer.position_of(down_effect.selection.caret).line.value;
    expect(down_effect.first_visible == LineNumber{16},
           "a huge Ctrl-d clamps its amount to the viewport height");
    expect(down_line >= down_effect.first_visible.value &&
               down_line < down_effect.first_visible.value + view.viewport.visible_lines,
           "Ctrl-d returns a caret inside the viewport it returns");
    expect(down.next.scroll_lines.value_or(nenenib::core::VimCount{0}).value == 10,
           "Ctrl-d stores the clamped amount");
    const auto up = vim_step(state, view, VimKey{VimSpecialKey::control_u});
    const auto &up_effect = std::get<VimNavigate>(up.effect);
    const std::size_t up_line = buffer.position_of(up_effect.selection.caret).line.value;
    expect(up_effect.first_visible == LineNumber{1}, "a huge Ctrl-u stops at the first line");
    expect(up_line >= up_effect.first_visible.value &&
               up_line < up_effect.first_visible.value + view.viewport.visible_lines,
           "Ctrl-u returns a caret inside the viewport it returns");
}

void verify_vim_viewport_page_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    state.count = nenenib::core::VimCount{999};
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10}};
    const auto page_down = vim_step(state, view, VimKey{VimSpecialKey::control_f});
    const auto &page_down_effect = std::get<VimNavigate>(page_down.effect);
    expect(page_down_effect.first_visible == LineNumber{30} &&
               buffer.position_of(page_down_effect.selection.caret).line == LineNumber{30},
           "a huge Ctrl-f reaches the last line without looping");
    const VimEditorView lower_view{buffer, collapsed_at(buffer.line_start(LineNumber{25})),
                                   VimViewport{LineNumber{21}, 10}};
    const auto page_up = vim_step(state, lower_view, VimKey{VimSpecialKey::control_b});
    const auto &page_up_effect = std::get<VimNavigate>(page_up.effect);
    expect(page_up_effect.first_visible == LineNumber{1} &&
               buffer.position_of(page_up_effect.selection.caret).line == LineNumber{6},
           "a huge Ctrl-b keeps the overlap of its last effective page");
    const VimEditorView top_view{buffer, collapsed_at(buffer.line_start(LineNumber{10})),
                                 VimViewport{LineNumber{1}, 10}};
    const auto page_at_top = vim_step(state, top_view, VimKey{VimSpecialKey::control_b});
    const auto &page_at_top_effect = std::get<VimNavigate>(page_at_top.effect);
    expect(page_at_top_effect.first_visible == LineNumber{1} &&
               page_at_top_effect.selection == top_view.selection,
           "Ctrl-b at the top preserves the caret exactly");
}

void verify_vim_viewport_mode_edges()
{
    const auto text = TextBuffer::from_utf8(vim_lines(30));
    expect(text.has_value(), "the viewport sample parses");
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(buffer.line_start(LineNumber{10}));
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{6}, 10}};
    VimState state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    VimState remembered = state;
    remembered.count = std::nullopt;
    remembered.scroll_lines = nenenib::core::VimCount{3};
    expect(nenenib::core::vim_after_resize(remembered, 10, 10).scroll_lines.has_value(),
           "an unchanged height preserves the half-page amount");
    expect(!nenenib::core::vim_after_resize(remembered, 10, 11).scroll_lines.has_value(),
           "a changed height resets the half-page amount");
    expect(nenenib::core::vim_resting_from(remembered, remembered.unnamed_register)
               .scroll_lines.has_value(),
           "resting transitions preserve the half-page amount");

    VimState inserting = remembered;
    inserting.mode = VimMode::insert;
    for (const VimSpecialKey key : {VimSpecialKey::control_d, VimSpecialKey::control_u,
                                    VimSpecialKey::control_f, VimSpecialKey::control_b})
    {
        const auto ignored = vim_step(inserting, view, VimKey{key});
        expect(std::holds_alternative<VimNoEffect>(ignored.effect),
               "INSERT ignores Ctrl viewport keys");
    }

    VimState visual = remembered;
    visual.mode = VimMode::visual;
    const std::array<std::pair<char32_t, LineNumber>, 3> screen_lines{
        {{U'H', LineNumber{6}}, {U'M', LineNumber{10}}, {U'L', LineNumber{15}}}};
    for (const auto &[key, expected] : screen_lines)
    {
        const auto moved = vim_step(visual, view, VimKey{VimCharacter{key}});
        const auto &effect = std::get<VimSelect>(moved.effect);
        expect(effect.selection.anchor == selection.anchor &&
                   buffer.position_of(effect.selection.caret).line == expected,
               "VISUAL H M L preserve the anchor and move on screen lines");
    }
    const auto visual_forward = vim_step(visual, view, VimKey{VimSpecialKey::control_f});
    const auto &visual_forward_effect = std::get<VimNavigate>(visual_forward.effect);
    expect(visual_forward_effect.selection.anchor == selection.anchor &&
               visual_forward_effect.first_visible == LineNumber{14} &&
               buffer.position_of(visual_forward_effect.selection.caret).line == LineNumber{14},
           "VISUAL Ctrl-f preserves the anchor and uses the NORMAL viewport transition");
    const auto visual_back = vim_step(visual, view, VimKey{VimSpecialKey::control_b});
    const auto &visual_back_effect = std::get<VimNavigate>(visual_back.effect);
    expect(visual_back_effect.selection.anchor == selection.anchor &&
               visual_back_effect.first_visible == LineNumber{1} &&
               buffer.position_of(visual_back_effect.selection.caret).line == LineNumber{7},
           "VISUAL Ctrl-b preserves the anchor and uses the NORMAL viewport transition");
}

void verify_vim_viewport_state_lifetime()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply(VisibleLines{10}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{10}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(ScrollLines{5}));
    vim_replay(controller, "3<C-d>j<C-d><Esc><C-u>");
    expect(controller.vim_state().scroll_lines.value_or(nenenib::core::VimCount{0}).value == 3,
           "ordinary Vim commands preserve an explicit half-page amount");
    static_cast<void>(controller.apply(VisibleLines{10}));
    expect(controller.vim_state().scroll_lines.has_value(),
           "a repeated notification of the same height preserves it");

    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    expect(controller.vim_state().scroll_lines.has_value(), "opening another file preserves it");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    expect(controller.vim_state().scroll_lines.has_value(), "mode toggles preserve it");

    static_cast<void>(controller.apply(ScrollLines{100}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    expect(controller.frame().first_visible == LineNumber{21},
           "ordinary mode restores the filled-viewport scroll bound");
    static_cast<void>(controller.apply(VisibleLines{11}));
    expect(!controller.vim_state().scroll_lines.has_value(),
           "a real height change resets the explicit half-page amount");
}

void verify_vim_visual_scroll_recovers_viewport()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply(VisibleLines{10}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{10}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(ScrollLines{5}));
    vim_replay(controller, "v");
    static_cast<void>(controller.apply(ScrollLines{15}));
    const auto frame = controller.apply(VimKeyPress{VimKey{VimSpecialKey::control_d}});
    expect(frame.vim_mode == VimMode::visual && frame.first_visible == LineNumber{15} &&
               frame.caret.position.line == LineNumber{15},
           "VISUAL Ctrl-d brings a wheel-hidden caret into its returned viewport");
}

void verify_vim_follow_at_document_end()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{vim_lines(30)});
    static_cast<void>(controller.apply(VisibleLines{10}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{15}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(ScrollLines{5}));
    vim_replay(controller, "99j");
    expect(controller.frame().caret.position.line == LineNumber{30} &&
               controller.frame().first_visible == LineNumber{21},
           "an automatic jump fills the last viewport");

    static_cast<void>(controller.apply(
        PlaceCaret{TextPosition{LineNumber{30}, Column{1}}, SelectionAnchoring::collapse}));
    static_cast<void>(controller.apply(ScrollLines{9}));
    vim_replay(controller, "<C-f>k");
    expect(controller.frame().caret.position.line == LineNumber{29} &&
               controller.frame().first_visible == LineNumber{29},
           "a motion after an explicit EOF page preserves its trailing blank area");
}

void verify_vim_insert_page_move_breaks_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("x\nx")});
    static_cast<void>(controller.apply(VisibleLines{2}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    vim_replay(controller, "ia<PageDown>b<Esc>u");
    expect(whole_vim_body(controller) == "ax\nx",
           "undo after INSERT PageDown takes back only the second insertion");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "x\nx",
           "the earlier insertion remains a separate undo unit");
}

// VISUAL の入口は選択そのもの（ADR 0018 の決定 2）。NORMAL / INSERT は anchor を読まない。
void verify_vim_visual_step_edges()
{
    const auto text = TextBuffer::from_utf8("abc\ndef");
    expect(text.has_value(), "the two line buffer parses");
    const auto &buffer = text.value();
    const VimState resting =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    const auto entered = vim_step(
        resting, VimEditorView{buffer, collapsed_at(Offset{1}), VimViewport{LineNumber{1}, 64}},
        VimKey{VimCharacter{U'v'}});
    expect(entered.next.mode == VimMode::visual, "v enters VISUAL");
    expect(std::get<VimSelect>(entered.effect).selection == Selection{Offset{1}, Offset{1}},
           "and anchors the selection where the caret is");
    VimState visual = resting;
    visual.mode = VimMode::visual;
    const Selection selection{Offset{1}, Offset{2}};
    // VISUAL で効かない鍵は選択もモードも動かさない（決定 7）。
    for (const VimKey &key : {VimKey{VimCharacter{U'p'}}, VimKey{VimCharacter{U'u'}},
                              VimKey{VimCharacter{U'D'}}, VimKey{VimCharacter{U'A'}},
                              VimKey{VimSpecialKey::enter}, VimKey{VimSpecialKey::backspace},
                              VimKey{VimSpecialKey::control_r}, VimKey{VimCharacter{U'z'}}})
    {
        const auto step =
            vim_step(visual, VimEditorView{buffer, selection, VimViewport{LineNumber{1}, 64}}, key);
        expect(std::holds_alternative<VimNoEffect>(step.effect),
               "a key outside this slice does nothing in VISUAL");
        expect(step.next.mode == VimMode::visual, "and stays in VISUAL");
    }
    const auto escaped =
        vim_step(visual, VimEditorView{buffer, selection, VimViewport{LineNumber{1}, 64}},
                 VimKey{VimSpecialKey::escape});
    expect(std::get<VimMoveTo>(escaped.effect).caret == Offset{2}, "Esc leaves the caret alone");
    expect(escaped.next.mode == VimMode::normal, "and goes back to NORMAL");
    // 表示の範囲と操作の範囲は同じ 1 本（決定 5 / ADR 0018 の強制）。
    const auto characters = vim_visual_range(buffer, selection, VimMode::visual);
    expect(characters.range == OffsetRange{Offset{1}, Offset{3}} &&
               characters.kind == VimRegisterKind::characters,
           "the charwise range takes the character under the far end");
    const auto lines = vim_visual_range(buffer, selection, VimMode::visual_line);
    expect(lines.range == OffsetRange{Offset{0}, Offset{3}} && lines.kind == VimRegisterKind::lines,
           "the linewise range takes whole lines");
    const auto across =
        vim_visual_range(buffer, Selection{Offset{1}, Offset{5}}, VimMode::visual_line);
    expect(across.range == OffsetRange{Offset{0}, Offset{7}}, "and spans both lines");
    const auto none = vim_visual_range(buffer, collapsed_at(Offset{2}), VimMode::normal);
    expect(is_empty(none.range), "NORMAL has no visual range");
    const auto inserting_none = vim_visual_range(buffer, collapsed_at(Offset{2}), VimMode::insert);
    expect(is_empty(inserting_none.range), "and neither has INSERT");
}

void verify_vim_replace_waiting()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nx\nabcdef").value();
    const VimEditorView view{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 3}};
    VimState initial = empty_vim_state();
    initial.count = nenenib::core::VimCount{2};
    initial.unnamed_register = {"saved", VimRegisterKind::characters};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(waits_for_prefix(waiting.next, VimPrefix::r) && waiting.next.count == initial.count &&
               std::holds_alternative<nenenib::core::VimNoEffect>(waiting.effect),
           "r waits for one target while keeping the explicit count");
    const auto replaced = vim_step(waiting.next, view, VimKey{VimCharacter{U'0'}});
    expect(std::holds_alternative<nenenib::core::VimReplaceRange>(replaced.effect),
           "a digit after r is its target character");
    expect(replaced.next.mode == VimMode::normal && !replaced.next.input_wait.has_value() &&
               !replaced.next.count.has_value() && replaced.next.unnamed_register.text == "saved",
           "replacement consumes transient input without overwriting the register");
    initial.count = nenenib::core::VimCount{std::numeric_limits<std::size_t>::max()};
    const auto too_many = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    expect(!too_many.next.input_wait.has_value() && !too_many.next.count.has_value() &&
               std::holds_alternative<nenenib::core::VimNoEffect>(too_many.effect),
           "a replacement that exceeds the line fails before waiting or allocating");
    const auto next = vim_step(too_many.next, view, VimKey{VimCharacter{U'x'}});
    expect(std::holds_alternative<nenenib::core::VimRemoveRange>(next.effect),
           "the next command after insufficient characters is not swallowed as an r target");
}

void verify_vim_replace_cancellation()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nx\nabcdef").value();
    const Selection selected{Offset{0}, Offset{3}};
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 3}};
    VimState initial = empty_vim_state();
    initial.mode = VimMode::visual;
    initial.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'r'}});
    // <CR> はここに並ばない。VISUAL の r<CR> は選んだ各文字を literal CR に置き換える
    // （ADR 0036 の決定 5・fixture literal-cr-visual-*）。
    for (const auto key : {VimSpecialKey::escape, VimSpecialKey::backspace,
                           VimSpecialKey::arrow_left, VimSpecialKey::home})
    {
        const auto cancelled = vim_step(waiting.next, view, VimKey{key});
        expect(cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value() &&
                   cancelled.next.wanted_column == initial.wanted_column &&
                   std::holds_alternative<nenenib::core::VimNoEffect>(cancelled.effect),
               "cancellation and unsupported targets preserve the visual selection and wish");
    }
    const auto down = vim_step(vim_step(waiting.next, view, VimKey{VimSpecialKey::escape}).next,
                               view, VimKey{VimCharacter{U'j'}});
    expect(std::get<VimSelect>(down.effect).selection == Selection{Offset{0}, Offset{6}},
           "the next independent motion still extends the preserved selection to line end");
}

void verify_vim_replace_history()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\ncdef\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "jlr界rZ");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\ncZef\r\ngh", "replacement preserves CRLF bytes");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nc界ef\r\ngh", "each r is a separate undo unit");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "undo restores the original code point bytes");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nc界ef\r\ngh", "redo restores the replacement");
}

void verify_vim_replace_newline()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "abcdef\r\nend";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "l3r<CR>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\nef\r\nend", "counted r Enter inserts one CRLF");
    expect(controller.frame().caret.position == TextPosition{LineNumber{2}, Column{1}},
           "the replacement caret accounts for the document newline width");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores the entire replaced range");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\nef\r\nend", "redo preserves the inserted CRLF");
}

void verify_vim_replace_visual_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\ncdef\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "lvjr界");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a界\r\n界界ef\r\ngh",
           "visual replacement changes characters while preserving document newlines");
    expect(controller.vim_state().mode == VimMode::normal &&
               controller.frame().lines.at(0).selection.presence == SelectionPresence::absent,
           "visual replacement finishes in NORMAL and collapses its selection");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a multiline visual replace");
    vim_replay(controller, "r");
    expect(waits_for_prefix(controller.vim_state(), VimPrefix::r), "replacement is awaiting input");
    applied(controller, SelectEditMode{EditMode::ordinary});
    applied(controller, SelectEditMode{EditMode::vim});
    expect(!controller.vim_state().input_wait.has_value(), "mode switching discards the r wait");
}

void verify_vim_replace_contracts()
{
    verify_vim_replace_waiting();
    verify_vim_replace_cancellation();
    verify_vim_replace_history();
    verify_vim_replace_newline();
    verify_vim_replace_visual_undo();
}

void verify_vim_replace_scope()
{
    constexpr std::array<std::string_view, 10> boundaries{"p-puts-the-character-after-the-caret",
                                                          "p-puts-the-line-below",
                                                          "capital-p-puts-the-line-above",
                                                          "capital-p-of-a-line-on-the-first-line",
                                                          "japanese-p-after-a-code-point",
                                                          "open-line-count-below",
                                                          "open-line-count-above",
                                                          "open-line-above",
                                                          "char-search-visual-y-f-range",
                                                          "line-jump-visual-gg-yank"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("replace-char-") || fixture.name.starts_with("literal-cr-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 76, "the scope replays 37 replacements, 29 literal-CR cases and 10 "
                           "shared-path boundaries");
    verify_vim_replace_contracts();
    verify_vim_character_search_waiting();
    verify_vim_line_jump_waiting();
}

void verify_vim_visual_yank_scope()
{
    constexpr std::array<std::string_view, 10> boundaries{
        "yy-does-not-move-the-caret",
        "yw-keeps-the-caret",
        "yb-moves-to-the-range-start",
        "y-dollar-keeps-the-caret",
        "yj-yanks-two-lines",
        "yk-moves-up-to-the-first-line",
        "yk-clamps-the-column-on-a-short-line",
        "v-y-yanks-the-selection",
        "v-j-y-across-lines",
        "V-indented-line-keeps-the-indent-in-the-register"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("visual-yank-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 32, "the scope replays 22 visual yanks and 10 shared-path boundaries");
}

void verify_vim_visual_wanted_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("visual-wanted-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 17, "the scope replays all 17 adopted visual-column fixtures");
}

void verify_vim_visual_wanted_continuations()
{
    for (const std::string_view keys :
         {"$vjj", "$Vjj", "$vVjj", "$Vvjj", "$vg<Esc>jj", "$vf<Esc>jj"})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(Bytes{std::string("abcd\nx\nabcdef")});
        applied(controller, VisibleLines{3});
        applied(controller, OpenDocument{sample_path()});
        applied(controller, SelectEditMode{EditMode::vim});
        vim_replay(controller, keys);
        expect(controller.frame().caret.position == TextPosition{LineNumber{3}, Column{7}},
               std::string(keys).c_str());
        expect(controller.vim_state().wanted_column ==
                   nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}},
               "visual transitions and waiting cancellation preserve the line-end wish");
        expect(vim_body(controller.frame()) == "abcd\nx\nabcdef",
               "visual transitions leave the document unchanged");
    }
}

void verify_vim_visual_wanted_counted()
{
    const auto buffer = TextBuffer::from_utf8("abcd\nabcdef\nxy").value();
    const VimEditorView view{buffer, collapsed_at(Offset{3}), VimViewport{LineNumber{1}, 3}};
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    state.count = nenenib::core::VimCount{2};
    const auto line = vim_step(state, view, VimKey{VimCharacter{U'V'}});
    expect(std::get<VimSelect>(line.effect).selection == Selection{Offset{3}, Offset{11}},
           "2V uses the inherited line-end wish on a longer destination line");
    expect(line.next.wanted_column == state.wanted_column && !line.next.count.has_value(),
           "2V preserves the wish but consumes its count");
    const auto character = vim_step(state, view, VimKey{VimCharacter{U'v'}});
    expect(character.next.wanted_column ==
               nenenib::core::VimWantedColumn{VimColumnWish::at_column, VirtualColumn{5}},
           "2v replaces the line-end wish with the widened endpoint column");
    state.count = nenenib::core::VimCount{1};
    const auto single = vim_step(state, view, VimKey{VimCharacter{U'v'}});
    expect(single.next.wanted_column == state.wanted_column,
           "explicit 1v preserves the wish without widening");
}

void verify_vim_visual_wanted_undo()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("abcd\r\nx\r\nabcdef\r\nTAIL")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "$vjjd");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "abcTAIL", "the selection includes the endpoint newline");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "abcd\r\nx\r\nabcdef\r\nTAIL",
           "the existing undo restores the exact CRLF bytes");
}

void verify_vim_visual_wanted_blocked_expansion()
{
    const auto buffer = TextBuffer::from_utf8("\nabcdef").value();
    VimState state = empty_vim_state();
    state.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    state.count = nenenib::core::VimCount{2};
    const VimEditorView empty{buffer, collapsed_at(Offset{0}), VimViewport{LineNumber{1}, 2}};
    const auto character = vim_step(state, empty, VimKey{VimCharacter{U'v'}});
    expect(character.next.wanted_column == state.wanted_column,
           "blocked horizontal expansion on an empty line preserves the wish");
    const auto down = vim_step(character.next, empty, VimKey{VimCharacter{U'j'}});
    expect(std::get<VimSelect>(down.effect).selection == Selection{Offset{0}, Offset{7}},
           "the next independent key reaches the longer line's NUL column");
    const VimEditorView last{buffer, collapsed_at(Offset{6}), VimViewport{LineNumber{1}, 2}};
    const auto line = vim_step(state, last, VimKey{VimCharacter{U'V'}});
    expect(std::get<VimSelect>(line.effect).selection == Selection{Offset{6}, Offset{6}},
           "2V at EOF does not move the caret to the NUL column");
    expect(line.next.wanted_column == state.wanted_column,
           "blocked vertical expansion preserves its wish");
}

void verify_vim_visual_wanted_contracts()
{
    verify_vim_visual_wanted_continuations();
    verify_vim_visual_wanted_counted();
    verify_vim_visual_wanted_blocked_expansion();
    verify_vim_visual_wanted_undo();
}

void verify_vim_visual_wanted_scope()
{
    verify_vim_visual_wanted_fixtures();
    verify_vim_visual_wanted_contracts();
    verify_vim_visual_step_edges();
}

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

// ---------------------------------------------------------------- 矩形 VISUAL（ADR 0035）

[[nodiscard]] EditorController &vim_block_editor(Editing &editing, std::string_view text)
{
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(text)});
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(controller.apply(OpenDocument{sample_path()}));
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    return controller;
}

// 矩形の選択は行ごとに描かれ、Ctrl+C / Ctrl+X も同じ行ごとの範囲を使う（決定 8）。fixture は
// 本文とレジスタしか見ないので、描く面と OS のクリップボードはここで測る。
void verify_vim_block_drawing()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "<C-v>jl");
    const auto frame = controller.frame();
    expect(frame.mode_label == "VISUAL BLOCK" && frame.caret.shape == CaretShape::block,
           "the block names itself on the status bar and keeps the block caret");
    expect(frame.lines.at(0).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "the first row of the block is drawn");
    expect(frame.lines.at(1).selection ==
               nenenib::core::SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
           "so is the second row, at the same columns");
    expect(frame.lines.at(2).selection.presence == SelectionPresence::absent,
           "the row below the block is not drawn");
    static_cast<void>(applied(controller, ClipboardAction{ClipboardOperation::copy}));
    expect(editing.clipboard().read().value() == "ab\ngh",
           "Ctrl+C joins the rows with the document newline");
    static_cast<void>(applied(controller, ClipboardAction{ClipboardOperation::cut}));
    expect(vim_body(controller.frame()) == "cdef\nijkl\nmnopqr",
           "Ctrl+X takes the same rows out of the body");
}

// 行が矩形より手前で終わるとき。レジスタには幅ぶんの空白が入り本文は動かず、`r` も書かない
// （Issue #112 で実測）。この 3 つは 1 回の `:normal!` では再現しない（走査の失敗が残りの打鍵を
// 捨てる・Issue #87）ので fixture に採れず、ここで測る（out/issue112-oracle/add-fixtures.txt）。
void verify_vim_block_short_lines()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\ngh\nij");
    vim_replay(controller, "3l<C-v>jjly");
    expect(controller.vim_state().unnamed_register.text == "cd\n\n",
           "the rows that end before the block put nothing in the register");
    expect(vim_register_kind(controller.vim_state().unnamed_register) == "\0262",
           "and the register is a block two columns wide");
    expect(controller.frame().caret.position == TextPosition{LineNumber{1}, Column{3}},
           "the yank leaves the caret at the top left corner");
    vim_replay(controller, "gg3l<C-v>jjld");
    expect(vim_body(controller.frame()) == "abef\ngh\nij",
           "and deleting the same block leaves those rows alone");
    vim_replay(controller, "ugg3l<C-v>jjlrZ");
    expect(vim_body(controller.frame()) == "abZZef\ngh\nij",
           "r writes nothing on the rows the block does not cover");
}

// 矩形の削除・置換・貼付は 1 つの undo 単位（決定 3）。oracle の `:normal!` は 1 回の実行を
// まるごと 1 単位にするので、単位の境はここで手で測る（ADR 0012 の決定 6）。
void verify_vim_block_undo_unit()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "x<C-v>jjlld");
    expect(vim_body(controller.frame()) == "ef\njkl\npqr", "x then a 3x3 block delete");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "bcdef\nghijkl\nmnopqr",
           "one undo takes back the whole block, all three rows at once");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr",
           "the undo before it is the single x");
}

// CRLF の本文でも矩形は CR を本文に残さない（ADR 0012 の決定 9）。fixture は LF だけなので
// 保存したバイト列で測る。貼付が足す行も文書の改行で書く。
void verify_vim_block_line_endings()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abc\r\ndef\r\nghi");
    vim_replay(controller, "<C-v>jld");
    expect(controller.vim_state().unnamed_register.text == "ab\nde",
           "the block register holds LF only");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghi",
           "the block delete kept the CRLF line endings");
    vim_replay(controller, "G$p");
    static_cast<void>(controller.apply(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(editing.files().written() == "c\r\nf\r\nghiab\r\n   de",
           "and the line the block paste added came back as CRLF");
}

// 矩形では範囲外の鍵（決定 5）。どれも本文を変えず、矩形のまま残る。
void verify_vim_block_unsupported_keys()
{
    Editing editing;
    EditorController &controller = vim_block_editor(editing, "abcdef\nghijkl\nmnopqr");
    vim_replay(controller, "yy<C-v>jl");
    constexpr std::array<std::string_view, 8> keys{"c", "I", "A", "C", ">", "J", "~", "p"};
    for (const std::string_view key : keys)
    {
        vim_replay(controller, key);
        expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr",
               "a key that is out of scope for the block leaves the body alone");
        expect(controller.frame().mode_label == "VISUAL BLOCK", "and leaves the block selected");
    }
    vim_replay(controller, "iw");
    expect(controller.frame().mode_label == "VISUAL BLOCK",
           "a text object does not turn the block into a characterwise selection");
    expect(vim_body(controller.frame()) == "abcdef\nghijkl\nmnopqr", "and changes nothing");
}

// core の純関数を直に測る。行ごとの範囲・左の桁・幅が 1 本で決まること（決定 2）。
void verify_vim_block_range()
{
    const auto text = TextBuffer::from_utf8("ab\tcd\nefghijkl\nmn\topq");
    expect(text.has_value(), "the block sample parses");
    if (!text.has_value())
    {
        return;
    }
    // 2 桁目の Tab は 3..8 桁を占める。矩形の角がその上に載ると Tab まるごとが矩形に入る。
    const Selection over_tab{Offset{2}, Offset{17}};
    const auto block = vim_block_range(text.value(), over_tab, VimColumnWish::at_column);
    expect(block.left == VirtualColumn{3} && block.width == VimBlockWidth{6},
           "the block spans the whole Tab");
    expect(block.lines.size() == 3 && block.first == LineNumber{1},
           "and covers the three lines the corners sit on");
    expect(vim_block_text(text.value(), block) == "\t\nghijkl\n\t",
           "the register text keeps the Tab and takes six columns from the plain line");
    const auto whole = vim_block_range(text.value(), over_tab, VimColumnWish::at_line_end);
    expect(whole.width == VimBlockWidth{9},
           "with $ the width comes from the longest line the block covers");
    expect(vim_block_text(text.value(), whole) == "\tcd\nghijkl\n\topq",
           "and every row runs to its own end");
}

// `.` の大きさ（決定 7）。左上をキャレットにして同じ行数と桁数の矩形を選び直す。
void verify_vim_block_reselect()
{
    const auto text = TextBuffer::from_utf8("abcd\nef\nijkl\nmn");
    expect(text.has_value(), "the reselect sample parses");
    if (!text.has_value())
    {
        return;
    }
    const auto columns = vim_visual_reselect(
        text.value(), Offset{9}, VimBlockExtent{2, VimColumnWish::at_column, VimBlockWidth{2}});
    expect(columns == Selection{Offset{9}, Offset{15}},
           "two lines and two columns from the third line's second column");
    const auto ends = vim_visual_reselect(
        text.value(), Offset{8}, VimBlockExtent{2, VimColumnWish::at_line_end, VimBlockWidth{2}});
    expect(ends == Selection{Offset{8}, Offset{15}},
           "$ puts the far end at the last line's content end and ignores the width");
}

void verify_vim_block_fixtures()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("block-"))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 144, "the scope replays its 144 blockwise oracle fixtures");
}

void verify_vim_block_contracts()
{
    verify_vim_block_drawing();
    verify_vim_block_short_lines();
    verify_vim_block_undo_unit();
    verify_vim_block_line_endings();
    verify_vim_block_unsupported_keys();
    verify_vim_block_range();
    verify_vim_block_reselect();
}

void verify_vim_block_scope()
{
    verify_vim_block_fixtures();
    verify_vim_block_contracts();
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

// 開行と反復は既存のINSERT・保存・undoの一単位（ADR 0028）。
void verify_open_line_round_trip(std::string_view initial, std::string_view keys,
                                 std::string_view expected)
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string(initial)});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, keys);
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == expected, "open-line saves the expected original bytes");
    expect(controller.vim_state().mode == VimMode::normal &&
               !controller.vim_state().insert_repeat.has_value(),
           "Esc clears the repeat session");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == initial, "one undo restores the document before opening");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == expected, "one redo restores opening and all insertions");
}

void verify_vim_open_line_undo()
{
    verify_open_line_round_trip("aa\r\nbb", "3oあ<CR>😀<Esc>",
                                "aa\r\nあ\r\n😀\r\nあ\r\n😀\r\nあ\r\n😀\r\nbb");
    verify_open_line_round_trip("aa\r\nbb", "3Oa<BS>日<Esc>", "日\r\n日\r\n日\r\naa\r\nbb");
    verify_open_line_round_trip("aa\nbb", "j3O<BS><BS>X<Esc>", "aX\nbb");
    verify_open_line_round_trip("aa\nbb", "3o<BS>X<Esc>", "aaXXX\nbb");
    verify_open_line_round_trip("aa\n", "GoX<Esc>", "aa\n\nX");
    verify_open_line_round_trip("", "3O<Esc>", "\r\n\r\n\r\n");
}

void verify_vim_open_line_intermediate()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, VisibleLines{2});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "G3o");
    const auto frame = controller.frame();
    expect(frame.total_lines == 3 && frame.vim_mode == VimMode::insert,
           "a counted open starts one empty line and enters INSERT immediately");
    expect(frame.caret.position == TextPosition{LineNumber{3}, Column{1}} &&
               frame.first_visible == LineNumber{2},
           "opening below EOF follows the caret");
    applied(controller, ComposeText{composed_of("に", {}, 3)});
    expect(controller.frame().composition.has_value(), "opened INSERT accepts composition");
    applied(controller, CommitText{"日本😀"});
    expect(controller.frame().total_lines == 3, "typing does not expand the count yet");
    vim_replay(controller, "<Esc>");
    expect(controller.frame().caret.position == TextPosition{LineNumber{5}, Column{3}},
           "Esc expands the committed UTF-8 input and rests on the last character");
    expect(whole_vim_body(controller) == "aa\nbb\n日本😀\n日本😀\n日本😀",
           "IME commit and direct characters share the insertion record");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "aa\nbb", "IME and opening form one undo unit");
}

void verify_vim_open_line_movement()
{
    constexpr std::array<VimSpecialKey, 8> keys{
        VimSpecialKey::arrow_left, VimSpecialKey::arrow_right, VimSpecialKey::arrow_up,
        VimSpecialKey::arrow_down, VimSpecialKey::home,        VimSpecialKey::end,
        VimSpecialKey::page_up,    VimSpecialKey::page_down};
    for (const VimSpecialKey key : keys)
    {
        Editing editing;
        EditorController &controller = editing.controller();
        applied(controller, SelectEditMode{EditMode::vim});
        vim_replay(controller, "3Oabc");
        applied(controller, VimKeyPress{VimKey{key}});
        expect(!controller.vim_state().insert_repeat.has_value(),
               "every INSERT movement cancels open-line repetition, including no-op moves");
        vim_replay(controller, "Z<Esc>u");
        expect(whole_vim_body(controller) == "abc\n", "movement seals the earlier insertion");
        vim_replay(controller, "u");
        expect(whole_vim_body(controller).empty(), "the earlier undo also removes the open line");
    }
}

void verify_vim_open_line_switch()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3oX");
    applied(controller, SelectEditMode{EditMode::ordinary});
    expect(!controller.vim_state().insert_repeat.has_value(), "ordinary mode cancels repetition");
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "iY<Esc>");
    expect(whole_vim_body(controller) == "\nYX", "a later INSERT cannot replay the old count");
}

void verify_history_inner_absorbing()
{
    const Edit typed{Offset{5}, "old", "ab\n"};
    expect(absorbed_edit(typed, Edit{Offset{5}, "", "X"}) == Edit{Offset{5}, "old", "Xab\n"},
           "inserting at the start preserves the old removal");
    expect(absorbed_edit(typed, Edit{Offset{6}, "b", "YZ"}) == Edit{Offset{5}, "old", "aYZ\n"},
           "a replacement inside the inserted span is composed into that edit");
    expect(absorbed_edit(typed, Edit{Offset{5}, "a", ""}) == Edit{Offset{5}, "old", "b\n"},
           "deleting before the trailing open-line newline stays in the same unit");
    expect(!absorbed(typed, Edit{Offset{7}, "\nx", ""}).has_value(),
           "a removal across the end cannot be absorbed as an inner edit");
}

void verify_vim_open_line_fixtures()
{
    std::size_t opened = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("open-line-"))
        {
            verify_vim_fixture(fixture);
            ++opened;
        }
        if (fixture.name.find("puts") != std::string_view::npos ||
            fixture.name == "i-inserts-before-the-caret" ||
            fixture.name == "a-inserts-after-the-caret")
        {
            verify_vim_fixture(fixture);
        }
    }
    expect(opened == 38, "all 38 measured open-line fixtures were replayed");
}

void verify_vim_open_line_capacity()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    const std::string count = std::to_string(std::numeric_limits<std::size_t>::max());
    vim_replay(controller, count + "oX<Esc>");
    expect(whole_vim_body(controller) == "aa\nX\nbb",
           "overflowing repetition preserves the initial insertion without expansion");
    expect(controller.vim_state().mode == VimMode::normal &&
               !controller.vim_state().insert_repeat.has_value(),
           "a refused repetition still exits INSERT and clears its count");
    vim_replay(controller, "uyy" + count + "p");
    expect(whole_vim_body(controller) == "aa\nbb", "put shares the checked repetition size");
    vim_replay(controller, "3o<BS><Esc>");
    expect(whole_vim_body(controller) == "aa\nbb", "empty repetition does not allocate or loop");
}

void verify_vim_open_line_recovery()
{
    verify_open_line_round_trip("", "3O<Esc>", "\r\n\r\n\r\n");
    verify_vim_open_line_capacity();
    verify_vim_open_line_fixtures();
}

void verify_vim_open_line_external_input()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("aa\nbb")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3OX");
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{2}}, SelectionAnchoring::collapse});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "even a same-position click cancels repeat");
    vim_replay(controller, "Y<Esc>u");
    expect(whole_vim_body(controller) == "X\naa\nbb", "a click seals the prior opening");
    vim_replay(controller, "u3oX");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(!controller.vim_state().insert_repeat.has_value(), "Ctrl+Z cancels the undone session");
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "aa\nbb", "Esc cannot replay input removed by Ctrl+Z");
    vim_replay(controller, "3oX");
    applied(controller, SelectAll{});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "external selection cancels repetition");
}

void verify_vim_open_line_external_edit()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "3Oab");
    applied(controller, InsertText{"PASTE"});
    expect(!controller.vim_state().insert_repeat.has_value(),
           "unrecorded edit cannot leave stale replay text");
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "abPASTE\n", "Esc preserves unrecorded input once");
    vim_replay(controller, "u");
    expect(whole_vim_body(controller) == "ab\n", "external edit has its own undo unit");
}

// 割り込みが捨てるものと保つもの（Issue #92）。範囲を決めるのは engine の純関数
// vim_interrupted で、controller は「どれが割り込みか」だけを決める（ARC-004）。
void verify_vim_interrupt_discards()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(Bytes{std::string("alpha beta\nsecond line")});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "x3Oab");
    expect(controller.vim_state().insert_repeat.has_value() &&
               controller.vim_state().recording.has_value(),
           "the counted open line is holding both a repetition and a half-built dot recording");
    applied(controller, SelectAll{});
    expect(controller.vim_state().mode == VimMode::insert &&
               !controller.vim_state().insert_repeat.has_value() &&
               !controller.vim_state().recording.has_value(),
           "an interruption drops the repetition and the recording it cannot replay, not the mode");
    expect(controller.vim_state().last_change.has_value(),
           "the change that was already confirmed is still what the dot repeats");

    // 割り込みは鍵ではないので、engine の純関数を直に呼んで捨てる範囲を確かめる。
    Editing pending;
    EditorController &waiting = pending.controller();
    pending.files().hold(Bytes{std::string("alpha beta\nsecond line")});
    applied(waiting, OpenDocument{sample_path()});
    applied(waiting, SelectEditMode{EditMode::vim});
    vim_replay(waiting, "fa2d3f");
    const VimState &before = waiting.vim_state();
    expect(before.count.has_value() && before.pending.has_value() &&
               before.input_wait.has_value() && before.last_character_search.has_value(),
           "the operator is pending with a count and is waiting for one more key");
    const VimState after = nenenib::core::vim_interrupted(before);
    expect(!after.count.has_value() && !after.pending.has_value() && !after.input_wait.has_value(),
           "the pure function drops the count, the pending operator and the awaited key");
    expect(after.mode == before.mode && after.last_character_search.has_value() &&
               after.last_character_search.value().target == U'a',
           "and keeps the mode and the remembered character search");
}

void verify_vim_open_line_external_scope()
{
    verify_vim_interrupt_discards();
    verify_vim_open_line_external_input();
    verify_vim_open_line_external_edit();
    verify_vim_open_line_movement();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_put_line_endings();
}

void verify_vim_open_line_contracts()
{
    verify_vim_open_line_undo();
    verify_vim_open_line_intermediate();
    verify_vim_open_line_movement();
    verify_vim_open_line_switch();
    verify_vim_interrupt_discards();
    verify_vim_open_line_external_input();
    verify_vim_open_line_external_edit();
    verify_vim_open_line_capacity();
    verify_history_inner_absorbing();
}

void verify_vim_open_line_scope()
{
    verify_vim_open_line_contracts();
    verify_vim_open_line_fixtures();
    verify_history_absorbing();
    verify_history_coalescing();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_insert_page_move_breaks_undo();
    verify_vim_put_line_endings();
}

// scope 専用の契約は verify_vim_scope_contracts の表が回す（Issue #97）。ここは scope を持たない
// Vim の共通部分だけを見る。
void verify_vim_engine()
{
    verify_vim_word_motions();
    verify_vim_caret_rules();
    verify_vim_step_edges();
    verify_vim_viewport_half_edges();
    verify_vim_viewport_page_edges();
    verify_vim_viewport_mode_edges();
    verify_vim_viewport_state_lifetime();
    verify_vim_visual_scroll_recovers_viewport();
    verify_vim_follow_at_document_end();
    verify_vim_insert_page_move_breaks_undo();
    verify_vim_visual_step_edges();
    verify_vim_key_notation();
    verify_vim_mode_labels();
    verify_vim_caret_shapes();
    verify_vim_visual_selection_and_clipboard();
    verify_vim_mode_entry();
    verify_vim_caret_placement();
    verify_vim_undo_boundaries();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_insert_motion_breaks_the_unit();
    verify_vim_put_line_endings();
    verify_vim_crlf();
    verify_vim_other_keys();
    verify_vim_fixtures();
}

void verify_vim_character_search_scope()
{
    verify_vim_character_search_contracts();
    verify_vim_character_search_fixtures();
    verify_vim_step_edges();
    verify_vim_visual_step_edges();
    verify_vim_viewport_mode_edges();
    verify_vim_insert_undo_unit();
    verify_vim_change_undo_unit();
    verify_vim_put_line_endings();
    verify_vim_crlf();
}

void verify_vim_line_jump_scope()
{
    verify_vim_line_jump_contracts();
    verify_vim_line_jump_fixtures();
    verify_vim_character_search_scope();
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

namespace core = nenenib::core;
namespace app = nenenib::application;

void verify_ex_evaluation()
{
    const auto settings = core::default_editor_settings();
    const auto query = core::evaluate_ex("colorscheme", settings, Appearance::dark).value();
    expect(!query.settings.has_value() &&
               query.message.text() == "colorscheme=ubuntu-aubergine (system)",
           "query resolves the system theme without saving");
    for (const auto &theme : core::builtin_themes)
    {
        const auto result =
            core::evaluate_ex("colorscheme " + std::string(theme.name), settings, Appearance::dark)
                .value();
        expect(result.settings.has_value(), "a theme command returns settings");
        const auto resolved = result.settings.value_or(settings);
        expect(core::selected_theme(resolved, Appearance::light).name == theme.name,
               "every built-in theme uses the shared table");
    }
    const auto font =
        core::evaluate_ex("set guifont=MS Gothic:h21.25", settings, Appearance::dark).value();
    expect(font.settings.has_value(), "guifont returns an atomic family and size change");
    const auto selected = font.settings.value_or(settings);
    expect(selected.font_family.text() == "MS Gothic" && selected.font_size.points() == 21.25F,
           "font names may contain spaces");
    const auto system =
        core::evaluate_ex(" colorscheme system ", selected, Appearance::light).value();
    expect(!system.settings.value_or(settings).theme.has_value() &&
               system.message.text() == "colorscheme=neutral-light (system)",
           "system follows the OS");
    for (const auto text : {"8", "40", "13.5", "2e1"})
    {
        expect(core::FontSize::parse(text).has_value(), "settings and Ex share numeric parsing");
    }
    for (const auto text : {"", "8px", "NaN", "inf", "7.99", "40.01", " 12", "12 "})
    {
        expect(!core::FontSize::parse(text), "invalid point text is rejected before storage");
    }
}

void verify_ex_rejections()
{
    const auto settings = core::default_editor_settings();
    for (const auto text :
         {"", "w", "2colorscheme dracula", "1,2set fontsize=12", ":colorscheme", "set",
          "set number", "set fontsize=41", "set fontsize=12|q", "colorscheme DRACULA",
          "colorscheme missing", "set guifont=:h12", "set guifont=Consolas",
          "set guifont=Consolas:h12x", "set guifont=Consolas:hNaN", "colorscheme\n"})
    {
        const auto result = core::evaluate_ex(text, settings, Appearance::dark);
        expect(!result, "unsupported Ex must not partially execute");
        if (!result)
        {
            expect(!core::ex_failure_message(result.error()).text().empty(),
                   "failures explain themselves");
        }
    }
    expect(!core::evaluate_ex(std::string(257, 'x'), settings, Appearance::dark),
           "long commands are rejected");
    expect(!core::evaluate_ex("\xFF", settings, Appearance::dark), "invalid UTF-8 is rejected");
    expect(!core::evaluate_ex("set guifont=Consolas|q:h12", settings, Appearance::dark),
           "a pipe cannot be stored as part of the font name");
}

void verify_command_editing()
{
    using core::CommandEdit;
    auto line = core::CommandLine::empty().inserted("a界😀z").value();
    expect(line.caret().value == 9, "caret counts UTF-8 bytes");
    line = line.edited(CommandEdit::left).edited(CommandEdit::backspace);
    expect(line.text() == "a界z" && line.caret().value == 4, "backspace removes one code point");
    line = line.edited(CommandEdit::left).edited(CommandEdit::erase);
    expect(line.text() == "az" && line.caret().value == 1, "delete respects a multibyte boundary");
    line = line.edited(CommandEdit::home).edited(CommandEdit::backspace).inserted("前").value();
    expect(line.text() == "前az", "home and empty-range backspace retain text");
    line = line.edited(CommandEdit::end).edited(CommandEdit::right).edited(CommandEdit::erase);
    expect(line.caret().value == 5 && line.text() == "前az", "end movement and delete clamp");
    expect(!line.inserted("\n") && !line.inserted("\t") && !line.inserted("\xFF"),
           "input is one valid line");
    expect(line.inserted("").has_value(), "empty clipboard text is harmless");
    const auto full = core::CommandLine::empty().inserted(std::string(256, 'x')).value();
    expect(!full.inserted("y") && full.text().size() == 256,
           "rejected input does not alter the line");
}

void verify_command_completions()
{
    using core::CommandEdit;
    auto line = core::CommandLine::empty().inserted("colorscheme ").value();
    const auto choices = line.completions();
    expect(choices.size() == core::builtin_themes.size() + 1, "theme candidates include system");
    for (std::size_t index = 0; index < choices.size() + 1; ++index)
    {
        line = line.edited(CommandEdit::complete_next);
        expect(line.text() == choices[index % choices.size()],
               "Tab cycles using the original prefix");
    }
    line = line.edited(CommandEdit::complete_previous);
    expect(line.text() == "colorscheme system", "Shift Tab wraps backwards");
    line = line.edited(CommandEdit::home);
    expect(!line.completion_index().has_value(), "manual movement resets completion");
    const auto first = core::CommandLine::empty().edited(CommandEdit::complete_previous);
    expect(first.text() == "set nohlsearch", "backwards completion starts at the last command");
    expect(core::command_completions("set f") == std::vector<std::string>{"set fontsize="},
           "option prefix");
    const auto unknown = core::CommandLine::empty().inserted("unknown").value();
    expect(unknown.edited(CommandEdit::complete_next).text() == "unknown",
           "missing candidates leave text");
    for (const auto dpi : {96U, 144U, 192U})
    {
        const auto status = core::status_bar_layout(800, 450, dpi);
        const auto layout = core::command_layout(status, dpi, 10);
        expect(layout.input.right < status.items.front().left && layout.visible_rows <= 6,
               "completion never covers the right status items");
        const auto tiny = core::command_layout(core::status_bar_layout(40, 10, dpi), dpi, 10);
        expect(core::width_of(tiny.input) == 0 && tiny.visible_rows == 0,
               "small windows do not invert rectangles");
    }
}

app::EditorFrame run_ex(EditorController &controller, std::string text)
{
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U':'}}));
    static_cast<void>(controller.apply(app::CommandText{std::move(text)}));
    return controller.apply(app::SubmitCommand{});
}

void verify_ex_controller()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"preserved"}));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
    const auto before = controller.frame();
    const auto result = run_ex(controller, "set guifont=Consolas:h18.5");
    expect(result.settings.font_size.points() == 18.5F && editor.settings().writes() == 1,
           "Ex saves once");
    expect(result.settings.font_family.text() == "Consolas" &&
               result.lines.front().text == "preserved",
           "command text is not inserted into the document");
    expect(result.caret.position.column == before.caret.position.column &&
               result.document.save_state == before.document.save_state,
           "settings preserve caret and dirty state");
    expect(!result.command_line.has_value() && result.command_message.has_value(),
           "Enter closes with a result");
    static_cast<void>(run_ex(controller, "set guifont=Consolas:h18.5"));
    static_cast<void>(run_ex(controller, "colorscheme"));
    expect(editor.settings().writes() == 1, "same settings and queries do not write");
    const auto notified = controller.apply(VisibleLines{12});
    expect(notified.command_message.has_value(), "layout notification preserves the result");
    const auto changed = run_ex(controller, "colorscheme neutral-light");
    expect(changed.appearance == Appearance::light, "Ex updates the palette immediately");
    const auto old_settings = changed.settings;
    editor.settings().fail(app::SettingsFailure::unwritable);
    const auto failed = run_ex(controller, "set fontsize=20");
    expect(core::same_settings(failed.settings, old_settings) &&
               failed.settings_failure.has_value(),
           "failed persistence leaves the appearance intact");
    expect(failed.command_message.has_value(), "save failure is visible in the command result");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "commands add no text undo entry");
}

void verify_ex_input_isolation()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U'y'}}));
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U'y'}}));
    const auto saved_register = controller.vim_state().unnamed_register.text;
    auto frame = controller.apply(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(frame.command_line.has_value(), "NORMAL colon opens a separate line");
    frame = controller.apply(app::EditCommand{core::CommandEdit::backspace});
    expect(!frame.command_line.has_value(), "empty backspace cancels");
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U':'}}));
    editor.clipboard().hold(std::string("set fontsize=19"));
    frame = controller.apply(app::PasteCommand{});
    expect(frame.command_line.has_value(), "paste keeps the command active");
    expect(frame.command_line.value_or(core::InputLineView{}).text == "set fontsize=19",
           "clipboard goes to Ex");
    editor.clipboard().hold(std::string("\nBAD"));
    frame = controller.apply(app::PasteCommand{});
    expect(frame.command_message.has_value() && frame.lines.front().text == "body",
           "multiline paste is rejected");
    frame = controller.apply(app::CancelCommand{});
    expect(!frame.command_line.has_value() && editor.settings().writes() == 0,
           "cancel does not save");
    expect(controller.vim_state().unnamed_register.text == saved_register,
           "command input preserves the register");
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U'2'}}));
    frame = controller.apply(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(!frame.command_line.has_value(),
           "unsupported counted Ex does not execute as an uncounted command");
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U'v'}}));
    frame = controller.apply(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(!frame.command_line.has_value() && frame.vim_mode == core::VimMode::visual,
           "visual ranges are not interpreted");
}

void verify_ex_settings()
{
    verify_font_sizes();
    verify_settings_loading();
    verify_settings_adjustment();
    verify_settings_failures();
    verify_ex_evaluation();
    verify_ex_rejections();
    verify_command_editing();
    verify_command_completions();
    verify_ex_controller();
    verify_ex_input_isolation();
}

std::vector<core::CommandChoice> choices_for(std::string_view query)
{
    return core::palette_choices(core::CommandLine::empty().inserted(query).value());
}

void verify_user_theme_values()
{
    for (const auto text : {"a", "my-theme", "my_theme", "theme-2026"})
    {
        expect(core::ThemeName::parse(text).has_value(), "valid user theme identifier");
    }
    expect(core::ThemeName::parse("my_theme").value() == core::ThemeName::parse("my-theme").value(),
           "theme name has one canonical spelling");
    for (const auto text :
         {"", "1theme", "Theme", "-theme", "a/b", "a\\b", "a..b", "a:b", "a b", "界", "a\n"})
    {
        expect(!core::ThemeName::parse(text), "invalid or path-like theme names rejected");
    }
    expect(core::ThemeName::parse(std::string(64, 'a')).has_value(), "maximum theme name accepted");
    expect(!core::ThemeName::parse(std::string(65, 'a')), "oversized theme name rejected");
    const auto &theme = core::theme_of(core::BuiltinTheme::dracula);
    core::ThemeDocument document{core::ThemeName::parse("my-theme").value(), theme.appearance,
                                 theme.ui, theme.body,
                                 core::OwnedThemeSource{core::DisplayText::parse("作者").value(),
                                                        core::DisplayText::parse("MIT").value(),
                                                        core::DisplayText::parse("local").value()}};
    const auto copy = document;
    document.name = core::ThemeName::parse("changed").value();
    document.source.author = core::DisplayText::parse("changed").value();
    const auto view = core::theme_view(copy);
    expect(view.name == "my-theme" && view.source.author == "作者",
           "copied ThemeDocument owns strings independently");
    expect(view.source.license == "MIT" && view.source.url == "local",
           "Theme view borrows all attribution fields");
    expect(view.ui.background == theme.ui.background && view.body.keyword == theme.body.keyword,
           "Theme view preserves color values");
}

core::ThemeName user_name(std::string_view text)
{
    return core::ThemeName::parse(text).value();
}

core::ThemeChoice user_choice(std::string_view name)
{
    const auto &base = core::theme_of(core::BuiltinTheme::neutral_light);
    return core::ThemeChoice::from(
        core::ThemeDocument{user_name(name),
                            base.appearance,
                            base.ui,
                            base.body,
                            {fixed_text("作者"), fixed_text("MIT"), fixed_text("local")}});
}

core::ThemeCatalog user_catalog()
{
    return core::ThemeCatalog::from(
               {{user_name("z-theme"), user_choice("z-theme")},
                {user_name("broken"), std::unexpected(core::ThemeFailure::invalid_color)},
                {user_name("my-theme"), user_choice("my-theme")}})
        .value();
}

void verify_theme_catalog()
{
    auto catalog = user_catalog();
    const auto names = catalog.names();
    expect(names.size() == core::builtin_themes.size() + 3 && names.at(9) == "broken" &&
               names.at(10) == "my-theme" && names.at(11) == "z-theme",
           "user names follow builtins in sorted order");
    const auto choice = catalog.find(user_name("my_theme")).value();
    const auto copy = catalog;
    catalog = core::ThemeCatalog::builtins();
    expect(choice.view().source.author == "作者" && choice.name() == "my-theme",
           "choice keeps owned data after its catalog is replaced");
    expect(copy.find(user_name("my-theme")).value() == choice, "catalog copy shares live choices");
    expect(catalog.records().empty(), "builtin-only catalog has no user records");
    const auto alias = catalog.find(user_name("night_owl_light")).value();
    expect(alias.name() == "night-owl-light", "built-in aliases keep their canonical spelling");
    const auto missing = copy.find(user_name("missing"));
    const auto broken = copy.find(user_name("broken"));
    expect(!missing && missing.error() == core::ThemeLookupFailure{user_name("missing"),
                                                                   core::ThemeFailure::not_found},
           "missing theme names survive lookup failure");
    expect(!broken && broken.error().reason == core::ThemeFailure::invalid_color,
           "broken themes keep their actual load failure");
    expect(!core::ThemeCatalog::from({{user_name("system"), user_choice("system")}}),
           "system is reserved");
    expect(!core::ThemeCatalog::from({{user_name("dracula"), user_choice("dracula")}}),
           "builtin is reserved");
    expect(!core::ThemeCatalog::from({{user_name("a"), user_choice("b")}}),
           "record must match value name");
    expect(!core::ThemeCatalog::from(
               {{user_name("a"), user_choice("a")}, {user_name("a"), user_choice("a")}}),
           "duplicate names are rejected");
}

void verify_user_theme_commands()
{
    const auto catalog = user_catalog();
    const auto settings = core::default_editor_settings();
    const auto selected =
        core::evaluate_ex("colorscheme my_theme", settings, Appearance::dark, catalog).value();
    expect(selected.settings.value_or(settings).theme == user_choice("my-theme"),
           "Ex resolves user aliases canonically");
    const auto resolved = selected.settings.value_or(settings);
    expect(core::selected_theme(resolved, Appearance::dark).appearance == Appearance::light,
           "explicit user appearance wins over system");
    const auto broken =
        core::evaluate_ex("colorscheme broken", settings, Appearance::dark, catalog);
    expect(!broken && core::ex_failure_message(broken.error()).text() ==
                          "broken: Invalid RGB or RGBA color",
           "Ex explains named load failure");
    auto line = core::CommandLine::empty(catalog).inserted("colorscheme my").value();
    line = line.edited(core::CommandEdit::complete_next);
    expect(line.text() == "colorscheme my-theme", "Ex Tab completes user theme");
    line = line.edited(core::CommandEdit::backspace);
    expect(line.completions().front() == "colorscheme my-theme", "editing preserves catalog");
    auto palette = core::CommandPalette::opened(catalog).inserted("my-t").value();
    expect(palette.choices().front().command == "colorscheme my-theme",
           "palette finds the same user theme");
    palette = palette.filled("colorscheme broken").value();
    expect(palette.choices().front().command == "colorscheme broken",
           "failed theme remains actionable after fill");
    const auto system =
        core::evaluate_ex("colorscheme system", selected.settings.value_or(settings),
                          Appearance::dark, catalog)
            .value();
    expect(system.settings.has_value() && !system.settings.value().theme.has_value(),
           "system clears explicit user choice");
}

void verify_user_theme_controller()
{
    Editing editor{std::nullopt, user_catalog()};
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    const auto before = controller.frame();
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"my-t"}));
    auto frame = controller.apply(app::SubmitCommand{});
    expect(frame.settings.theme == user_choice("my-theme") && frame.appearance == Appearance::light,
           "controller applies user theme and appearance");
    expect(editor.settings().writes() == 1 &&
               editor.settings().written().value_or(core::default_editor_settings()).theme ==
                   user_choice("my-theme"),
           "controller persists the resolved choice once");
    expect(frame.lines.front().text == "body" && frame.caret == before.caret,
           "theme leaves body and caret intact");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"broken"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
                   "broken: Invalid RGB or RGBA color" &&
               editor.settings().writes() == 1,
           "failed selection reports reason without writing settings");
    expect(frame.settings.theme == user_choice("my-theme"),
           "failed selection keeps previous appearance");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "",
           "theme commands do not enter document history");
}

template <typename T>
concept CanBorrowName = requires(T &&value) { std::forward<T>(value).name(); };
template <typename T>
concept CanBorrowRecords = requires(T &&value) { std::forward<T>(value).records(); };
template <typename T>
concept CanSelectTheme =
    requires(T &&value) { core::selected_theme(std::forward<T>(value), Appearance::dark); };
static_assert(CanBorrowName<const core::ThemeChoice &> && !CanBorrowName<core::ThemeChoice>);
static_assert(CanBorrowRecords<const core::ThemeCatalog &> &&
              !CanBorrowRecords<core::ThemeCatalog>);
static_assert(CanSelectTheme<const core::EditorSettings &> &&
              !CanSelectTheme<core::EditorSettings>);

void verify_theme_startup_notice()
{
    ScriptedAppearance appearance{Reading{Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedFiles files;
    files.hold(std::string("initial body"));
    ScriptedCodePages pages;
    ScriptedSettings settings;
    ScriptedThemes themes{user_catalog(), fixed_text("invalid_name.v1.theme: invalid name")};
    const auto initial = app::OpenDocument{FilePath::parse("initial.txt").value()};
    EditorController controller{
        app::EditorPorts{appearance, clipboard, files, pages, settings, themes}, initial};
    auto frame = controller.frame();
    expect(frame.lines.front().text == "initial body", "initial document uses normal file load");
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
               "invalid_name.v1.theme: invalid name",
           "startup diagnostic survives initial file opening");
    frame = controller.apply(VisibleLines{8});
    expect(frame.command_message.has_value(), "first layout retains startup diagnostic");
    frame = controller.apply(InsertText{"x"});
    expect(!frame.command_message.has_value() && themes.reads() == 1,
           "user input clears notice without rereading themes");
    ScriptedSettings broken{SettingsReading{std::unexpect, app::SettingsFailure::malformed}};
    const EditorController unreadable{
        app::EditorPorts{appearance, clipboard, files, pages, broken, themes}, initial};
    expect(unreadable.frame().lines.front().text == "initial body" &&
               unreadable.frame().settings_failure == app::SettingsFailure::malformed,
           "failed settings do not prevent opening initial document");
}

template <typename T>
concept CanBorrowCatalog = requires(T &&value) { std::forward<T>(value).catalog(); };
static_assert(CanBorrowCatalog<const core::CommandLine &> && !CanBorrowCatalog<core::CommandLine>);

void verify_blocked_theme_noop()
{
    const app::SettingsIssue failure =
        core::ThemeLookupFailure{user_name("missing"), core::ThemeFailure::not_found};
    Editing editor{SettingsReading{std::unexpect, failure}};
    editor.settings().fail(failure);
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"colorscheme system"}));
    const auto frame = controller.apply(app::SubmitCommand{});
    expect(frame.settings_failure == failure,
           "same-setting command cannot clear blocked startup error");
    expect(frame.command_message.value_or(fixed_text("none")).text() ==
               "Settings could not be saved",
           "blocked no-op must not report a successful setting change");
    expect(editor.settings().writes() == 1 && !editor.settings().written().has_value(),
           "same value still consults the blocked settings port");
}

void verify_user_theme_selection()
{
    verify_theme_catalog();
    verify_user_theme_commands();
    verify_user_theme_controller();
    verify_theme_startup_notice();
    verify_blocked_theme_noop();
}

void verify_palette_choices()
{
    const auto all = choices_for(":");
    expect(all.size() == core::ex_command_candidates().size(), "Ex and palette share the catalog");
    expect(all.front().command == "colorscheme", "empty query has deterministic ranking");
    for (const auto &theme : core::builtin_themes)
    {
        const auto matched = choices_for(theme.name);
        expect(matched.front().command == "colorscheme " + std::string(theme.name),
               "every built-in theme is reachable");
    }
    expect(choices_for(":ntrl-l").front().command == "colorscheme neutral-light",
           "subsequence finds a theme");
    expect(choices_for("DRAC").front().command == "colorscheme dracula",
           "ASCII case insensitive search");
    expect(choices_for("colorscheme drac").front().command == "colorscheme dracula",
           "command prefix still permits partial theme names");
    expect(choices_for("colorscheme missing").front().command == "colorscheme missing",
           "unknown explicit theme goes to the shared Ex evaluator");
    expect(choices_for("fz").front().command == "set fontsize=", "short option search");
    expect(choices_for("?").empty() && choices_for("界").empty(),
           "unknown queries have no candidate");
    expect(choices_for("set fontsize=").front().kind == core::CommandChoiceKind::fill,
           "empty size stages input");
    expect(choices_for(":set guifont=").front().kind == core::CommandChoiceKind::fill,
           "empty font stages input");
    expect(choices_for("set guifont=ＭＳ ゴシック:h18").front().command ==
               "set guifont=ＭＳ ゴシック:h18",
           "literal font retains UTF-8 and spaces");
    expect(choices_for("set fontsize=90").front().kind == core::CommandChoiceKind::execute,
           "the Ex evaluator decides validity");
}

void verify_palette_editing()
{
    using core::CommandEdit;
    auto palette = core::CommandPalette::opened();
    expect(palette.input().text() == ":" && palette.selected() == 0,
           "palette opens in command mode");
    const auto count = palette.choices().size();
    palette = palette.edited(CommandEdit::complete_previous);
    expect(palette.selected() == count - 1, "previous wraps backwards");
    palette = palette.edited(CommandEdit::complete_next);
    expect(palette.selected() == 0 && palette.input().text() == ":",
           "next wraps without changing query");
    palette = palette.selected_at(4).inserted("drac").value();
    expect(palette.selected() == 0 && palette.choices().size() == 1,
           "filter resets the selected row");
    expect(palette.selected_at(50).selected() == 0, "invalid row leaves selection intact");
    palette = palette.filled("set fontsize=").value();
    expect(palette.input().text() == ":set fontsize=", "fill retains command prefix");
    expect(!palette.filled(std::string(256, 'x')),
           "fill respects shared input limit including prefix");
    palette = palette.filled("界😀").value().edited(CommandEdit::backspace);
    expect(palette.input().text() == ":界", "palette reuses Unicode code point editing");
    expect(!palette.inserted("\n") && !palette.inserted("\xFF"),
           "palette rejects invalid one-line text");
    palette = palette.edited(CommandEdit::complete_next);
    expect(palette.selected() == 0 && palette.choices().empty(),
           "no results can be navigated safely");
    palette =
        palette.edited(CommandEdit::home).edited(CommandEdit::erase).edited(CommandEdit::erase);
    expect(palette.input().text().empty(), "home and delete use shared editing");
    expect(palette.edited(CommandEdit::backspace).input().text().empty(),
           "empty input can stay open");
}

void verify_palette_geometry()
{
    for (const auto dpi : {96U, 120U, 192U})
    {
        const auto layout = core::palette_layout(1600, 1200, dpi, 13);
        expect(core::width_of(layout.panel) == core::to_pixels(640, dpi),
               "panel uses approved DIP width");
        expect(layout.visible_rows == 8 && layout.row_height == core::to_pixels(40, dpi),
               "row height and count bounded");
        expect(core::palette_first_visible(layout, 12) == 5, "selected last row stays visible");
        for (std::size_t row = 0; row < layout.visible_rows; ++row)
        {
            const auto box = core::palette_row(layout, row);
            expect(core::palette_hit(layout, box.left + 1, box.top + 1) == row,
                   "drawing and clicking share rows");
        }
        expect(!core::palette_hit(layout, layout.input.left, layout.input.top),
               "query is not a candidate");
        const auto narrow = core::palette_layout(350, 240, dpi, 13);
        expect(narrow.panel.left >= 0 && narrow.panel.right <= 350 && narrow.panel.bottom <= 240,
               "narrow panel fits client");
    }
    for (const auto extent : {0, 10, 50})
    {
        const auto tiny = core::palette_layout(extent, extent, 120, 13);
        expect(tiny.visible_rows == 0 && core::width_of(tiny.input) >= 0 &&
                   tiny.panel.bottom <= extent,
               "tiny layout stays nonnegative and bounded");
        expect(!core::palette_hit(tiny, 0, 0), "tiny panel has no clickable row");
    }
    expect(core::palette_layout(1000, 800, 96, 0).visible_rows == 1,
           "no results reserves a hint row");
}

void verify_palette_controller()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    const auto before = controller.apply(app::SelectAll{});
    auto frame = controller.apply(app::OpenCommandPalette{});
    expect(frame.command_palette.has_value() && frame.command_line.has_value(),
           "ordinary mode opens shared input");
    expect(frame.caret == before.caret &&
               frame.lines.front().selection == before.lines.front().selection,
           "opening keeps body selection and caret");
    static_cast<void>(controller.apply(app::CommandText{"fz"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_line.value_or(core::InputLineView{}).text == ":set fontsize=",
           "Enter on a fill candidate stages the value");
    expect(frame.command_palette.has_value() && editor.settings().writes() == 0,
           "fill does not save");
    static_cast<void>(controller.apply(app::CommandText{"18"}));
    frame = controller.apply(app::ActivateCommandChoice{0});
    expect(!frame.command_line.has_value() && frame.settings.font_size.points() == 18 &&
               editor.settings().writes() == 1,
           "click activation shares Ex evaluation and persistence");
    expect(frame.lines.front().selection == before.lines.front().selection &&
               frame.lines.front().text == "body",
           "execution preserves body and selection");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"set fontsize=90"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_message.has_value() && editor.settings().writes() == 1,
           "invalid value uses Ex failure and does not save");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"drac"}));
    editor.settings().fail(app::SettingsFailure::unwritable);
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_message.has_value() && !frame.settings.theme.has_value(),
           "failed save keeps the previous theme");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}).empty(),
           "palette work adds no body undo entries");
}

void verify_palette_unknown_theme()
{
    Editing editor;
    auto &controller = editor.controller();
    for (const auto query : {"colorscheme missing", "colorscheme systemx", "colorscheme dracula|q"})
    {
        static_cast<void>(controller.apply(app::OpenCommandPalette{}));
        static_cast<void>(controller.apply(app::CommandText{query}));
        const auto frame = controller.apply(app::SubmitCommand{});
        const auto expected = core::evaluate_ex(query, frame.settings, frame.appearance);
        expect(!frame.command_palette.has_value() && frame.command_message.has_value(),
               "invalid explicit theme closes with an error");
        if (!expected && frame.command_message.has_value())
        {
            expect(frame.command_message.value().text() ==
                       core::ex_failure_message(expected.error()).text(),
                   "palette reports the same Ex error");
        }
    }
    expect(editor.settings().writes() == 0, "unknown themes and pipes do not save");
}

void verify_palette_input_isolation()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    auto frame = controller.apply(app::OpenCommandPalette{});
    expect(!frame.command_palette.has_value() && frame.composition.has_value(),
           "active composition prevents opening");
    static_cast<void>(controller.apply(CancelComposition{}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 0)}));
    frame = controller.apply(CommitText{"界"});
    expect(!frame.composition.has_value() && frame.lines.front().text == "body",
           "late IME events cannot edit the body");
    editor.clipboard().hold(std::string("set guifont=MS Gothic:h17"));
    frame = controller.apply(app::PasteCommand{});
    expect(frame.command_line.value_or(core::InputLineView{}).text == ":set guifont=MS Gothic:h17",
           "paste targets the palette query");
    editor.clipboard().hold(std::string("\nbody leak"));
    frame = controller.apply(app::PasteCommand{});
    expect(frame.command_message.has_value(),
           "invalid paste reports inline without replacing input");
    frame = controller.apply(app::OpenCommandPalette{});
    expect(!frame.command_palette.has_value() && editor.settings().writes() == 0,
           "Ctrl+P again cancels");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
    frame = controller.apply(app::EditCommand{core::CommandEdit::backspace});
    expect(frame.command_palette.has_value(), "backspace in an empty palette keeps it open");
    static_cast<void>(controller.apply(app::CommandText{"?"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_palette.has_value() && editor.settings().writes() == 0,
           "Enter with no candidates is harmless");
    frame = controller.apply(app::ActivateCommandChoice{90});
    expect(frame.command_palette.has_value(), "stale row activation is ignored");
}

void verify_palette_vim_modes()
{
    for (const auto entry : {U' ', U'i', U'v', U'V', U'd', U'2'})
    {
        Editing editor;
        auto &controller = editor.controller();
        static_cast<void>(controller.apply(InsertText{"body"}));
        static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
        static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{entry}}));
        const auto before = controller.frame();
        const auto vim = controller.vim_state();
        static_cast<void>(controller.apply(app::OpenCommandPalette{}));
        auto frame = controller.apply(app::VimKeyPress{core::VimCharacter{U'x'}});
        expect(frame.lines.front().text == "body" && frame.vim_mode == before.vim_mode,
               "palette blocks body Vim commands without resetting mode");
        static_cast<void>(controller.apply(app::CommandText{"drac"}));
        frame = controller.apply(app::SubmitCommand{});
        expect(frame.settings.theme == core::ThemeChoice::from(core::BuiltinTheme::dracula),
               "theme executes in every Vim state");
        expect(frame.caret == before.caret &&
                   frame.lines.front().selection == before.lines.front().selection,
               "Vim caret and selection survive palette execution");
        const auto after = controller.vim_state();
        expect(after.count == vim.count && after.pending.has_value() == vim.pending.has_value(),
               "counts and pending presence survive");
        if (vim.pending.has_value() && after.pending.has_value())
        {
            expect(after.pending.value().operation == vim.pending.value().operation &&
                       after.pending.value().count == vim.pending.value().count,
                   "pending operation and count survive");
        }
    }
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply(app::VimKeyPress{core::VimCharacter{U':'}}));
    auto frame = controller.apply(app::OpenCommandPalette{});
    expect(frame.command_palette.has_value(), "palette replaces an Ex input session");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    frame = controller.apply(app::VimKeyPress{core::VimCharacter{U':'}});
    expect(frame.command_line.has_value() && !frame.command_palette.has_value(),
           "Ex still opens independently after palette cancellation");
}

void verify_command_palette()
{
    verify_palette_choices();
    verify_palette_editing();
    verify_palette_geometry();
    verify_palette_controller();
    verify_palette_unknown_theme();
    verify_palette_input_isolation();
    verify_palette_vim_modes();
}

void verify_controller_intents()
{
    verify_font_geometry();
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

// ---------------------------------------------------------------- `.`（Issue #87 / ADR 0030）

void open_vim_document(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), VisibleLines{vim_visible_lines});
    applied(editing.controller(), OpenDocument{sample_path()});
    applied(editing.controller(), SelectEditMode{EditMode::vim});
}

// 記録は回数 1 つと、回数の桁を除いた鍵の列（決定 1・2）。表示値に出ないのでここで直接見る。
[[nodiscard]] bool dot_record_is(const VimState &state,
                                 const std::optional<nenenib::core::VimCount> &count,
                                 std::string_view keys)
{
    if (!state.last_change.has_value())
    {
        return false;
    }
    const auto &record = state.last_change.value();
    return record.count == count && record.keys == vim_keys_of(keys);
}

// 直前の変更が「VISUAL の大きさを持つ記録」かどうか（持っていなければ NORMAL の記録か空）。
[[nodiscard]] bool dot_record_has_extent(const VimState &state)
{
    return state.last_change.has_value() && state.last_change.value().visual.has_value();
}

void verify_vim_dot_recording()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma\nsecond line");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x") &&
               !controller.vim_state().recording.has_value(),
           "a delete is confirmed as the last change and leaves no recording behind");
    vim_replay(controller, "wyyu");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "motion, yank and undo throw their own keys away and keep the last change");
    vim_replay(controller, "d");
    expect(controller.vim_state().recording.has_value(),
           "a pending operator is still being recorded");
    vim_replay(controller, "<Esc>");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a cancelled operator drops its keys and keeps the previous change");
    vim_replay(controller, "2d3w");
    expect(dot_record_is(controller.vim_state(), nenenib::core::VimCount{6}, "dw"),
           "the operator and motion counts fold into one and no digit is recorded");
    vim_replay(controller, ".");
    expect(dot_record_is(controller.vim_state(), nenenib::core::VimCount{6}, "dw"),
           "the replay records the command again and never records the dot itself");
}

void verify_vim_dot_counts()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x3.");
    expect(vim_body(controller.frame()) == "efghij" &&
               dot_record_is(controller.vim_state(), nenenib::core::VimCount{3}, "x"),
           "a counted dot replaces the count of the recorded change and keeps the new one");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "hij",
           "the next dot repeats with the count that was given");
    vim_replay(controller, "2.");
    expect(vim_body(controller.frame()) == "j" &&
               dot_record_is(controller.vim_state(), nenenib::core::VimCount{2}, "x"),
           "and a smaller count replaces it again");
}

void verify_vim_dot_history()
{
    Editing editing;
    open_vim_document(editing, "alpha beta\nsecond line");
    EditorController &controller = editing.controller();
    vim_replay(controller, "3ofoo<Esc>");
    const std::string opened = vim_body(controller.frame());
    const std::string repeated = "alpha beta\nfoo\nfoo\nfoo\nfoo\nfoo\nfoo\nsecond line";
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == repeated,
           "the dot repeats a counted open-line command through the same effects");
    vim_replay(controller, "u");
    expect(vim_body(controller.frame()) == opened, "one dot is one undo unit");
    vim_replay(controller, "<C-r>");
    expect(vim_body(controller.frame()) == repeated, "and redo puts the whole replay back");
    vim_replay(controller, "uu");
    expect(vim_body(controller.frame()) == "alpha beta\nsecond line",
           "the command before the dot is a unit of its own");
}

void verify_vim_dot_document()
{
    Editing editing;
    open_vim_document(editing, "ab\r\ncd");
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    vim_replay(editing.controller(), "ofoo<Esc>.");
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nfoo\r\nfoo\r\ncd",
           "the replayed open-line writes the document's own CRLF bytes");

    Editing narrow;
    open_vim_document(narrow, "one\ntwo\nthree\nfour");
    applied(narrow.controller(), VisibleLines{1});
    vim_replay(narrow.controller(), "dd.");
    expect(narrow.controller().frame().total_lines == 2,
           "the replay passes no window: a one-line viewport repeats the same delete");
}

void verify_vim_dot_modes()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "xjvlld");
    expect(dot_record_has_extent(controller.vim_state()),
           "a change finished in VISUAL replaces the last change (ADR 0033 decision 3)");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "bcdefghij\nqrst",
           "and the dot after it repeats the VISUAL delete by its extent");

    Editing typing;
    open_vim_document(typing, "abc");
    vim_replay(typing.controller(), "xv<Esc>");
    expect(dot_record_is(typing.controller().vim_state(), std::nullopt, "x"),
           "entering and leaving VISUAL keeps the previous change");
    vim_replay(typing.controller(), "i.<Esc>");
    expect(vim_body(typing.controller().frame()) == ".bc" &&
               dot_record_is(typing.controller().vim_state(), std::nullopt, "i.<Esc>"),
           "in INSERT the dot is an ordinary character and the insert is what repeats");
}

// ---------------------------------------------------------- VISUAL の `.`（Issue #91 / ADR 0033）

// 記録の中身は表示値に出ないので直接見る。VISUAL の記録は回数を持たない（決定 3・実測）。
[[nodiscard]] bool dot_visual_is(const VimState &state,
                                 const nenenib::core::VimVisualExtent &extent,
                                 std::string_view keys)
{
    if (!state.last_change.has_value())
    {
        return false;
    }
    const auto &record = state.last_change.value();
    return record.visual.has_value() && record.visual.value() == extent &&
           record.keys == vim_keys_of(keys) && !record.count.has_value();
}

[[nodiscard]] nenenib::core::VimVisualExtent characters_extent(std::size_t lines,
                                                               std::size_t column)
{
    return nenenib::core::VimCharacterExtent{lines, nenenib::core::VimColumnWish::at_column,
                                             VirtualColumn{column}};
}

// 何を記録するか（決定 3）。変更を起こす鍵から記録が始まり、そのときの選択の大きさが載る。
void verify_vim_visual_dot_records()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst\nuvwxyz0123\nABCDEFGHIJ");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlld");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "a VISUAL delete records the width of the selection and the key that finished it");
    vim_replay(controller, "vlly");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "a VISUAL yank changes nothing and keeps the last change");
    vim_replay(controller, "vlloh<Esc>");
    expect(dot_visual_is(controller.vim_state(), characters_extent(1, 3), "d"),
           "moving, swapping the ends and leaving VISUAL record no keys at all");
    vim_replay(controller, "vjld");
    expect(dot_visual_is(controller.vim_state(), characters_extent(2, 2), "d"),
           "several lines record the line count and the last line's own column");
    vim_replay(controller, "v$d");
    expect(dot_visual_is(controller.vim_state(),
                         nenenib::core::VimCharacterExtent{
                             1, nenenib::core::VimColumnWish::at_line_end, VirtualColumn{1}},
                         "d"),
           "a selection made with $ stays to the end of the line instead of freezing a column");
    vim_replay(controller, "Vjd");
    expect(dot_visual_is(controller.vim_state(), nenenib::core::VimLineExtent{2}, "d"),
           "linewise VISUAL records only the number of lines");

    Editing waiting;
    open_vim_document(waiting, "abcd\nefgh\nijkl\nmnop");
    vim_replay(waiting.controller(), "vjlrZ");
    expect(dot_visual_is(waiting.controller().vim_state(), characters_extent(2, 2), "rZ"),
           "r keeps recording until the character it waits for arrives");
    vim_replay(waiting.controller(), "jvlcZZ<Esc>");
    expect(dot_visual_is(waiting.controller().vim_state(), characters_extent(1, 2), "cZZ<Esc>"),
           "c keeps recording the inserted keys until Esc");
    vim_replay(waiting.controller(), "0vlcZ<Home>Y<Esc>");
    expect(!dot_record_has_extent(waiting.controller().vim_state()) &&
               dot_record_is(waiting.controller().vim_state(), std::nullopt, "iY<Esc>"),
           "a move inside the insert retakes the record from i and drops the extent (measured)");
}

// 選び直しと再生（決定 4・5）。期待値は out/issue91-oracle/probe*.txt の固定 Vim 9.1 と同じ。
void verify_vim_visual_dot_replays()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij");
    vim_replay(editing.controller(), "vlld.");
    expect(vim_body(editing.controller().frame()) == "ghij",
           "the dot reselects the same width at the caret and deletes it again");
    vim_replay(editing.controller(), ".");
    expect(vim_body(editing.controller().frame()) == "j", "and the next dot keeps repeating");

    Editing lines;
    open_vim_document(lines, "alpha beta\nsecond line\nthird line\nfourth line");
    vim_replay(lines.controller(), "Vdj.");
    expect(vim_body(lines.controller().frame()) == "second line\nfourth line",
           "linewise repeats the same number of lines at the caret's line");

    Editing across;
    open_vim_document(across, "abcdefghij\nklmnopqrst\nuvwxyz0123\nABCDEFGHIJ");
    vim_replay(across.controller(), "vjldj0.");
    expect(vim_body(across.controller().frame()) == "mnopqrst\nCDEFGHIJ",
           "several lines end at the recorded absolute column of the last line");

    Editing replacing;
    open_vim_document(replacing, "abcdefghij\nklmnopqrst");
    vim_replay(replacing.controller(), "vllrZj0.");
    expect(vim_body(replacing.controller().frame()) == "ZZZdefghij\nZZZnopqrst",
           "the replayed r replaces the same width");

    Editing changing;
    open_vim_document(changing, "abcdefghij\nklmnopqrst");
    vim_replay(changing.controller(), "vllcZZ<Esc>j0.");
    expect(vim_body(changing.controller().frame()) == "ZZdefghij\nZZnopqrst",
           "the replayed c changes the same width and inserts the same text");

    Editing ending;
    open_vim_document(ending, "abc\nklmnopqrst\nuvwxyz0123");
    vim_replay(ending.controller(), "v$d0.");
    expect(vim_body(ending.controller().frame()) == "uvwxyz0123",
           "a $ record takes the whole of a longer line, not the recorded number of columns");
}

// 回数は VISUAL の記録では使わない（決定 6・実測）。大きさが範囲を決める。
void verify_vim_visual_dot_counts()
{
    Editing editing;
    open_vim_document(editing, "abcdefghijklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlld2.");
    expect(vim_body(controller.frame()) == "ghijklmnopqrst",
           "a counted dot repeats the recorded extent once, not N times");
    vim_replay(controller, ".");
    expect(vim_body(controller.frame()) == "jklmnopqrst",
           "and the count is not carried into the next dot either");

    Editing lines;
    open_vim_document(lines, "one\ntwo\nthree\nfour\nfive\nsix\nseven");
    vim_replay(lines.controller(), "Vd2.");
    expect(vim_body(lines.controller().frame()) == "three\nfour\nfive\nsix\nseven",
           "a counted dot on a linewise record still repeats one line");
}

// 範囲が足りないとき（決定 5）と、畳まれても記録は縮まないこと（決定 4・実測）。
void verify_vim_visual_dot_limits()
{
    Editing columns;
    open_vim_document(columns, "abcdefghij\nkl\nmnopqrstuv");
    vim_replay(columns.controller(), "vlldj0.");
    expect(vim_body(columns.controller().frame()) == "defghij\nmnopqrstuv",
           "a line too short for the extent reaches its content end and takes the line break");
    vim_replay(columns.controller(), "j0.");
    expect(vim_body(columns.controller().frame()) == "defghij\npqrstuv",
           "and the clamped replay does not shrink the record for the next dot");

    Editing last;
    open_vim_document(last, "abcdefghij\nkl");
    vim_replay(last.controller(), "vlldj0.");
    expect(vim_body(last.controller().frame()) == "defghij\n",
           "on the last line there is no line break to take");

    Editing document;
    open_vim_document(document, "abcdefghij");
    vim_replay(document.controller(), "vlld$.");
    expect(vim_body(document.controller().frame()) == "defghi",
           "at the end of the document the extent stops at the last character");
    vim_replay(document.controller(), "0.");
    expect(vim_body(document.controller().frame()) == "ghi",
           "and the record still holds the original three columns");

    Editing rows;
    open_vim_document(rows, "abcdefghij\nklmnopqrst\nuvwxyz0123");
    vim_replay(rows.controller(), "vjdG0.");
    expect(vim_body(rows.controller().frame()) == "lmnopqrst\nvwxyz0123",
           "too few lines left folds the extent onto the last line");

    Editing backwards;
    open_vim_document(backwards, "abcdefghij\nklmnopqrst\nuvwxyz0123");
    vim_replay(backwards.controller(), "vjldG4l.");
    expect(vim_body(backwards.controller().frame()) == "mnopqrst\nuz0123",
           "a recorded column to the left of the caret reselects backwards");
}

// NORMAL と VISUAL の記録は入れ替わる（決定 2・3）。undo は `.` 1 回で 1 単位（決定 7）。
void verify_vim_visual_dot_boundaries()
{
    Editing editing;
    open_vim_document(editing, "abcdefghij\nklmnopqrst");
    EditorController &controller = editing.controller();
    vim_replay(controller, "vlldjx.");
    expect(vim_body(controller.frame()) == "defghij\nmnopqrst",
           "a NORMAL change after a VISUAL one takes the record back");

    Editing inside;
    open_vim_document(inside, "abcdefghij\nklmnopqrst");
    vim_replay(inside.controller(), "xjvll.");
    expect(inside.controller().vim_state().mode == nenenib::core::VimMode::visual,
           "the dot does nothing inside VISUAL and does not leave it");
    vim_replay(inside.controller(), "d");
    expect(vim_body(inside.controller().frame()) == "bcdefghij\nnopqrst",
           "so the selection it did not touch is what the operator uses");

    Editing history;
    open_vim_document(history, "abcdefghij");
    vim_replay(history.controller(), "vlld.");
    vim_replay(history.controller(), "u");
    expect(vim_body(history.controller().frame()) == "defghij",
           "one replayed dot is one undo unit");
    vim_replay(history.controller(), "<C-r>");
    expect(vim_body(history.controller().frame()) == "ghij", "and redo puts that unit back");
    vim_replay(history.controller(), "uu");
    expect(vim_body(history.controller().frame()) == "abcdefghij",
           "the VISUAL change before it is a unit of its own");

    Editing crlf;
    open_vim_document(crlf, "abcdefghij\r\nklmnopqrst");
    applied(crlf.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    vim_replay(crlf.controller(), "vlldj0.");
    applied(crlf.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    expect(crlf.files().written() == "defghij\r\nnopqrst",
           "the replayed VISUAL delete keeps the document's own CRLF bytes");
}

// 桁は仮想桁（表示幅）で数える（ADR 0034 の決定 4）。ADR 0033 が残した Tab と幅の混在の穴は
// ここで閉じた。同じ 3 つは virtcol- の fixture にも採ってあり、これは engine 側の説明である。
void verify_vim_visual_dot_columns()
{
    Editing tabs;
    open_vim_document(tabs, "ab\tcd\nxyzwvutsrq");
    vim_replay(tabs.controller(), "vlldj0.");
    expect(vim_body(tabs.controller().frame()) == "cd\nrq",
           "a Tab reaches the next tabstop, so the record covers eight columns");

    Editing kana;
    open_vim_document(kana, "あいうえおかきくけこ\nさしすせそたちつてと");
    vim_replay(kana.controller(), "vlldj0.");
    expect(vim_body(kana.controller().frame()) == "えおかきくけこ\nせそたちつてと",
           "uniform double-width text agrees with Vim because every column is two cells wide");

    Editing mixed;
    open_vim_document(mixed, "abcdefghij\nあいうえおかきくけこ");
    vim_replay(mixed.controller(), "vlldj0.");
    expect(vim_body(mixed.controller().frame()) == "defghij\nうえおかきくけこ",
           "three columns of ASCII reach into the second double-width character");
}

// 取消になった命令は自分の鍵を捨て、直前の変更を変えない（ADR 0030 の決定 3）。固定 Vim では
// ビープが `:normal!` の残りの鍵を捨てるので fixture では測れない。実測は
// out/issue87-oracle/probe4.py が鍵を区切って行い、ここでは同じ規則を直接確かめる。
void verify_vim_dot_cancel(std::string_view before, std::string_view cancelled,
                           std::string_view after)
{
    Editing editing;
    open_vim_document(editing, "ab\ncd\nef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "x");
    vim_replay(controller, before);
    vim_replay(controller, cancelled);
    vim_replay(controller, after);
    vim_replay(controller, ".");
    const std::string note = std::string(cancelled) + " is cancelled and keeps the last change";
    expect(vim_body(controller.frame()) == "b\nd\nef" &&
               dot_record_is(controller.vim_state(), std::nullopt, "x"),
           note.c_str());
}

void verify_vim_dot_cancels()
{
    for (const std::string_view cancelled :
         {"dk", "yk", "3rz", "dfz", "d;", "dq", "dgz", "d<Esc>", "r<Esc>"})
    {
        verify_vim_dot_cancel("", cancelled, "j");
    }
    // 最終行でだけ範囲が作れない命令。先に G で降りてから k で戻る。
    for (const std::string_view cancelled : {"2dd", "2D", "dj"})
    {
        verify_vim_dot_cancel("G", cancelled, "k");
    }
}

void verify_vim_dot_fixtures()
{
    constexpr std::array<std::string_view, 7> boundaries{
        "open-line-count-below", "open-line-count-above", "replace-char-one",  "replace-char-count",
        "char-search-f-count",   "char-search-t-first",   "line-jump-gg-count"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("dot-") || fixture.name.starts_with("visual-dot-") ||
            fixture.name.starts_with("counted-insert-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 181,
           "the scope replays 96 dot and 78 VISUAL dot fixtures and 7 shared boundaries");
}

void verify_vim_dot_contracts()
{
    verify_vim_dot_cancels();
    verify_vim_dot_recording();
    verify_vim_dot_counts();
    verify_vim_dot_history();
    verify_vim_dot_document();
    verify_vim_dot_modes();
    verify_vim_visual_dot_records();
    verify_vim_visual_dot_replays();
    verify_vim_visual_dot_counts();
    verify_vim_visual_dot_limits();
    verify_vim_visual_dot_boundaries();
    verify_vim_visual_dot_columns();
}

void verify_vim_dot_scope()
{
    verify_vim_dot_fixtures();
    verify_vim_dot_contracts();
}

void verify_vim_line_jump_recovery()
{
    verify_vim_line_jump_continuations();
    verify_vim_line_jump_operator_undo();
}

// ---------------------------------------------------------------- テキストオブジェクト（ADR 0031）

[[nodiscard]] bool waits_for_object(const VimState &state,
                                    nenenib::core::VimTextObjectScope scope) noexcept
{
    return state.input_wait.has_value() &&
           std::holds_alternative<nenenib::core::VimTextObjectScope>(state.input_wait.value()) &&
           std::get<nenenib::core::VimTextObjectScope>(state.input_wait.value()) == scope;
}

void verify_vim_text_object_waiting()
{
    const auto buffer = TextBuffer::from_utf8("alpha beta gamma").value();
    const VimEditorView view{buffer, collapsed_at(Offset{6}), VimViewport{LineNumber{1}, 8}};
    VimState initial = empty_vim_state();
    initial.unnamed_register = {"saved", VimRegisterKind::characters};
    const auto pending = vim_step(vim_step(initial, view, VimKey{VimCharacter{U'2'}}).next, view,
                                  VimKey{VimCharacter{U'd'}});
    const auto inner = vim_step(pending.next, view, VimKey{VimCharacter{U'i'}});
    const auto kept = inner.next.pending.has_value() ? inner.next.pending.value().count
                                                     : std::optional<nenenib::core::VimCount>{};
    expect(waits_for_object(inner.next, nenenib::core::VimTextObjectScope::inner) &&
               kept == nenenib::core::VimCount{2} &&
               std::holds_alternative<nenenib::core::VimNoEffect>(inner.effect),
           "an operator's i waits for one object key and keeps the operator count");
    const auto around = vim_step(pending.next, view, VimKey{VimCharacter{U'a'}});
    expect(waits_for_object(around.next, nenenib::core::VimTextObjectScope::around),
           "an operator's a waits for the same key with the around scope");
    const auto inserted = vim_step(initial, view, VimKey{VimCharacter{U'i'}});
    expect(inserted.next.mode == VimMode::insert && !inserted.next.input_wait.has_value(),
           "i without a pending operator is still the insert command");
    const auto completed = vim_step(inner.next, view, VimKey{VimCharacter{U'w'}});
    expect(std::get<nenenib::core::VimRemoveRange>(completed.effect).range ==
               OffsetRange{Offset{6}, Offset{11}},
           "d2iw covers the word and the blanks after it");
    expect(completed.next.mode == VimMode::normal && !completed.next.input_wait.has_value() &&
               !completed.next.pending.has_value() && !completed.next.count.has_value(),
           "a finished object leaves no wait, operator or count behind");
}

void verify_vim_text_object_cancellation()
{
    const auto buffer = TextBuffer::from_utf8("alpha beta gamma").value();
    const Selection selected{Offset{2}, Offset{4}};
    const VimEditorView view{buffer, selected, VimViewport{LineNumber{1}, 8}};
    VimState initial = empty_vim_state();
    initial.mode = VimMode::visual;
    initial.wanted_column =
        nenenib::core::VimWantedColumn{VimColumnWish::at_line_end, VirtualColumn{1}};
    initial.last_character_search =
        nenenib::core::VimCharacterSearch{VimCharacterSearchKind::find_forward, U'b'};
    const auto waiting = vim_step(initial, view, VimKey{VimCharacter{U'i'}});
    expect(waits_for_object(waiting.next, nenenib::core::VimTextObjectScope::inner) &&
               waiting.next.mode == VimMode::visual,
           "VISUAL i is the object prefix, not an insert command");
    for (const VimKey &key : {VimKey{VimCharacter{U'x'}}, VimKey{VimCharacter{U'3'}},
                              VimKey{VimSpecialKey::escape}, VimKey{VimSpecialKey::arrow_left}})
    {
        const auto cancelled = vim_step(waiting.next, view, key);
        expect(cancelled.next.mode == VimMode::visual && !cancelled.next.input_wait.has_value() &&
                   cancelled.next.wanted_column == initial.wanted_column &&
                   last_search_is(cancelled.next, VimCharacterSearchKind::find_forward, U'b') &&
                   std::holds_alternative<nenenib::core::VimNoEffect>(cancelled.effect),
               "an unknown object key or Esc keeps the mode, selection, wish and search memory");
    }
    VimState operating = empty_vim_state();
    operating.pending =
        nenenib::core::VimPendingOperator{nenenib::core::VimOperator::remove, std::nullopt};
    const auto pending_wait = vim_step(operating, view, VimKey{VimCharacter{U'a'}});
    const auto dropped = vim_step(pending_wait.next, view, VimKey{VimSpecialKey::escape});
    expect(dropped.next.mode == VimMode::normal && !dropped.next.pending.has_value() &&
               !dropped.next.input_wait.has_value(),
           "cancelling an object drops the pending operator without touching the text");
}

void verify_vim_text_object_visual()
{
    Editing editing;
    open_vim_document(editing, "start(\n  body one\n  body two\n) end");
    EditorController &controller = editing.controller();
    vim_replay(controller, "jvi(");
    expect(controller.vim_state().mode == VimMode::visual,
           "a linewise inner block keeps VISUAL characterwise (measured against Vim 9.1)");
    vim_replay(controller, "y");
    expect(controller.vim_state().unnamed_register.text == "  body one\n  body two\n" &&
               controller.vim_state().unnamed_register.kind == VimRegisterKind::characters,
           "the VISUAL selection reaches through the last line break");
    vim_replay(controller, "Vi(");
    expect(controller.vim_state().mode == VimMode::visual,
           "an object switches linewise VISUAL to characterwise");
    vim_replay(controller, "<Esc>");
    Editing words;
    open_vim_document(words, "alpha beta gamma");
    EditorController &other = words.controller();
    vim_replay(other, "2lviw");
    expect(other.frame().lines.at(0).selection.presence != SelectionPresence::absent,
           "viw selects the word under the caret");
    vim_replay(other, "iwy");
    expect(other.vim_state().unnamed_register.text == "alpha ",
           "a second iw grows the selection by one more chunk");
    vim_replay(other, "2lvi(y");
    expect(other.vim_state().unnamed_register.text == "p",
           "an object that is not found leaves the one-character selection alone");
}

void verify_vim_text_object_history()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const std::string original = "ab\r\nalpha beta\r\ngh";
    editing.files().hold(Bytes{original});
    applied(controller, OpenDocument{sample_path()});
    applied(controller, SelectEditMode{EditMode::vim});
    vim_replay(controller, "jciwZZ<Esc>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nZZ beta\r\ngh",
           "ciw changes the word and preserves the document CRLF bytes");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a change through an object");
    vim_replay(controller, "<C-r>");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nZZ beta\r\ngh", "redo restores the change");
    vim_replay(controller, "u");
    vim_replay(controller, "daw");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "ab\r\nbeta\r\ngh", "daw takes the word and its blanks");
    vim_replay(controller, "u");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == original, "one undo restores a delete through an object");
}

void verify_vim_text_object_dot()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma delta");
    EditorController &controller = editing.controller();
    vim_replay(controller, "diw");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "diw"),
           "an object command records its own keys with no extra machinery");
    vim_replay(controller, "w.");
    expect(vim_body(controller.frame()) == "  gamma delta",
           ". replays the object command at the new caret");
    Editing quoted;
    open_vim_document(quoted, "a \"bb\" c \"dd\" e");
    EditorController &other = quoted.controller();
    vim_replay(other, "3lci\"x<Esc>");
    expect(dot_record_is(other.vim_state(), std::nullopt, "ci\"x<Esc>"),
           "a change through an object records the inserted text as well");
    vim_replay(other, "6l.");
    expect(vim_body(other.frame()) == "a \"x\" c \"x\" e", ". replays a change through an object");
    Editing cancelled;
    open_vim_document(cancelled, "alpha beta gamma");
    EditorController &third = cancelled.controller();
    vim_replay(third, "x");
    vim_replay(third, "di<Esc>");
    expect(dot_record_is(third.vim_state(), std::nullopt, "x"),
           "a cancelled object leaves the last change alone");
}

// 回数が尽きた取消のキャレットと選択（Issue #99・ADR 0031 の補足）。ビープが後続の鍵を捨てる
// ので VISUAL の取消は 1 回の `:normal!` に乗らず、fixture にできない（Issue #87 の教訓）。
// 固定 Vim を区切って測った値をここで engine に固定する（`out/issue99-oracle/probe*.txt`）。
void verify_vim_text_object_residuals()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma");
    EditorController &controller = editing.controller();
    vim_replay(controller, "2lv9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha beta gamma",
           "a count that runs out leaves the selection it managed to build (measured)");
    vim_replay(controller, "<Esc>02lvl9iwy");
    expect(controller.vim_state().unnamed_register.text == "pha beta gamma",
           "a forward selection keeps its anchor when the count runs out");
    vim_replay(controller, "<Esc>04lvhh9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha",
           "a backward selection keeps its anchor and the caret stops at the start of the text");
    vim_replay(controller, "<Esc>09lvhh9iwy");
    expect(controller.vim_state().unnamed_register.text == "alpha beta",
           "the backward walk crosses as many units as it can before it gives up");
    vim_replay(controller, "<Esc>02lV9iwy");
    expect(controller.vim_state().unnamed_register.kind == VimRegisterKind::lines,
           "a cancelled object in linewise VISUAL stays linewise");
    vim_replay(controller, "<Esc>0x");
    vim_replay(controller, "d9iw");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a cancelled object moves the caret but is not a change for .");
    Editing blocks;
    open_vim_document(blocks, "if {a; b;} else");
    EditorController &other = blocks.controller();
    vim_replay(other, "5lvl9i{y");
    expect(other.vim_state().unnamed_register.text == "; ",
           "a block that is not found leaves the selection and the caret alone");
}

// 引用符の対の選び方は選択の向きで変わる（決定 6・Issue #111 で実測）。取消はビープが後続の鍵を
// 捨てて 1 回の `:normal!` に乗らないので（Issue #87）、fixture に採れないぶんをここで固定する。
void verify_vim_text_object_quote_pairs()
{
    Editing editing;
    open_vim_document(editing, "a \"bb\" c \"dd\" e");
    EditorController &controller = editing.controller();
    vim_replay(controller, "3lvhi\"y");
    expect(controller.vim_state().unnamed_register.text == "\"b",
           "a backward selection on the first quote of the line cancels (measured on Vim 9.1)");
    vim_replay(controller, "<Esc>02lvhi\"y");
    expect(controller.vim_state().unnamed_register.text == " \"",
           "a backward selection with no quote before the caret cancels");
    vim_replay(controller, "<Esc>012lvli\"y");
    expect(controller.vim_state().unnamed_register.text == "\" ",
           "a forward selection with no pair after the caret cancels");
    Editing odd;
    open_vim_document(odd, "it's a 'quoted' word");
    EditorController &other = odd.controller();
    vim_replay(other, "16lvhi'y");
    expect(other.vim_state().unnamed_register.text == " w",
           "a backward selection past the last unpaired quote cancels");
    Editing across;
    open_vim_document(across, "x \"aa\" y\nz \"bb\" w");
    EditorController &two_lines = across.controller();
    vim_replay(two_lines, "j7lvki\"y");
    expect(two_lines.vim_state().unnamed_register.text == "y\nz \"bb\" w",
           "a selection whose ends are on two lines cancels (quotes are read within one line)");
}

void verify_vim_text_object_fixtures()
{
    constexpr std::array<std::string_view, 6> boundaries{
        "replace-char-one",    "replace-char-count", "char-search-f-count",
        "char-search-t-first", "line-jump-gg-count", "dot-remove-word"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("text-object-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 272,
           "the scope replays 266 text-object fixtures and 6 shared next-key boundaries");
}

void verify_vim_text_object_contracts()
{
    verify_vim_text_object_waiting();
    verify_vim_text_object_cancellation();
    verify_vim_text_object_visual();
    verify_vim_text_object_history();
    verify_vim_text_object_dot();
    verify_vim_text_object_residuals();
    verify_vim_text_object_quote_pairs();
}

void verify_vim_text_object_scope()
{
    verify_vim_text_object_fixtures();
    verify_vim_text_object_contracts();
}

// ---------------------------------------------------------------- 検索（Issue #100・ADR 0032）

// 照合器の答えは「行の中の一致の始まり」。パターンが部分集合の外なら検査そのものを落とす。
[[nodiscard]] std::optional<std::size_t> matched_begin(std::string_view pattern,
                                                       std::string_view line, std::size_t from)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    expect(parsed.has_value(), "the measured pattern is inside the supported subset");
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    const auto match = parsed.value().matched(line, from);
    return match.has_value() ? std::optional<std::size_t>{match.value().begin} : std::nullopt;
}

[[nodiscard]] std::optional<VimSearchHit> searched(std::string_view text, std::size_t from,
                                                   std::string_view pattern,
                                                   VimSearchDirection direction)
{
    const auto buffer = TextBuffer::from_utf8(text);
    const auto parsed = VimPattern::parse(pattern, direction);
    expect(buffer.has_value() && parsed.has_value(), "the searched body and pattern are valid");
    if (!buffer.has_value() || !parsed.has_value())
    {
        return std::nullopt;
    }
    const auto hit =
        vim_find_match(buffer.value(), parsed.value(),
                       VimMatchRequest{buffer.value().position_of(Offset{from}), direction, 1});
    return hit.has_value() ? std::optional<VimSearchHit>{hit.value()} : std::nullopt;
}

// 着いた位置を本文のバイト位置で読む（期待値をバイトで書くため）。
[[nodiscard]] Offset landed(std::string_view text, const VimSearchHit &hit)
{
    const auto buffer = TextBuffer::from_utf8(text);
    return buffer.has_value() ? buffer.value().offset_of(hit.position) : Offset{0};
}

[[nodiscard]] bool found_at(std::string_view text, std::size_t from, std::string_view pattern,
                            std::size_t expected)
{
    const auto hit = searched(text, from, pattern, VimSearchDirection::forward);
    return hit.has_value() && landed(text, hit.value()) == Offset{expected};
}

[[nodiscard]] bool found_back_at(std::string_view text, std::size_t from, std::string_view pattern,
                                 std::size_t expected)
{
    const auto hit = searched(text, from, pattern, VimSearchDirection::backward);
    return hit.has_value() && landed(text, hit.value()) == Offset{expected};
}

[[nodiscard]] bool rejects(std::string_view pattern, VimPatternFailure failure)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    return !parsed.has_value() && parsed.error() == failure;
}

// 決定 4 の部分集合。期待値は固定 Vim 9.1 の 218 ケース（out/issue100-oracle）から取った桁。
void verify_vim_pattern_subset()
{
    const std::string_view three = "one two three";
    expect(matched_begin("^four", "four five six", 0) == std::optional<std::size_t>{0},
           "the caret anchors at the start of the line");
    expect(!matched_begin("o^n", three, 0).has_value(),
           "the caret in the middle is just a character");
    expect(matched_begin("three$", three, 0) == std::optional<std::size_t>{8},
           "the dollar anchors at the end of the line");
    expect(!matched_begin("three$four", three, 0).has_value(),
           "the dollar in the middle is a character");
    expect(matched_begin("t.o", three, 0) == std::optional<std::size_t>{4},
           "a dot is one character");
    expect(matched_begin("f.*e", "four five six", 0) == std::optional<std::size_t>{0},
           "a star is greedy and backs off until the rest fits");
    expect(matched_begin("fo*bar", "foo foobar barfoo foo", 0) == std::optional<std::size_t>{4},
           "a star repeats the atom before it");
    expect(matched_begin("fz*oo", "foo foobar barfoo foo", 0) == std::optional<std::size_t>{0},
           "a star allows zero of the atom before it");
    const std::string_view mixed = "a1b2c3";
    expect(matched_begin("[abc]", mixed, 0) == std::optional<std::size_t>{0}, "a character set");
    expect(matched_begin("[a-c]1", mixed, 0) == std::optional<std::size_t>{0}, "a set range");
    expect(matched_begin("[^a-z]", mixed, 0) == std::optional<std::size_t>{1}, "a negated set");
    expect(matched_begin("[0-9]*c", mixed, 0) == std::optional<std::size_t>{3}, "a repeated set");
    expect(matched_begin("[", "[bracket] {brace}", 0) == std::optional<std::size_t>{0},
           "an unclosed set is the bracket itself");
    expect(matched_begin("\\[bracket", "[bracket] {brace}", 0) == std::optional<std::size_t>{0},
           "an escaped bracket is the character");
    expect(matched_begin("\\d", mixed, 0) == std::optional<std::size_t>{1}, "a digit class");
    expect(matched_begin("\\D", mixed, 0) == std::optional<std::size_t>{0}, "a non-digit class");
    expect(matched_begin("\\w", mixed, 0) == std::optional<std::size_t>{0}, "a word byte class");
    expect(matched_begin("\\W", "x.y.z", 0) == std::optional<std::size_t>{1}, "a non-word class");
    expect(matched_begin("\\s", "[bracket] {brace}", 0) == std::optional<std::size_t>{9},
           "a blank class");
    expect(matched_begin("\\S", " x", 0) == std::optional<std::size_t>{1}, "a non-blank class");
    expect(matched_begin("a\\*b", "a*b c", 0) == std::optional<std::size_t>{0},
           "an escaped star is the character");
    expect(matched_begin("a\\/b", "a/b c", 0) == std::optional<std::size_t>{0},
           "an escaped separator is the character");
    expect(matched_begin("a\\\\b", "a\\b c", 0) == std::optional<std::size_t>{0},
           "an escaped backslash is the character");
    expect(matched_begin("x\\.y", "x.y.z", 0) == std::optional<std::size_t>{0},
           "an escaped dot is the character");
    expect(matched_begin("foo", "Foo foo", 0) == std::optional<std::size_t>{4},
           "the search is case sensitive (noignorecase)");
}

// 語の境界は iskeyword ではなく文字の種類の表で切る（決定 4 の追記・ADR 0031 の決定 2）。
void verify_vim_pattern_word_boundaries()
{
    const std::string_view foos = "foo foobar barfoo foo";
    expect(matched_begin("\\<foo", foos, 0) == std::optional<std::size_t>{0}, "a word start");
    expect(matched_begin("\\<foo", foos, 1) == std::optional<std::size_t>{4},
           "a word start skips the middle of a word");
    expect(matched_begin("foo\\>", foos, 0) == std::optional<std::size_t>{0},
           "a word end holds where a blank follows");
    expect(matched_begin("foo\\>", foos, 1) == std::optional<std::size_t>{14},
           "a word end skips the middle of foobar");
    expect(matched_begin("\\<foo\\>", foos, 0) == std::optional<std::size_t>{0},
           "both anchors hold on a whole word");
    expect(matched_begin("\\<foo\\>", foos, 1) == std::optional<std::size_t>{18},
           "both anchors skip foobar and barfoo");
    expect(matched_begin("\\<ab\\>", "a_b ab", 0) == std::optional<std::size_t>{4},
           "the underscore is a word byte, so the first three bytes are one word");
    expect(matched_begin("\\<1\\>", "a1 1 a1", 0) == std::optional<std::size_t>{3},
           "digits are word bytes");
    // 種類が変われば境界になる。ADR 0032 の補足が「一致しない」と書いた例は実測では一致する。
    const std::string_view kana_kanji =
        "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe6\xbc\xa2\xe5\xad\x97";
    expect(matched_begin("\\<\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\\>", kana_kanji, 0) ==
               std::optional<std::size_t>{0},
           "kana followed by kanji is a class change, so the word ends there");
    expect(matched_begin("\\<\xe6\xbc\xa2\xe5\xad\x97", kana_kanji, 0) ==
               std::optional<std::size_t>{9},
           "the same class change also starts a word");
}

// 未対応の構文は黙って別の意味にせず、閉じた失敗で拒否する（決定 4）。
void verify_vim_pattern_rejections()
{
    expect(rejects("\\(foo\\)", VimPatternFailure::group), "a group is refused");
    expect(rejects("foo\\|bar", VimPatternFailure::branch), "a branch is refused");
    expect(rejects("fo\\{2}", VimPatternFailure::quantifier), "a brace count is refused");
    expect(rejects("fo\\+", VimPatternFailure::quantifier), "one or more is refused");
    expect(rejects("fo\\=", VimPatternFailure::quantifier), "an optional atom is refused");
    expect(rejects("fo\\?", VimPatternFailure::quantifier), "its other spelling too");
    expect(rejects("\\v(foo)", VimPatternFailure::magic), "very magic is refused");
    expect(rejects("\\Mfoo", VimPatternFailure::magic), "nomagic is refused");
    expect(rejects("\\cfoo", VimPatternFailure::ignore_case), "a case switch is refused");
    expect(rejects("\\Cfoo", VimPatternFailure::ignore_case), "both case switches are refused");
    expect(rejects("\\%(foo", VimPatternFailure::escape), "a percent escape is refused");
    expect(rejects("\\_foo", VimPatternFailure::escape), "a multiline escape is refused");
    expect(rejects("alpha\\r$", VimPatternFailure::escape), "a carriage return escape is refused");
    expect(rejects("foo\\", VimPatternFailure::escape), "a trailing backslash is refused");
    expect(rejects("~", VimPatternFailure::previous_substitute),
           "the previous substitute is refused");
    expect(rejects("foo/e", VimPatternFailure::offset), "a forward offset is refused");
    const auto backward = VimPattern::parse("foo?e", VimSearchDirection::backward);
    expect(!backward.has_value() && backward.error() == VimPatternFailure::offset,
           "a backward offset is refused at its own separator");
    const auto question = VimPattern::parse("a?b", VimSearchDirection::forward);
    expect(question.has_value() && question.value().matched("a?b c", 0).has_value(),
           "a question mark is a plain character in a forward search");
}

// 走査の規則（決定 4 の追記）。必要な桁に足りない一致は終端まで飛ばし、行の長さで打ち切る。
void verify_vim_search_scan()
{
    expect(found_at("aaaa", 0, "aa", 2), "four a characters find the overlap at the third column");
    expect(found_at("aaaaaa", 2, "aa", 4), "the scan restarts from the head of the line");
    const auto overlap = searched("ababa", 0, "aba", VimSearchDirection::forward);
    expect(overlap.has_value() && landed("ababa", overlap.value()) == Offset{0} &&
               overlap.value().wrapped,
           "skipping to the end of the match passes over the overlap and wraps");
    expect(found_at("abc", 0, ".*", 0), "an always-matching pattern does not move");
    expect(found_at("abc\ndef", 0, ".*", 4), "it does move to the next line");
    expect(found_at("abc\ndef", 0, "^a*", 4), "a zero-length match on the next line");
    expect(found_at("abc", 0, "a*", 1), "a zero-length match steps one character");
    expect(found_at("abc", 1, "a*", 2), "and the next one steps again");
    expect(found_at("alpha beta gamma", 0, "$", 16), "the dollar matches at the end of the line");
    expect(found_at("xa xa", 0, "xa", 3), "the second match on the same line");
    expect(found_at("xa xa", 3, "xa", 0), "and then it wraps to the first");
    expect(found_back_at("aaaa", 3, "a", 2), "backwards takes the last match before the cursor");
    expect(found_back_at("aaaa", 3, "aa", 2), "overlapping matches count backwards too");
    expect(found_back_at("xa xa", 3, "xa", 0), "a match at the cursor is not a backward match");
}

// 折り返しと向き（決定 3・4）。wrapscan は既定どおり有効で、越えたときだけ報せが出る。
void verify_vim_search_wrap()
{
    const std::string_view body = "alpha beta\nbeta gamma\ndelta beta";
    expect(found_at(body, 0, "beta", 6), "the first match after the caret");
    expect(found_at(body, 6, "beta", 11), "the next match is on the next line");
    const auto wrapped = searched(body, 31, "alpha", VimSearchDirection::forward);
    expect(wrapped.has_value() && landed(body, wrapped.value()) == Offset{0} &&
               wrapped.value().wrapped,
           "a forward search wraps from the bottom and says so");
    const auto plain = searched(body, 0, "beta", VimSearchDirection::forward);
    expect(plain.has_value() && !plain.value().wrapped, "a match below the caret does not wrap");
    expect(found_back_at(body, 31, "beta", 28), "a backward search stays on the line");
    const auto back = searched(body, 0, "gamma", VimSearchDirection::backward);
    expect(back.has_value() && landed(body, back.value()) == Offset{16} && back.value().wrapped,
           "a backward search wraps from the top and says so");
    expect(!searched(body, 0, "zzz", VimSearchDirection::forward).has_value(),
           "nothing is found for a pattern that is not there");
    expect(!searched(body, 0, "three.four", VimSearchDirection::forward).has_value(),
           "a match never crosses a line");
    const auto only = searched("solo word", 0, "solo", VimSearchDirection::forward);
    expect(only.has_value() && landed("solo word", only.value()) == Offset{0} &&
               only.value().wrapped,
           "the only match is the one under the caret, found by wrapping");
}

// UTF-8 と CRLF（決定 4・6）。CRLF は fixture にできないのでここで守る。
void verify_vim_search_encoding()
{
    const std::string_view kana = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86 \xe3\x81\x8b\xe3\x81\x8d"
                                  "\xe3\x81\x8f\n\xe6\xbc\xa2\xe5\xad\x97 \xe3\x81\x82\xe3\x81\x84"
                                  "\xe3\x81\x86";
    expect(found_at(kana, 0, "\xe3\x81\x82.\xe3\x81\x86", 27),
           "a dot matches one multibyte character");
    expect(found_at(kana, 0, "\xe6\xbc\xa2\xe5\xad\x97", 20), "a multibyte literal");
    expect(found_at(kana, 0, "[\xe3\x81\x82\xe3\x81\x8b]", 10), "a multibyte character set");
    expect(found_at(kana, 0, "\xe3\x81\x8f$", 16), "the dollar after a multibyte character");
    expect(found_at(kana, 0, "\\<\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\\>", 27),
           "word anchors around a multibyte word");
    // CRLF の本文では行の内容の終わりが CR の前なので、錨は CR に当たらない（保存形は変わらない）。
    expect(found_at("alpha\r\nbeta", 0, "beta", 7), "a search over CRLF finds the next line");
    const auto anchored = searched("alpha\r\nbeta", 0, "alpha$", VimSearchDirection::forward);
    expect(anchored.has_value() && landed("alpha\r\nbeta", anchored.value()) == Offset{0},
           "the dollar sits before the carriage return");
}

[[nodiscard]] std::string word_under(std::string_view text, std::size_t at)
{
    const auto buffer = TextBuffer::from_utf8(text);
    expect(buffer.has_value(), "the word body is valid UTF-8");
    if (!buffer.has_value())
    {
        return {};
    }
    const auto range = vim_word_at(buffer.value(), Offset{at});
    if (!range.has_value())
    {
        return {};
    }
    return buffer.value().text_range(range.value().begin, range.value().end);
}

// `*` / `#` が使う語（決定 3）。空白と記号の上では同じ行の次の語で、行に語が無ければ無い。
void verify_vim_search_word()
{
    expect(word_under("foo bar\nbaz foo", 0) == "foo", "the word under the caret");
    expect(word_under("foo foobar barfoo foo", 1) == "foo",
           "the caret inside a word takes all of it");
    expect(word_under("alpha beta gamma", 5) == "beta", "a blank takes the next word on the line");
    expect(word_under("a.b x a.b", 1) == "b", "punctuation takes the next word too");
    expect(word_under("[bracket] {brace}", 0) == "bracket", "and so does a bracket");
    expect(word_under("   \nfoo", 0).empty(), "a blank line has no word (it does not cross lines)");
    expect(word_under("beta gamma beta  ", 16).empty(), "trailing blanks have no word after them");
    expect(word_under("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe6\xbc\xa2\xe5\xad\x97", 0) ==
               "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86",
           "the word is the run of one character class");
    expect(word_under("a1_b c a1_b", 0) == "a1_b", "digits and underscores are one word");
    expect(word_under("foo-bar x foo-bar", 3) == "bar", "the hyphen takes the word after it");
}

// 期待値は固定 Vim 9.1 の 218 ケース（out/issue100-oracle）から取った。fixture にできない
// 取消・未対応構文・報せの文言はここが正本の検査である（ADR 0032 の補足）。
[[nodiscard]] bool caret_at(const EditorFrame &frame, std::size_t line, std::size_t column)
{
    return frame.caret.position.line == LineNumber{line} &&
           frame.caret.position.column == Column{column};
}

[[nodiscard]] bool message_is(const EditorFrame &frame, std::string_view text)
{
    return frame.command_message.has_value() && frame.command_message.value().text() == text;
}

[[nodiscard]] bool register_is(const EditorController &controller, std::string_view text,
                               std::string_view kind)
{
    return controller.vim_state().unnamed_register.text == text &&
           vim_register_kind(controller.vim_state().unnamed_register) == kind;
}

// 決定 3 の到達位置・向き・回数。
void verify_vim_search_keys()
{
    const std::string body = "alpha beta\nbeta gamma\ndelta beta";
    Editing forward;
    open_vim_document(forward, body);
    EditorController &first = forward.controller();
    vim_replay(first, "/beta<CR>");
    expect(caret_at(first.frame(), 1, 7) && vim_body(first.frame()) == body,
           "a forward search moves to the next match and leaves the body alone");
    vim_replay(first, "n");
    expect(caret_at(first.frame(), 2, 1), "n keeps the direction");
    vim_replay(first, "N");
    expect(caret_at(first.frame(), 1, 7), "N takes the opposite direction");
    vim_replay(first, "/<CR>");
    expect(caret_at(first.frame(), 2, 1), "an empty pattern reuses the previous one");
    Editing backward;
    open_vim_document(backward, body);
    EditorController &second = backward.controller();
    vim_replay(second, "G$?beta<CR>");
    expect(caret_at(second.frame(), 3, 7), "a backward search moves to the previous match");
    vim_replay(second, "n");
    expect(caret_at(second.frame(), 2, 1), "n after ? keeps going backwards");
    vim_replay(second, "N");
    expect(caret_at(second.frame(), 3, 7), "N after ? goes forwards");
    Editing counted;
    open_vim_document(counted, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &third = counted.controller();
    vim_replay(third, "3/aaa<CR>");
    expect(caret_at(third.frame(), 1, 1), "a count takes the third match, which wraps");
    Editing repeated;
    open_vim_document(repeated, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = repeated.controller();
    vim_replay(fourth, "/aaa<CR>3n");
    expect(caret_at(fourth.frame(), 2, 5), "a count on n multiplies the same way");
}

// 決定 3 のオペレータ（exclusive）と VISUAL の端点。
void verify_vim_search_operators()
{
    Editing removed;
    open_vim_document(removed, "alpha beta gamma");
    EditorController &first = removed.controller();
    vim_replay(first, "d/gamma<CR>");
    expect(vim_body(first.frame()) == "gamma" && register_is(first, "alpha beta ", "v") &&
               caret_at(first.frame(), 1, 1),
           "an operator takes the range up to the match (exclusive)");
    vim_replay(first, "u");
    expect(vim_body(first.frame()) == "alpha beta gamma", "and it is one undo unit");
    Editing yanked;
    open_vim_document(yanked, "alpha beta gamma");
    EditorController &second = yanked.controller();
    vim_replay(second, "y/gamma<CR>");
    expect(vim_body(second.frame()) == "alpha beta gamma" &&
               register_is(second, "alpha beta ", "v"),
           "a yank through a search leaves the body alone");
    Editing changed;
    open_vim_document(changed, "alpha beta gamma");
    EditorController &third = changed.controller();
    vim_replay(third, "c/gamma<CR>ZZ<Esc>");
    expect(vim_body(third.frame()) == "ZZgamma", "a change through a search opens INSERT");
    Editing linewise;
    open_vim_document(linewise, "foo bar\nbaz qux");
    EditorController &fourth = linewise.controller();
    vim_replay(fourth, "d/baz<CR>");
    expect(vim_body(fourth.frame()) == "baz qux" && register_is(fourth, "foo bar\n", "V"),
           "a range that ends in column 1 from the indent becomes linewise");
    Editing shortened;
    open_vim_document(shortened, "foo bar\nbaz qux");
    EditorController &fifth = shortened.controller();
    vim_replay(fifth, "4ld/baz<CR>");
    expect(vim_body(fifth.frame()) == "foo \nbaz qux" && register_is(fifth, "bar", "v"),
           "and from the middle of the line it ends at the end of the previous line");
    Editing counted;
    open_vim_document(counted, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &sixth = counted.controller();
    vim_replay(counted.controller(), "2d/aaa<CR>");
    expect(register_is(sixth, "aaa bbb\nccc aaa\nddd ", "v"),
           "the operator count survives the input line");
    Editing empty;
    open_vim_document(empty, "foo bar");
    EditorController &seventh = empty.controller();
    vim_replay(seventh, "d/foo<CR>");
    expect(vim_body(seventh.frame()) == "foo bar" && register_is(seventh, "", ""),
           "a range that wraps onto itself changes neither the body nor the register");
    Editing visual;
    open_vim_document(visual, "alpha beta gamma");
    EditorController &eighth = visual.controller();
    vim_replay(eighth, "v/gamma<CR>");
    expect(eighth.frame().vim_mode == VimMode::visual, "VISUAL stays VISUAL while searching");
    vim_replay(eighth, "d");
    expect(vim_body(eighth.frame()) == "amma", "and the search moved the end of the selection");
    Editing visual_line;
    open_vim_document(visual_line, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &ninth = visual_line.controller();
    vim_replay(ninth, "V/gamma<CR>d");
    expect(vim_body(ninth.frame()) == "delta beta", "a line VISUAL takes whole lines");
}

// 決定 3 の `*` / `#`。語の先頭から探すが、オペレータの範囲は元のキャレットから。
void verify_vim_search_word_keys()
{
    Editing star;
    open_vim_document(star, "foo bar\nbaz foo\nfoo qux");
    EditorController &first = star.controller();
    vim_replay(first, "*");
    expect(caret_at(first.frame(), 2, 5), "the star searches the word under the caret");
    vim_replay(first, "n");
    expect(caret_at(first.frame(), 3, 1), "and n keeps that pattern");
    Editing hash;
    open_vim_document(hash, "foo bar\nbaz foo\nfoo qux");
    EditorController &second = hash.controller();
    vim_replay(second, "G#");
    expect(caret_at(second.frame(), 2, 5), "the hash searches backwards");
    Editing bounded;
    open_vim_document(bounded, "foo foobar barfoo foo");
    EditorController &third = bounded.controller();
    vim_replay(third, "*");
    expect(caret_at(third.frame(), 1, 19),
           "the word is anchored, so foobar and barfoo are skipped");
    Editing pending;
    open_vim_document(pending, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = pending.controller();
    vim_replay(fourth, "d*");
    expect(vim_body(fourth.frame()) == "aaa\nddd aaa eee" &&
               register_is(fourth, "aaa bbb\nccc ", "v"),
           "an operator before the star takes the range to the match");
    Editing anchored;
    open_vim_document(anchored, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fifth = anchored.controller();
    vim_replay(fifth, "G$d#");
    expect(vim_body(fifth.frame()) == "aaa bbb\nccc aaa\nddd aaa e" &&
               register_is(fifth, "ee", "v"),
           "the range ends at the original caret, not at the start of the word");
}

// 決定 5 の報せ。Vim にある文言は Vim のまま、未対応構文の拒否だけが本実装のもの。
void verify_vim_search_messages()
{
    Editing missing;
    open_vim_document(missing, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &first = missing.controller();
    vim_replay(first, "/zzz<CR>");
    expect(message_is(first.frame(), "E486: Pattern not found: zzz") &&
               caret_at(first.frame(), 1, 1),
           "a pattern that is not there says so and does not move");
    vim_replay(first, "n");
    expect(message_is(first.frame(), "E486: Pattern not found: zzz"),
           "a failed search is still remembered, so n repeats the same failure");
    vim_replay(first, "x");
    expect(!first.frame().command_message.has_value(), "the next key clears the message");
    Editing fresh;
    open_vim_document(fresh, "alpha beta");
    EditorController &second = fresh.controller();
    vim_replay(second, "n");
    expect(message_is(second.frame(), "E35: No previous regular expression"),
           "n without a previous pattern says so");
    vim_replay(second, "/<CR>");
    expect(message_is(second.frame(), "E35: No previous regular expression"),
           "and so does an empty pattern");
    Editing wordless;
    open_vim_document(wordless, "   \nfoo");
    EditorController &third = wordless.controller();
    vim_replay(third, "*");
    expect(message_is(third.frame(), "E348: No string under cursor"),
           "the star on a blank line says there is no word");
    Editing wrapped;
    open_vim_document(wrapped, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &fourth = wrapped.controller();
    vim_replay(fourth, "G$/alpha<CR>");
    expect(message_is(fourth.frame(), "search hit BOTTOM, continuing at TOP") &&
               caret_at(fourth.frame(), 1, 1),
           "a forward search that wraps says so");
    Editing backwards;
    open_vim_document(backwards, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &fifth = backwards.controller();
    vim_replay(fifth, "?gamma<CR>");
    expect(message_is(fifth.frame(), "search hit TOP, continuing at BOTTOM") &&
               caret_at(fifth.frame(), 2, 6),
           "and so does a backward one");
    Editing refused;
    open_vim_document(refused, "foo foobar");
    EditorController &sixth = refused.controller();
    vim_replay(sixth, "/\\(foo\\)<CR>");
    expect(message_is(sixth.frame(), "Unsupported pattern item: \\( \\)") &&
               caret_at(sixth.frame(), 1, 1),
           "an unsupported item is refused instead of meaning something else");
    vim_replay(sixth, "/foo/e<CR>");
    expect(message_is(sixth.frame(), "Search offset is not supported"),
           "and so is a search offset");
}

// 決定 1 の入力行と取消。入力中に本文は変わらず、Esc と空の Backspace で取消になる。
void verify_vim_search_input()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma");
    EditorController &controller = editing.controller();
    vim_replay(controller, "2l/fo");
    const auto typing = controller.frame();
    expect(typing.command_line.has_value() &&
               typing.command_line.value().prompt == nenenib::core::InputLinePrompt::search_forward,
           "the search input line is drawn with its own prompt");
    const auto shown = typing.command_line.value_or(nenenib::core::InputLineView{});
    expect(shown.text == "fo" && shown.completions.empty(),
           "the typed pattern is in the input line and there are no completions");
    expect(vim_body(typing) == "alpha beta gamma" && caret_at(typing, 1, 3),
           "typing a pattern changes neither the body nor the caret");
    vim_replay(controller, "<Esc>");
    expect(!controller.frame().command_line.has_value() && caret_at(controller.frame(), 1, 3),
           "Esc closes the input line without searching");
    expect(!controller.vim_state().last_search.has_value(), "a cancelled search is not remembered");
    vim_replay(controller, "/<BS>");
    expect(!controller.frame().command_line.has_value() && caret_at(controller.frame(), 1, 3),
           "Backspace on an empty input line cancels too");
    vim_replay(controller, "d/<Esc>x");
    expect(vim_body(controller.frame()) == "alha beta gamma",
           "a cancelled search drops the pending operator and the next key acts alone");
    Editing backwards;
    open_vim_document(backwards, "alpha beta gamma");
    EditorController &second = backwards.controller();
    vim_replay(second, "?al");
    expect(second.frame().command_line.value_or(nenenib::core::InputLineView{}).prompt ==
               nenenib::core::InputLinePrompt::search_backward,
           "the backward search has the other prompt");
}

// 決定 3 の `.`。検索は鍵 1 つとして記録に乗るので、追加の仕掛けは要らない（ADR 0030）。
void verify_vim_search_dot()
{
    Editing operated;
    open_vim_document(operated, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &first = operated.controller();
    vim_replay(first, "d/bbb<CR>j0.");
    expect(vim_body(first.frame()) == "ccc aaa\nddd aaa eee" && register_is(first, "bbb\n", "V"),
           ". replays the search as one key");
    Editing starred;
    open_vim_document(starred, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &second = starred.controller();
    vim_replay(second, "*dn.");
    expect(vim_body(second.frame()) == "aaa eee" && register_is(second, "aaa bbb\nccc ", "v"),
           ". replays an operator over n");
    Editing pending;
    open_vim_document(pending, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &third = pending.controller();
    vim_replay(third, "d*.");
    expect(vim_body(third.frame()) == "aaa eee" && register_is(third, "aaa\nddd ", "v"),
           ". replays the star as the key it is");
    Editing plain;
    open_vim_document(plain, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = plain.controller();
    vim_replay(fourth, "x/aaa<CR>.");
    expect(vim_body(fourth.frame()) == "aa bbb\nccc aa\nddd aaa eee",
           "a plain search is a move, so . still replays the last change");
}

void verify_vim_search_fixtures()
{
    // 検索の 135 件と、鍵の分類の表・次キー待ちを共有する境界の代表（`.`・r・f/t・gg）。
    constexpr std::array<std::string_view, 6> boundaries{
        "replace-char-one",    "replace-char-count", "char-search-f-count",
        "char-search-t-first", "line-jump-gg-count", "dot-remove-word"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("search-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 141, "the scope replays 135 search fixtures and 6 shared boundaries");
}

// 次の一致を求める 1 本の経路（ADR 0041 の決定 1）。確定の検索の鍵と incsearch の preview が
// 同じ関数を呼ぶので、位置は行と桁（桁は文字で 1 から）で返り、回数と折り返しもここで決まる。
[[nodiscard]] std::expected<VimSearchHit, core::VimSearchNoticeKind>
match_found(std::string_view text, std::string_view pattern, const VimMatchRequest &request)
{
    const auto buffer = TextBuffer::from_utf8(text);
    const auto parsed = VimPattern::parse(pattern, request.direction);
    expect(buffer.has_value() && parsed.has_value(), "the matched body and pattern are valid");
    if (!buffer.has_value() || !parsed.has_value())
    {
        return std::unexpected(core::VimSearchNoticeKind::pattern_not_found);
    }
    return vim_find_match(buffer.value(), parsed.value(), request);
}

[[nodiscard]] TextPosition at_position(std::size_t line, std::size_t column)
{
    return TextPosition{LineNumber{line}, Column{column}};
}

void expect_match(const std::expected<VimSearchHit, core::VimSearchNoticeKind> &hit,
                  TextPosition expected, bool wrapped, const char *what)
{
    expect(hit.has_value() && hit.value().position == expected, what);
    expect(hit.has_value() && hit.value().wrapped == wrapped, what);
}

void expect_no_match(const std::expected<VimSearchHit, core::VimSearchNoticeKind> &hit,
                     const char *what)
{
    expect(!hit.has_value() && hit.error() == core::VimSearchNoticeKind::pattern_not_found, what);
}

void verify_vim_find_match()
{
    using enum VimSearchDirection;
    const std::string_view body = "alpha beta\nbeta gamma\ndelta beta";
    const std::string_view kana = "\xe3\x81\x82\xe3\x81\x84 \xe3\x81\x8b\n\xe6\xbc\xa2";
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 1}), at_position(1, 7),
                 false, "forward finds the first match after the caret");
    expect_match(match_found(body, "beta", {at_position(1, 7), forward, 1}), at_position(2, 1),
                 false, "a match under the caret is skipped and the next line is found");
    expect_match(match_found("xa xa", "xa", {at_position(1, 1), forward, 1}), at_position(1, 4),
                 false, "the next match on the same line");
    expect_match(match_found(body, "beta", {at_position(3, 7), backward, 1}), at_position(2, 1),
                 false, "backward finds the last match before the caret");
    expect_match(match_found(body, "beta", {at_position(3, 10), backward, 1}), at_position(3, 7),
                 false, "backward stays on the line when a match is before the caret");
    expect_match(match_found(body, "alpha", {at_position(3, 7), forward, 1}), at_position(1, 1),
                 true, "forward wraps from the bottom to the top");
    expect_match(match_found(body, "gamma", {at_position(1, 1), backward, 1}), at_position(2, 6),
                 true, "backward wraps from the top to the bottom");
    expect_match(match_found("solo word", "solo", {at_position(1, 1), forward, 1}),
                 at_position(1, 1), true, "the only match is reached again by wrapping");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 2}), at_position(2, 1),
                 false, "a count of two moves twice");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 3}), at_position(3, 7),
                 false, "a count of three reaches the last line");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 4}), at_position(1, 7),
                 true, "a count that passes the end wraps and says so");
    expect_match(match_found(body, "beta", {at_position(3, 7), backward, 2}), at_position(1, 7),
                 false, "a backward count of two moves twice");
    expect_match(match_found("solo word", "solo", {at_position(1, 1), forward, 2}),
                 at_position(1, 1), true, "a count keeps wrapping back to the only match");
    expect_match(match_found(kana, "\xe3\x81\x8b", {at_position(1, 1), forward, 1}),
                 at_position(1, 4), false, "the column counts characters, not bytes");
    expect_match(match_found(kana, "\xe6\xbc\xa2", {at_position(1, 4), forward, 1}),
                 at_position(2, 1), false, "a multibyte match on the next line");
    expect_match(match_found("alpha\r\nbeta", "beta", {at_position(1, 1), forward, 1}),
                 at_position(2, 1), false, "CRLF lines are searched by their content");
    expect_no_match(match_found(body, "zzz", {at_position(1, 1), forward, 1}),
                    "a pattern that is not there is pattern_not_found");
    expect_no_match(match_found(body, "zzz", {at_position(1, 1), backward, 3}),
                    "a count does not hide a missing pattern");
    expect_no_match(match_found(body, "three.four", {at_position(1, 1), forward, 1}),
                    "a match never crosses a line");
    expect_no_match(match_found("", "a", {at_position(1, 1), forward, 1}),
                    "an empty body has no match");
    expect_no_match(match_found("", "a", {at_position(1, 1), backward, 2}),
                    "an empty body has no match backwards either");
}

// `:set incsearch` / `:set noincsearch`（ADR 0041 の決定 6）。hlsearch と同じ経路で評価し、
// 設定にも強調にも触れない。既定は on で、Vim の鍵を食べ終えても持ち越す。
void verify_ex_incsearch_commands()
{
    const auto settings = core::default_editor_settings();
    const auto on = core::evaluate_ex("set incsearch", settings, Appearance::dark);
    expect(on.has_value() && on.value().incsearch == std::optional<bool>{true} &&
               !on.value().settings.has_value() && !on.value().highlight.has_value() &&
               on.value().message.text() == "incsearch=on",
           ":set incsearch turns the preview on without touching the settings");
    const auto off = core::evaluate_ex("set noincsearch", settings, Appearance::dark);
    expect(off.has_value() && off.value().incsearch == std::optional<bool>{false} &&
               off.value().message.text() == "incsearch=off",
           ":set noincsearch turns it off");
    const auto again = core::evaluate_ex("set incsearch", settings, Appearance::dark);
    expect(again.has_value() && again.value().incsearch == std::optional<bool>{true},
           ":set incsearch turns it back on");
    const auto typo = core::evaluate_ex("set incserch", settings, Appearance::dark);
    expect(!typo.has_value() &&
               typo.error() == core::ExEvaluationFailure{core::ExFailure::unknown_option},
           "a misspelled option is the existing unknown option failure");
    const auto theme = core::evaluate_ex("colorscheme dracula", settings, Appearance::dark);
    expect(theme.has_value() && !theme.value().incsearch.has_value(),
           "commands that are not about incsearch leave it alone");
    const auto completions = core::command_completions("set noi");
    expect(completions.size() == 1 && completions.front() == "set noincsearch",
           "the option is offered as a completion");
    auto state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    expect(state.incsearch, "incsearch is on by default");
    state.incsearch = false;
    const auto rested = nenenib::core::vim_resting_from(
        state, VimRegister{std::string{}, VimRegisterKind::characters});
    expect(!rested.incsearch, "a finished key keeps incsearch as it was");
}

void verify_vim_search_contracts()
{
    verify_vim_find_match();
    verify_ex_incsearch_commands();
    verify_vim_pattern_subset();
    verify_vim_pattern_word_boundaries();
    verify_vim_pattern_rejections();
    verify_vim_search_scan();
    verify_vim_search_wrap();
    verify_vim_search_encoding();
    verify_vim_search_word();
    verify_vim_search_keys();
    verify_vim_search_operators();
    verify_vim_search_word_keys();
    verify_vim_search_messages();
    verify_vim_search_input();
    verify_vim_search_dot();
}

void verify_vim_search_scope()
{
    verify_vim_search_fixtures();
    verify_vim_search_contracts();
}

// ------------------------------------ 検索の当たりの強調（Issue #123 / ADR 0037・D17）

using MatchSpan = std::pair<std::size_t, std::size_t>;

[[nodiscard]] std::vector<MatchSpan> matches_of(std::string_view line, std::string_view pattern)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    std::vector<MatchSpan> spans;
    for (const auto &match : nenenib::core::vim_line_matches(line, parsed.value()))
    {
        spans.emplace_back(match.begin.value, match.end.value);
    }
    return spans;
}

// 行の中の一致の列（決定 3）。数え方は vim_search と同じ 1 本で、総数は固定 Vim 9.1 の
// searchcount() で測った（out/issue123-oracle/probe2.json）。
void verify_vim_line_matches()
{
    expect(matches_of("aaaa", "aa") == std::vector<MatchSpan>{{0, 2}, {2, 4}},
           "the scan restarts at the end of the match, so four a characters hold two");
    expect(matches_of("aaaa", "a").size() == 4, "every character can be its own match");
    expect(matches_of("abab", "ab") == std::vector<MatchSpan>{{0, 2}, {2, 4}},
           "two matches that do not touch");
    expect(matches_of("ababa", "aba") == std::vector<MatchSpan>{{0, 3}},
           "the overlap of a three character match is skipped, so there is only one");
    expect(matches_of("xa xa", "xa") == std::vector<MatchSpan>{{0, 2}, {3, 5}},
           "two matches with a gap between them");
    expect(matches_of("abc", "a*") == std::vector<MatchSpan>{{0, 1}, {1, 1}, {2, 2}},
           "a zero-length match counts and steps one character");
    expect(matches_of("abc", "^") == std::vector<MatchSpan>{{0, 0}}, "the line start matches once");
    expect(matches_of("abc", "$") == std::vector<MatchSpan>{{3, 3}}, "and so does the line end");
    expect(matches_of("abc", "zzz").empty(), "a pattern that is not there has no match");
    expect(matches_of("あいう かきく", "あいう") == std::vector<MatchSpan>{{0, 9}},
           "a multibyte match is measured in bytes");
    expect(matches_of("Foo foo", "foo") == std::vector<MatchSpan>{{4, 7}},
           "the scan is case sensitive like the search (noignorecase)");
}

[[nodiscard]] bool highlight_is(const EditorController &controller, VimSearchHighlight expected)
{
    return controller.vim_state().highlight == expected;
}

// 3 値の遷移（決定 1）。期待値は固定 Vim 9.1 の `v:hlsearch` を 29 ケース測った
// out/issue123-oracle/probe.json と probe3.json。強調そのものは Vim の報告に出ないので
// fixture にできず、ここが正本の検査である（決定 8）。
void verify_vim_search_highlight_state()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    expect(highlight_is(controller, VimSearchHighlight::on),
           "the highlight is on as soon as Vim mode starts (D17)");
    vim_replay(controller, "/beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on), "a search leaves it on");
    static_cast<void>(run_ex(controller, "nohlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::suspended), ":nohlsearch suspends it");
    vim_replay(controller, "j");
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           "a motion does not bring it back");
    vim_replay(controller, "x");
    expect(highlight_is(controller, VimSearchHighlight::suspended), "neither does an edit");
    vim_replay(controller, "n");
    expect(highlight_is(controller, VimSearchHighlight::on), "n brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           ":noh is the same command in short");
    vim_replay(controller, "N");
    expect(highlight_is(controller, VimSearchHighlight::on), "N brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "*");
    expect(highlight_is(controller, VimSearchHighlight::on), "* brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "?beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on), "a backward search brings it back");
    static_cast<void>(run_ex(controller, "noh"));
    vim_replay(controller, "/zzz<CR>");
    expect(highlight_is(controller, VimSearchHighlight::on),
           "a search that finds nothing brings it back too (E486)");
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::off), ":set nohlsearch turns it off");
    vim_replay(controller, "/beta<CR>");
    expect(highlight_is(controller, VimSearchHighlight::off),
           "a search does not turn an option that is off back on");
    static_cast<void>(run_ex(controller, "noh"));
    expect(highlight_is(controller, VimSearchHighlight::off),
           ":noh has nothing to suspend while the option is off");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::on), ":set hlsearch turns it on");
    static_cast<void>(run_ex(controller, "noh"));
    static_cast<void>(run_ex(controller, "set hlsearch"));
    expect(highlight_is(controller, VimSearchHighlight::on),
           ":set hlsearch also clears a suspension");
    Editing empty;
    open_vim_document(empty, "alpha beta\nbeta gamma");
    EditorController &second = empty.controller();
    static_cast<void>(run_ex(second, "noh"));
    vim_replay(second, "n");
    expect(highlight_is(second, VimSearchHighlight::on),
           "a search key with no previous pattern brings it back too (E35)");
}

// 保留を捨てる resting の形と、モードの出入り（決定 1）。
void verify_vim_search_highlight_carry()
{
    const VimState rested = empty_vim_state();
    expect(rested.highlight == VimSearchHighlight::on, "the resting state starts on");
    VimState suspended = rested;
    suspended.highlight = VimSearchHighlight::suspended;
    expect(nenenib::core::vim_resting_from(suspended, suspended.unnamed_register).highlight ==
               VimSearchHighlight::suspended,
           "finishing a key does not forget the suspension");
    expect(nenenib::core::vim_after_resize(suspended, 10, 20).highlight ==
               VimSearchHighlight::suspended,
           "a resize does not forget it either");
    expect(nenenib::core::requested_highlight(
               VimSearchHighlight::off, VimSearchHighlight::suspended) == VimSearchHighlight::off,
           ":noh leaves an option that is off alone");
    expect(nenenib::core::requested_highlight(VimSearchHighlight::suspended,
                                              VimSearchHighlight::on) == VimSearchHighlight::on,
           ":set hlsearch replaces whatever was there");
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma");
    EditorController &controller = session.controller();
    vim_replay(controller, "/beta<CR>");
    static_cast<void>(run_ex(controller, "noh"));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::vim}));
    expect(highlight_is(controller, VimSearchHighlight::suspended),
           "leaving and re-entering Vim mode keeps it, like the remembered pattern");
}

// Ex の 4 命令と補完（決定 2）。
void verify_ex_highlight_commands()
{
    const auto settings = core::default_editor_settings();
    const auto on = core::evaluate_ex("set hlsearch", settings, Appearance::dark).value();
    expect(on.highlight == std::optional<VimSearchHighlight>{VimSearchHighlight::on} &&
               !on.settings.has_value() && on.message.text() == "hlsearch=on",
           ":set hlsearch turns the highlight on without touching the saved settings");
    const auto off = core::evaluate_ex("set nohlsearch", settings, Appearance::dark).value();
    expect(off.highlight == std::optional<VimSearchHighlight>{VimSearchHighlight::off} &&
               off.message.text() == "hlsearch=off",
           ":set nohlsearch turns it off");
    for (const auto text : {"nohlsearch", "noh", " noh "})
    {
        const auto cleared = core::evaluate_ex(text, settings, Appearance::dark).value();
        expect(cleared.highlight ==
                       std::optional<VimSearchHighlight>{VimSearchHighlight::suspended} &&
                   !cleared.settings.has_value() && cleared.message.text() == "hlsearch=suspended",
               ":nohlsearch and its short spelling only suspend");
    }
    const auto theme = core::evaluate_ex("colorscheme dracula", settings, Appearance::dark).value();
    expect(!theme.highlight.has_value(),
           "commands that are not about the highlight leave it alone");
    for (const auto text : {"nohlsearch now", "noh 1", "set hlsearch=1", "set hls", "hlsearch"})
    {
        expect(!core::evaluate_ex(text, settings, Appearance::dark),
               "only the four measured spellings are accepted");
    }
    const auto completions = core::command_completions("set ");
    expect(std::ranges::find(completions, "set hlsearch") != completions.end() &&
               std::ranges::find(completions, "set nohlsearch") != completions.end(),
           "both option spellings are offered as completions");
    expect(core::command_completions("set noh").size() == 1 &&
               core::command_completions("set noh").front() == "set nohlsearch",
           "a prefix narrows to the one that matches");
    expect(choices_for("hls").front().command == "set hlsearch",
           "the palette reaches the option through the shared catalog");
}

[[nodiscard]] std::vector<MatchSpan> frame_matches(const EditorFrame &frame, std::size_t index)
{
    std::vector<MatchSpan> spans;
    for (const auto &span : frame.lines.at(index).matches)
    {
        spans.emplace_back(span.begin.value, span.end.value);
    }
    return spans;
}

[[nodiscard]] bool current_match_is(const EditorFrame &frame, std::size_t index,
                                    std::optional<MatchSpan> expected)
{
    const auto &current = frame.lines.at(index).current_match;
    if (!current.has_value())
    {
        return !expected.has_value();
    }
    return expected == std::optional<MatchSpan>{
                           MatchSpan{current.value().begin.value, current.value().end.value}};
}

// 見えている行ごとの当たりと現在の当たり（決定 3・4）。桁は選択と同じ span_of で作る。
void verify_search_highlight_frame()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    expect(frame_matches(controller.frame(), 0).empty(),
           "nothing is highlighted before the first search");
    vim_replay(controller, "/beta<CR>");
    const auto found = controller.frame();
    expect(frame_matches(found, 0) == std::vector<MatchSpan>{{7, 11}} &&
               frame_matches(found, 1) == std::vector<MatchSpan>{{1, 5}} &&
               frame_matches(found, 2) == std::vector<MatchSpan>{{7, 11}},
           "every visible match is a span in display columns");
    expect(caret_at(found, 1, 7), "the caret sits on the head of the first match below it");
    expect(current_match_is(found, 0, MatchSpan{7, 11}) &&
               current_match_is(found, 1, std::nullopt) && current_match_is(found, 2, std::nullopt),
           "the current match is the one that holds the caret");
    vim_replay(controller, "l");
    expect(current_match_is(controller.frame(), 0, MatchSpan{7, 11}),
           "a caret inside the match is still the current one");
    vim_replay(controller, "n");
    expect(current_match_is(controller.frame(), 1, MatchSpan{1, 5}) &&
               current_match_is(controller.frame(), 0, std::nullopt),
           "n moves the current match to the next line");
    vim_replay(controller, "$");
    expect(current_match_is(controller.frame(), 1, std::nullopt) &&
               frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 5}},
           "a caret outside every match leaves the spans but no current one");
    vim_replay(controller, "0x");
    expect(frame_matches(controller.frame(), 1).empty(),
           "an edit that breaks the word takes the match away with it");
}

// 強調しない場合（決定 3・6）と、見えていない行（決定 3）。
void verify_search_highlight_line_scope()
{
    Editing session;
    open_vim_document(session, "beta one\nbeta two\nbeta three\nbeta four");
    EditorController &controller = session.controller();
    vim_replay(controller, "/beta<CR>");
    static_cast<void>(controller.apply(VisibleLines{2}));
    const auto narrow = controller.frame();
    expect(narrow.lines.size() == 2, "only the visible lines are in the frame");
    expect(frame_matches(narrow, 0) == std::vector<MatchSpan>{{1, 5}} &&
               frame_matches(narrow, 1) == std::vector<MatchSpan>{{1, 5}},
           "and only those lines are scanned");
    static_cast<void>(controller.apply(VisibleLines{vim_visible_lines}));
    static_cast<void>(run_ex(controller, "nohlsearch"));
    expect(frame_matches(controller.frame(), 0).empty() &&
               !controller.frame().lines.at(0).current_match.has_value(),
           "a suspended highlight paints nothing");
    vim_replay(controller, "n");
    expect(!frame_matches(controller.frame(), 0).empty(), "the next search paints again");
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    expect(frame_matches(controller.frame(), 0).empty(), "and so does an option that is off");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    static_cast<void>(controller.apply(app::SelectEditMode{EditMode::ordinary}));
    expect(frame_matches(controller.frame(), 0).empty(),
           "the ordinary mode has no search, so it highlights nothing");
}

// 桁の作り方が選択と同じ 1 本であること（決定 4）。全角・Tab・CRLF で確かめる。
void verify_search_highlight_columns()
{
    Editing wide;
    open_vim_document(wide, "あいbetaう\tbeta");
    EditorController &controller = wide.controller();
    vim_replay(controller, "/beta<CR>");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{3, 7}, {9, 13}},
           "multibyte characters and a tab each count as one column, as they do for a selection");
    vim_replay(controller, "vll");
    const auto selected = controller.frame();
    expect(selected.lines.at(0).selection.begin == selected.lines.at(0).matches.at(0).begin,
           "a VISUAL selection that starts on the match starts at the same column");
    Editing crlf;
    crlf.files().hold(Bytes{std::string("alpha beta\r\nbeta gamma\r\n")});
    applied(crlf.controller(), VisibleLines{vim_visible_lines});
    applied(crlf.controller(), OpenDocument{sample_path()});
    applied(crlf.controller(), SelectEditMode{EditMode::vim});
    vim_replay(crlf.controller(), "/beta<CR>");
    const auto lines = crlf.controller().frame();
    expect(frame_matches(lines, 0) == std::vector<MatchSpan>{{7, 11}} &&
               frame_matches(lines, 1) == std::vector<MatchSpan>{{1, 5}},
           "a CRLF document counts the same columns (the carriage return is not content)");
    Editing anchored;
    open_vim_document(anchored, "alpha\nbeta");
    vim_replay(anchored.controller(), "/^<CR>");
    expect(frame_matches(anchored.controller().frame(), 0).empty() &&
               frame_matches(anchored.controller().frame(), 1).empty(),
           "a zero-length match has no face to paint");
}

void verify_vim_search_highlight_contracts()
{
    verify_vim_line_matches();
    verify_vim_search_highlight_state();
    verify_vim_search_highlight_carry();
    verify_ex_highlight_commands();
    verify_search_highlight_frame();
    verify_search_highlight_line_scope();
    verify_search_highlight_columns();
}

void verify_vim_search_highlight_scope()
{
    verify_vim_search_highlight_contracts();
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

// incsearch の preview（ADR 0041）。入力中の当たりは engine の外の別欄で、表示値の matches と
// current_match と先頭行にだけ現れる。本文・キャレット・last_search は確定まで動かない。
struct IncsearchCase
{
    std::string_view typed;
    std::optional<std::size_t> line;
    MatchSpan span;
};

// 今の一致を持つ見えている行と、その桁。無ければ absent。
[[nodiscard]] std::optional<std::pair<std::size_t, MatchSpan>>
frame_current(const EditorFrame &frame)
{
    for (std::size_t index = 0; index < frame.lines.size(); ++index)
    {
        const auto &current = frame.lines.at(index).current_match;
        if (current.has_value())
        {
            return std::pair{index,
                             MatchSpan{current.value().begin.value, current.value().end.value}};
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool frame_paints_nothing(const EditorFrame &frame)
{
    return std::ranges::all_of(frame.lines, [](const app::LineView &line)
                               { return line.matches.empty() && !line.current_match.has_value(); });
}

[[nodiscard]] std::size_t first_line_of(const EditorFrame &frame)
{
    return frame.lines.empty() ? 0 : frame.lines.front().number.value;
}

// 打った途中の各入力で、当たりの位置・本文とキャレットの不変・報せ無し・Esc の巻き戻し（決定
// 3・5）。
void verify_incsearch_typed_cases()
{
    constexpr std::array<IncsearchCase, 12> cases{{
        {"b", 0, {7, 8}},
        {"be", 0, {7, 9}},
        {"beta", 0, {7, 11}},
        {"gam", 1, {6, 9}},
        {"del", 2, {1, 4}},
        {"a b", 0, {5, 8}},
        {"^b", 1, {1, 2}},
        {"a$", 0, {10, 11}},
        {"al", 0, {1, 3}},
        {"zzz", std::nullopt, {0, 0}},
        {"x", std::nullopt, {0, 0}},
        {"\\(", std::nullopt, {0, 0}},
    }};
    for (const auto &item : cases)
    {
        Editing session;
        open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
        EditorController &controller = session.controller();
        vim_replay(controller, "/");
        static_cast<void>(controller.apply(app::CommandText{std::string(item.typed)}));
        const auto typing = controller.frame();
        const auto current = frame_current(typing);
        if (item.line.has_value())
        {
            expect(current == std::optional{std::pair{item.line.value(), item.span}},
                   "the typed pattern shows its next match as the current one");
        }
        else
        {
            expect(frame_paints_nothing(typing),
                   "an invalid or missing pattern shows nothing while typing");
        }
        expect(vim_body(typing) == "alpha beta\nbeta gamma\ndelta beta" && caret_at(typing, 1, 1),
               "the preview moves neither the body nor the caret");
        expect(!typing.command_message.has_value(),
               "the preview reports nothing, not even E486 or a wrap");
        vim_replay(controller, "<Esc>");
        const auto cancelled = controller.frame();
        expect(frame_paints_nothing(cancelled) && caret_at(cancelled, 1, 1) &&
                   !cancelled.command_line.has_value(),
               "Esc takes the preview away and the caret stays");
        expect(!controller.vim_state().last_search.has_value(),
               "a previewed pattern is not remembered until it is submitted");
    }
}

// 全一致の面と、打ち足し・Backspace での更新、Enter で着く位置（決定 3・4・5）。
void verify_incsearch_frame()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    vim_replay(controller, "/be");
    const auto typing = controller.frame();
    expect(frame_matches(typing, 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_matches(typing, 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_matches(typing, 2) == std::vector<MatchSpan>{{7, 9}},
           "every visible match of the typed pattern is painted");
    static_cast<void>(controller.apply(app::CommandText{"t"}));
    expect(frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 4}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 10}}},
           "typing one more character narrows the preview");
    vim_replay(controller, "<BS>");
    expect(frame_matches(controller.frame(), 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "Backspace goes back to the shorter preview");
    vim_replay(controller, "<BS><BS>");
    expect(frame_paints_nothing(controller.frame()) && controller.frame().command_line.has_value(),
           "an emptied input line shows nothing and stays open");
    vim_replay(controller, "be<CR>");
    const auto landed = controller.frame();
    expect(caret_at(landed, 1, 7) && !landed.command_line.has_value(),
           "Enter lands on the match the preview showed");
    const auto &remembered = controller.vim_state().last_search;
    expect(remembered.has_value() &&
               remembered.value_or(nenenib::core::VimSearchPattern{}).pattern == "be",
           "only Enter remembers the pattern");
    expect(frame_current(landed) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "after Enter the current match is the one under the caret");
    vim_replay(controller, "/gam");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{1}, MatchSpan{6, 9}}} &&
               frame_matches(controller.frame(), 0).empty(),
           "a new search previews its own pattern, not the remembered one");
    vim_replay(controller, "<Esc>");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{7, 9}} &&
               caret_at(controller.frame(), 1, 7),
           "Esc goes back to the remembered highlight and the caret");
}

// 見える範囲の外の当たりへ画面が動き、Esc で入力前の先頭行へ戻る（決定 3・5）。
void verify_incsearch_scroll()
{
    std::string body;
    for (std::size_t number = 1; number <= 30; ++number)
    {
        body += number == 20 ? "target line\n" : "plain line\n";
    }
    body += "last";
    const auto narrow = [&body](Editing &session)
    {
        open_vim_document(session, body);
        applied(session.controller(), VisibleLines{5});
    };
    Editing session;
    narrow(session);
    EditorController &controller = session.controller();
    expect(first_line_of(controller.frame()) == 1, "the narrow view starts at the first line");
    vim_replay(controller, "/tar");
    const auto typing = controller.frame();
    const auto current = frame_current(typing);
    expect(first_line_of(typing) > 1 && current.has_value() &&
               typing.lines.at(current.value_or(std::pair{std::size_t{0}, MatchSpan{}}).first)
                       .number.value == 20,
           "the view follows the previewed match out of sight");
    expect(caret_at(typing, 1, 1), "while the caret itself stays on the first line");
    const std::size_t shown = first_line_of(typing);
    static_cast<void>(controller.apply(app::CommandText{"x"}));
    expect(first_line_of(controller.frame()) == 1 && frame_paints_nothing(controller.frame()),
           "a pattern that stops matching goes back to where typing began");
    vim_replay(controller, "<BS>");
    expect(first_line_of(controller.frame()) == shown,
           "and the same input shows the same view again");
    vim_replay(controller, "<Esc>");
    expect(first_line_of(controller.frame()) == 1 && caret_at(controller.frame(), 1, 1),
           "Esc restores the first line the view had before typing");
    vim_replay(controller, "/tar<CR>");
    const auto previewed = controller.frame();
    Editing plain;
    narrow(plain);
    static_cast<void>(run_ex(plain.controller(), "set noincsearch"));
    vim_replay(plain.controller(), "/tar<CR>");
    const auto unpreviewed = plain.controller().frame();
    expect(caret_at(previewed, 20, 1) && caret_at(unpreviewed, 20, 1),
           "Enter lands on the same line with and without the preview");
    expect(first_line_of(previewed) == first_line_of(unpreviewed),
           "and shows the same view, because the search starts from the view before typing");
    vim_replay(controller, "gg/tar");
    static_cast<void>(controller.apply(VisibleLines{8}));
    vim_replay(controller, "<Esc>");
    expect(first_line_of(controller.frame()) == 1 && controller.frame().lines.size() == 8,
           "a resize while typing keeps the new height and restores the first line only");
}

// 後ろ向きと折り返し（決定 3）。報せは出さない。
void verify_incsearch_directions()
{
    Editing backward;
    open_vim_document(backward, "beta one\nalpha\nbeta two");
    EditorController &controller = backward.controller();
    vim_replay(controller, "j?be");
    expect(frame_current(controller.frame()) ==
               std::optional{std::pair{std::size_t{0}, MatchSpan{1, 3}}},
           "? previews the match before the caret");
    expect(caret_at(controller.frame(), 2, 1), "and the caret stays where it was");
    vim_replay(controller, "<Esc>gg?two");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{2}, MatchSpan{6, 9}}} &&
               !controller.frame().command_message.has_value(),
           "? wraps to the bottom without a notice");
    vim_replay(controller, "<Esc>G/alp");
    expect(frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{1}, MatchSpan{1, 4}}} &&
               !controller.frame().command_message.has_value(),
           "/ wraps to the top without a notice");
    vim_replay(controller, "<CR>");
    expect(caret_at(controller.frame(), 2, 1), "and Enter lands on the wrapped match");
}

// `:set noincsearch` / `:set incsearch` が状態を変え、off では入力中に何も見せない（決定 6）。
void verify_incsearch_option()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma");
    EditorController &controller = session.controller();
    expect(controller.vim_state().incsearch, "incsearch is on by default");
    static_cast<void>(run_ex(controller, "set noincsearch"));
    expect(!controller.vim_state().incsearch, ":set noincsearch turns the state off");
    vim_replay(controller, "/be");
    expect(frame_paints_nothing(controller.frame()), "with incsearch off typing shows nothing");
    vim_replay(controller, "<CR>");
    expect(caret_at(controller.frame(), 1, 7) && !frame_matches(controller.frame(), 0).empty(),
           "the submitted search still lands and highlights");
    vim_replay(controller, "/gam");
    expect(frame_matches(controller.frame(), 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_current(controller.frame()) ==
                   std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with incsearch off the remembered highlight stays while typing");
    vim_replay(controller, "<Esc>");
    static_cast<void>(run_ex(controller, "set incsearch"));
    expect(controller.vim_state().incsearch, ":set incsearch turns it back on");
    vim_replay(controller, "/gam");
    expect(frame_current(controller.frame()) ==
               std::optional{std::pair{std::size_t{1}, MatchSpan{6, 9}}},
           "and typing previews again");
    static_cast<void>(controller.apply(SelectEditMode{EditMode::ordinary}));
    expect(frame_paints_nothing(controller.frame()) && !controller.frame().command_line.has_value(),
           "leaving Vim while typing takes the preview away with the input line");
}

// 入力中の全一致の面は hlsearch が on のときだけ。off でも preview の当たりの枠は出る（Vim と同じ・
// ADR 0041 の決定 4）。
void verify_incsearch_hlsearch()
{
    Editing session;
    open_vim_document(session, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &controller = session.controller();
    static_cast<void>(run_ex(controller, "set nohlsearch"));
    vim_replay(controller, "/be");
    const auto off = controller.frame();
    expect(std::ranges::all_of(off.lines,
                               [](const app::LineView &line) { return line.matches.empty(); }) &&
               frame_current(off) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with hlsearch off typing shows only the current match");
    vim_replay(controller, "<Esc>");
    static_cast<void>(run_ex(controller, "set hlsearch"));
    vim_replay(controller, "/be");
    const auto on = controller.frame();
    expect(frame_matches(on, 0) == std::vector<MatchSpan>{{7, 9}} &&
               frame_matches(on, 1) == std::vector<MatchSpan>{{1, 3}} &&
               frame_current(on) == std::optional{std::pair{std::size_t{0}, MatchSpan{7, 9}}},
           "with hlsearch on typing paints every match and the current one");
}

void verify_vim_search_incremental_contracts()
{
    verify_incsearch_typed_cases();
    verify_incsearch_frame();
    verify_incsearch_scroll();
    verify_incsearch_directions();
    verify_incsearch_option();
    verify_incsearch_hlsearch();
}

void verify_vim_search_incremental_scope()
{
    verify_vim_search_incremental_contracts();
}

// 既定実行（引数なし）が回す scope 専用の契約。selector は「その scope だけを速く回す」絞り込みで、
// 契約そのものは既定実行にも載る（Issue #97）。新しい scope を足したら、下の selector の表と対に
// してここへも 1 行足す。fixture の再生は verify_vim_fixtures が全件行うので、ここには載せない。
// 契約を持たない scope（--vim-visual-yank）と、既に載っている契約を別の切り口で束ねただけの scope
// （--vim-open-line-external / --vim-open-line-recovery / --vim-line-jump-recovery）は出てこない。
void verify_vim_scope_contracts()
{
    constexpr std::array<std::pair<std::string_view, void (*)()>, 12> contracts{{
        {"--vim-dot", verify_vim_dot_contracts},
        {"--vim-search", verify_vim_search_contracts},
        {"--vim-search-highlight", verify_vim_search_highlight_contracts},
        {"--vim-search-incremental", verify_vim_search_incremental_contracts},
        {"--vim-text-objects", verify_vim_text_object_contracts},
        {"--vim-replace", verify_vim_replace_contracts},
        {"--vim-visual-wanted", verify_vim_visual_wanted_contracts},
        {"--vim-open-lines", verify_vim_open_line_contracts},
        {"--vim-character-search", verify_vim_character_search_contracts},
        {"--vim-line-jumps", verify_vim_line_jump_contracts},
        {"--vim-virtual-column", verify_vim_virtual_column_contracts},
        {"--vim-visual-block", verify_vim_block_contracts},
    }};
    for (const auto &scope : contracts)
    {
        scope.second();
    }
}

[[nodiscard]] bool verify_selected_scope(std::string_view command)
{
    constexpr std::array<std::pair<std::string_view, void (*)()>, 21> scopes{{
        {"--display-line", verify_display_line_scope},
        {"--vim-dot", verify_vim_dot_scope},
        {"--vim-search", verify_vim_search_scope},
        {"--vim-search-highlight", verify_vim_search_highlight_scope},
        {"--vim-search-incremental", verify_vim_search_incremental_scope},
        {"--vim-text-objects", verify_vim_text_object_scope},
        {"--vim-replace", verify_vim_replace_scope},
        {"--vim-visual-yank", verify_vim_visual_yank_scope},
        {"--vim-visual-wanted", verify_vim_visual_wanted_scope},
        {"--vim-open-lines", verify_vim_open_line_scope},
        {"--vim-open-line-external", verify_vim_open_line_external_scope},
        {"--vim-open-line-recovery", verify_vim_open_line_recovery},
        {"--vim-character-search", verify_vim_character_search_scope},
        {"--vim-line-jumps", verify_vim_line_jump_scope},
        {"--vim-line-jump-recovery", verify_vim_line_jump_recovery},
        {"--vim-virtual-column", verify_vim_virtual_column_scope},
        {"--user-theme-selection", verify_user_theme_selection},
        {"--user-theme-values", verify_user_theme_values},
        {"--command-palette", verify_command_palette},
        {"--ex-settings", verify_ex_settings},
        {"--vim-visual-block", verify_vim_block_scope},
    }};
    for (const auto &[name, verify] : scopes)
    {
        if (command == name)
        {
            verify();
            return true;
        }
    }
    return false;
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
    const std::string_view command = argc == 2 ? std::string_view{argv[1]} : std::string_view{};
    if (verify_selected_scope(command))
    {
        return report();
    }
    verify_display_text_accepts_ascii();
    verify_palette();
    verify_editor_state();
    if (command == "--coverage-negative")
    {
        return report();
    }
    verify_text_and_caret();
    verify_display_line();
    verify_display_line_views();
    verify_controller_intents();
    verify_vim_scope_contracts();
    verify_ex_settings();
    verify_command_palette();
    verify_user_theme_values();
    verify_user_theme_selection();
    verify_look();
    return report();
}
