// Issue #1049 / ADR 0010 の追補（2026-10-29）の決定 1〜4。保存の文字コードと改行を選ぶ。選ぶ操作が
// 変えるのは文書の保存の形だけで、本文のバイト列と undo には触れない。保存は encoded_parts の 1
// 本が 本文の形と違うときだけ写す。
#include "ActivateCommandChoice.hpp"
#include "CodePageFailure.hpp"
#include "DocumentFormat.hpp"
#include "DocumentFormatKind.hpp"
#include "Editing.hpp"
#include "ExResult.hpp"
#include "FileFailure.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineEndingConversion.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "OpenDocumentFormatList.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectDocumentFormat.hpp"
#include "SelectEditMode.hpp"
#include "StatusBarLayout.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;

[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

void open_held(Editing &editing, std::string bytes)
{
    editing.files().hold(Bytes{std::move(bytes)});
    applied(editing.controller(), app::VisibleLines{5});
    applied(editing.controller(), app::OpenDocument{sample_path()});
}

[[nodiscard]] app::EditorFrame saved(Editing &editing)
{
    const auto frame = editing.controller().frame();
    return editing.controller().apply_frame(
        app::SaveDocument{sample_path(), frame.document.encoding});
}

// `:` の行は Vim の NORMAL から入る（決定 2 の (b) は Vim の入口）。
[[nodiscard]] app::EditorFrame run_ex(Editing &editing, std::string_view text)
{
    vim_replay(editing.controller(), ":" + std::string(text) + "<CR>");
    return editing.controller().frame();
}

[[nodiscard]] std::string line_ending_shown(const app::EditorFrame &frame)
{
    return std::string(frame.status_items.at(2).text());
}

// 決定 3 の写しの規則（ADR 0036 の決定 6 の追補）。単独の `\r` はどちらでも残す。
void verify_format_conversion()
{
    expect(core::line_endings_converted("a\nb", core::LineEnding::crlf) == "a\r\nb",
           "a bare LF becomes CRLF");
    expect(core::line_endings_converted("a\r\nb\r\n", core::LineEnding::lf) == "a\nb\n",
           "every CRLF becomes LF");
    expect(core::line_endings_converted("a\r\nb\nc", core::LineEnding::crlf) == "a\r\nb\r\nc",
           "an existing CRLF is kept when writing CRLF");
    expect(core::line_endings_converted("a\rb\nc\r", core::LineEnding::crlf) == "a\rb\r\nc\r",
           "a lone CR stays a character when writing CRLF");
    expect(core::line_endings_converted("a\rb\r\nc\r", core::LineEnding::lf) == "a\rb\nc\r",
           "a lone CR stays a character when writing LF");
    expect(core::line_endings_converted("x\r\ny", core::LineEnding::crlf) == "x\r\ny",
           "a literal CR right before LF becomes one CRLF (the boundary of ADR 0036)");
    expect(core::line_endings_converted("a\r\r\nb", core::LineEnding::lf) == "a\r\nb",
           "a literal CR before a CRLF newline stays when writing LF");
    expect(core::line_endings_converted("a\r\r\nb", core::LineEnding::crlf) == "a\r\r\nb",
           "a literal CR before a CRLF newline is kept as is when writing CRLF");
    expect(core::line_endings_converted("", core::LineEnding::crlf).empty() &&
               core::line_endings_converted("\n", core::LineEnding::crlf) == "\r\n" &&
               core::line_endings_converted("\r\n", core::LineEnding::lf) == "\n",
           "an empty body and a lone newline convert at the edges");
}

// 決定 2 の (b): Vim と同じ名前（utf-8-bom だけは Nib の名前）。名前は表と往復する。
void verify_format_option_names()
{
    for (const core::TextEncoding encoding : core::text_encodings)
    {
        expect(core::encoding_of_option_name(core::encoding_option_name(encoding)) == encoding,
               "every encoding name reads back to the same encoding");
    }
    for (const core::LineEnding ending : core::line_endings)
    {
        expect(core::line_ending_of_option_name(core::line_ending_option_name(ending)) == ending,
               "every fileformat name reads back to the same line ending");
    }
    expect(core::encoding_option_name(core::TextEncoding::shift_jis) == "cp932" &&
               core::encoding_option_name(core::TextEncoding::utf8_bom) == "utf-8-bom" &&
               core::line_ending_option_name(core::LineEnding::crlf) == "dos" &&
               core::line_ending_option_name(core::LineEnding::lf) == "unix",
           "the names follow Vim");
    expect(!core::encoding_of_option_name("latin1").has_value() &&
               !core::line_ending_of_option_name("mac").has_value(),
           "names outside the closed sets are not read");
}

[[nodiscard]] std::optional<core::ExFormatRequest> format_of(std::string_view text)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    if (!result)
    {
        return std::nullopt;
    }
    return result.value().format;
}

void verify_format_ex_parse()
{
    const auto encoding = format_of("set fenc=cp932");
    expect(encoding.has_value() && encoding.value().kind == core::DocumentFormatKind::encoding &&
               encoding.value().chosen == core::DocumentFormat{core::TextEncoding::shift_jis},
           "set fenc=cp932 chooses Shift_JIS");
    const auto long_name = format_of("set fileencoding=utf-16be");
    expect(long_name.has_value() &&
               long_name.value().chosen == core::DocumentFormat{core::TextEncoding::utf16_be},
           "the long name fileencoding works too");
    const auto ending = format_of("set ff=dos");
    expect(ending.has_value() && ending.value().kind == core::DocumentFormatKind::line_ending &&
               ending.value().chosen == core::DocumentFormat{core::LineEnding::crlf},
           "set ff=dos chooses CRLF");
    const auto asked = format_of("set fileformat?");
    expect(asked.has_value() && asked.value().kind == core::DocumentFormatKind::line_ending &&
               !asked.value().chosen.has_value(),
           "set fileformat? asks for the current value");
    const auto unknown = core::evaluate_ex("set fenc=latin1", core::default_editor_settings(),
                                           core::Appearance::dark);
    expect(!unknown.has_value(), "an encoding outside the closed set is refused");
}

// 決定 1・3・4: LF の本文を CRLF にして保存するとバイト列が CRLF になる。本文・undo は変わらない。
void verify_format_lf_to_crlf()
{
    Editing editing;
    open_held(editing, "a\nb");
    auto &controller = editing.controller();
    const auto chosen = controller.apply_frame(app::SelectDocumentFormat{core::LineEnding::crlf});
    expect(line_ending_shown(chosen) == "CRLF" &&
               chosen.document.save_state == core::SaveState::modified,
           "choosing CRLF shows it and marks the document unsaved");
    const auto undone = controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(line_ending_shown(undone) == "CRLF" &&
               undone.document.save_state == core::SaveState::modified,
           "undo does not bring the line ending back");
    const auto stored = saved(editing);
    expect(editing.files().written() == "a\r\nb" &&
               stored.document.save_state == core::SaveState::saved &&
               line_ending_shown(stored) == "CRLF",
           "the save writes CRLF and clears the unsaved mark");
    applied(controller, app::InsertText{"c"});
    static_cast<void>(saved(editing));
    expect(editing.files().written() == "ca\r\nb",
           "later saves keep writing the chosen line ending");
}

void verify_format_crlf_to_lf()
{
    Editing editing;
    open_held(editing, "a\r\nb\rc");
    applied(editing.controller(), app::SelectDocumentFormat{core::LineEnding::lf});
    const auto stored = saved(editing);
    expect(editing.files().written() == "a\nb\rc" &&
               stored.document.save_state == core::SaveState::saved,
           "CRLF to LF writes LF and keeps the lone CR");
}

// 形を保存時点の値に戻したら、未保存の印は消える（判定は save_state_of の 1 か所）。
void verify_format_choose_back()
{
    Editing editing;
    open_held(editing, "a\nb");
    auto &controller = editing.controller();
    applied(controller, app::SelectDocumentFormat{core::TextEncoding::utf16_le});
    applied(controller, app::SelectDocumentFormat{core::LineEnding::crlf});
    applied(controller, app::SelectDocumentFormat{core::TextEncoding::utf8});
    const auto partly = controller.frame();
    expect(partly.document.save_state == core::SaveState::modified,
           "with the line ending still changed the document stays unsaved");
    const auto back = controller.apply_frame(app::SelectDocumentFormat{core::LineEnding::lf});
    expect(back.document.save_state == core::SaveState::saved,
           "choosing the saved forms again clears the unsaved mark");
    applied(controller, app::InsertText{"x"});
    applied(controller, app::SelectDocumentFormat{core::LineEnding::crlf});
    const auto edited = controller.apply_frame(app::HistoryAction{core::HistoryDirection::undo});
    expect(edited.document.save_state == core::SaveState::modified,
           "undoing the edit back to the saved position still leaves the changed form unsaved");
}

// 決定 3: 文字コードは保存の符号化だけを変える（BOM と UTF-16）。
void verify_format_encodings()
{
    Editing editing;
    open_held(editing, "ab");
    auto &controller = editing.controller();
    applied(controller, app::SelectDocumentFormat{core::TextEncoding::utf8_bom});
    const auto bom = saved(editing);
    expect(editing.files().written() == "\xEF\xBB\xBF"
                                        "ab" &&
               bom.document.encoding == core::TextEncoding::utf8_bom &&
               bom.status_items.at(1).text() == "UTF-8 BOM" &&
               bom.document.save_state == core::SaveState::saved,
           "UTF-8 BOM writes the mark in front of the body");
    applied(controller, app::SelectDocumentFormat{core::TextEncoding::utf16_le});
    applied(controller, app::SelectDocumentFormat{core::LineEnding::lf});
    static_cast<void>(saved(editing));
    expect(editing.files().written() == std::string("\xFF\xFE"
                                                    "a\0b\0",
                                                    6),
           "UTF-16 LE writes its mark and the body in two-byte units");
}

// 決定 3: Shift_JIS で表せない文字は既存の unencodable の失敗で止め、値は戻さない。
void verify_format_unencodable()
{
    Editing editing;
    open_held(editing, "ab");
    editing.code_pages().encode_to(std::unexpected(app::CodePageFailure::unencodable));
    applied(editing.controller(), app::SelectDocumentFormat{core::TextEncoding::shift_jis});
    const auto refused = saved(editing);
    expect(refused.document.last_failure == app::FileFailure::unencodable &&
               refused.document.encoding == core::TextEncoding::shift_jis &&
               refused.document.save_state == core::SaveState::modified &&
               editing.files().written().empty(),
           "an unencodable body stops the save and keeps the chosen encoding");
}

// 決定 3: 保存の形と本文の形が同じなら写さない（本文の断片をそのまま渡す）。
void verify_format_same_form_is_free()
{
    Editing editing;
    open_held(editing, "a\nb");
    auto &controller = editing.controller();
    applied(controller, app::InsertText{"x"});
    static_cast<void>(saved(editing));
    const std::size_t pieces = editing.files().written_segments().size();
    expect(pieces > 1 && editing.files().written() == "xa\nb",
           "a save in the body's own form hands over the body's segments unchanged");
    applied(controller, app::SelectDocumentFormat{core::LineEnding::lf});
    static_cast<void>(saved(editing));
    expect(editing.files().written_segments().size() == pieces,
           "choosing the body's own form explicitly still does not convert");
    applied(controller, app::SelectDocumentFormat{core::LineEnding::crlf});
    static_cast<void>(saved(editing));
    expect(editing.files().written_segments().size() == 1 && editing.files().written() == "xa\r\nb",
           "only a different form writes one converted part");
}

// 決定 2 の (b): `:set fenc?` `:set ff?` は今の値を Vim の名前で 1 行、選ぶ形は何も出さない。
void verify_format_ex_answers()
{
    Editing editing;
    open_held(editing, "a\nb");
    applied(editing.controller(), app::SelectEditMode{core::EditMode::vim});
    expect(notice(run_ex(editing, "set fenc?")) == "fileencoding=utf-8",
           "set fenc? tells the current encoding");
    expect(notice(run_ex(editing, "set ff?")) == "fileformat=unix",
           "set ff? tells the current line ending");
    const auto chosen = run_ex(editing, "set ff=dos");
    expect(line_ending_shown(chosen) == "CRLF" &&
               chosen.document.save_state == core::SaveState::modified,
           "set ff=dos chooses CRLF");
    expect(notice(run_ex(editing, "set fileformat?")) == "fileformat=dos",
           "the answer follows the chosen value");
    const auto encoding = run_ex(editing, "set fileencoding=cp932");
    expect(encoding.document.encoding == core::TextEncoding::shift_jis &&
               notice(run_ex(editing, "set fenc?")) == "fileencoding=cp932",
           "set fileencoding=cp932 chooses Shift_JIS");
    expect(notice(run_ex(editing, "set fenc=latin1")).starts_with("E474"),
           "an unknown name is refused with E474");
}

// 決定 2 の (a)・(c): 面は閉じた enum の全部で、選択は今の値、今の値に印。選ぶと意図。
void verify_format_palette()
{
    Editing editing;
    open_held(editing, "a\nb");
    auto &controller = editing.controller();
    const auto opened =
        controller.apply_frame(app::OpenDocumentFormatList{core::DocumentFormatKind::encoding});
    expect(opened.command_palette.has_value() && opened.command_palette.value().total == 5 &&
               opened.command_palette.value().selected == 0 &&
               opened.command_palette.value().rows.at(0).detail.has_value() &&
               !opened.command_palette.value().rows.at(1).detail.has_value() &&
               opened.command_palette.value().rows.at(2).label.text() == "Shift_JIS",
           "the encoding face lists the five encodings and marks the current one");
    const auto picked = controller.apply_frame(app::ActivateCommandChoice{2});
    expect(!picked.command_palette.has_value() &&
               picked.document.encoding == core::TextEncoding::shift_jis &&
               picked.document.save_state == core::SaveState::modified,
           "choosing a row sets the encoding and closes the face");
    const auto endings =
        controller.apply_frame(app::OpenDocumentFormatList{core::DocumentFormatKind::line_ending});
    expect(endings.command_palette.has_value() && endings.command_palette.value().total == 2 &&
               endings.command_palette.value().selected == 0 &&
               endings.command_palette.value().rows.at(1).label.text() == "CRLF",
           "the line ending face lists LF and CRLF with the current one selected");
    const auto crlf = controller.apply_frame(app::ActivateCommandChoice{1});
    expect(line_ending_shown(crlf) == "CRLF", "choosing CRLF in the face sets it");
    const auto again =
        controller.apply_frame(app::OpenDocumentFormatList{core::DocumentFormatKind::line_ending});
    expect(again.command_palette.has_value() && again.command_palette.value().selected == 1 &&
               again.command_palette.value().rows.at(1).detail.has_value(),
           "the face opens on the chosen line ending");
}

// 保存の形は文書の値で、タブを切り替えても付いていく。
void verify_format_tabs_carry_the_form()
{
    Editing editing;
    open_held(editing, "a\nb");
    auto &controller = editing.controller();
    applied(controller, app::SelectDocumentFormat{core::LineEnding::crlf});
    applied(controller, app::NewTab{});
    const auto fresh = controller.frame();
    expect(line_ending_shown(fresh) == "CRLF" &&
               fresh.document.save_state == core::SaveState::saved,
           "a new tab has its own form");
    const auto back = controller.apply_frame(app::SwitchTab{0});
    expect(line_ending_shown(back) == "CRLF" &&
               back.document.save_state == core::SaveState::modified,
           "the chosen form comes back with its tab");
}

// 決定 2 の (a): ステータスバーの文字コードと改行の項目が当たる。
void verify_format_status_bar_hits()
{
    const auto layout = core::status_bar_layout(960, 540, 96);
    const auto centre = [&layout](std::size_t index)
    {
        const auto &item = layout.items.at(index);
        return std::pair{(item.left + item.right) / 2, (item.top + item.bottom) / 2};
    };
    const auto [encoding_x, encoding_y] = centre(1);
    const auto [ending_x, ending_y] = centre(2);
    const auto [position_x, position_y] = centre(0);
    expect(core::status_bar_hit(layout, encoding_x, encoding_y) == core::StatusBarHit::encoding,
           "the encoding item is hit");
    expect(core::status_bar_hit(layout, ending_x, ending_y) == core::StatusBarHit::line_ending,
           "the line ending item is hit");
    expect(core::status_bar_hit(layout, position_x, position_y) == core::StatusBarHit::none,
           "the caret position item is not a target");
}
} // namespace

void verify_document_format_contracts()
{
    verify_format_conversion();
    verify_format_option_names();
    verify_format_ex_parse();
    verify_format_lf_to_crlf();
    verify_format_crlf_to_lf();
    verify_format_choose_back();
    verify_format_encodings();
    verify_format_unencodable();
    verify_format_same_form_is_free();
    verify_format_ex_answers();
    verify_format_palette();
    verify_format_tabs_carry_the_form();
    verify_format_status_bar_hits();
}
} // namespace nenenib::tests
