#include "ScriptedDocumentReads.hpp"
#include "ScriptedLanguageServer.hpp"
#include "ScriptedTextLayouts.hpp"
// application の単体テスト（ADR 0042 決定 3）。controller・文書・IME。
#include "Appearance.hpp"
#include "AppearanceReadFailure.hpp"
#include "BodyLayout.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CancelSelection.hpp"
#include "CaretMotion.hpp"
#include "CaretShape.hpp"
#include "CaretView.hpp"
#include "ClauseEmphasis.hpp"
#include "ClipboardAction.hpp"
#include "ClipboardFailure.hpp"
#include "ClipboardOperation.hpp"
#include "ClipboardText.hpp"
#include "CodePageFailure.hpp"
#include "Column.hpp"
#include "CommandInput.hpp"
#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "Composition.hpp"
#include "CompositionClause.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DisplayForm.hpp"
#include "DocumentByteLimit.hpp"
#include "DocumentDecoding.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorPorts.hpp"
#include "EditorState.hpp"
#include "FileFailure.hpp"
#include "FilePath.hpp"
#include "FontSize.hpp"
#include "GiantLine.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "ImeStance.hpp"
#include "InputLinePrompt.hpp"
#include "InsertText.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "ModeLabel.hpp"
#include "MoveCaret.hpp"
#include "NewLine.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "Palette.hpp"
#include "PlaceCaret.hpp"
#include "RefreshAppearance.hpp"
#include "RgbColor.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedAutosave.hpp"
#include "ScriptedBell.hpp"
#include "ScriptedBookmarks.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedFonts.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ScrollLines.hpp"
#include "ScrollState.hpp"
#include "SearchLine.hpp"
#include "SelectAll.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "SelectionPresence.hpp"
#include "SelectionSpan.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextLayoutFailure.hpp"
#include "TextPosition.hpp"
#include "TextViewGeometry.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimLineRows.hpp"
#include "VimMode.hpp"
#include "VimRoundTrip.hpp"
#include "VimRowSpan.hpp"
#include "VimSearchDirection.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CancelComposition;
using nenenib::application::CancelSelection;
using nenenib::application::CaretView;
using nenenib::application::ClipboardAction;
using nenenib::application::ClipboardOperation;
using nenenib::application::CommitText;
using nenenib::application::ComposeText;
using nenenib::application::DeleteText;
using nenenib::application::EditorFrame;
using nenenib::application::EditorState;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::MoveCaret;
using nenenib::application::NewLine;
using nenenib::application::OpenDocument;
using nenenib::application::PlaceCaret;
using nenenib::application::RefreshAppearance;
using nenenib::application::SaveDocument;
using nenenib::application::ScrollLines;
using nenenib::application::ScrollState;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::SwitchTab;
using nenenib::application::VimKeyPress;
using nenenib::application::VisibleLines;
using nenenib::core::body_layout;
using nenenib::core::byte_order_mark;
using nenenib::core::CaretMotion;
using nenenib::core::CaretShape;
using nenenib::core::ClauseEmphasis;
using nenenib::core::clipboard_line_feeds;
using nenenib::core::collapsed_at;
using nenenib::core::Column;
using nenenib::core::composition_underlines;
using nenenib::core::CompositionClause;
using nenenib::core::DeleteDirection;
using nenenib::core::Edit;
using nenenib::core::EditBoundary;
using nenenib::core::EditHistory;
using nenenib::core::EditMode;
using nenenib::core::has_selection;
using nenenib::core::HistoryDirection;
using nenenib::core::LineEnding;
using nenenib::core::LineNumber;
using nenenib::core::mode_label;
using nenenib::core::Offset;
using nenenib::core::OffsetRange;
using nenenib::core::Palette;
using nenenib::core::palette_for;
using nenenib::core::RgbColor;
using nenenib::core::SaveState;
using nenenib::core::Selection;
using nenenib::core::SelectionAnchoring;
using nenenib::core::SelectionPresence;
using nenenib::core::SelectionSpan;
using nenenib::core::TextEncoding;
using nenenib::core::TextPosition;
using nenenib::core::VimCharacter;
using nenenib::core::VimKey;
using nenenib::core::VimMode;

// 消費した入力そのものは読まず、保持した元と返された値を観測する（ADR 0081）。
void expect_state_values(const EditorState &left, const EditorState &right)
{
    const auto &left_message = left.command_message();
    const auto &right_message = right.command_message();
    expect(left.text().text() == right.text().text() && left.line_ending() == right.line_ending() &&
               left.selection() == right.selection(),
           "state updates preserve the text and selection value");
    expect(left.history().size() == right.history().size() &&
               left.history().position() == right.history().position() &&
               left.history().undo() == right.history().undo() &&
               left.history().redo() == right.history().redo(),
           "state updates preserve applied and redo history");
    expect(left.document().path == right.document().path &&
               left.document().encoding == right.document().encoding &&
               left.document().saved_position == right.document().saved_position,
           "state updates preserve the document and save point");
    expect(left.scroll() == right.scroll() && left.mode() == right.mode() &&
               left.appearance() == right.appearance() &&
               same_settings(left.settings(), right.settings()),
           "state updates preserve viewport and window settings");
    expect(
        left.vim().mode == right.vim().mode &&
            left.vim().unnamed_register.value().text == right.vim().unnamed_register.value().text &&
            left.vim().unnamed_register.value().kind == right.vim().unnamed_register.value().kind &&
            left.vim().registers.registers.at(0).value().text ==
                right.vim().registers.registers.at(0).value().text &&
            left.vim().last_macro == right.vim().last_macro,
        "state updates preserve the owned Vim values");
    expect(left.tab_count() == right.tab_count() && left.active_tab() == right.active_tab() &&
               left.parked() == right.parked() && left.tab_walking() == right.tab_walking() &&
               std::ranges::equal(left.recency().order(), right.recency().order()),
           "state updates share parked bundles and preserve MRU");
    expect(left.last_failure() == right.last_failure() && left.closing() == right.closing() &&
               left.close_request() == right.close_request() &&
               left.operation_request() == right.operation_request() && left_message.has_value() &&
               right_message.has_value() &&
               left_message.value().text() == right_message.value().text(),
           "state updates preserve intent requests and messages");
}

[[nodiscard]] EditorState state_for_update()
{
    using nenenib::application::Document;
    auto vim = empty_vim_state();
    vim.unnamed_register = nenenib::core::VimRegisterSnapshot::from(
        nenenib::core::VimRegister{"owned", nenenib::core::VimRegisterKind::characters});
    vim.registers.registers.at(0) = vim.unnamed_register;
    vim.last_macro = 'a';
    const auto history =
        EditHistory::empty().pushed(Edit{Offset{0}, "", "one", Offset{0}}, EditBoundary::separate);
    return EditorState::create(Appearance::dark, EditMode::vim)
        .with_new_tab()
        .with_edit(buffer_of("one"), Selection{Offset{1}, Offset{3}}, history,
                   Document{sample_path(), TextEncoding::utf8_bom, 1})
        .with_vim(std::move(vim))
        .with_scroll(ScrollState{LineNumber{1}, 5})
        .with_walked(1)
        .with_failure(FileFailure::unwritable)
        .with_closing(true)
        .with_close_request(0)
        .with_operation_request(nenenib::core::EditorOperation::save)
        .with_command_message(nenenib::core::DisplayText::parse("kept").value());
}

void verify_state_update(const EditorState &source, const auto &update)
{
    const auto saved = source;
    const auto copied = update(source);
    auto consumed = source;
    const auto moved = update(std::move(consumed));
    expect_state_values(copied, moved);
    expect_state_values(source, saved);
}

void verify_consuming_updates()
{
    const auto source = state_for_update();
    verify_state_update(
        source, [](auto &&value)
        { return std::forward<decltype(value)>(value).with_selection(collapsed_at(Offset{2})); });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            return std::forward<decltype(value)>(value).with_scroll(
                                ScrollState{LineNumber{2}, 7});
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            auto history = value.history().undone();
                            return std::forward<decltype(value)>(value).with_history(
                                std::move(history));
                        });
    verify_state_update(
        source,
        [](auto &&value)
        {
            return std::forward<decltype(value)>(value).with_document(
                nenenib::application::Document{std::nullopt, TextEncoding::utf8, std::nullopt});
        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            auto vim = value.vim();
                            vim.unnamed_register =
                                nenenib::core::VimRegisterSnapshot::from(nenenib::core::VimRegister{
                                    "changed", vim.unnamed_register.value().kind,
                                    vim.unnamed_register.value().width});
                            return std::forward<decltype(value)>(value).with_vim(std::move(vim));
                        });
    verify_state_update(
        source,
        [](auto &&value)
        {
            auto history = value.history().undone();
            return std::forward<decltype(value)>(value).with_edit(
                buffer_of("two"), collapsed_at(Offset{3}), std::move(history),
                nenenib::application::Document{std::nullopt, TextEncoding::utf8, std::nullopt});
        });
}

void verify_self_borrowed_updates()
{
    const auto source = state_for_update();
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &selection = value.selection();
                            return std::forward<decltype(value)>(value).with_selection(selection);
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &scroll = value.scroll();
                            return std::forward<decltype(value)>(value).with_scroll(scroll);
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &history = value.history();
                            return std::forward<decltype(value)>(value).with_history(history);
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &document = value.document();
                            return std::forward<decltype(value)>(value).with_document(document);
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &vim = value.vim();
                            return std::forward<decltype(value)>(value).with_vim(vim);
                        });
    verify_state_update(source,
                        [](auto &&value)
                        {
                            const auto &text = value.text();
                            const auto &selection = value.selection();
                            const auto &history = value.history();
                            const auto &document = value.document();
                            const auto scroll = value.scroll();
                            return std::forward<decltype(value)>(value)
                                .with_edit(text, selection, history, document)
                                .with_scroll(scroll);
                        });
}

void verify_intent_cleared()
{
    using nenenib::core::EditorOperation;
    const auto source = state_for_update();
    for (unsigned fields = 0; fields < 16; ++fields)
    {
        const auto pending =
            source
                .with_failure((fields & 1U) != 0 ? std::optional{FileFailure::unwritable}
                                                 : std::nullopt)
                .with_closing((fields & 2U) != 0)
                .with_close_request((fields & 4U) != 0 ? std::optional<std::size_t>{0}
                                                       : std::nullopt)
                .with_operation_request((fields & 8U) != 0 ? std::optional{EditorOperation::save}
                                                           : std::nullopt);
        const auto cleared = pending.with_intent_cleared();
        const auto expected = pending.with_failure(std::nullopt)
                                  .with_closing(false)
                                  .with_close_request(std::nullopt)
                                  .with_operation_request(std::nullopt);
        expect_state_values(cleared, expected);
        expect(pending.closing() == ((fields & 2U) != 0) &&
                   pending.last_failure().has_value() == ((fields & 1U) != 0) &&
                   pending.close_request().has_value() == ((fields & 4U) != 0) &&
                   pending.operation_request().has_value() == ((fields & 8U) != 0),
               "clearing any combination leaves its source unchanged");
    }
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

void verify_controller_initial_appearance()
{
    ScriptedAppearance light{Reading{Appearance::light}};
    ScriptedClipboard board;
    ScriptedFiles files;
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    ScriptedSession session;
    ScriptedHistory history;
    ScriptedFolders folders;
    ScriptedAutosave autosave;
    ScriptedFonts fonts;
    ScriptedBell bell;
    ScriptedMarkdown markdown;
    ScriptedBookmarks bookmarks;
    ScriptedTextLayouts text_layouts;
    ScriptedDocumentReads reads{files, code_pages};
    ScriptedLanguageServer language_server;
    EditorController from_light(nenenib::application::EditorPorts{
        light, board, files, code_pages, settings, themes, session, history, folders, bookmarks,
        text_layouts, autosave, fonts, bell, markdown, reads, language_server});
    expect(from_light.frame().palette.background == RgbColor{0xF4, 0xF5, 0xF7},
           "a readable light setting is used");
    ScriptedAppearance dark{Reading{Appearance::dark}};
    EditorController from_dark(nenenib::application::EditorPorts{
        dark, board, files, code_pages, settings, themes, session, history, folders, bookmarks,
        text_layouts, autosave, fonts, bell, markdown, reads, language_server});
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
    ScriptedSession session;
    ScriptedHistory history;
    ScriptedFolders folders;
    ScriptedAutosave autosave;
    ScriptedFonts fonts;
    ScriptedBell bell;
    ScriptedMarkdown markdown;
    ScriptedBookmarks bookmarks;
    ScriptedTextLayouts text_layouts;
    ScriptedDocumentReads reads{files, code_pages};
    ScriptedLanguageServer language_server;
    for (const auto failure :
         {AppearanceReadFailure::unavailable, AppearanceReadFailure::unreadable})
    {
        ScriptedAppearance port{Reading{std::unexpect, failure}};
        EditorController controller(nenenib::application::EditorPorts{
            port, board, files, code_pages, settings, themes, session, history, folders, bookmarks,
            text_layouts, autosave, fonts, bell, markdown, reads, language_server});
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
    ScriptedSession session;
    ScriptedHistory history;
    ScriptedFolders folders;
    ScriptedAutosave autosave;
    ScriptedFonts fonts;
    ScriptedBell bell;
    ScriptedMarkdown markdown;
    ScriptedBookmarks bookmarks;
    ScriptedTextLayouts text_layouts;
    ScriptedDocumentReads reads{files, code_pages};
    ScriptedLanguageServer language_server;
    EditorController controller(nenenib::application::EditorPorts{
        port, board, files, code_pages, settings, themes, session, history, folders, bookmarks,
        text_layouts, autosave, fonts, bell, markdown, reads, language_server});
    port.script(Reading{Appearance::dark});
    const auto frame = controller.apply_frame(RefreshAppearance{});
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

// 空行を挟む選択と、line_endが決めた末尾CRの扱い（ADR 0095）。
void verify_frame_selection_line_ends()
{
    for (const auto text : {"a\r\n\r\nb\r", "a\n\nb\r"})
    {
        Editing editing;
        EditorController &controller = editing.controller();
        editing.files().hold(std::string(text));
        applied(controller, VisibleLines{10});
        applied(controller, OpenDocument{sample_path()});
        applied(controller, SelectAll{});
        const auto all = controller.frame();
        expect(all.lines.at(0).selection ==
                   SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
               "selecting the first line includes exactly one newline column");
        expect(all.lines.at(1).text.empty() &&
                   all.lines.at(1).selection ==
                       SelectionSpan{SelectionPresence::present, Column{1}, Column{2}},
               "an empty line has one selected newline column");
        expect(all.lines.at(2).selection ==
                   SelectionSpan{SelectionPresence::present, Column{1}, Column{3}},
               "SelectAll includes terminal CR as content or one terminator column");
        expect(all.lines.at(2).text == (text[1] == '\r' ? "b" : "b\r"),
               "the owned line keeps the canonical CRLF or LF content boundary");
        applied(controller,
                PlaceCaret{TextPosition{LineNumber{1}, Column{2}}, SelectionAnchoring::collapse});
        applied(controller,
                PlaceCaret{TextPosition{LineNumber{3}, Column{2}}, SelectionAnchoring::extend});
        const auto partial = controller.frame();
        expect(partial.lines.at(0).selection ==
                   SelectionSpan{SelectionPresence::present, Column{2}, Column{3}},
               "a cross-line selection can begin on the terminator");
        expect(partial.lines.at(1).selection ==
                   SelectionSpan{SelectionPresence::present, Column{1}, Column{2}},
               "the middle empty line keeps its one terminator column");
        expect(partial.lines.at(2).selection ==
                   SelectionSpan{SelectionPresence::present, Column{1}, Column{2}},
               "an end before terminal CR selects only the final content character");
    }
}

void verify_frame_selection_mixed_columns()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::string("日😀\t\x01\u200bZ\nend"));
    applied(controller, VisibleLines{10});
    applied(controller, OpenDocument{sample_path()});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{2}}, SelectionAnchoring::collapse});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{6}}, SelectionAnchoring::extend});
    const auto mixed = controller.frame();
    expect(mixed.lines.at(0).text == "日😀\t\x01\u200bZ",
           "selection projection borrows the raw mixed line without rewriting it");
    expect(mixed.lines.at(0).selection ==
               SelectionSpan{SelectionPresence::present, Column{2}, Column{6}},
           "emoji, Tab, control and format characters each count as one source column");
    expect(mixed.lines.at(1).selection.presence == SelectionPresence::absent,
           "a same-line selection leaves the following line absent");
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

// 書き込みの失敗の知らせ（ADR 0134 の決定 3）。ステータスバーの 1 行に出る。
[[nodiscard]] bool clipboard_notice_shown(EditorController &controller)
{
    const auto &message = controller.frame().command_message;
    return message.has_value() && message.value().text() == "クリップボードに書けませんでした";
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
    expect(!controller.frame().command_message.has_value(), "a written copy shows no notice");
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
    expect(clipboard_notice_shown(controller), "a refused copy says so on the status bar");
    expect(applied(controller, ClipboardAction{ClipboardOperation::cut}) == "abc",
           "a refused write does not cut the text away");
    expect(clipboard_notice_shown(controller), "a refused cut says so on the status bar");
    // 開けない・変換できないも同じ 1 行（決定 1・2）。
    const std::array<ClipboardFailure, 2> others{ClipboardFailure::unavailable,
                                                 ClipboardFailure::unrepresentable};
    for (const ClipboardFailure failure : others)
    {
        Editing failing;
        failing.clipboard().refuse_writes(failure);
        applied(failing.controller(), VisibleLines{10});
        applied(failing.controller(), InsertText{"abc"});
        applied(failing.controller(), SelectAll{});
        applied(failing.controller(), ClipboardAction{ClipboardOperation::copy});
        expect(clipboard_notice_shown(failing.controller()),
               "every refused write shows the same notice");
    }
    Editing nothing_held;
    applied(nothing_held.controller(), VisibleLines{10});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}).empty(),
           "pasting an empty clipboard changes nothing");
    nothing_held.clipboard().hold(Content{"pasted"});
    expect(applied(nothing_held.controller(), ClipboardAction{ClipboardOperation::paste}) ==
               "pasted",
           "a clipboard filled from outside pastes");
}

// クリップボードの本文 → 畳んだ本文（ADR 0055 の決定 1・3）。`\r\r\n` は `\r`（文字）と改行。
using LineFeedFold = std::pair<std::string_view, std::string_view>;

constexpr std::array<LineFeedFold, 7> line_feed_folds{{
    {"", ""},
    {"abc", "abc"},
    {"a\r\nb", "a\nb"},
    {"a\nb", "a\nb"},
    {"a\rb", "a\rb"},
    {"a\r\r\nb", "a\r\nb"},
    {"a\r\n\r\nb", "a\n\nb"},
}};

void verify_clipboard_line_feeds()
{
    for (const auto &[text, folded] : line_feed_folds)
    {
        expect(clipboard_line_feeds(text) == folded,
               "CRLF folds to LF and a lone CR stays a character");
    }
}

// 文書を開いて末尾へキャレットを置いた足場。文書の改行の形は開いた本文が決める（ADR 0036）。
void open_at_end(Editing &editing, std::string text)
{
    editing.files().hold(Bytes{std::move(text)});
    applied(editing.controller(), VisibleLines{10});
    applied(editing.controller(), OpenDocument{sample_path()});
    applied(editing.controller(),
            MoveCaret{CaretMotion::document_end, SelectionAnchoring::collapse});
}

// 本文のバイト列そのもの。保存の口へ出して替え玉が受けたものを読む（改行の形まで見る）。
[[nodiscard]] std::string saved_body(Editing &editing)
{
    static_cast<void>(
        editing.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    return editing.files().written();
}

// 文書の本文・OS の本文・貼った後の本文（ADR 0055 の決定 2・3）。キャレットは文書の末尾。
using DocumentPaste = std::tuple<std::string_view, std::string_view, std::string_view>;

constexpr std::array<DocumentPaste, 9> document_pastes{{
    {"X1\nX2", "a\r\nb", "X1\nX2a\nb"},
    {"X1\r\nX2", "a\nb", "X1\r\nX2a\r\nb"},
    {"X1\r\nX2", "a\r\nb", "X1\r\nX2a\r\nb"},
    {"X1\nX2", "a\rb\r\n", "X1\nX2a\rb\n"},
    {"X1\r\nX2", "a\rb", "X1\r\nX2a\rb"},
    {"X1\nX2", "a\r\r\nb", "X1\nX2a\r\nb"},
    {"X1\nX2", "abc\r\n", "X1\nX2abc\n"},
    {"X1\r\nX2", "abc\n", "X1\r\nX2abc\r\n"},
    {"X1\nX2", "a\nb", "X1\nX2a\nb"},
}};

void verify_controller_paste_line_endings()
{
    for (const auto &[document, os_text, expected] : document_pastes)
    {
        Editing editing;
        open_at_end(editing, std::string(document));
        editing.clipboard().hold(Content{std::string(os_text)});
        applied(editing.controller(), ClipboardAction{ClipboardOperation::paste});
        expect(saved_body(editing) == expected,
               "a paste takes the document's line ending and keeps a lone CR");
    }
    Editing replacing;
    open_at_end(replacing, "X1\nX2");
    applied(replacing.controller(), SelectAll{});
    replacing.clipboard().hold(Content{std::string("a\r\nb")});
    applied(replacing.controller(), ClipboardAction{ClipboardOperation::paste});
    expect(replacing.controller().frame().caret.position == TextPosition{LineNumber{2}, Column{2}},
           "the caret lands after the pasted text");
    expect(saved_body(replacing) == "a\nb", "a paste over a selection replaces it in LF");
    Editing vim;
    open_vim_document(vim, "X1\nX2");
    vim_replay(vim.controller(), "i");
    vim.clipboard().hold(Content{std::string("a\r\nb")});
    applied(vim.controller(), ClipboardAction{ClipboardOperation::paste});
    expect(saved_body(vim) == "a\nbX1\nX2", "Ctrl+V in Vim INSERT takes the same fold");
}

void verify_controller_paste_undo_and_failure()
{
    Editing undoing;
    open_at_end(undoing, "X1\r\nX2");
    undoing.clipboard().hold(Content{std::string("a\nb\nc")});
    applied(undoing.controller(), ClipboardAction{ClipboardOperation::paste});
    expect(saved_body(undoing) == "X1\r\nX2a\r\nb\r\nc", "the pasted lines are CRLF");
    applied(undoing.controller(), HistoryAction{HistoryDirection::undo});
    expect(saved_body(undoing) == "X1\r\nX2", "one undo takes the whole paste back");
    Editing failing;
    open_at_end(failing, "X1\nX2");
    failing.clipboard().hold(Content{std::unexpected(ClipboardFailure::read_failed)});
    applied(failing.controller(), ClipboardAction{ClipboardOperation::paste});
    expect(saved_body(failing) == "X1\nX2", "a failed read leaves the body alone");
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

// 意図を 1 つ送る。framed_each なら 1 意図ごとに frame を読み（即時の追従と同じ観測）、そうで
// なければ frame を読まない（applied は frame を読むので使わない）。
void send_intent(EditorController &controller, bool framed_each,
                 const nenenib::application::EditorIntent &intent)
{
    if (framed_each)
    {
        static_cast<void>(controller.apply_frame(intent));
        return;
    }
    static_cast<void>(controller.apply(intent));
}

// 追従は頼むだけで、frame を読むときに 1 回だけ解決する（ADR 0128）。200 打鍵を 1 枚の frame で
// 読んでも、1 打鍵ごとに frame を読んだときと同じ所が見える。打鍵だけでは layout を作らない。
[[nodiscard]] EditorFrame typed_burst(Editing &editing, bool framed_each)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{40, 20, 96}});
    const std::size_t before = editing.text_layouts().requests();
    for (std::size_t typed = 1; typed <= 200; ++typed)
    {
        send_intent(controller, framed_each, InsertText{"x"});
        if (typed % 50 == 25)
        {
            send_intent(controller, framed_each, NewLine{});
        }
    }
    if (!framed_each)
    {
        expect(editing.text_layouts().requests() == before,
               "typing alone lays nothing out; the caret follow waits for the frame");
    }
    return controller.frame();
}

void verify_follow_settles_at_frame()
{
    Editing each;
    Editing burst;
    const auto framed = typed_burst(each, true);
    const auto settled = typed_burst(burst, false);
    expect(!framed.lines.empty() && !settled.lines.empty(), "both views show lines");
    expect(framed.first_visible == settled.first_visible &&
               framed.lines.front().screen_row == settled.lines.front().screen_row,
           "one frame after 200 keys shows what a frame after every key shows");
    expect(settled.first_visible.value > 1, "the frame resolved the follow and scrolled down");
    const std::size_t read = burst.text_layouts().requests();
    const auto again = burst.controller().frame();
    expect(again.first_visible == settled.first_visible &&
               again.lines.front().screen_row == settled.lines.front().screen_row,
           "reading the frame again keeps the settled scroll");
    expect(burst.text_layouts().requests() > read, "the frame still lays out its visible lines");
}

// incsearch の origin は解決済みのスクロール（ADR 0128 の決定 2）。追従を頼んだまま検索を開いて
// 取り消しても、1 意図ごとに frame を読んだときと同じ所へ戻る。
[[nodiscard]] EditorFrame searched_and_cancelled(Editing &editing, bool framed_each)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{40, 20, 96}});
    const auto send = [&controller, framed_each](const nenenib::application::EditorIntent &intent)
    { send_intent(controller, framed_each, intent); };
    for (std::size_t line = 0; line < 12; ++line)
    {
        send(InsertText{"ab"});
        send(NewLine{});
    }
    send(SelectEditMode{EditMode::vim});
    send(VimKeyPress{VimKey{VimCharacter{U'/'}}});
    send(VimKeyPress{VimKey{VimCharacter{U'z'}}});
    send(VimKeyPress{VimKey{nenenib::core::VimSpecialKey::escape}});
    return controller.frame();
}

void verify_follow_settles_before_search_origin()
{
    Editing each;
    Editing burst;
    const auto framed = searched_and_cancelled(each, true);
    const auto settled = searched_and_cancelled(burst, false);
    expect(framed.first_visible.value > 1, "typing twelve lines scrolled the window down");
    expect(!framed.lines.empty() && !settled.lines.empty() &&
               framed.first_visible == settled.first_visible &&
               framed.lines.front().screen_row == settled.lines.front().screen_row,
           "the incsearch origin holds the settled scroll, not the one before the follow");
}

// 問いの間の保留は文書を離れるときに捨てる（ADR 0128 の決定 7）の足場。短い行の文書と巨大な行の
// 文書を置き、短い行の文書を開いてキャレットを先頭に残したまま下へ送る。
[[nodiscard]] LineNumber scrolled_small_document(Editing &editing)
{
    std::string lines;
    for (std::size_t line = 0; line < 40; ++line)
    {
        lines += "line\n";
    }
    editing.files().hold_at("C:\\work\\lines.txt", Bytes{std::move(lines)});
    editing.files().hold_at(
        "C:\\work\\giant.txt",
        Bytes{"head\n" + std::string(nenenib::core::giant_line_threshold + 1, 'x') + "\ntail"});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{5, nenenib::core::TextViewGeometry{400, 20, 96}});
    applied(controller,
            OpenDocument{nenenib::core::FilePath::parse("C:\\work\\lines.txt").value()});
    applied(controller, ScrollLines{20});
    return controller.frame().first_visible;
}

// 巨大な行の文書で問いを待つ間に頼んだ追従は、SwitchTab で戻った文書のスクロールを動かさない。
void verify_follow_dropped_on_leaving_question()
{
    Editing editing;
    const LineNumber before = scrolled_small_document(editing);
    expect(before.value > 1, "the small document is scrolled down with the caret at its top");
    EditorController &controller = editing.controller();
    expect(controller
               .apply(OpenDocument{nenenib::core::FilePath::parse("C:\\work\\giant.txt").value()})
               .display_question,
           "the giant document asks");
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    applied(controller, SwitchTab{0});
    expect(controller.frame().first_visible == before,
           "a follow asked while the question was open does not move the next document");
}

// 問いの間に字の大きさを変えた設定が頼んだ clamp も、文書を離れるときに捨てる（ADR 0128 の
// 決定 7・8）の足場。短い行の文書を下へ送ってから巨大な行の文書を開き、問いの間に字を変える
// （resize が偽なら変えない）。SwitchTab で短い行の文書へ戻り、切り替えの間の文字組みの回数を返す。
[[nodiscard]] std::size_t switched_back_after_question(Editing &editing, bool resize)
{
    const LineNumber before = scrolled_small_document(editing);
    EditorController &controller = editing.controller();
    expect(controller
               .apply(OpenDocument{nenenib::core::FilePath::parse("C:\\work\\giant.txt").value()})
               .display_question,
           "the giant document asks");
    if (resize)
    {
        static_cast<void>(controller.apply(nenenib::application::OpenCommandPalette{}));
        static_cast<void>(controller.apply(nenenib::application::CommandText{":set fontsize=22"}));
        static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
    }
    const std::size_t asked = editing.text_layouts().requests();
    applied(controller, SwitchTab{0});
    const std::size_t switched = editing.text_layouts().requests() - asked;
    expect(controller.frame().first_visible == before,
           "a clamp asked while the question was open does not move the next document");
    return switched;
}

// 戻った文書のスクロールは切り替える前と同じで、切り替えは字を変えなかったときと同じ回数しか
// 組まない（持ち越した clamp の文字組みをしない）。
void verify_clamp_dropped_on_leaving_question()
{
    Editing resized;
    Editing kept;
    expect(switched_back_after_question(resized, true) == switched_back_after_question(kept, false),
           "leaving the question drops the pending clamp with the pending follow");
}

// Vim の INSERT の文字は表示領域を読まないので、鍵ごとに追従を解決せず layout も作らない
// （ADR 0129 の決定 2・#476 の速さの根拠）。frame を 1 回読むと、1
// 鍵ごとに読んだときと同じ所が見える。
[[nodiscard]] EditorFrame vim_typed_burst(Editing &editing, bool framed_each)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{40, 20, 96}});
    applied(controller, SelectEditMode{EditMode::vim});
    send_intent(controller, framed_each, VimKeyPress{VimKey{VimCharacter{U'i'}}});
    const std::size_t before = editing.text_layouts().requests();
    for (std::size_t typed = 1; typed <= 200; ++typed)
    {
        send_intent(controller, framed_each, VimKeyPress{VimKey{VimCharacter{U'x'}}});
        if (typed % 50 == 25)
        {
            send_intent(controller, framed_each,
                        VimKeyPress{VimKey{nenenib::core::VimSpecialKey::enter}});
        }
    }
    if (!framed_each)
    {
        expect(editing.text_layouts().requests() == before,
               "typing in Vim INSERT lays nothing out; the caret follow waits for the frame");
    }
    return controller.frame();
}

void verify_vim_insert_follow_settles_at_frame()
{
    Editing each;
    Editing burst;
    const auto framed = vim_typed_burst(each, true);
    const auto settled = vim_typed_burst(burst, false);
    expect(!framed.lines.empty() && !settled.lines.empty(), "both Vim views show lines");
    expect(settled.first_visible.value > 1, "the frame resolved the Vim follow and scrolled down");
    expect(framed.first_visible == settled.first_visible &&
               framed.lines.front().screen_row == settled.lines.front().screen_row,
           "one frame after 200 Vim keys shows what a frame after every key shows");
}

// 画面を読む鍵（H）は、頼まれた追従をその鍵の前に解決した表示領域を読む（ADR 0129 の決定 2）。
// frame を読まずに打った後の H も、1 鍵ごとに frame を読んだときと同じ行へ着く。
[[nodiscard]] EditorFrame vim_typed_then_high(Editing &editing, bool framed_each)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{40, 20, 96}});
    applied(controller, SelectEditMode{EditMode::vim});
    const auto send = [&controller, framed_each](const VimKey &key)
    { send_intent(controller, framed_each, VimKeyPress{key}); };
    send(VimKey{VimCharacter{U'i'}});
    for (std::size_t line = 0; line < 12; ++line)
    {
        send(VimKey{VimCharacter{U'a'}});
        send(VimKey{VimCharacter{U'b'}});
        send(VimKey{nenenib::core::VimSpecialKey::enter});
    }
    send(VimKey{nenenib::core::VimSpecialKey::escape});
    send(VimKey{VimCharacter{U'H'}});
    return controller.frame();
}

void verify_vim_viewport_settles_before_screen_keys()
{
    Editing each;
    Editing burst;
    const auto framed = vim_typed_then_high(each, true);
    const auto settled = vim_typed_then_high(burst, false);
    expect(framed.first_visible.value > 1, "typing twelve Vim lines scrolled the window down");
    expect(settled.caret.position.line == settled.first_visible &&
               settled.first_visible == framed.first_visible &&
               settled.caret.position.line == framed.caret.position.line,
           "H lands on the settled top line, not the one before the follow");
}

// Vim の問いの往復の破れの判定（ADR 0133 の追補 2026-10-18・#665）。viewport の二重要求は本物の
// core の engine には無い形なので、判定の純関数だけで確かめる。
void verify_vim_round_trip_judgements()
{
    using nenenib::application::TextLayoutFailure;
    const nenenib::core::VimViewport viewport{LineNumber{1}, 3, 0, 0, 0};
    expect(nenenib::application::vim_viewport_unasked(std::nullopt).has_value(),
           "#665 the first viewport request is answered");
    expect(nenenib::application::vim_viewport_unasked(viewport).error() ==
               TextLayoutFailure::inconsistent,
           "#665 (a) a viewport requested again is a broken round trip, not a layout failure");
    const std::vector<nenenib::core::VimLineRows> rows{{LineNumber{2}, 3}};
    expect(nenenib::application::vim_rows_unasked(rows, LineNumber{1}).has_value(),
           "#665 rows of a new line are answered");
    expect(nenenib::application::vim_rows_unasked(rows, LineNumber{2}).error() ==
               TextLayoutFailure::inconsistent,
           "#665 (b) rows of the same line asked again are a broken round trip");
    const std::vector<nenenib::core::VimRowSpan> spans{
        {LineNumber{1}, 0, Offset{0}, Offset{8}, 3, 0}};
    expect(nenenib::application::vim_span_unanswered(
               spans, {LineNumber{1}, 1, Offset{8}, Offset{16}, 3, 8})
               .has_value(),
           "#665 a new screen row is answered");
    expect(nenenib::application::vim_span_unanswered(spans,
                                                     {LineNumber{1}, 0, Offset{0}, Offset{8}, 3, 0})
                   .error() == TextLayoutFailure::inconsistent,
           "#665 (b) the same screen row answered again is a broken round trip");
    expect(nenenib::application::blocks_drawing(TextLayoutFailure::unavailable) &&
               !nenenib::application::blocks_drawing(TextLayoutFailure::inconsistent),
           "#665 only a layout that cannot be made blocks drawing");
}

// 番号の問いに違う段を答える文字組みでは、engine が同じ画面行を問い直し、controller は鍵を拒否して
// 1 行知らせる。破れは配達に 1 回だけ載り、frame と次の配達には載らない（作り直しを頼まない）。
void verify_vim_inconsistent_round_trip()
{
    using nenenib::application::TextLayoutFailure;
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{80, 20, 96}});
    static_cast<void>(controller.apply(InsertText{"abcdefghijklmnopqrstuvwxyz"}));
    applied(controller, SelectEditMode{EditMode::vim});
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'0'}}}));
    const auto before = controller.frame().caret.position;
    editing.text_layouts().misnumber_rows();
    static_cast<void>(controller.apply(VimKeyPress{VimKey{VimCharacter{U'g'}}}));
    const auto refused = controller.apply(VimKeyPress{VimKey{VimCharacter{U'j'}}});
    expect(refused.layout_failure == std::optional{TextLayoutFailure::inconsistent},
           "#665 a screen row asked again is delivered as inconsistent");
    expect(refused.command_message.has_value() &&
               refused.command_message.value().text() == "内部の不整合: 画面行の問い（鍵は無効）",
           "#665 a broken round trip raises the one-line notice");
    expect(!controller.delivery().layout_failure.has_value(),
           "#665 inconsistent is delivered once and not in the next delivery");
    const auto frame = controller.frame();
    expect(!frame.layout_failure.has_value() && frame.caret.position == before,
           "#665 inconsistent is not in the frame and the key is not applied");
    // layout を作れない失敗は今どおり frame に載る（ui は作り直す）。
    Editing failing;
    EditorController &rejected = failing.controller();
    applied(rejected, VisibleLines{3, nenenib::core::TextViewGeometry{80, 20, 96}});
    static_cast<void>(rejected.apply(InsertText{"abcdefghijklmnopqrstuvwxyz"}));
    applied(rejected, SelectEditMode{EditMode::vim});
    failing.text_layouts().reject();
    const auto unavailable = rejected.apply(VimKeyPress{VimKey{VimCharacter{U'M'}}});
    expect(unavailable.layout_failure == std::optional{TextLayoutFailure::unavailable} &&
               rejected.frame().layout_failure == std::optional{TextLayoutFailure::unavailable},
           "#665 a layout that cannot be made stays in the delivery and the frame");
}

// 字の大きさを変えた設定の clamp と追従は、窓が送る新しい行高の幾何を待って 1 回だけ解決する。
// 古い行高と新しい字の組み合わせで layout を作らないので、見える行は 1 行 1 回だけ組まれる
// （ADR 0128 の決定 8・#582）。
void verify_font_size_lays_out_once()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{400, 20, 96}});
    static_cast<void>(controller.apply(InsertText{"first"}));
    static_cast<void>(controller.apply(NewLine{}));
    static_cast<void>(controller.apply(InsertText{"second"}));
    static_cast<void>(controller.frame());
    static_cast<void>(controller.apply(nenenib::application::OpenCommandPalette{}));
    static_cast<void>(controller.apply(nenenib::application::CommandText{":set fontsize=22"}));
    const std::size_t before = editing.text_layouts().made();
    static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
    static_cast<void>(
        controller.apply(VisibleLines{2, nenenib::core::TextViewGeometry{400, 30, 96}}));
    const auto frame = controller.frame();
    expect(frame.settings.font_size.points() == 22 && frame.lines.size() == 2,
           "the new font size shows both lines");
    expect(editing.text_layouts().made() - before == frame.lines.size(),
           "each visible line is laid out once, with the new font and the new line height");
}

// 格子の画面移動（ADR 0144 の決定 5・6・9・#595）。1 升 10 画素（ScriptedTextLayouts）で 8 升の幅、
// 見えている画面行は 3。200 字の 1 行は 25 画面行で、キャレットは 0 行目の升 2（3
// 桁目）から始まる。
constexpr nenenib::core::TextViewGeometry page_grid_geometry{85, 20, 96};

void open_grid_line(Editing &editing, EditMode mode)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, page_grid_geometry});
    applied(controller, InsertText{std::string(200, 'a')});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{3}}, SelectionAnchoring::collapse});
    applied(controller, nenenib::application::SelectDisplay{nenenib::core::DisplayForm::grid});
    applied(controller, SelectEditMode{mode});
}

void press_vim(EditorController &controller, std::string_view keys)
{
    for (const auto &key : vim_keys_of(keys))
    {
        static_cast<void>(controller.apply(VimKeyPress{key}));
    }
}

// 最上段の画面行（first_row）とキャレットの桁。
[[nodiscard]] std::pair<std::size_t, std::size_t> grid_top_and_column(EditorController &controller)
{
    const auto frame = controller.frame();
    const std::size_t top = frame.grid.empty() ? 999 : frame.grid.front().row;
    return {top, frame.caret.position.column.value};
}

void verify_grid_vim_pages()
{
    Editing editing;
    open_grid_line(editing, EditMode::vim);
    EditorController &controller = editing.controller();
    press_vim(controller, "<PageDown>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{3, 27},
           "grid PgDn moves the top 3 screen rows and the caret 3 rows down on the same cell");
    press_vim(controller, "<C-d>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{4, 35},
           "grid Ctrl-d moves the top and the caret half a screen (1 row)");
    press_vim(controller, "<C-u>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{3, 27},
           "grid Ctrl-u moves back half a screen");
    press_vim(controller, "<C-b>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{0, 3},
           "grid Ctrl-b moves back a screen");
    press_vim(controller, "<PageUp>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{0, 3},
           "grid PgUp at the first screen row changes nothing");
    press_vim(controller, "99<C-f>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{24, 195},
           "grid Ctrl-f stops when the last screen row is the top");
    press_vim(controller, "<PageDown>x");
    expect(grid_top_and_column(controller).first == 24 && controller.text().size_bytes() == 199,
           "grid PgDn at the end changes nothing and does not fail the next key");
}

void verify_grid_vim_page_visual_and_dot()
{
    Editing editing;
    open_grid_line(editing, EditMode::vim);
    EditorController &controller = editing.controller();
    press_vim(controller, "v<PageDown>");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{3, 27},
           "grid PgDn in VISUAL moves the caret 3 rows");
    press_vim(controller, "d");
    expect(controller.text().size_bytes() == 175,
           "the VISUAL range of grid PgDn keeps the anchor (columns 3 to 27 are removed)");
    press_vim(controller, "x<PageDown>.");
    expect(controller.text().size_bytes() == 173 && grid_top_and_column(controller).first == 3,
           "a grid PgDn between x and . is not a change; . repeats x");
    Editing inserting;
    open_grid_line(inserting, EditMode::vim);
    press_vim(inserting.controller(), "i<PageDown>");
    expect(grid_top_and_column(inserting.controller()) ==
               std::pair<std::size_t, std::size_t>{3, 27},
           "grid PgDn in INSERT moves by screen rows too");
}

// 通常モードの PgDn は画面の先頭も 1 画面ぶん送る（ADR 0144 の決定 7・D55）。普通の文書と格子。
void verify_ordinary_pages()
{
    Editing plain;
    EditorController &controller = plain.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{400, 20, 96}});
    for (std::size_t line = 0; line < 9; ++line)
    {
        applied(controller, InsertText{"x"});
        applied(controller, NewLine{});
    }
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    applied(controller, MoveCaret{CaretMotion::page_down, SelectionAnchoring::collapse});
    const auto down = controller.frame();
    expect(down.first_visible == LineNumber{4} && down.caret.position.line == LineNumber{4},
           "the first ordinary PgDn moves the top and the caret a whole screen");
    applied(controller, MoveCaret{CaretMotion::page_up, SelectionAnchoring::collapse});
    const auto up = controller.frame();
    expect(up.first_visible == LineNumber{1} && up.caret.position.line == LineNumber{1},
           "ordinary PgUp moves back a whole screen");
    Editing grid;
    open_grid_line(grid, EditMode::ordinary);
    applied(grid.controller(), MoveCaret{CaretMotion::page_down, SelectionAnchoring::collapse});
    expect(grid_top_and_column(grid.controller()) == std::pair<std::size_t, std::size_t>{3, 27},
           "ordinary PgDn in the grid moves the top and the caret 3 screen rows");
}

// 本文を開いて格子で見せ、Vim にする（ADR 0144 の決定 9 の追補・レビュー #630）。しきい値を越える
// 行は開くと問いが出て、grid の答えで格子になる（ADR 0140）。
// ファイルを開いて表示の形を選び（巨大な行の問いには答える）、Vim にする。
void open_display_file(Editing &editing, std::string text, std::size_t visible,
                       nenenib::core::DisplayForm form)
{
    editing.files().hold_at("C:\\work\\grid.txt", Bytes{std::move(text)});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{visible, page_grid_geometry});
    applied(controller, OpenDocument{nenenib::core::FilePath::parse("C:\\work\\grid.txt").value()});
    applied(controller, nenenib::application::SelectDisplay{form});
    applied(controller, SelectEditMode{EditMode::vim});
}

void open_grid_file(Editing &editing, std::string text, std::size_t visible)
{
    open_display_file(editing, std::move(text), visible, nenenib::core::DisplayForm::grid);
}

// 格子の gj gk（ADR 0144 の決定 3・4・9・#596）。8 升の幅に巨大な行（「あ」と a の列・しきい値を
// 越える）を格子で開く。0 画面行は「あ」（升 0・1）と a が 6 つ、以後の画面行は a が 8
// つ。キャレットは 0 画面行の升 3（3 桁目）から始まる。
void open_grid_display_line(Editing &editing)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, page_grid_geometry});
    applied(controller,
            InsertText{"あ" + std::string(nenenib::core::giant_line_threshold + 8, 'a')});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{1}, Column{3}}, SelectionAnchoring::collapse});
    applied(controller, nenenib::application::SelectDisplay{nenenib::core::DisplayForm::grid});
    applied(controller, SelectEditMode{EditMode::vim});
}

void place_at(EditorController &controller, std::size_t line, std::size_t column)
{
    applied(controller, PlaceCaret{TextPosition{LineNumber{line}, Column{column}},
                                   SelectionAnchoring::collapse});
}

using RowPair = std::pair<std::size_t, std::size_t>;

// キャレットの行と桁。
[[nodiscard]] RowPair caret_of(EditorController &controller)
{
    const auto caret = controller.frame().caret.position;
    return {caret.line.value, caret.column.value};
}

// しきい値を越える実際の巨大な行と、64 KiB の塊をまたぐ PgDn（I-4・I-5）。
void verify_grid_giant_pages()
{
    Editing giant;
    open_grid_file(giant, std::string(nenenib::core::giant_line_threshold + 1, 'a'), 3);
    press_vim(giant.controller(), "<PageDown>");
    expect(grid_top_and_column(giant.controller()) == RowPair{3, 25},
           "PgDn on a line past the threshold shown as the grid moves 3 screen rows");
    Editing chunked;
    open_grid_file(chunked, std::string(70'000, 'a'), 3);
    EditorController &controller = chunked.controller();
    place_at(controller, 1, 65'531);
    const std::size_t before = grid_top_and_column(controller).first;
    press_vim(controller, "<PageDown>");
    expect(grid_top_and_column(controller) == RowPair{before + 3, 65'555},
           "PgDn from the last row of the first 64 KiB chunk lands on the same cell 3 rows down");
}

// 5 行の格子。行をまたいで画面行を数え、99<C-f> は最後の画面行を先頭にして終わる（I-1・Q-4）。
void verify_grid_many_lines()
{
    Editing editing;
    std::string text;
    for (std::size_t line = 0; line < 5; ++line)
    {
        text += std::string(20, 'a') + (line < 4 ? "\n" : "");
    }
    open_grid_file(editing, std::move(text), 3);
    EditorController &controller = editing.controller();
    press_vim(controller, "<PageDown>");
    expect(controller.frame().first_visible == LineNumber{2} &&
               grid_top_and_column(controller).first == 0 && caret_of(controller) == RowPair{2, 1},
           "PgDn over a line of 3 screen rows moves the top and the caret to the next line");
    press_vim(controller, "99<C-f>");
    expect(controller.frame().first_visible == LineNumber{5} &&
               grid_top_and_column(controller).first == 2 && caret_of(controller) == RowPair{5, 17},
           "99 Ctrl-f over 5 lines ends with the last screen row at the top");
}

// 欲しい列（決定 3 の追補・I-2）。$ の後は画面行の末尾、k の後は覚えた欲しい列、Tab
// と全角は仮想桁。
void verify_grid_wanted_column()
{
    const std::string forty(40, 'a');
    Editing ended;
    open_grid_file(ended, forty + "\n" + forty + "\n" + forty, 3);
    press_vim(ended.controller(), "$<PageDown>");
    expect(caret_of(ended.controller()) == RowPair{2, 24},
           "PgDn after $ lands on the end of the screen row");
    press_vim(ended.controller(), "<PageDown>");
    expect(caret_of(ended.controller()) == RowPair{3, 8}, "the next PgDn keeps the line end wish");
    Editing raised;
    open_grid_file(raised, "abc\n" + forty + "\n" + forty, 3);
    place_at(raised.controller(), 2, 7);
    press_vim(raised.controller(), "k<PageDown>");
    expect(caret_of(raised.controller()) == RowPair{2, 23},
           "PgDn after k lands on the wanted column, not on the short line's column");
    Editing mixed;
    std::string rows;
    for (std::size_t pair = 0; pair < 6; ++pair)
    {
        rows += "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"
                "ab"
                "a\t";
    }
    open_grid_file(mixed, std::move(rows), 3);
    place_at(mixed.controller(), 1, 3);
    press_vim(mixed.controller(), "<PageDown>");
    expect(caret_of(mixed.controller()) == RowPair{1, 14},
           "PgDn from a wide character lands on the Tab covering the same virtual column");
    press_vim(mixed.controller(), "<PageUp>");
    expect(caret_of(mixed.controller()) == RowPair{1, 5},
           "PgUp from the Tab goes to the Tab's last column on the wide row");
}

// 半画面（決定 5 の追補・Q-1）。回数は 'scroll' を置き換え、端では画面が止まりキャレットだけ動く。
void verify_grid_half_pages()
{
    Editing counted;
    open_grid_line(counted, EditMode::vim);
    press_vim(counted.controller(), "3<C-d>");
    expect(grid_top_and_column(counted.controller()) == RowPair{3, 27},
           "3 Ctrl-d moves the top and the caret 3 screen rows");
    press_vim(counted.controller(), "<C-d>");
    expect(grid_top_and_column(counted.controller()) == RowPair{6, 51},
           "the next Ctrl-d moves by the replaced 'scroll'");
    Editing bottom;
    open_grid_line(bottom, EditMode::vim);
    EditorController &controller = bottom.controller();
    press_vim(controller, "7<C-f><C-d>");
    expect(grid_top_and_column(controller) == RowPair{22, 179},
           "Ctrl-d scrolls until the last screen row is shown");
    press_vim(controller, "<C-d>");
    expect(grid_top_and_column(controller) == RowPair{22, 187},
           "Ctrl-d at the end moves only the caret");
    press_vim(controller, "<C-d><C-d>");
    expect(grid_top_and_column(controller) == RowPair{22, 195},
           "the caret stops at the last screen row");
    Editing top;
    open_grid_line(top, EditMode::vim);
    place_at(top.controller(), 1, 19);
    press_vim(top.controller(), "<C-u>");
    expect(grid_top_and_column(top.controller()) == RowPair{0, 11},
           "Ctrl-u at the first screen row moves only the caret");
}

// VISUAL の V と矩形は anchor を保つ。d<C-d> は何もしない（スクロールは motion でない・Vim
// と同じ）。
void verify_grid_visual_and_operator()
{
    Editing lined;
    open_grid_line(lined, EditMode::vim);
    press_vim(lined.controller(), "V<PageDown>");
    expect(grid_top_and_column(lined.controller()) == RowPair{3, 27},
           "PgDn in VISUAL line moves the caret 3 rows");
    press_vim(lined.controller(), "o");
    expect(grid_top_and_column(lined.controller()).second == 3,
           "o after PgDn in VISUAL line goes back to the kept anchor");
    Editing blocked;
    open_grid_line(blocked, EditMode::vim);
    press_vim(blocked.controller(), "<C-v><PageDown>d");
    expect(blocked.controller().text().size_bytes() == 175,
           "the block of grid PgDn keeps the anchor (columns 3 to 27 are removed)");
    Editing pending;
    open_grid_line(pending, EditMode::vim);
    press_vim(pending.controller(), "d<C-d>");
    expect(pending.controller().text().size_bytes() == 200 &&
               grid_top_and_column(pending.controller()) == RowPair{0, 3},
           "d Ctrl-d changes nothing");
    press_vim(pending.controller(), "x");
    expect(pending.controller().text().size_bytes() == 199, "the operator does not linger");
}

// 通常モードの端（Q-2・決定 5 の追補）。画面は末尾を埋めたまま、キャレットは最後の画面行。
void verify_ordinary_page_end()
{
    Editing plain;
    EditorController &controller = plain.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{400, 20, 96}});
    for (std::size_t line = 0; line < 9; ++line)
    {
        applied(controller, InsertText{"x"});
        applied(controller, NewLine{});
    }
    applied(controller, MoveCaret{CaretMotion::document_start, SelectionAnchoring::collapse});
    for (std::size_t press = 0; press < 5; ++press)
    {
        applied(controller, MoveCaret{CaretMotion::page_down, SelectionAnchoring::collapse});
    }
    expect(controller.frame().first_visible == LineNumber{8} &&
               controller.frame().caret.position.line == LineNumber{10},
           "ordinary PgDn at the end keeps the last screen filled with the caret on the last line");
    Editing grid;
    open_grid_line(grid, EditMode::ordinary);
    for (std::size_t press = 0; press < 10; ++press)
    {
        applied(grid.controller(), MoveCaret{CaretMotion::page_down, SelectionAnchoring::collapse});
    }
    expect(grid_top_and_column(grid.controller()) == RowPair{22, 195},
           "ordinary PgDn in the grid stops with the last 3 screen rows shown");
}

void verify_grid_display_moves()
{
    Editing editing;
    open_grid_display_line(editing);
    EditorController &controller = editing.controller();
    vim_normal(controller, "gj");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{0, 11},
           "grid gj moves to the same cell of the next screen row");
    vim_normal(controller, "gk");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{0, 3},
           "grid gk moves back to the same cell");
    vim_normal(controller, "3gj");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{1, 27},
           "grid 3gj moves 3 screen rows and the view follows the caret");
    vim_normal(controller, "0gjgk");
    expect(grid_top_and_column(controller).second == 1,
           "grid gk onto the right half of a wide character lands on its head");
    vim_normal(controller, "100000gkx");
    expect(grid_top_and_column(controller) == std::pair<std::size_t, std::size_t>{0, 1} &&
               controller.text().size_bytes() == nenenib::core::giant_line_threshold + 11,
           "grid gk past the first screen row moves to it, fails and drops the next key");
    vim_normal(controller, "$gjx");
    expect(controller.text().size_bytes() == nenenib::core::giant_line_threshold + 11,
           "grid gj on the last screen row fails and drops the next key");
}

void verify_grid_display_ranges()
{
    Editing removing;
    open_grid_display_line(removing);
    vim_normal(removing.controller(), "dgj");
    expect(removing.controller().vim_state().unnamed_register.value().text == std::string(8, 'a') &&
               grid_top_and_column(removing.controller()).second == 3,
           "grid dgj removes up to the same cell of the next screen row, exclusive");
    Editing selecting;
    open_grid_display_line(selecting);
    EditorController &controller = selecting.controller();
    vim_normal(controller, "vgj");
    expect(grid_top_and_column(controller).second == 11,
           "grid vgj moves the caret to the same cell of the next screen row");
    vim_normal(controller, "o");
    expect(grid_top_and_column(controller).second == 3, "grid vgj keeps the anchor");
    vim_normal(controller, "od");
    expect(controller.vim_state().unnamed_register.value().text == std::string(9, 'a'),
           "the VISUAL range of grid gj includes the landing character");
}

// 格子の $ の後の gk gj（決定 3 の追補・#596 の差し戻し 2）。最後の画面行は a が 2 つで、$ の後の
// gk は 1 つ上の画面行の末尾、続く gj は行末の印を保って行末へ戻る。
void verify_grid_display_line_end()
{
    Editing editing;
    open_grid_display_line(editing);
    EditorController &controller = editing.controller();
    vim_normal(controller, "$");
    const std::size_t end = nenenib::core::giant_line_threshold + 9;
    expect(caret_of(controller) == RowPair{1, end}, "$ goes to the last character of the line");
    vim_normal(controller, "gk");
    expect(caret_of(controller) == RowPair{1, end - 2},
           "grid gk after $ lands on the end of the screen row above");
    vim_normal(controller, "gj");
    expect(caret_of(controller) == RowPair{1, end}, "grid gj keeps the line end wish of $");
}

// 論理行をまたぐ格子の gj gk（決定 3・4）。1 行目は 10 字（8 + 2）、2 行目は 20 字（8 + 8 + 4）、
// 3 行目は 3 字。1 行目の最後の画面行の升 1 から始める。
void verify_grid_display_lines()
{
    Editing editing;
    open_grid_file(editing, std::string(10, 'a') + "\n" + std::string(20, 'b') + "\nccc", 5);
    EditorController &controller = editing.controller();
    place_at(controller, 1, 10);
    vim_normal(controller, "gj");
    expect(caret_of(controller) == RowPair{2, 2},
           "grid gj from the last screen row of a line goes to the first screen row of the next");
    vim_normal(controller, "gk");
    expect(caret_of(controller) == RowPair{1, 10}, "grid gk goes back to the same cell");
    vim_normal(controller, "3gj");
    expect(caret_of(controller) == RowPair{2, 18},
           "grid 3gj crosses the line and stops on the last screen row of the next line");
    vim_normal(controller, "gj");
    expect(caret_of(controller) == RowPair{3, 2}, "grid gj goes on to the third line");
    vim_normal(controller, "gk");
    expect(caret_of(controller) == RowPair{2, 18}, "grid gk comes back to the same cell");
    vim_normal(controller, "ggVgjd");
    expect(controller.text().text() == std::string(20, 'b') + "\nccc",
           "grid Vgj within the first line removes that line only");
}

// 巨大な行の窓の形（text・いいえ）は画面行が 1 つで、gj gk は j k と同じ論理行の道で着く
// （I-1・決定 2）。1 行だけなら失敗して次の鍵を捨てる。
void verify_giant_window_display_moves()
{
    const std::string giant(nenenib::core::giant_line_threshold + 1, 'a');
    Editing alone;
    open_display_file(alone, giant, 3, nenenib::core::DisplayForm::text);
    place_at(alone.controller(), 1, 7);
    vim_normal(alone.controller(), "gjx");
    expect(caret_of(alone.controller()) == RowPair{1, 7} &&
               alone.controller().text().size_bytes() == giant.size(),
           "gj on the only giant line in the window form fails like j and drops the next key");
    constexpr std::array<std::pair<std::string_view, std::string_view>, 3> keys{
        {{"gj", "j"}, {"gjgj", "jj"}, {"2gjgk", "2jk"}}};
    for (const auto &[display, line] : keys)
    {
        Editing shown;
        open_display_file(shown, "head\n" + giant + "\ntail", 3, nenenib::core::DisplayForm::text);
        place_at(shown.controller(), 1, 3);
        vim_normal(shown.controller(), display);
        Editing plain;
        open_display_file(plain, "head\n" + giant + "\ntail", 3, nenenib::core::DisplayForm::text);
        place_at(plain.controller(), 1, 3);
        vim_normal(plain.controller(), line);
        expect(caret_of(shown.controller()) == caret_of(plain.controller()),
               "gj gk across a giant line in the window form land like j k");
    }
}

// 文字組みが無効な view（幅 0・nowrap の text）では gj gk は j k に読み替わる（決定 4・I-2）。
// 1 行目は 30 字、2 行目は 2 字、3 行目は 30 字。幅 85 画素は 8 升なので、折り返せば 1 行目は 4
// 画面行になる。
void open_unlaid(Editing &editing, std::int32_t width, bool unwrapped)
{
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, nenenib::core::TextViewGeometry{width, 20, 96}});
    applied(controller, InsertText{std::string(30, 'a')});
    applied(controller, NewLine{});
    applied(controller, InsertText{"bb"});
    applied(controller, NewLine{});
    applied(controller, InsertText{std::string(30, 'c')});
    if (unwrapped)
    {
        static_cast<void>(controller.apply(nenenib::application::OpenCommandPalette{}));
        static_cast<void>(controller.apply(nenenib::application::CommandText{":set nowrap"}));
        static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
    }
    applied(controller, SelectEditMode{EditMode::vim});
    place_at(controller, 1, 13);
}

[[nodiscard]] bool same_registers(EditorController &left, EditorController &right)
{
    const auto &one = left.vim_state().unnamed_register.value();
    const auto &other = right.vim_state().unnamed_register.value();
    return one.text == other.text && one.kind == other.kind;
}

void verify_unlaid_display_moves()
{
    Editing wrapped;
    open_unlaid(wrapped, 85, false);
    vim_normal(wrapped.controller(), "gj");
    expect(caret_of(wrapped.controller()).first == 1, "the wrapped sample moves gj by screen rows");
    constexpr std::array<std::pair<std::string_view, std::string_view>, 5> keys{
        {{"gj", "j"}, {"gk", "k"}, {"2gjgk", "2jk"}, {"3gjx", "3jx"}, {"dgj", "dj"}}};
    for (const bool unwrapped : {false, true})
    {
        for (const auto &[display, line] : keys)
        {
            Editing shown;
            open_unlaid(shown, unwrapped ? 85 : 0, unwrapped);
            vim_normal(shown.controller(), display);
            Editing plain;
            open_unlaid(plain, unwrapped ? 85 : 0, unwrapped);
            vim_normal(plain.controller(), line);
            expect(caret_of(shown.controller()) == caret_of(plain.controller()) &&
                       shown.controller().text().text() == plain.controller().text().text() &&
                       same_registers(shown.controller(), plain.controller()),
                   "gj gk without a layout (width 0, nowrap) act as j k, dgj linewise like dj");
        }
    }
}

// ---------------------------------------------- 窓と wrap off の横送り（ADR 0145・#634）

// 1 升 10 画素・幅 40 升の窓（平均の字幅も 10 画素）。
constexpr nenenib::core::TextViewGeometry unwrapped_geometry{400, 20, 96, 10.0F};

void open_unwrapped(Editing &editing, std::string text, std::size_t visible)
{
    editing.files().hold_at("C:\\work\\wide.txt", Bytes{std::move(text)});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{visible, unwrapped_geometry});
    applied(controller, OpenDocument{nenenib::core::FilePath::parse("C:\\work\\wide.txt").value()});
    static_cast<void>(controller.apply(nenenib::application::OpenCommandPalette{}));
    static_cast<void>(controller.apply(nenenib::application::CommandText{":set nowrap"}));
    static_cast<void>(controller.apply(nenenib::application::SubmitCommand{}));
    applied(controller, SelectEditMode{EditMode::vim});
}

// 巨大な行の窓（text 形・いいえ）。折り返す設定のままでも横に送る（ADR 0139 の決定 5）。
void open_giant_window(Editing &editing, std::string text)
{
    editing.files().hold_at("C:\\work\\giant.txt", Bytes{std::move(text)});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{3, unwrapped_geometry});
    applied(controller,
            OpenDocument{nenenib::core::FilePath::parse("C:\\work\\giant.txt").value()});
    applied(controller, nenenib::application::SelectDisplay{nenenib::core::DisplayForm::text});
    applied(controller, SelectEditMode{EditMode::vim});
}

// 対の関数 horizontal_of_column / column_of_horizontal の往復（決定 8）。zl を 1 つずつ打つたびに
// 横の送りがちょうど 1 升ずつ増え、揺れない。左端で半分欠けた全角の leftcol も同じ値に戻る。
void verify_horizontal_round_trip()
{
    Editing editing;
    std::string wide;
    for (std::size_t index = 0; index < 60; ++index)
    {
        wide += "あ";
    }
    open_unwrapped(editing, std::string(300, 'a') + "\n" + wide, 3);
    auto &controller = editing.controller();
    bool steady = true;
    for (std::size_t step = 1; step <= 45; ++step)
    {
        press_vim(controller, "zl");
        steady = steady && controller.frame().horizontal == static_cast<float>(step * 10);
    }
    expect(steady && caret_of(controller) == RowPair{1, 46},
           "each zl adds exactly one cell to the unwrapped scroll and pushes the caret");
    place_at(controller, 2, 1);
    for (std::size_t step = 1; step <= 5; ++step)
    {
        press_vim(controller, "zl");
        steady = steady && controller.frame().horizontal == static_cast<float>(step * 10);
    }
    for (std::size_t step = 1; step <= 5; ++step)
    {
        press_vim(controller, "zh");
        steady = steady && controller.frame().horizontal == static_cast<float>((5 - step) * 10);
    }
    expect(steady, "a leftcol inside a fullwidth character survives the pixel round trip");
}

// 巨大な行で押されたキャレットが錨の刻み（2,048）をまたいでも、横位置は新しい窓の始まりで計り直し、
// 画面は窓の中にある（決定 9）。
void verify_giant_horizontal_scroll()
{
    Editing editing;
    open_giant_window(editing, std::string(nenenib::core::giant_line_threshold + 1, 'a'));
    auto &controller = editing.controller();
    place_at(controller, 1, 6001);
    const auto inside = [&controller](std::size_t left)
    {
        const auto frame = controller.frame();
        const auto &window = frame.lines.front().window;
        const auto shown = window.column + static_cast<std::size_t>(frame.horizontal / 10.0F);
        return window.active && frame.horizontal >= 0.0F && shown == left &&
               shown + 40 <= window.end_column;
    };
    press_vim(controller, "zs");
    expect(inside(6000) && caret_of(controller) == RowPair{1, 6001},
           "zs on a giant line puts the caret at the left edge inside its window");
    press_vim(controller, "300zl");
    expect(inside(6300) && caret_of(controller) == RowPair{1, 6301},
           "a caret pushed across the anchor step keeps the screen inside the new window");
    const auto before = controller.frame().horizontal;
    press_vim(controller, "zh");
    expect(inside(6299) && controller.frame().horizontal == before - 10.0F &&
               caret_of(controller) == RowPair{1, 6301},
           "the next zh measures from the new window origin");
}

// 横に送らない文書（wrap on の普通の行・格子）では 6 鍵は何もせず、失敗にもならない（決定 2）。
void verify_horizontal_scroll_without_columns()
{
    Editing wrapped;
    wrapped.files().hold_at("C:\\work\\wrapped.txt", Bytes{std::string(300, 'a')});
    auto &controller = wrapped.controller();
    applied(controller, VisibleLines{3, unwrapped_geometry});
    applied(controller,
            OpenDocument{nenenib::core::FilePath::parse("C:\\work\\wrapped.txt").value()});
    applied(controller, SelectEditMode{EditMode::vim});
    place_at(controller, 1, 100);
    vim_normal(controller, "zlzhzLzHzszex");
    expect(controller.text().size_bytes() == 299 && caret_of(controller) == RowPair{1, 100} &&
               controller.frame().horizontal == 0.0F,
           "horizontal scroll keys do nothing on a wrapped document and do not fail");
    Editing grid;
    open_grid_file(grid, std::string(nenenib::core::giant_line_threshold + 1, 'a'), 3);
    place_at(grid.controller(), 1, 5);
    vim_normal(grid.controller(), "zlzszex");
    expect(grid.controller().text().size_bytes() == nenenib::core::giant_line_threshold &&
               caret_of(grid.controller()) == RowPair{1, 5} &&
               grid.controller().frame().horizontal == 0.0F,
           "horizontal scroll keys do nothing on the grid and do not fail");
}

// 縦の移動（first_column 無し）は横位置を 0 に戻さず、キャレットが見える最小の送りへ
// 寄せる（決定 8）。
void verify_vertical_moves_keep_horizontal()
{
    Editing editing;
    std::string text;
    for (std::size_t line = 0; line < 12; ++line)
    {
        text += std::string(250, ' ') + std::string(50, 'x') + (line + 1 < 12 ? "\n" : "");
    }
    open_unwrapped(editing, std::move(text), 3);
    auto &controller = editing.controller();
    place_at(controller, 1, 251);
    const auto before = controller.frame().horizontal;
    press_vim(controller, "<C-f>");
    const auto frame = controller.frame();
    expect(before > 0.0F && frame.caret.position.line.value > 1 &&
               frame.caret.position.column.value == 251 && frame.horizontal == before,
           "Ctrl-f keeps the unwrapped horizontal scroll that already shows the caret");
    press_vim(controller, "02<C-f>");
    const auto start = controller.frame();
    expect(start.horizontal <= 2500.0F && 2500.0F <= start.horizontal + 397.0F &&
               start.caret.position.column.value == 251,
           "Ctrl-f after 0 follows the caret back into view without resetting to the line start");
}

void verify_vim_horizontal_scroll()
{
    verify_horizontal_round_trip();
    verify_giant_horizontal_scroll();
    verify_horizontal_scroll_without_columns();
    verify_vertical_moves_keep_horizontal();
}

void verify_screen_row_pages()
{
    verify_grid_vim_pages();
    verify_grid_vim_page_visual_and_dot();
    verify_ordinary_pages();
    verify_grid_giant_pages();
    verify_grid_many_lines();
    verify_grid_wanted_column();
    verify_grid_half_pages();
    verify_grid_visual_and_operator();
    verify_ordinary_page_end();
    verify_grid_display_moves();
    verify_grid_display_ranges();
    verify_grid_display_line_end();
    verify_grid_display_lines();
    verify_giant_window_display_moves();
    verify_unlaid_display_moves();
    verify_vim_horizontal_scroll();
}

void verify_controller_frame()
{
    Editing editing;
    EditorController &controller = editing.controller();
    const auto frame = controller.apply_frame(VisibleLines{5});
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
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.caret.shape == CaretShape::block, "the Vim caret is a block");
    expect(!(vim.caret == frame.caret), "caret views compare on the shape too");
}

void verify_controller_mode_selection()
{
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().mode == EditMode::ordinary, "the editor starts in ordinary mode");
    expect(controller.frame().mode_label == "通常", "the initial label is 通常");
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.mode == EditMode::vim && vim.mode_label == "NORMAL", "select vim enters vim mode");
    const auto again = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(again.mode == EditMode::vim, "selecting the mode already in force changes nothing");
    const auto ordinary = controller.apply_frame(SelectEditMode{EditMode::ordinary});
    expect(ordinary.mode == EditMode::ordinary, "select ordinary returns to ordinary");
    expect(ordinary.palette.background == RgbColor{0x30, 0x0A, 0x24},
           "the palette survives the mode change");
}

// 100 行の本文を幅の決まった窓（5 行・折り返さない）で開き、キャレットを行 caret_line の 4 桁目に
// 置く（#785 の足場）。
void open_hundred_lines(Editing &editing, std::size_t caret_line)
{
    std::string lines;
    for (std::size_t line = 0; line < 100; ++line)
    {
        lines += "abcdefgh\n";
    }
    editing.files().hold(Bytes{std::move(lines)});
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{5, nenenib::core::TextViewGeometry{400, 20, 96}});
    applied(controller, OpenDocument{sample_path()});
    for (std::size_t line = 1; line < caret_line; ++line)
    {
        applied(controller, MoveCaret{CaretMotion::next_line, SelectionAnchoring::collapse});
    }
    for (std::size_t step = 0; step < 3; ++step)
    {
        applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::collapse});
    }
}

[[nodiscard]] bool nothing_selected(const EditorFrame &frame)
{
    return std::ranges::all_of(frame.lines, [](const auto &line)
                               { return line.selection.presence == SelectionPresence::absent; });
}

// 通常 → Vim は画面を動かさず、外れたキャレットを見えている範囲の端の行へ入れる（#785）。
void verify_mode_switch_keeps_view_below()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_hundred_lines(editing, 1);
    applied(controller, MoveCaret{CaretMotion::next_character, SelectionAnchoring::extend});
    applied(controller, ScrollLines{200});
    const LineNumber top = controller.frame().first_visible;
    expect(top.value > 1, "the wheel left the caret above the window");
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.first_visible == top, "entering Vim keeps the window where the wheel left it");
    expect(vim.caret.position == TextPosition{top, Column{5}},
           "the caret comes into the first visible line at its column");
    expect(nothing_selected(vim), "entering Vim still collapses the selection");
}

// Vim → 通常は画面もキャレットも動かさない（#785）。
void verify_mode_switch_back_keeps_view()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_hundred_lines(editing, 1);
    applied(controller, ScrollLines{200});
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    const auto back = controller.apply_frame(SelectEditMode{EditMode::ordinary});
    expect(back.first_visible == vim.first_visible && back.caret.position == vim.caret.position,
           "returning to ordinary moves neither the window nor the caret");
}

void verify_mode_switch_keeps_view_above()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_hundred_lines(editing, 100);
    applied(controller, ScrollLines{-200});
    const auto before = controller.frame();
    expect(before.first_visible == LineNumber{1}, "the wheel left the caret below the window");
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.first_visible == LineNumber{1}, "entering Vim keeps the window at the top");
    expect(vim.caret.position == TextPosition{LineNumber{5}, Column{4}},
           "the caret comes into the last visible line at its column");
}

void verify_mode_switch_keeps_visible_caret()
{
    Editing editing;
    EditorController &controller = editing.controller();
    open_hundred_lines(editing, 3);
    applied(controller, MoveCaret{CaretMotion::line_end, SelectionAnchoring::collapse});
    const auto vim = controller.apply_frame(SelectEditMode{EditMode::vim});
    expect(vim.first_visible == LineNumber{1}, "a visible caret leaves the window alone");
    expect(vim.caret.position == TextPosition{LineNumber{3}, Column{8}},
           "a visible caret only rests on the last character");
}

void verify_document_open()
{
    Editing editing;
    EditorController &controller = editing.controller();
    editing.files().hold(std::string("一行目\r\n二行目"));
    const auto frame = controller.apply_frame(OpenDocument{sample_path()});
    expect(editing.files().read_path() == "C:\\work\\note.txt", "the path went through the port");
    static_assert(nenenib::core::maximum_document_bytes == std::size_t{1024} * 1024U * 1024U,
                  "ADR 0159 decision 7 raises the open limit to 1 GiB (D63)");
    static_assert(nenenib::core::maximum_decoded_document_bytes == std::size_t{64} * 1024U * 1024U,
                  "ADR 0159 decision 7 keeps 64 MiB for bodies that need decoding");
    expect(editing.files().read_limit() == nenenib::core::maximum_document_bytes,
           "the 1 GiB limit is handed to the port, not measured after the read");
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
    const auto bom = with_bom.controller().apply_frame(OpenDocument{sample_path()});
    expect(bom.document.encoding == TextEncoding::utf8_bom, "the BOM is remembered");
    expect(bom.lines.at(0).text == "本文", "the BOM is not part of the body");
    expect(bom.status_items.at(2).text() == "LF", "an LF file keeps LF");
    Editing japanese;
    japanese.files().hold(std::string("\x93\xFA\x96\x7B"));
    japanese.code_pages().decode_to(std::string("日本"));
    const auto read = japanese.controller().apply_frame(OpenDocument{sample_path()});
    expect(read.document.encoding == TextEncoding::shift_jis, "CP932 bytes open as Shift_JIS");
    expect(read.lines.at(0).text == "日本", "the port turned them into UTF-8");
    expect(read.status_items.at(1).text() == "Shift_JIS", "the status bar says so");
    // 開いてもモードは保たれる（決定 8）。
    Editing vim;
    vim.files().hold(std::string("x"));
    static_cast<void>(vim.controller().apply_frame(SelectEditMode{EditMode::vim}));
    const auto kept = vim.controller().apply_frame(OpenDocument{sample_path()});
    expect(kept.mode == EditMode::vim, "opening a file keeps the editing mode");
}

void expect_open_failure(Bytes content, FileFailure expected, const char *description)
{
    Editing editing;
    editing.files().hold(std::move(content));
    const auto frame = editing.controller().apply_frame(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == expected,
           description);
    expect(frame.document.title.text() == "無題", "a failed open does not change the tab");
    expect(frame.total_lines == 1 && frame.lines.at(0).text.empty(),
           "a failed open does not change the body");
}

// 上限は 2 つ（ADR 0159 の決定 7）。UTF-8 / UTF-8 BOM は開く上限（1 GiB・port が読む前に断る）
// だけ、Shift_JIS / UTF-16 は入力のバイト数で 64 MiB まで。境界の値はちょうどが通り 1 つ先が断る。
void verify_document_open_limits()
{
    constexpr std::size_t decoded = nenenib::core::maximum_decoded_document_bytes;
    constexpr std::size_t whole = nenenib::core::maximum_document_bytes;
    using nenenib::application::exceeds_decoded_limit;
    expect(!exceeds_decoded_limit(TextEncoding::shift_jis, decoded) &&
               exceeds_decoded_limit(TextEncoding::shift_jis, decoded + 1U) &&
               !exceeds_decoded_limit(TextEncoding::utf16_le, decoded) &&
               exceeds_decoded_limit(TextEncoding::utf16_le, decoded + 1U) &&
               !exceeds_decoded_limit(TextEncoding::utf16_be, decoded) &&
               exceeds_decoded_limit(TextEncoding::utf16_be, decoded + 1U),
           "Shift_JIS and UTF-16 pass at 64 MiB and stop one byte past it");
    expect(!exceeds_decoded_limit(TextEncoding::utf8, whole) &&
               !exceeds_decoded_limit(TextEncoding::utf8_bom, whole) &&
               !exceeds_decoded_limit(TextEncoding::utf8, decoded + 1U),
           "UTF-8 is bound only by the open limit");
    // CP932 の 2 バイト文字を並べた本文。ちょうど 64 MiB は開き、2 バイト先は断る。
    std::string japanese;
    japanese.reserve(decoded + 2U);
    for (std::size_t at = 0; at < decoded / 2U; ++at)
    {
        japanese.append("\x93\xFA");
    }
    Editing at_limit;
    at_limit.files().hold(japanese);
    at_limit.code_pages().decode_to(std::string("日"));
    const auto opened = at_limit.controller().apply_frame(OpenDocument{sample_path()});
    expect(opened.document.encoding == TextEncoding::shift_jis &&
               !opened.document.last_failure.has_value() && opened.lines.at(0).text == "日",
           "a Shift_JIS body of exactly 64 MiB opens");
    japanese.append("\x93\xFA");
    expect_open_failure(std::move(japanese), FileFailure::too_large_to_decode,
                        "a Shift_JIS body two bytes past 64 MiB is too_large_to_decode");
    std::string utf16(nenenib::core::utf16_le_byte_order_mark());
    utf16.append(decoded, 'A');
    expect_open_failure(std::move(utf16), FileFailure::too_large_to_decode,
                        "a UTF-16 body past 64 MiB is refused before it is converted");
    // UTF-8 は 64 MiB を超えても開く（開く上限は 1 GiB）。
    std::string plain(decoded + 1U, 'a');
    Editing past_decoded;
    past_decoded.files().hold(std::move(plain));
    const auto large = past_decoded.controller().apply_frame(OpenDocument{sample_path()});
    expect(!large.document.last_failure.has_value() &&
               large.document.encoding == TextEncoding::utf8 && large.total_lines == 1,
           "a UTF-8 body one byte past 64 MiB opens");
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
    expect_open_failure(std::unexpected(FileFailure::too_large_to_decode),
                        FileFailure::too_large_to_decode,
                        "the port may also report a body too large to decode");
    expect_open_failure(std::string("\xFF\x41", 2), FileFailure::undecodable,
                        "bytes that are neither UTF-8 nor CP932 are undecodable");
    expect_open_failure(std::string("\xFF\xFE\x41", 3), FileFailure::undecodable,
                        "an odd UTF-16 body is undecodable");
    expect_open_failure(std::string("\xFE\xFF\xD8\x3D\x00\x41", 6), FileFailure::undecodable,
                        "an unpaired surrogate in UTF-16 is undecodable");
    expect_open_failure(std::string("\xFF\xFE\x00\x00\x41\x00\x00\x00", 8),
                        FileFailure::unsupported_utf32,
                        "a UTF-32 LE BOM is refused by name, not as undecodable (#787)");
    expect_open_failure(std::string("\x00\x00\xFE\xFF\x00\x00\x00\x41", 8),
                        FileFailure::unsupported_utf32,
                        "a UTF-32 BE BOM is refused by name, not as undecodable (#787)");
    Editing refused;
    refused.files().hold(std::string("\x93\xFA"));
    refused.code_pages().decode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame = refused.controller().apply_frame(OpenDocument{sample_path()});
    expect(frame.document.last_failure.has_value() &&
               frame.document.last_failure.value() == FileFailure::unencodable,
           "the code page port carries its own reason out");
    Editing broken;
    broken.files().hold(std::string("\x93\xFA"));
    broken.code_pages().decode_to(std::string("\xFF"));
    const auto invalid = broken.controller().apply_frame(OpenDocument{sample_path()});
    expect(invalid.document.last_failure.has_value() &&
               invalid.document.last_failure.value() == FileFailure::undecodable,
           "a port that returns broken UTF-8 is undecodable");
}

void verify_document_open_boundaries()
{
    Editing empty;
    empty.files().hold(std::string(byte_order_mark()));
    const auto bom = empty.controller().apply_frame(OpenDocument{sample_path()});
    expect(bom.document.encoding == TextEncoding::utf8_bom && bom.lines.at(0).text.empty(),
           "a BOM-only file opens as an empty UTF-8 BOM document");
    Editing nul;
    nul.files().hold(std::string("a\0b\n", 4));
    static_cast<void>(nul.controller().apply_frame(OpenDocument{sample_path()}));
    static_cast<void>(
        nul.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(nul.files().written() == std::string("a\0b\n", 4),
           "opening preserves embedded NUL and the trailing newline in the model");
    expect_open_failure(std::string(byte_order_mark()) + "\x93\xFA", FileFailure::undecodable,
                        "an invalid BOM file is not decoded as CP932 by the controller");
}

void verify_document_save()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, InsertText{"あ"});
    expect(controller.frame().document.save_state == SaveState::modified,
           "typing marks it unsaved");
    expect(controller.frame().document.title.text() == "● 無題", "the mark is on the tab");
    const auto saved = controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "あ", "the body went to the port as UTF-8");
    expect(editing.files().written_path() == "C:\\work\\note.txt", "to the path in the intent");
    expect(saved.document.save_state == SaveState::saved, "saving clears the mark");
    expect(saved.document.title.text() == "note.txt", "and the tab takes the name");
    expect(saved.document.encoding == TextEncoding::utf8, "the encoding is the one that was asked");
    expect(saved.caret.position == TextPosition{LineNumber{1}, Column{2}},
           "saving does not move the caret");
    expect(!saved.document.last_failure.has_value(), "a good save reports nothing");
}

// BOM の本文は列の先頭を piece でずらして持つ（ADR 0159 の決定 8）。保存は BOM を 1
// つだけ付け直し、 開いたバイト列のまま書く。
void verify_document_bom_shift_save()
{
    Editing editing;
    auto &controller = editing.controller();
    const std::string bytes = std::string(byte_order_mark()) + "本文\r\nつづき\r\n";
    editing.files().hold(bytes);
    const auto opened = controller.apply_frame(OpenDocument{sample_path()});
    expect(opened.lines.at(0).text == "本文" && opened.total_lines == 3 &&
               opened.caret.position == TextPosition{LineNumber{1}, Column{1}},
           "the shifted BOM body starts at line 1 column 1");
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8_bom});
    expect(editing.files().written() == bytes &&
               editing.files().written_segments() == std::vector<std::size_t>{3, bytes.size() - 3U},
           "saving a shifted BOM body writes one BOM and the read body as one piece");
}

void verify_document_save_segments()
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(std::string("a\r\n日本\nz"));
    applied(controller, OpenDocument{sample_path()});
    applied(controller,
            PlaceCaret{TextPosition{LineNumber{2}, Column{1}}, SelectionAnchoring::collapse});
    applied(controller, InsertText{"X"});
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8_bom});
    expect(editing.files().written() == std::string(byte_order_mark()) + "a\r\nX日本\nz",
           "fragmented saving preserves BOM, mixed newlines and trailing content");
    expect(editing.files().written_segments() == std::vector<std::size_t>{3, 3, 1, 8},
           "UTF-8 BOM and body pieces reach the port without a flattened body");
    applied(controller, HistoryAction{HistoryDirection::undo});
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\n日本\nz",
           "saving after undo writes the restored bytes");
    applied(controller, HistoryAction{HistoryDirection::redo});
    applied(controller, SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written() == "a\r\nX日本\nz" &&
               editing.files().written_segments().size() == 3,
           "redo preserves piece order for plain UTF-8 saving");
}

void verify_document_save_empty()
{
    Editing editing;
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8});
    expect(editing.files().written().empty() && editing.files().written_segments().empty(),
           "an empty UTF-8 body writes an empty segment list");
    applied(editing.controller(), SaveDocument{sample_path(), TextEncoding::utf8_bom});
    expect(editing.files().written() == byte_order_mark() &&
               editing.files().written_segments() == std::vector<std::size_t>{3},
           "an empty BOM body writes only the BOM segment");
}

void verify_document_save_encodings()
{
    Editing with_bom;
    applied(with_bom.controller(), InsertText{"あ"});
    static_cast<void>(
        with_bom.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::utf8_bom}));
    expect(with_bom.files().written() == std::string(byte_order_mark()) + "あ",
           "a BOM file keeps its BOM");
    Editing japanese;
    applied(japanese.controller(), InsertText{"日"});
    japanese.code_pages().encode_to(std::string("\x93\xFA"));
    const auto frame =
        japanese.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(japanese.code_pages().encoded_from() == "日", "the body went through the code page");
    expect(japanese.files().written() == "\x93\xFA", "the CP932 bytes are what is written");
    expect(frame.document.save_state == SaveState::saved, "the save took");
}

// UTF-16 のファイルを開き、1 文字足して保存すると、BOM と並びと改行の形がそのまま戻る（ADR 0149）。
void expect_utf16_round_trip(TextEncoding encoding, const std::string &bytes,
                             const std::string &expected, std::string_view label)
{
    Editing editing;
    auto &controller = editing.controller();
    editing.files().hold(bytes);
    const auto opened = controller.apply_frame(OpenDocument{sample_path()});
    expect(opened.document.encoding == encoding && opened.lines.at(0).text == "あ",
           "a UTF-16 file opens as its own encoding with a UTF-8 body");
    expect(opened.status_items.at(1).text() == label && opened.status_items.at(2).text() == "CRLF",
           "the status bar shows the UTF-16 order and the newline that was read");
    applied(controller, InsertText{"b"});
    const auto saved =
        controller.apply_frame(SaveDocument{sample_path(), controller.frame().document.encoding});
    expect(saved.document.save_state == SaveState::saved && saved.document.encoding == encoding,
           "the UTF-16 save took and kept the encoding");
    expect(editing.files().written() == expected,
           "the written bytes keep the BOM, the byte order and CRLF");
    Editing again;
    again.files().hold(editing.files().written());
    const auto reread = again.controller().apply_frame(OpenDocument{sample_path()});
    expect(reread.document.encoding == encoding && reread.lines.at(0).text == "bあ" &&
               reread.total_lines == 2,
           "the written UTF-16 file reads back as the edited body");
}

void verify_document_utf16_round_trip()
{
    expect_utf16_round_trip(
        TextEncoding::utf16_le, std::string("\xFF\xFE\x42\x30\x0D\x00\x0A\x00\x3D\xD8\x00\xDE", 12),
        std::string("\xFF\xFE\x62\x00\x42\x30\x0D\x00\x0A\x00\x3D\xD8\x00\xDE", 14), "UTF-16 LE");
    expect_utf16_round_trip(
        TextEncoding::utf16_be, std::string("\xFE\xFF\x30\x42\x00\x0D\x00\x0A\xD8\x3D\xDE\x00", 12),
        std::string("\xFE\xFF\x00\x62\x30\x42\x00\x0D\x00\x0A\xD8\x3D\xDE\x00", 14), "UTF-16 BE");
    Editing written;
    applied(written.controller(), InsertText{"日"});
    applied(written.controller(), SaveDocument{sample_path(), TextEncoding::utf16_be});
    expect(written.files().written() == "\xFE\xFF\x65\xE5" &&
               written.files().written_segments() == std::vector<std::size_t>{2, 2},
           "UTF-16 saving hands the BOM and the converted body to the port as two parts");
}

void verify_document_save_failures()
{
    Editing refused;
    applied(refused.controller(), InsertText{"😀"});
    refused.code_pages().encode_to(std::unexpected(CodePageFailure::unencodable));
    const auto frame =
        refused.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::shift_jis});
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
        unwritable.controller().apply_frame(SaveDocument{sample_path(), TextEncoding::utf8});
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
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
    expect(controller.frame().document.save_state == SaveState::saved, "saved again");
    applied(controller, HistoryAction{HistoryDirection::undo});
    expect(controller.frame().document.save_state == SaveState::modified,
           "undo preserves the future save point");
    applied(controller, HistoryAction{HistoryDirection::redo});
    expect(controller.frame().document.save_state == SaveState::saved,
           "redo reaches the preserved save point");
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
    static_cast<void>(controller.apply_frame(SaveDocument{sample_path(), TextEncoding::utf8}));
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
    const auto failed = controller.apply_frame(OpenDocument{sample_path()});
    expect(failed.document.last_failure.has_value(), "the failure is on the frame that made it");
    const auto next = controller.apply_frame(InsertText{"a"});
    expect(!next.document.last_failure.has_value(), "the next intent clears the failure");
    expect(!controller.frame().document.last_failure.has_value(),
           "and reading the frame again does not bring it back");
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
    const auto state =
        EditorState::create(Appearance::dark, EditMode::ordinary)
            .with_edit(buffer_of("hi"), collapsed_at(Offset{2}),
                       EditHistory::empty().pushed(Edit{Offset{0}, "", "hi", Offset{0}},
                                                   EditBoundary::separate),
                       nenenib::application::Document{std::nullopt, TextEncoding::utf8, 0});
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
    const auto composing = controller.apply_frame(ComposeText{composed_of(
        "にほん", {clause_of(0, 3, ClauseEmphasis::other), clause_of(3, 9, ClauseEmphasis::target)},
        3)});
    expect(composing.lines.at(0).text == "a", "the buffer does not carry the composed text");
    expect(composed_summary(composing) == "にほん@3|O0-3|T3-9",
           "the frame carries the composition, its caret and the folded underlines instead");
    expect(applied(controller, HistoryAction{HistoryDirection::undo}) == "",
           "undo while composing still only sees the typed 'a'");
    applied(controller, HistoryAction{HistoryDirection::redo});
    const auto committed = controller.apply_frame(CommitText{"日本"});
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
    const auto composing = controller.apply_frame(ComposeText{composed_of("あ", {}, 0)});
    expect(composing.composition.has_value(), "the composition is on the frame");
    expect(!controller.apply_frame(CancelComposition{}).composition.has_value(),
           "CancelComposition takes it away");
    expect(!controller.apply_frame(CancelComposition{}).composition.has_value(),
           "and cancelling again does nothing");
    static_cast<void>(controller.apply_frame(ComposeText{composed_of("あ", {}, 0)}));
    expect(!controller.apply_frame(SelectEditMode{EditMode::vim}).composition.has_value(),
           "changing the editing mode drops the composition");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::ordinary}));
    static_cast<void>(controller.apply_frame(ComposeText{composed_of("あ", {}, 0)}));
    editing.files().hold(Bytes{std::string("x")});
    expect(!controller.apply_frame(OpenDocument{sample_path()}).composition.has_value(),
           "opening a file drops the composition");
}

// Vim の NORMAL では IME を切ってあるので変換は来ないが、来たら捨てる（決定 4）。
void verify_composition_vim_normal()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    applied(controller, InsertText{"abc"});
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    const auto composing = controller.apply_frame(ComposeText{composed_of("に", {}, 0)});
    expect(!composing.composition.has_value(), "NORMAL drops the composition");
    expect(composing.vim_mode == VimMode::normal, "the frame carries the Vim mode for the window");
    const auto committed = controller.apply_frame(CommitText{"日本"});
    expect(committed.lines.at(0).text == "abc", "NORMAL drops the committed text too");
    expect(controller.vim_state().mode == VimMode::normal, "and stays in NORMAL");
}

// Vim の INSERT では確定した文字列が code point ごとの打鍵として engine を通る（決定 4）。
void verify_composition_vim_insert()
{
    Editing editing;
    EditorController &controller = editing.controller();
    applied(controller, VisibleLines{10});
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U'i'}}}));
    const auto composing = controller.apply_frame(ComposeText{composed_of("にほん", {}, 9)});
    expect(composed_summary(composing) == "にほん@9|O0-9",
           "INSERT shows the composition like ordinary mode");
    expect(composing.vim_mode == VimMode::insert, "the frame says INSERT");
    expect(composing.lines.at(0).text.empty(), "the buffer is still empty while composing");
    const auto committed = controller.apply_frame(CommitText{"日本語"});
    expect(committed.lines.at(0).text == "日本語", "the commit goes through the Vim engine");
    expect(!committed.composition.has_value(), "and the composition is gone");
    expect(controller.vim_state().mode == VimMode::insert, "INSERT is still INSERT afterwards");
    expect(committed.caret.position.column == Column{4}, "the caret sits past the three glyphs");
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimSpecialKey::escape}}));
    vim_replay(controller, "x");
    expect(controller.frame().lines.at(0).text == "日本",
           "Vim sees the committed text as characters it typed itself");
}

// IME の構えの表（ADR 0061 の決定 1）。入力があれば入力の種類が決め（面・Ex の行・検索の行は
// closed_once・D57）、無ければモードが決める（通常と INSERT は as_left・NORMAL / VISUAL は
// closed）。
void verify_ime_stance_table()
{
    using nenenib::application::CommandInput;
    using nenenib::application::ime_stance_of;
    using nenenib::application::ImeStance;
    const std::array<std::optional<CommandInput>, 4> inputs{
        std::nullopt, CommandInput{nenenib::core::CommandLine::empty()},
        CommandInput{nenenib::core::SearchLine::opened(nenenib::core::VimSearchDirection::forward)},
        CommandInput{
            nenenib::core::CommandPalette::opened({}, "", nenenib::core::EditMode::ordinary)}};
    const std::array<ImeStance, 4> typed{ImeStance::as_left, ImeStance::closed_once,
                                         ImeStance::closed_once, ImeStance::closed_once};
    const std::array<ImeStance, 4> blocked{ImeStance::closed, ImeStance::closed_once,
                                           ImeStance::closed_once, ImeStance::closed_once};
    const std::array<VimMode, 5> vim_modes{VimMode::normal, VimMode::insert, VimMode::visual,
                                           VimMode::visual_line, VimMode::visual_block};
    for (std::size_t index = 0; index < inputs.size(); ++index)
    {
        for (const VimMode vim_mode : vim_modes)
        {
            expect(ime_stance_of(EditMode::ordinary, vim_mode, inputs.at(index)) == typed.at(index),
                   "ordinary mode ignores the Vim mode");
            const auto &expected = vim_mode == VimMode::insert ? typed : blocked;
            expect(ime_stance_of(EditMode::vim, vim_mode, inputs.at(index)) == expected.at(index),
                   "Vim INSERT is as_left and NORMAL / VISUAL are closed without an input");
        }
    }
}

// controller の frame に載る構え（ADR 0061 の決定 1）。面を閉じると元のモードの値に戻る。
void verify_ime_stance_frame()
{
    using nenenib::application::ImeStance;
    using nenenib::application::OpenCommandPalette;
    Editing editing;
    EditorController &controller = editing.controller();
    expect(controller.frame().ime == ImeStance::as_left, "ordinary mode leaves the IME as it was");
    expect(controller.apply_frame(OpenCommandPalette{}).ime == ImeStance::closed_once,
           "Ctrl+P closes the IME once in ordinary mode");
    expect(controller.apply_frame(nenenib::application::CancelCommand{}).ime == ImeStance::as_left,
           "closing the palette returns to as_left");
    static_cast<void>(controller.apply_frame(SelectEditMode{EditMode::vim}));
    expect(controller.frame().ime == ImeStance::closed, "Vim NORMAL keeps the IME closed");
    expect(controller.apply_frame(OpenCommandPalette{}).ime == ImeStance::closed_once,
           "Ctrl+P closes the IME once in Vim NORMAL");
    expect(controller.apply_frame(nenenib::application::CancelCommand{}).ime == ImeStance::closed,
           "closing the palette returns to closed");
    expect(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U':'}}}).ime ==
               ImeStance::closed_once,
           "the Ex line closes the IME once (D57)");
    expect(controller.apply_frame(nenenib::application::CancelCommand{}).ime == ImeStance::closed,
           "leaving the Ex line returns to closed");
    expect(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U'/'}}}).ime ==
               ImeStance::closed_once,
           "the search line closes the IME once (D57)");
    static_cast<void>(controller.apply_frame(nenenib::application::CancelCommand{}));
    // Ex の確定で面が開く（`:tabs`）と、構えは closed_once のまま入力行の種類が Ex → 面に変わる。
    // ui はこの変わり目で IME を 1 度閉じ直す（D32・#635 の差し戻し 1）。delivery と frame の
    // 両方で種類を区別できる。
    using nenenib::core::InputLinePrompt;
    static_cast<void>(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U':'}}}));
    const auto typed = controller.apply(nenenib::application::CommandText{"tabs"});
    expect(typed.ime == ImeStance::closed_once &&
               typed.input_line == std::optional{InputLinePrompt::ex},
           "the Ex line is delivered as its own kind");
    const auto listed = controller.apply(nenenib::application::SubmitCommand{});
    const auto listed_frame = controller.frame();
    expect(listed.ime == ImeStance::closed_once &&
               listed.input_line == std::optional{InputLinePrompt::palette} &&
               listed_frame.command_line.has_value() &&
               listed_frame.command_line.value().prompt == InputLinePrompt::palette,
           "`:tabs` turns the Ex line into the palette, a different kind");
    expect(controller.apply(nenenib::application::CancelCommand{}).input_line == std::nullopt,
           "closing the palette delivers no input line");
    expect(controller.apply_frame(VimKeyPress{VimKey{VimCharacter{U'i'}}}).ime ==
               ImeStance::as_left,
           "Vim INSERT leaves the IME as it was");
}

void verify_composition()
{
    verify_composition_underlines();
    verify_composition_state();
    verify_composition_ordinary();
    verify_composition_cancelling();
    verify_composition_vim_normal();
    verify_composition_vim_insert();
    verify_ime_stance_table();
    verify_ime_stance_frame();
}
} // namespace

void verify_editor_state()
{
    verify_consuming_updates();
    verify_self_borrowed_updates();
    verify_intent_cleared();
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
    const auto edited = state.with_edit(buffer_of("hi"), collapsed_at(Offset{2}),
                                        EditHistory::empty(), state.document());
    expect(edited.text().text() == "hi" && state.text().size_bytes() == 0,
           "with_edit leaves the source alone");
}

void verify_document_read_contracts()
{
    verify_document_open();
    verify_document_open_encodings();
    verify_document_open_failures();
    verify_document_open_limits();
    verify_document_open_boundaries();
    verify_document_save_contracts();
}

void verify_document_save_contracts()
{
    verify_document_save_segments();
    verify_document_bom_shift_save();
    verify_document_save_empty();
    verify_document_save();
    verify_document_save_encodings();
    verify_document_utf16_round_trip();
    verify_document_save_failures();
    verify_save_state_transitions();
    verify_unreachable_save_point();
}

void verify_application_scope()
{
    verify_editor_state();
    verify_controller_intents();
}

void verify_frame_selection_scope()
{
    verify_controller_selection();
    verify_controller_place_caret();
    verify_controller_cancel_selection();
    verify_controller_frame();
    verify_controller_mode_selection();
    verify_mode_switch_keeps_view_below();
    verify_mode_switch_keeps_view_above();
    verify_mode_switch_keeps_visible_caret();
    verify_mode_switch_back_keeps_view();
    verify_frame_selection_line_ends();
    verify_frame_selection_mixed_columns();
}

void verify_controller_intents()
{
    verify_delivery_contracts();
    verify_font_geometry();
    verify_controller_initial_appearance();
    verify_controller_read_failures();
    verify_controller_refresh();
    verify_controller_typing();
    verify_controller_undo();
    verify_controller_deletion();
    verify_frame_selection_scope();
    verify_controller_clipboard();
    verify_controller_clipboard_failures();
    verify_clipboard_line_feeds();
    verify_controller_paste_line_endings();
    verify_controller_paste_undo_and_failure();
    verify_controller_scrolling();
    verify_follow_settles_at_frame();
    verify_follow_settles_before_search_origin();
    verify_follow_dropped_on_leaving_question();
    verify_clamp_dropped_on_leaving_question();
    verify_vim_insert_follow_settles_at_frame();
    verify_vim_viewport_settles_before_screen_keys();
    verify_vim_round_trip_judgements();
    verify_vim_inconsistent_round_trip();
    verify_font_size_lays_out_once();
    verify_screen_row_pages();
    verify_document_read_contracts();
    verify_document_failure_clearing();
    verify_vim_engine();
    verify_composition();
}
} // namespace nenenib::tests
