// scope `--display` の単体テスト（ADR 0042 決定 2・ADR 0140 の決定 2・3・5・Issue #545）。
// 開いたときの格子表示の問い（判定は改行の索引だけ・文書ごとに 1 回）、表示形式の意図
// SelectDisplay と Ex の `:set display=` の往復、格子の位置解決が core の純関数と一致し文字組みの
// port（rows / locate / hit）を呼ばないこと、開く道が文字組み以外の port に触れないこと。
#include "CaretAffinity.hpp"
#include "CaretMotion.hpp"
#include "DevicePixels.hpp"
#include "DisplayForm.hpp"
#include "DisplayFormLabel.hpp"
#include "DisplayQuestion.hpp"
#include "Document.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "GiantLine.hpp"
#include "Grid.hpp"
#include "MoveCaret.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "ScriptedTextLayouts.hpp"
#include "SelectDisplay.hpp"
#include "SelectEditMode.hpp"
#include "SelectionAnchoring.hpp"
#include "Session.hpp"
#include "SessionEnd.hpp"
#include "SessionTab.hpp"
#include "SettleRecentTab.hpp"
#include "StatusBarLayout.hpp"
#include "SwitchTab.hpp"
#include "TabStep.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "Utf8.hpp"
#include "VirtualColumnIndex.hpp"
#include "VisibleLines.hpp"
#include "VisualTextView.hpp"
#include "WalkRecentTab.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorDelivery;
using app::EditorFrame;
using app::OpenDocument;
using app::SelectDisplay;
using core::DisplayForm;

constexpr std::string_view giant_path = "C:\\work\\giant.txt";
constexpr std::string_view small_path = "C:\\work\\small.txt";
// 1 升 10 画素（ScriptedTextLayouts）で 8 升の幅。
constexpr core::TextViewGeometry grid_geometry{85, 20, 96};

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string repeated(std::string_view unit, std::size_t count)
{
    std::string text;
    text.reserve(unit.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += unit;
    }
    return text;
}

[[nodiscard]] std::string giant_text()
{
    return "head\n" + repeated("x", core::giant_line_threshold + 1) + "\ntail";
}

void hold_documents(ScriptedFiles &files)
{
    files.hold_at(std::string(giant_path), Bytes{giant_text()});
    files.hold_at(std::string(small_path), Bytes{std::string("small\nfile")});
}

[[nodiscard]] OpenDocument open_of(std::string_view path)
{
    return OpenDocument{path_of(path)};
}

// 問いの判定（決定 2）。しきい値はコードポイントの数で、ちょうどは聞かず 1 つ超えたら聞く。
void verify_asks_grid()
{
    const auto threshold = core::giant_line_threshold;
    expect(!app::asks_grid(buffer_of("")), "an empty text never asks");
    expect(!app::asks_grid(buffer_of(repeated("a", threshold))),
           "a line of exactly the threshold does not ask");
    expect(app::asks_grid(buffer_of(repeated("a", threshold + 1))),
           "a line one past the threshold asks");
    expect(app::asks_grid(buffer_of("a\n" + repeated("b", threshold + 1) + "\nc")),
           "a giant line between short lines asks");
    expect(app::asks_grid(buffer_of("a\r\n" + repeated("b", threshold + 1) + "\r\nc")),
           "a giant line in a CRLF text asks");
    expect(!app::asks_grid(buffer_of(repeated(repeated("x", 80) + "\n", 2'000))),
           "many short lines do not ask");
    expect(!app::asks_grid(buffer_of(repeated("日", threshold))),
           "a wide line past the threshold in bytes but not in code points does not ask");
    expect(app::asks_grid(buffer_of(repeated("日", threshold + 1))),
           "a wide line past the threshold in code points asks");
    expect(
        app::asks_grid(buffer_of(repeated("日", threshold) + "\n" + repeated("a", threshold + 1))),
        "a shorter line in bytes that is past the threshold in code points asks");
}

// 文書ごとに 1 回（決定 2）。印は 1 意図だけ立ち、同じ文書へ戻っても立たない。
void verify_question_once()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, grid_geometry});
    const EditorDelivery opened = controller.apply(open_of(giant_path));
    expect(opened.display_question, "opening a giant line raises the question");
    expect(controller.frame().display_question,
           "the frame carries the same question until the next intent");
    const EditorDelivery next = controller.apply(app::VisibleLines{10, grid_geometry});
    expect(!next.display_question, "the next intent clears the question");
    expect(!controller.apply(open_of(small_path)).display_question,
           "opening a file without giant lines does not ask");
    expect(!controller.apply(app::SwitchTab{0}).display_question,
           "switching back to an asked document does not ask again");
    expect(!controller.apply(open_of(giant_path)).display_question,
           "opening the same file again switches without asking");
    expect(!controller.apply(app::NewTab{}).display_question, "a new tab never asks");
    expect(controller.frame().display == DisplayForm::text,
           "the question itself leaves the display as text");
}

// 起動（引数・前回のタブ）は最後にアクティブになった文書だけを聞き、ほかは見るときに聞く。
void verify_startup_question()
{
    {
        Editing editing(SessionReading{std::nullopt}, hold_documents);
        expect(!editing.controller().delivery().display_question,
               "starting without files and without a list does not ask");
    }
    {
        const auto tab = [](std::string_view path, std::size_t rank)
        {
            return app::SessionTab{path_of(path),
                                   core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                   core::LineNumber{1}, rank};
        };
        Editing restored(SessionReading{std::optional<app::Session>{
                             app::Session{{tab(small_path, 1), tab(giant_path, 0)}, 1}}},
                         hold_documents);
        expect(restored.controller().delivery().display_question,
               "restoring a giant active tab asks before the first frame");
    }
    ScriptedAppearance appearance{Reading{core::Appearance::dark}};
    ScriptedClipboard clipboard;
    ScriptedFiles files;
    hold_documents(files);
    ScriptedCodePages code_pages;
    ScriptedSettings settings;
    ScriptedThemes themes;
    ScriptedSession session;
    ScriptedHistory history;
    ScriptedFolders folders;
    ScriptedBookmarks bookmarks;
    ScriptedTextLayouts text_layouts;
    app::EditorController controller(app::EditorPorts{appearance, clipboard, files, code_pages,
                                                      settings, themes, session, history, folders,
                                                      bookmarks, text_layouts},
                                     {open_of(giant_path), open_of(small_path)});
    expect(!controller.delivery().display_question,
           "starting with a small file last does not ask about the giant one yet");
    expect(!controller.apply(app::WalkRecentTab{core::TabStep::next}).display_question,
           "walking onto the giant tab with Ctrl held does not ask");
    expect(controller.apply(app::SettleRecentTab{}).display_question,
           "settling the walk on the giant tab asks once");
    expect(!controller.apply(app::SwitchTab{1}).display_question &&
               !controller.apply(app::SwitchTab{0}).display_question,
           "the giant tab is asked only once");
}

// SelectDisplay の往復（決定 3）。文書ごとの値で、設定にもタブの一覧にも入れない。
void verify_select_display()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, grid_geometry});
    applied(controller, open_of(small_path));
    expect(controller.frame().display == DisplayForm::text, "a document opens as text");
    const EditorFrame grid = controller.apply_frame(SelectDisplay{DisplayForm::grid});
    expect(grid.display == DisplayForm::grid && core::display_form_label(grid.display) == "grid",
           "SelectDisplay grid shows the grid mark");
    applied(controller, app::NewTab{});
    expect(controller.frame().display == DisplayForm::text, "another tab keeps its own display");
    applied(controller, app::SwitchTab{0});
    expect(controller.frame().display == DisplayForm::grid,
           "switching back restores the parked display");
    const EditorFrame text = controller.apply_frame(SelectDisplay{DisplayForm::text});
    expect(text.display == DisplayForm::text && core::display_form_label(text.display).empty(),
           "SelectDisplay text removes the mark");
    expect(editing.settings().writes() == 0, "the display is never written to the settings");
    applied(controller, SelectDisplay{DisplayForm::grid});
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    Editing restored(editing.session().written(), hold_documents);
    expect(restored.controller().frame().display == DisplayForm::text,
           "a restored tab opens as text (the list has no display)");
}

// `:set display=`（決定 3）。候補と補完は同じ表の 2 行で、Ctrl+P の `:` も同じ表。
void verify_ex_display()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    const EditorFrame grid = run_ex(controller, "set display=grid");
    expect(grid.display == DisplayForm::grid && grid.command_message.has_value() &&
               grid.command_message.value().text() == "display=grid",
           ":set display=grid selects the grid");
    const EditorFrame text = run_ex(controller, "set display=text");
    expect(text.display == DisplayForm::text && text.command_message.has_value() &&
               text.command_message.value().text() == "display=text",
           ":set display=text returns to text");
    const EditorFrame wrong = run_ex(controller, "set display=hex");
    expect(wrong.display == DisplayForm::text, "an unknown display value changes nothing");
    expect(editing.settings().writes() == 0, ":set display= never writes the settings");
    const auto evaluated = core::evaluate_ex("set display=grid", core::default_editor_settings(),
                                             core::Appearance::dark);
    expect(evaluated.has_value() && !evaluated.value().settings.has_value() &&
               evaluated.value().display == DisplayForm::grid,
           "the Ex result carries the display without settings");
    expect(core::command_completions("set d") ==
               std::vector<std::string>{"set display=grid", "set display=text"},
           "completion lists both display values");
    const auto all = core::ex_command_candidates();
    const auto nowrap = std::ranges::find(all, std::string("set nowrap"));
    expect(nowrap != all.end() && std::next(nowrap) != all.end() &&
               *std::next(nowrap) == "set display=grid" && std::next(nowrap, 2) != all.end() &&
               *std::next(nowrap, 2) == "set display=text",
           "the two rows follow set nowrap in the candidate table");
    const auto listed = choices_for(":set disp");
    expect(listed.size() >= 2 && listed.at(0).command == "set display=grid" &&
               listed.at(1).command == "set display=text",
           "the Ctrl+P : list shows the same rows");
}

// ステータスバーの印の場所（決定 3）。最初の項目の左隣で、案内の右端より右。
void verify_status_mark()
{
    for (const std::uint32_t dpi : {96U, 120U, 144U})
    {
        const auto status =
            core::status_bar_layout(core::to_pixels(800, dpi), core::to_pixels(450, dpi), dpi);
        const auto &first = status.items.front();
        expect(status.display.right <= first.left &&
                   status.display.left >= first.left - core::to_pixels(28, dpi) &&
                   status.display.left < status.display.right &&
                   status.display.top == status.band.top &&
                   status.display.bottom == status.band.bottom,
               "the grid mark sits between the guide and the first status item");
    }
    expect(core::display_form_label(DisplayForm::grid) == "grid" &&
               core::display_form_label(DisplayForm::text).empty(),
           "the mark text comes from the core table");
}

[[nodiscard]] std::vector<core::Offset> code_point_starts(const core::TextBuffer &text)
{
    const auto all = text.text();
    std::vector<core::Offset> starts;
    core::Offset at{0};
    while (at.value <= all.size())
    {
        starts.push_back(at);
        if (at.value == all.size())
        {
            break;
        }
        at = core::next_code_point(all, at);
    }
    return starts;
}

constexpr std::size_t grid_columns = 8;

[[nodiscard]] std::size_t line_rows(const core::VirtualColumnIndex &index,
                                    const core::TextBuffer &text, core::LineNumber number)
{
    return core::grid_rows(index.cursor_at(text, text.line_end(number)).cell, grid_columns);
}

[[nodiscard]] bool rows_match(const app::VisualTextView &view,
                              const core::VirtualColumnIndex &index, const core::TextBuffer &text)
{
    bool matched = true;
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        const auto rows = view.rows(core::LineNumber{line});
        matched = matched && rows.has_value() &&
                  rows.value() == line_rows(index, text, core::LineNumber{line});
    }
    return matched;
}

[[nodiscard]] bool locations_match(const app::VisualTextView &view,
                                   const core::VirtualColumnIndex &index,
                                   const core::TextBuffer &text)
{
    bool matched = true;
    for (const core::Offset at : code_point_starts(text))
    {
        const auto point = core::grid_cell_of(index, text, at);
        const auto located = view.location(core::TextCaret{text.position_of(at)});
        matched = matched && located.has_value() && located.value().row == point.row &&
                  located.value().x == static_cast<float>(point.cell) * 10.0F;
    }
    return matched;
}

// 各升の左寄りを叩いた位置が grid_byte_of と一致するか（1 行ぶん）。
[[nodiscard]] bool line_hits_match(const app::VisualTextView &view,
                                   const core::VirtualColumnIndex &index,
                                   const core::TextBuffer &text, core::LineNumber number)
{
    bool matched = true;
    const auto rows = line_rows(index, text, number);
    for (std::size_t at = 0; at < rows * grid_columns; ++at)
    {
        const core::GridPoint point{at / grid_columns, at % grid_columns};
        const auto expected = text.position_of(core::grid_byte_of(index, text, number, point));
        const auto hit = view.caret_at(core::VisualPosition{number, point.row},
                                       (static_cast<float>(point.cell) * 10.0F) + 2.0F);
        matched = matched && hit.has_value() && hit.value().position == expected;
    }
    return matched;
}

[[nodiscard]] bool hits_match(const app::VisualTextView &view,
                              const core::VirtualColumnIndex &index, const core::TextBuffer &text)
{
    bool matched = true;
    for (std::size_t line = 1; line <= text.line_count(); ++line)
    {
        matched = matched && line_hits_match(view, index, text, core::LineNumber{line});
    }
    return matched;
}

// 格子の位置解決（決定 4・5）。rows / location / caret_at が core の純関数と一致し、文字組みの
// rows / locate / hit を 1 回も呼ばない。1 升は port の cell から 1 回だけ読む。
void verify_grid_resolution()
{
    const std::string body =
        "abcdefghij\tk\n日本語のテキストです\n\nxe\xCC\x81y" + repeated("z", 20);
    const auto text = buffer_of(body);
    const auto state = app::EditorState::create(core::Appearance::dark, core::EditMode::ordinary)
                           .with_opened(text, app::Document{std::nullopt, core::TextEncoding::utf8,
                                                            std::size_t{0}})
                           .with_display(DisplayForm::grid);
    ScriptedTextLayouts layouts;
    const app::VisualTextView view(state, layouts, grid_geometry);
    const auto index = core::VirtualColumnIndex::for_grid(text, grid_columns);
    expect(rows_match(view, index, text), "grid rows equal grid_rows of the line's cells");
    expect(locations_match(view, index, text),
           "grid locations equal grid_cell_of in cells of 10 pixels");
    expect(hits_match(view, index, text), "grid hits equal grid_byte_of at the left of each cell");
    expect(layouts.requests() == 0, "the grid never asks the layout for rows, locate or hit");
    expect(layouts.cells() == 1, "the cell is read once per resolution");
    const auto shown_as_text = state.with_display(DisplayForm::text);
    const app::VisualTextView text_view(shown_as_text, layouts, grid_geometry);
    static_cast<void>(text_view.rows(core::LineNumber{1}));
    expect(layouts.requests() == 1 && layouts.cells() == 1,
           "the text display keeps using the layout and never reads the cell");
}

// 格子の追従（決定 5・7）。画面行の単位で、横の送りは 0。
void verify_grid_follow()
{
    Editing editing;
    editing.files().hold_at(std::string(small_path), Bytes{repeated("a", 40) + "\nb"});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3, grid_geometry});
    applied(controller, open_of(small_path));
    applied(controller, SelectDisplay{DisplayForm::grid});
    const auto before = editing.text_layouts().requests();
    static_cast<void>(controller.apply(
        app::MoveCaret{core::CaretMotion::line_end, core::SelectionAnchoring::collapse}));
    expect(editing.text_layouts().requests() == before,
           "moving in the grid resolves without the layout");
    const EditorFrame ended = controller.frame();
    expect(ended.caret.position.column.value == 9 &&
               ended.caret.affinity == core::CaretAffinity::trailing,
           "End stops at the right edge of the first screen row");
    static_cast<void>(
        controller.apply(app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{41}},
                                         core::SelectionAnchoring::collapse}));
    const EditorFrame frame = controller.frame();
    expect(!frame.lines.empty() && frame.lines.front().screen_row == -2 && frame.horizontal == 0.0F,
           "the end of a 5-row line follows to the last 3 screen rows without horizontal scroll");
}

// 開く道は、ファイルの 1 回の読みと文字組みのほかの port に触れない（決定 2）。
void verify_quiet_open()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, grid_geometry});
    applied(controller, open_of(giant_path));
    applied(controller, open_of(small_path));
    applied(controller, app::SwitchTab{0});
    expect(editing.files().reads() == 2 && editing.settings().writes() == 0 &&
               editing.session().writes() == 0 && editing.history().reads() == 0 &&
               editing.history().writes() == 0 && editing.folders().requests().empty() &&
               editing.bookmarks().writes() == 0 && editing.text_layouts().cells() == 0,
           "opening and asking read each file once and touch no other port");
}
} // namespace

void verify_display_contracts()
{
    verify_asks_grid();
    verify_question_once();
    verify_startup_question();
    verify_select_display();
    verify_ex_display();
    verify_status_mark();
    verify_grid_resolution();
    verify_grid_follow();
    verify_quiet_open();
}

void verify_display_scope()
{
    verify_display_contracts();
}
} // namespace nenenib::tests
