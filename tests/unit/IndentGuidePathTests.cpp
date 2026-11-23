// scope `--indent-guides` の続き（ADR 0193 の決定 7・9・14 と 2026-11-23 の追補・#1975 の
// レビュー
// R3〜R5）。推し当ての道（reach_tab・自動退避からの復元・保存と切り替えで推し当て直さない）・
// 推し当ての 64 KiB の打ち切りと窓の端の
// whole_code_points・行頭の字下げの窓の上限・プレビューで欄を 付けない・上の端の空の行の 1
// 画面の打ち切り。
#include "AutosaveForm.hpp"
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "Column.hpp"
#include "DisplayText.hpp"
#include "Editing.hpp"
#include "EditorFeature.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FeatureCosts.hpp"
#include "FilePath.hpp"
#include "IndentGuide.hpp"
#include "IndentGuides.hpp"
#include "IndentUnit.hpp"
#include "InsertText.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "LineView.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "PreviewFrame.hpp"
#include "PreviewPlacement.hpp"
#include "RestoreAutosave.hpp"
#include "SaveDocument.hpp"
#include "Scopes.hpp"
#include "ScriptedAutosave.hpp"
#include "SelectionAnchoring.hpp"
#include "Session.hpp"
#include "SessionTab.hpp"
#include "SetPreviewPlacement.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "TogglePreview.hpp"
#include "VirtualColumn.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using core::IndentUnit;

constexpr std::string_view two_path = "C:\\work\\two.txt";
constexpr std::string_view eight_path = "C:\\work\\eight.txt";
constexpr std::string_view two_space = "a\n  b\n    c\n  d\n";
constexpr std::string_view eight_space = "a\n        b\nc\n        d\n";

[[nodiscard]] core::EditorSettings guides_on()
{
    return core::feature_set(core::default_editor_settings(), core::EditorFeature::indent_guides,
                             true);
}

[[nodiscard]] core::FilePath path_of(std::string_view path)
{
    return core::FilePath::parse(path).value();
}

// 最初の欄のある行の段の幅（欄が無ければ 0）。
[[nodiscard]] std::size_t step_of(const app::EditorFrame &frame)
{
    const auto found = std::ranges::find_if(frame.lines, [](const app::LineView &line)
                                            { return line.indent_guides.has_value(); });
    return found == frame.lines.end()
               ? 0
               : found->indent_guides.value_or(app::IndentGuides{0, 0, 0}).step;
}

void place(app::EditorController &controller, std::size_t line)
{
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{line}, core::Column{1}},
                                        core::SelectionAnchoring::collapse});
}

// まだ読んでいない文書を読む道（reach_tab → loaded_tab）も開く道と同じく推し当てる（決定 7）。
void verify_reached_tabs_guess()
{
    const auto listed = [](std::string_view path, std::size_t rank)
    {
        return app::SessionTab{path_of(path),
                               core::TextPosition{core::LineNumber{1}, core::Column{1}},
                               core::LineNumber{1}, rank};
    };
    Editing editing(SettingsReading{guides_on()},
                    SessionReading{std::optional<app::Session>{
                        app::Session{{listed(two_path, 0), listed(eight_path, 1)}, 0}}},
                    [](ScriptedFiles &files)
                    {
                        files.hold_at(std::string(two_path), Bytes{std::string(two_space)});
                        files.hold_at(std::string(eight_path), Bytes{std::string(eight_space)});
                    });
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    expect(step_of(controller.frame()) == 2, "the tab read at the start guesses its unit");
    expect(step_of(controller.apply_frame(app::SwitchTab{1})) == 8,
           "a tab read when it is first shown guesses its unit");
    expect(step_of(controller.apply_frame(app::SwitchTab{0})) == 2,
           "switching back keeps the guessed unit");
}

// 保存と切り替えは推し当て直さない（決定 7）。
void verify_save_and_switch_keep_unit()
{
    Editing editing(SettingsReading{guides_on()});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.files().hold_at(std::string(eight_path), Bytes{std::string(eight_space)});
    applied(controller, app::OpenDocument{path_of(eight_path)});
    applied(controller, app::InsertText{"x\n  y\n    z\n  w\n  v\n"});
    const auto saved =
        controller.apply_frame(app::SaveDocument{path_of(eight_path), core::TextEncoding::utf8});
    expect(step_of(saved) == 8, "saving a body that now reads as two-space keeps the unit");
    editing.files().hold_at(std::string(two_path), Bytes{std::string(two_space)});
    applied(controller, app::OpenDocument{path_of(two_path)});
    expect(step_of(controller.frame()) == 2, "the second document guesses its own unit");
    expect(step_of(controller.apply_frame(app::SwitchTab{0})) == 8,
           "switching back does not guess again");
}

// 前回のプロセスが残した退避の 1 本。
[[nodiscard]] app::AutosaveRemnant remnant_of(std::string location,
                                              std::optional<core::FilePath> path,
                                              std::optional<app::AutosaveForm> form)
{
    return app::AutosaveRemnant{std::move(location),
                                app::AutosaveId{0},
                                std::move(path),
                                core::DisplayText::parse("remnant").value(),
                                core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                form};
}

// 自動退避からの復元:
// 名前のある退避は開く道と同じ入口の関数で推し当て、名前の無い退避は無題の文書と
// 同じく設定の幅（2026-11-23 の追補・レビュー R3）。一覧に形のある退避と無い退避の両方の道。
void verify_restored_remnants_guess()
{
    const std::optional<app::AutosaveForm> form{
        app::AutosaveForm{core::TextEncoding::utf8, std::nullopt, core::LineEnding::lf}};
    for (const auto &shape : {std::optional<app::AutosaveForm>{}, form})
    {
        Editing editing(SettingsReading{guides_on()});
        auto &controller = editing.controller();
        applied(controller, app::VisibleLines{10});
        editing.autosave().remain(remnant_of("1f/0", path_of(two_path), shape),
                                  std::string(two_space));
        editing.autosave().remain(remnant_of("1f/1", std::nullopt, shape), std::string(two_space));
        static_cast<void>(controller.autosave_remnants());
        const auto restored = controller.apply_frame(app::RestoreAutosave{});
        expect(restored.tabs.size() == 3 && step_of(restored) == 2,
               "a restored named remnant guesses its unit like opening");
        expect(step_of(controller.apply_frame(app::SwitchTab{2})) == 4,
               "a restored untitled remnant uses the configured width");
    }
}

// 推し当ての 64 KiB の打ち切りと窓の端の whole_code_points（決定 7）。
void verify_guess_byte_window()
{
    // 2 票（差 2）の後を、票にならない長い行で 64 KiB の手前まで埋める。
    std::string head = "a\n  b\nc\n";
    const std::string filler = std::string(1000, 'x') + "\n";
    while (head.size() + filler.size() <= core::indent_guess_bytes - 6)
    {
        head += filler;
    }
    head += std::string(core::indent_guess_bytes - 6 - head.size() - 1, 'x') + "\n";
    // 窓の端に掛かる行: 字下げ 4 の後の「あ」（3 バイト）の 2 バイトだけが窓に入る。端で割った字を
    // 外すと行は空白だけ（票なし・計 2 票で値なし）。外さなければ字下げ 4 の行として 3 票目になる。
    const std::string straddling = head + "    \xE3\x81\x82\nz\n";
    expect(straddling.find("\xE3\x81\x82") == core::indent_guess_bytes - 2,
           "the straddling character starts two bytes before the window's end");
    expect(!core::guessed_indent_unit(buffer_of(straddling)).has_value(),
           "the window's end drops the split character before the lines are counted");
    std::string late = head + "x\n";
    late += "a\n  b\nc\n  d\ne\n  f\n";
    expect(late.size() > core::indent_guess_bytes &&
               !core::guessed_indent_unit(buffer_of(late)).has_value(),
           "lines after the first 64 KiB do not vote");
    expect(core::guessed_indent_unit(buffer_of("a\n  b\nc\n  d\n" + late)) == IndentUnit::two,
           "the same lines within the window vote");
}

// 行頭の字下げの窓の上限（2026-11-23 の追補 5・レビュー R4）。
void verify_indent_window_limit()
{
    const std::size_t long_run = (core::indent_guess_bytes * 3) + 5;
    const auto blank = buffer_of("x\n" + std::string(long_run, ' ') + "\ny\n");
    expect(core::line_indent_cells(blank, core::LineNumber{2}) ==
               std::optional<std::size_t>{core::indent_guess_bytes},
           "a giant line of spaces answers the indent cut at the window's limit");
    const auto tabs = buffer_of(std::string(long_run, '\t') + "z");
    expect(core::line_indent_cells(tabs, core::LineNumber{1}) ==
               std::optional<std::size_t>{core::indent_guess_bytes * core::vim_tab_stop},
           "the cut counts the Tabs inside the limit");
    const auto under = buffer_of(std::string(core::indent_guess_bytes - 1, ' ') + "z");
    expect(core::line_indent_cells(under, core::LineNumber{1}) ==
               std::optional<std::size_t>{core::indent_guess_bytes - 1},
           "an indent just under the limit is counted whole");
    const auto short_blank = buffer_of("x\n" + std::string(core::indent_guess_bytes - 1, ' '));
    expect(!core::line_indent_cells(short_blank, core::LineNumber{2}).has_value(),
           "a line of spaces under the limit is blank");
}

// 孤立した CR（行の途中の `\r`・`^M` で見える字）は空白に数えない。CRLF の行の終わりの `\r` だけを
// 空白と同じに読む（レビュー L3）。
void verify_carriage_returns()
{
    expect(core::blank_indent_line("  \r") && !core::blank_indent_line("  \r  ") &&
               !core::blank_indent_line("\r\r"),
           "only the carriage return that ends a CRLF line counts as blank");
    const auto text = buffer_of("    a\r\n  \r  \r\n  \r\n");
    expect(core::line_indent_cells(text, core::LineNumber{2}) == std::optional<std::size_t>{2} &&
               !core::line_indent_cells(text, core::LineNumber{3}).has_value(),
           "a line with a lone carriage return keeps its indent and a CRLF blank line is blank");
}

// プレビュー（Markdown の脇・全面）は本文の行ではないので欄を付けない（決定 14）。
void verify_previews_have_no_guides()
{
    constexpr std::string_view note = "C:\\work\\note.md";
    Editing editing(SettingsReading{guides_on()});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    editing.files().hold_at(std::string(note), Bytes{std::string("# a\n  b\n    c\n  d\n")});
    applied(controller, app::OpenDocument{path_of(note)});
    const auto side = controller.apply_frame(app::TogglePreview{});
    expect(side.other.has_value() &&
               std::holds_alternative<app::PreviewFrame>(side.other.value()) && step_of(side) == 2,
           "the side preview is not body lines while the body keeps its guides");
    const auto full = controller.apply_frame(app::SetPreviewPlacement{app::PreviewPlacement::full});
    expect(full.full_preview.has_value() &&
               std::ranges::none_of(full.lines, [](const app::LineView &line)
                                    { return line.indent_guides.has_value(); }),
           "the full preview carries no guide fields");
}

// 上の端の空の行も、見えている範囲の外を 1 画面まで探す（決定 9）。
void verify_top_edge_reach()
{
    Editing reaching(SettingsReading{guides_on()});
    applied(reaching.controller(), app::VisibleLines{3});
    reaching.files().hold_at(std::string(two_path), Bytes{std::string("        y\n\n\n\n    x\n")});
    applied(reaching.controller(), app::OpenDocument{path_of(two_path)});
    place(reaching.controller(), 5);
    const auto reached = reaching.controller().frame();
    expect(reached.lines.size() == 3 && reached.lines.front().number == core::LineNumber{3} &&
               reached.lines.front().indent_guides.value_or(app::IndentGuides{0, 0, 0}).cells == 8,
           "a blank line at the top edge reads the line above within one screen");
    Editing distant(SettingsReading{guides_on()});
    applied(distant.controller(), app::VisibleLines{3});
    distant.files().hold_at(std::string(two_path),
                            Bytes{std::string("        y\n\n\n\n\n\n\n    x\n")});
    applied(distant.controller(), app::OpenDocument{path_of(two_path)});
    place(distant.controller(), 8);
    const auto cut = distant.controller().frame();
    expect(cut.lines.size() == 3 && cut.lines.front().number == core::LineNumber{6} &&
               cut.lines.front().indent_guides.value_or(app::IndentGuides{0, 0, 0}).cells == 4,
           "beyond one screen above the top edge keeps the side below");
}
} // namespace

void verify_indent_guide_paths()
{
    verify_reached_tabs_guess();
    verify_save_and_switch_keep_unit();
    verify_restored_remnants_guess();
    verify_guess_byte_window();
    verify_indent_window_limit();
    verify_carriage_returns();
    verify_previews_have_no_guides();
    verify_top_edge_reach();
}
} // namespace nenenib::tests
