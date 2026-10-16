// scope `--display` の単体テスト（ADR 0042 決定 2・ADR 0140 の決定 2・3・5・Issue #545）。
// 開いたときの格子表示の問い（判定は改行の索引だけ・文書ごとに 1 回・最初の本文の文字組みより
// 前）、表示形式の意図 SelectDisplay と Ex の `:set display=` の往復、格子の位置解決が本文の索引を
// 借りて core の純関数と一致し文字組みの port（rows / locate / hit）を呼ばないこと、索引を持つのは
// 巨大な行の文書と格子の文書だけであること、開く道が文字組み以外の port に触れないこと。
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
#include "GridCells.hpp"
#include "InsertText.hpp"
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
#include "VimTestSupport.hpp"
#include "VirtualColumnIndex.hpp"
#include "VisibleLines.hpp"
#include "VisualTextView.hpp"
#include "WalkRecentTab.hpp"

#include <algorithm>
#include <array>
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
    const auto text = buffer_of(body).with_virtual_columns();
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
    const auto unheld =
        state
            .with_opened(buffer_of(body),
                         app::Document{std::nullopt, core::TextEncoding::utf8, std::size_t{0}})
            .with_display(DisplayForm::grid);
    const app::VisualTextView borrowed(unheld, layouts, grid_geometry);
    expect(!borrowed.rows(core::LineNumber{1}).has_value(),
           "the grid view never builds an index for a text that holds none");
    const auto shown_as_text = state.with_display(DisplayForm::text);
    const app::VisualTextView text_view(shown_as_text, layouts, grid_geometry);
    static_cast<void>(text_view.rows(core::LineNumber{1}));
    expect(layouts.requests() == 1 && layouts.cells() == 2,
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
    expect(!frame.grid.empty() && frame.grid.front().row == 2 &&
               frame.grid.front().screen_row == 0 && frame.horizontal == 0.0F,
           "the end of a 5-row line follows to the last 3 screen rows without horizontal scroll");
}

[[nodiscard]] std::string mixed_lines(std::size_t count)
{
    const std::array<std::string, 3> heads{"abc日本語\tdef", repeated("x", 37) + "全角",
                                           "e\xCC\x81\x01z"};
    std::string text;
    for (std::size_t line = 0; line < count; ++line)
    {
        text += heads.at(line % heads.size());
        text += (line % 50 == 49) ? repeated("漢", 30'000) + "\n" : "\n";
    }
    return text;
}

// 行 number の中の照らす位置（code point の先頭）。長い行は約 97 等分の間隔で間引く。
[[nodiscard]] std::vector<core::Offset> probes_of(const core::TextBuffer &text,
                                                  core::LineNumber number)
{
    const auto begin = text.line_start(number).value;
    const auto all = text.text_range(core::Offset{begin}, text.line_end(number));
    const auto stride = (all.size() / 97) + 1;
    std::vector<core::Offset> probes;
    core::Offset at{0};
    std::size_t next = 0;
    while (at.value < all.size())
    {
        if (at.value >= next)
        {
            probes.push_back(core::Offset{begin + at.value});
            next = at.value + stride;
        }
        at = core::next_code_point(all, at);
    }
    probes.push_back(core::Offset{begin + all.size()});
    return probes;
}

// 1 行ぶん、GridCells の答えが core の参照（for_grid の索引）と一致するか。
[[nodiscard]] bool line_cells_match(app::GridCells &cells,
                                    const core::VirtualColumnIndex &reference,
                                    const core::TextBuffer &text, core::LineNumber number)
{
    bool matched =
        cells.rows(number) ==
        core::grid_rows(reference.cursor_at(text, text.line_end(number)).cell, cells.columns());
    for (const core::Offset at : probes_of(text, number))
    {
        const auto shown = cells.point_of(number, at);
        matched =
            matched && shown == core::grid_cell_of(reference, text, at) &&
            cells.byte_of(number, shown) == core::grid_byte_of(reference, text, number, shown);
    }
    return matched;
}

// GridCells（決定 4・5）は本文の索引を借り、送りを含む升を for_grid（core の参照）と同じに答える。
// 塊をまたぐ全角・Tab・制御文字・結合文字の行で、行の中の位置と升の往復を照らす。
void verify_grid_cells_match()
{
    const auto text = buffer_of(mixed_lines(120)).with_virtual_columns();
    const auto &held = text.virtual_columns().value().get();
    for (const std::size_t columns : {std::size_t{7}, std::size_t{80}})
    {
        const auto reference = core::VirtualColumnIndex::for_grid(text, columns);
        app::GridCells cells(held, text, columns);
        bool matched = true;
        for (std::size_t line = 1; line <= text.line_count(); line += 7)
        {
            matched = line_cells_match(cells, reference, text, core::LineNumber{line}) && matched;
        }
        expect(matched, "GridCells answers the same cells as the grid index of core");
    }
}

constexpr std::size_t sixteen_mebibytes = 16U * 1024U * 1024U;
constexpr std::string_view ascii_path = "C:\\work\\ascii.txt";

// 16 MiB の ASCII の 1 行（決定 4）。位置解決は索引を借り、塊を 1 つも歩かない（割り算だけ）。
void verify_grid_cells_ascii()
{
    const auto text = buffer_of(std::string(sixteen_mebibytes, 'x')).with_virtual_columns();
    app::GridCells cells(text.virtual_columns().value().get(), text, 100);
    const core::LineNumber line{1};
    const core::Offset middle{(sixteen_mebibytes / 2) + 3};
    const auto point = cells.point_of(line, middle);
    expect(point == core::GridPoint{middle.value / 100, middle.value % 100},
           "the cell of an ASCII offset is the division by the columns");
    expect(cells.byte_of(line, point) == middle, "the offset of an ASCII cell is the product");
    expect(cells.rows(line) == (sixteen_mebibytes / 100) + 1,
           "the rows are the division rounded up");
    expect(cells.point_of(line, core::Offset{sixteen_mebibytes}) ==
               core::GridPoint{sixteen_mebibytes / 100, sixteen_mebibytes % 100},
           "the line end sits after the last cell");
    expect(cells.scanned() == 0, "an ASCII line is resolved without walking any chunk");
}

[[nodiscard]] const core::VirtualColumnIndex *held_index(const app::EditorController &controller)
{
    const auto index = controller.text().virtual_columns();
    return index ? &index.value().get() : nullptr;
}

// 索引を持つのは巨大な行の文書（答えに依らない）と格子にした文書だけ（決定 4）。普通の文書の
// 編集は索引に触れない。格子の 1 打鍵は索引を作り直さない（編集は触れた塊だけ）。
void verify_index_holders()
{
    Editing editing;
    hold_documents(editing.files());
    editing.files().hold_at(std::string(ascii_path), Bytes{std::string(sixteen_mebibytes, 'x')});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, grid_geometry});
    applied(controller, open_of(small_path));
    expect(!controller.text().has_virtual_columns(), "a small document holds no column index");
    applied(controller, app::InsertText{"a"});
    expect(!controller.text().has_virtual_columns() &&
               controller.text().rebuilt_column_chunks() == 0,
           "editing a document without an index touches no index");
    applied(controller, SelectDisplay{DisplayForm::grid});
    expect(controller.text().has_virtual_columns(), "turning a document into the grid holds one");
    applied(controller, open_of(giant_path));
    expect(controller.text().has_virtual_columns(), "opening a giant line holds the index at once");
    applied(controller, SelectDisplay{DisplayForm::text});
    expect(controller.text().has_virtual_columns() &&
               controller.frame().display == DisplayForm::text,
           "answering no keeps the index for the text window");
    applied(controller, open_of(ascii_path));
    applied(controller, SelectDisplay{DisplayForm::grid});
    const auto *before = held_index(controller);
    static_cast<void>(controller.apply(
        app::MoveCaret{core::CaretMotion::next_character, core::SelectionAnchoring::collapse}));
    static_cast<void>(controller.frame());
    expect(before != nullptr && held_index(controller) == before,
           "a grid keystroke borrows the same index without rebuilding it");
    applied(controller, app::InsertText{"y"});
    expect(controller.text().rebuilt_column_chunks() == 1,
           "typing into the 16 MiB grid line rebuilds only the touched chunk");
}

// 起動の文書の問い（決定 2）。巨大な行の文書の最初の frame は本文を組まない。問いの無い文書の
// 起動は待たない。
void verify_startup_before_layout()
{
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
    const app::EditorPorts ports{appearance, clipboard, files,   code_pages, settings,    themes,
                                 session,    history,   folders, bookmarks,  text_layouts};
    app::EditorController started(ports, {open_of(giant_path)});
    expect(started.delivery().display_question, "starting with a giant line asks");
    const auto first = started.apply(app::VisibleLines{10, grid_geometry});
    const EditorFrame shown = started.frame();
    expect(!first.display_question && shown.display_pending && shown.lines.empty() &&
               text_layouts.requests() == 0,
           "starting with a giant line draws the first frame without laying out the body");
    app::EditorController plain(ports, {open_of(small_path)});
    expect(!plain.delivery().display_question && !plain.frame().display_pending,
           "starting with a small file neither asks nor waits");
}

// 問いは最初の本文の文字組みより前（決定 2・D53）。答えを待つ間は本文の文字組みの port を 1 回も
// 呼ばず、frame は本文の行を持たず display_pending を立てる。窓の寸法が替わっても同じで、問いは
// 1 回だけ出る。答え（いいえも）が届いてから文字組みを始める。
void verify_question_before_layout()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10, grid_geometry});
    const auto before = editing.text_layouts().requests();
    const EditorDelivery opened = controller.apply(open_of(giant_path));
    const EditorFrame waiting = controller.frame();
    expect(opened.display_question && waiting.display_pending && waiting.lines.empty(),
           "opening a giant line asks with an empty body");
    const EditorDelivery resized = controller.apply(app::VisibleLines{12, grid_geometry});
    expect(!resized.display_question && controller.frame().display_pending,
           "a resize while the question is open neither asks again nor answers");
    expect(editing.text_layouts().requests() == before, "nothing is laid out before the answer");
    const EditorFrame answered = controller.apply_frame(SelectDisplay{DisplayForm::text});
    expect(!answered.display_pending && !answered.lines.empty() &&
               editing.text_layouts().requests() > before,
           "the answer no lays out the body as text");
    verify_startup_before_layout();
}

[[nodiscard]] EditorFrame grid_frame_of(Editing &editing, std::string text, std::size_t visible)
{
    editing.files().hold_at(std::string(small_path), Bytes{std::move(text)});
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{visible, grid_geometry});
    applied(controller, open_of(small_path));
    applied(controller, SelectDisplay{DisplayForm::grid});
    return controller.frame();
}

[[nodiscard]] bool row_is(const app::GridRowView &row, std::string_view text,
                          const std::vector<std::size_t> &starts)
{
    return row.text == text && row.starts == starts;
}

[[nodiscard]] std::vector<std::size_t> counted(std::size_t count)
{
    std::vector<std::size_t> starts(count + 1);
    for (std::size_t index = 0; index <= count; ++index)
    {
        starts.at(index) = index;
    }
    return starts;
}

// 格子の frame は見えている画面行だけ（決定 5）。本文の行を文字組みせず、画面の下に半分かかる
// 画面行も 1 つ作る。各画面行は升数ぶんの文字列。
void verify_grid_frame_rows()
{
    Editing editing;
    static_cast<void>(grid_frame_of(editing, repeated("a", 200) + "\nb", 3));
    const auto before = editing.text_layouts().requests();
    const EditorFrame frame = editing.controller().frame();
    expect(editing.text_layouts().requests() == before,
           "the grid frame lays out no body line through the port");
    expect(frame.lines.empty() && frame.grid.size() == 4,
           "a 25-row line in 3 visible rows makes the 3 rows and the half row below");
    bool rows = frame.grid.size() == 4;
    for (std::size_t index = 0; rows && index < frame.grid.size(); ++index)
    {
        const auto &row = frame.grid.at(index);
        rows = row.number == core::LineNumber{1} && row.row == index &&
               row.screen_row == static_cast<std::int64_t>(index) &&
               row_is(row, "aaaaaaaa", counted(8));
    }
    expect(rows, "each screen row carries its 8 cells of text and the cell of every character");
}

// 幅 2 は 2 升で、右端に収まらなければ空白を置いて次の画面行へ送る。Tab は空白の升、制御文字は
// `^A` の 1 升ずつで置き換えの升を持つ（決定 4・ADR 0040）。
void verify_grid_row_cells()
{
    Editing editing;
    const EditorFrame frame = grid_frame_of(editing, "abcdefg\xE6\x97\xA5h\nab\tc\nx\x01y", 10);
    expect(frame.grid.size() == 5, "the three lines take 2, 2 and 1 screen rows");
    if (frame.grid.size() != 5)
    {
        return;
    }
    expect(row_is(frame.grid.at(0), "abcdefg ", counted(8)) &&
               row_is(frame.grid.at(1), "\xE6\x97\xA5h", {0, 2, 3}),
           "a wide character that does not fit is sent to the next row and takes 2 cells");
    expect(row_is(frame.grid.at(2), "ab      ", counted(8)) &&
               row_is(frame.grid.at(3), "c", {0, 1}),
           "a tab is blank cells up to the next stop of 8");
    expect(row_is(frame.grid.at(4), "x^Ay", counted(4)) &&
               frame.grid.at(4).replaced == std::vector<app::GridCellSpan>{{1, 3}},
           "a control character is drawn as ^A in one cell per letter and marked replaced");
}

// 本文の位置の範囲は画面行ごとの升の範囲へ写る（決定 5）。画面行をまたぐ選択と検索の当たりは
// 画面行ごとに 1 つずつ、改行を越える選択は行の最後の画面行に印。
void verify_grid_marks()
{
    Editing editing;
    static_cast<void>(grid_frame_of(editing, "aaaaaaXYZaaaa\nb", 10));
    auto &controller = editing.controller();
    applied(controller, app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{7}},
                                        core::SelectionAnchoring::collapse});
    const EditorFrame selected = controller.apply_frame(
        app::PlaceCaret{core::TextPosition{core::LineNumber{2}, core::Column{1}},
                        core::SelectionAnchoring::extend});
    expect(selected.grid.size() >= 2 && selected.grid.at(0).selection == app::GridCellSpan{6, 8} &&
               selected.grid.at(1).selection == app::GridCellSpan{0, 5} &&
               !selected.grid.at(0).newline_selected && selected.grid.at(1).newline_selected,
           "a selection over a row boundary and the newline is one span per row and a mark");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    vim_normal(controller, "gg/XYZ<CR>");
    const EditorFrame found = controller.frame();
    expect(found.grid.size() >= 2 &&
               found.grid.at(0).matches == std::vector<app::GridCellSpan>{{6, 8}} &&
               found.grid.at(1).matches == std::vector<app::GridCellSpan>{{0, 1}} &&
               found.grid.at(0).current_match == app::GridCellSpan{6, 8} &&
               found.grid.at(1).current_match == app::GridCellSpan{0, 1},
           "a match over a row boundary is painted as two spans, one on each row");
    expect(found.grid.size() >= 2 && found.grid.at(0).caret == app::GridCellSpan{6, 7} &&
               !found.grid.at(1).caret.has_value(),
           "the block caret is the cell of the character under it");
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
    verify_grid_cells_match();
    verify_grid_cells_ascii();
    verify_index_holders();
    verify_question_before_layout();
    verify_grid_follow();
    verify_grid_frame_rows();
    verify_grid_row_cells();
    verify_grid_marks();
    verify_quiet_open();
}

void verify_display_scope()
{
    verify_display_contracts();
}
} // namespace nenenib::tests
