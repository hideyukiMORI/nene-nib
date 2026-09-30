// scope `--command-palette` の単体テスト（ADR 0042 決定 2）。
#include "ActivateCommandChoice.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandLine.hpp"
#include "CommandPalette.hpp"
#include "CommandPaletteView.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "DevicePixels.hpp"
#include "DisplayText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "FileFailure.hpp"
#include "FileHistory.hpp"
#include "FileHistoryFailure.hpp"
#include "FilePath.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InputLineView.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "NewTab.hpp"
#include "OpenCommandPalette.hpp"
#include "OpenDocument.hpp"
#include "OpenTabList.hpp"
#include "PaletteLayout.hpp"
#include "PaletteMarks.hpp"
#include "PaletteOrigin.hpp"
#include "PaletteQuery.hpp"
#include "PaletteScope.hpp"
#include "PasteCommand.hpp"
#include "Scopes.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedHistory.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "SubmitCommand.hpp"
#include "SwitchTab.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "ThemeChoice.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::CancelComposition;
using nenenib::application::CommitText;
using nenenib::application::ComposeText;
using nenenib::application::HistoryAction;
using nenenib::application::InsertText;
using nenenib::application::SelectAll;
using nenenib::application::SelectEditMode;
using nenenib::application::VimKeyPress;
using nenenib::core::builtin_themes;
using nenenib::core::BuiltinTheme;
using nenenib::core::EditMode;
using nenenib::core::HistoryDirection;
using nenenib::core::to_pixels;
using nenenib::core::VimCharacter;
using nenenib::core::width_of;

namespace core = nenenib::core;
namespace app = nenenib::application;

void verify_palette_choices()
{
    const auto all = choices_for(":");
    expect(all.size() == core::ex_command_candidates().size(), "Ex and palette share the catalog");
    // 空の入力の点数は候補の長さなので、最も短いタブの一覧 `tabs`（ADR 0057 の決定 7）が先頭。
    expect(all.front().command == "tabs" && all.at(1).command == "tabnew",
           "empty query has deterministic ranking");
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
    auto palette = core::CommandPalette::opened({}, ":", 0);
    expect(palette.input().text() == ":" && palette.selected() == 0 &&
               palette.scope() == core::PaletteScope::commands,
           "the colon mark lists the commands");
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
    static_cast<void>(controller.apply(app::CommandText{":fz"}));
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
    static_cast<void>(controller.apply(app::CommandText{":set fontsize=90"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.command_message.has_value() && editor.settings().writes() == 1,
           "invalid value uses Ex failure and does not save");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{":drac"}));
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
        static_cast<void>(controller.apply(app::CommandText{":" + std::string(query)}));
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
    static_cast<void>(controller.apply(app::CommandText{":"}));
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
        static_cast<void>(controller.apply(app::CommandText{":drac"}));
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
[[nodiscard]] core::CommandChoice tab_choice(std::string_view title, std::size_t number,
                                             std::optional<std::string_view> folder = std::nullopt)
{
    return core::CommandChoice{
        fixed_text(title), "tabnext " + std::to_string(number), core::CommandChoiceKind::execute,
        folder.has_value() ? std::optional{fixed_text(folder.value())} : std::nullopt,
        core::PaletteOrigin::tab};
}

[[nodiscard]] std::vector<std::string> commands_of(const std::vector<core::CommandChoice> &choices)
{
    std::vector<std::string> commands;
    for (const auto &choice : choices)
    {
        commands.push_back(choice.command);
    }
    return commands;
}

[[nodiscard]] bool query_is(std::string_view input, core::PaletteScope scope,
                            std::string_view query)
{
    const auto split = core::palette_query_of(input);
    return split.scope == scope && split.query == query;
}

// 出どころの記号の表（ADR 0060 の決定 2）。先頭の 1 文字が表にあればその出どころと残り、無ければ
// files と入力の全体。案内の文字列も同じ表から作る。
void verify_palette_marks()
{
    expect(query_is("", core::PaletteScope::files, ""), "an empty input lists every file");
    expect(query_is("#abc", core::PaletteScope::tabs, "abc"), "the hash mark selects the tabs");
    expect(query_is(":set", core::PaletteScope::commands, "set"),
           "the colon mark selects the commands");
    expect(query_is("abc", core::PaletteScope::files, "abc"), "plain text searches the files");
    expect(query_is("@a", core::PaletteScope::history, "a"), "the at mark selects the history");
    expect(query_is("*a", core::PaletteScope::files, "*a") &&
               query_is("/a", core::PaletteScope::files, "/a"),
           "marks outside the table are plain search text");
    expect(query_is("a#", core::PaletteScope::files, "a#"), "only the first character is a mark");
    expect(core::palette_mark_hint().text() == "# タブ\u3000@ 履歴\u3000: 設定",
           "the hint is built from the table");
    expect(core::palette_origin_label(core::PaletteOrigin::tab) == "開いているタブ",
           "the tab origin has its label");
    expect(core::palette_origin_label(core::PaletteOrigin::history) == "履歴",
           "the history origin has its label");
}

// 絞り込みと順（ADR 0060 の決定 4）。scope で残し、query が空なら列の順、名前の当たりが場所だけの
// 当たりより先、同点は列の順、どちらにも当たらなければ落ちる。
void verify_listed_choices()
{
    const std::vector<core::CommandChoice> entries{
        tab_choice("● note.txt", 1, "C:\\work"), tab_choice("無題", 2),
        tab_choice("Notes.md", 3, "C:\\docs"), tab_choice("readme.md", 4, "C:\\notes"),
        core::CommandChoice{fixed_text("tabs"), "tabs", core::CommandChoiceKind::execute}};
    const std::vector<std::string> all{"tabnext 1", "tabnext 2", "tabnext 3", "tabnext 4"};
    expect(commands_of(core::listed_choices(entries, core::PaletteScope::files, "")) == all,
           "files keeps every entry with an origin in list order");
    expect(commands_of(core::listed_choices(entries, core::PaletteScope::tabs, "")) == all,
           "tabs keeps the tab entries in list order");
    expect(core::listed_choices(entries, core::PaletteScope::commands, "").empty(),
           "commands are not listed from the entries");
    expect(commands_of(core::listed_choices(entries, core::PaletteScope::tabs, "NOTE")) ==
               std::vector<std::string>{"tabnext 3", "tabnext 1", "tabnext 4"},
           "name hits come first in score order and a folder hit follows, ignoring case");
    const std::vector<core::CommandChoice> folder_first{
        tab_choice("a.md", 1, "C:\\note"),
        tab_choice("x-note-with-a-rather-long-name.txt", 2, "C:\\work")};
    expect(commands_of(core::listed_choices(folder_first, core::PaletteScope::files, "note")) ==
               std::vector<std::string>{"tabnext 2", "tabnext 1"},
           "a name hit beats a shorter folder-only hit");
    const std::vector<core::CommandChoice> twins{tab_choice("x.txt", 1), tab_choice("a.txt", 2),
                                                 tab_choice("a.txt", 3)};
    expect(commands_of(core::listed_choices(twins, core::PaletteScope::tabs, "a")) ==
               std::vector<std::string>{"tabnext 2", "tabnext 3"},
           "equal scores keep list order");
    expect(core::listed_choices(entries, core::PaletteScope::files, "zq").empty(),
           "an entry that matches neither name nor folder is dropped");
    expect(commands_of(core::listed_choices(entries, core::PaletteScope::files, "work")) ==
               std::vector<std::string>{"tabnext 1"},
           "the folder and the name are searched together");
}

// CommandPalette は開いたときの列を持ち、入力の先頭の記号で出どころを切り替える（決定 1・5）。
void verify_palette_sources()
{
    const std::vector<core::CommandChoice> tabs{tab_choice("● note.txt", 1), tab_choice("無題", 2),
                                                tab_choice("Notes.md", 3),
                                                tab_choice("another note", 4)};
    const std::vector<std::string> all{"tabnext 1", "tabnext 2", "tabnext 3", "tabnext 4"};
    const auto empty = core::CommandPalette::opened(tabs, "", 0);
    expect(empty.scope() == core::PaletteScope::files && commands_of(empty.choices()) == all &&
               empty.selected() == 0,
           "an empty input lists every entry with an origin");
    const auto listed = core::CommandPalette::opened(tabs, "#", 2);
    expect(listed.scope() == core::PaletteScope::tabs && listed.input().text() == "#" &&
               listed.selected() == 2 && commands_of(listed.choices()) == all,
           "the hash input opens on the given row");
    expect(core::CommandPalette::opened(tabs, "#", 9).selected() == 0,
           "a row outside the choices selects the first");
    const auto filtered = listed.inserted("NOTE").value();
    expect(commands_of(filtered.choices()) ==
                   std::vector<std::string>{"tabnext 3", "tabnext 4", "tabnext 1"} &&
               filtered.selected() == 0,
           "a query keeps the matching titles in score order, ignoring case");
    expect(listed.edited(core::CommandEdit::complete_next).selected() == 3 &&
               listed.edited(core::CommandEdit::complete_previous).selected() == 1,
           "up and down move the selection over the tab rows");
    const auto erased = listed.edited(core::CommandEdit::backspace);
    expect(erased.input().text().empty() && erased.scope() == core::PaletteScope::files &&
               commands_of(erased.choices()) == all,
           "erasing the mark lists every entry");
    for (const auto query : {":", ":drac", ":fz", ":set fontsize=18", ":colorscheme missing"})
    {
        const auto commands = empty.inserted(query).value();
        expect(commands.scope() == core::PaletteScope::commands &&
                   commands_of(commands.choices()) == commands_of(choices_for(query)),
               "the colon mark gives the same commands as before");
    }
    auto staged = empty.inserted(":fz").value().filled("set fontsize=").value();
    expect(staged.input().text() == ":set fontsize=" &&
               staged.scope() == core::PaletteScope::commands,
           "fill stages the command behind the colon mark");
    while (!staged.input().text().empty())
    {
        staged = staged.edited(core::CommandEdit::backspace);
    }
    expect(commands_of(staged.inserted("#").value().choices()) == all,
           "the entries survive a fill");
}

// Ctrl+P と「∨」は同じ列を開き、Enter はタブを切り替え、`:` の後ろは今までどおり（決定 6）。
void verify_palette_entries_controller()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"one"}));
    static_cast<void>(controller.apply(app::NewTab{}));
    static_cast<void>(controller.apply(InsertText{"two"}));
    static_cast<void>(controller.apply(app::NewTab{}));
    static_cast<void>(controller.apply(InsertText{"three"}));
    static_cast<void>(controller.apply(app::SwitchTab{1}));
    const std::vector<std::string> all{"tabnext 1", "tabnext 2", "tabnext 3"};
    auto frame = controller.apply(app::OpenCommandPalette{});
    const auto opened = frame.command_palette.value_or(app::CommandPaletteView{});
    bool marked = frame.command_palette.has_value();
    for (const auto &choice : opened.choices)
    {
        marked = marked && choice.origin == std::optional{core::PaletteOrigin::tab};
    }
    expect(marked && commands_of(opened.choices) == all,
           "Ctrl+P lists the tabs in band order with the tab origin");
    expect(frame.command_line.value_or(core::InputLineView{}).text.empty() &&
               frame.command_line.value_or(core::InputLineView{}).completions.empty() &&
               opened.selected == 0,
           "Ctrl+P opens with an empty input on the first row without Ex completions");
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.active_tab == 0 && !frame.command_palette.has_value() &&
               frame.lines.front().text == "one",
           "Enter on a Ctrl+P row switches to that tab");
    frame = controller.apply(app::OpenTabList{});
    const auto listed = frame.command_palette.value_or(app::CommandPaletteView{});
    expect(frame.command_line.value_or(core::InputLineView{}).text == "#" && listed.selected == 0 &&
               commands_of(listed.choices) == all,
           "the tab list opens the same entries behind the hash mark on the active tab");
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::complete_next}));
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::complete_next}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.active_tab == 2 && !frame.command_palette.has_value() &&
               frame.lines.front().text == "three",
           "Enter on a tab list row switches like SwitchTab");
    frame = controller.apply(app::OpenTabList{});
    expect(frame.command_palette.value_or(app::CommandPaletteView{}).selected == 2,
           "the tab list selects the active tab");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    frame = controller.apply(app::CommandText{":colo"});
    expect(frame.command_line.value_or(core::InputLineView{}).completions ==
               core::CommandLine::empty().inserted(":colo").value().completions(),
           "the colon mark keeps the Ex completions of its input");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{":colorscheme dracula"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.settings.theme == core::ThemeChoice::from(core::BuiltinTheme::dracula) &&
               !frame.command_palette.has_value() && frame.active_tab == 2,
           "a command behind the colon mark changes the theme as before");
}

// 行の右端の補足と検索欄の案内の欄（ADR 0060 の決定 9）。描画と同じ core の配置を読む。
void verify_palette_notes_geometry()
{
    for (const auto dpi : {96U, 120U})
    {
        const auto inset = to_pixels(8, dpi) * 2;
        const auto layout = core::palette_layout(1600, 1200, dpi, 13);
        const auto row = core::palette_row(layout, 0);
        const core::LayoutRect inside{row.left + inset, row.top, row.right - inset, row.bottom};
        const auto note = core::palette_row_note(row, dpi);
        const auto label = core::palette_row_label(row, dpi, true);
        expect(note.right == inside.right && width_of(note) == to_pixels(112, dpi) &&
                   note.top == row.top && note.bottom == row.bottom,
               "the note sits at the right end of the row inside");
        expect(label.left == inside.left && note.left - label.right == to_pixels(12, dpi),
               "the name ends 12 DIP before the note without overlapping");
        expect(core::palette_row_label(row, dpi, false) == inside,
               "an unmarked row keeps the whole inside for the name");
        const auto hint = core::palette_input_hint(layout);
        expect(hint.right == layout.input.right &&
                   hint.left == layout.input.left + width_of(layout.input) / 2 &&
                   hint.top == layout.input.top && hint.bottom == layout.input.bottom &&
                   width_of(hint) > 0,
               "the mark hint takes the right half of the query");
        const core::LayoutRect least{0, 0, to_pixels(244, dpi) + inset * 2, row.bottom - row.top};
        expect(width_of(core::palette_row_note(least, dpi)) == to_pixels(112, dpi),
               "the note stays while the name keeps its minimum");
        const core::LayoutRect narrow{0, 0, to_pixels(240, dpi) + inset * 2, row.bottom - row.top};
        const core::LayoutRect narrow_inside{inset, 0, to_pixels(240, dpi) + inset,
                                             row.bottom - row.top};
        expect(width_of(core::palette_row_note(narrow, dpi)) == 0 &&
                   core::palette_row_label(narrow, dpi, true) == narrow_inside,
               "a narrow row drops the note and keeps the name as before");
        const auto smallest = core::palette_layout(to_pixels(360, dpi), 1200, dpi, 13);
        expect(width_of(core::palette_input_hint(smallest)) == 0,
               "the smallest window drops the mark hint");
    }
}

// 案内は入力が空のときだけ出す（ADR 0060 の決定 9）。判断は application に置く。
void verify_palette_hint_view()
{
    Editing editor;
    auto &controller = editor.controller();
    const auto hint_of = [](const app::EditorFrame &frame)
    { return frame.command_palette.value_or(app::CommandPaletteView{}).hint; };
    auto frame = controller.apply(app::OpenCommandPalette{});
    const auto opened = hint_of(frame);
    expect(opened.has_value() && opened.value().text() == core::palette_mark_hint().text(),
           "Ctrl+P opens with the mark hint");
    frame = controller.apply(app::CommandText{"a"});
    expect(frame.command_palette.has_value() && !hint_of(frame).has_value(),
           "one typed character hides the hint");
    frame = controller.apply(app::EditCommand{core::CommandEdit::backspace});
    expect(hint_of(frame).has_value(), "an emptied input shows the hint again");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    frame = controller.apply(app::OpenTabList{});
    expect(frame.command_palette.has_value() && !hint_of(frame).has_value(),
           "the tab list behind the hash mark has no hint");
}

// ---------------------------------------------------------------- 履歴（#259・ADR 0060 の決定
// 7・8）

[[nodiscard]] app::FileHistory history_of(const std::vector<std::string_view> &texts)
{
    app::FileHistory history;
    for (const std::string_view text : texts)
    {
        history.files.push_back(core::FilePath::parse(text).value());
    }
    return history;
}

[[nodiscard]] std::string history_text(const HistoryReading &reading)
{
    std::string text;
    for (const core::FilePath &path : reading.value_or(app::FileHistory{}).files)
    {
        text += std::string(path.text()) + "|";
    }
    return text;
}

[[nodiscard]] app::CommandPaletteView palette_of(const app::EditorFrame &frame)
{
    return frame.command_palette.value_or(app::CommandPaletteView{});
}

[[nodiscard]] std::string notice_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

// 開いている C:\work\a.txt（履歴の中では大文字の綴り）と、履歴の x y。
void open_history_editor(Editing &editor)
{
    editor.files().hold_at("C:\\work\\a.txt", Bytes{"a"});
    editor.files().treat_as_same("C:\\Work\\A.TXT", "C:\\work\\a.txt");
    editor.history().serve(history_of({"C:\\docs\\x.txt", "C:\\Work\\A.TXT", "C:\\docs\\y.txt"}));
    applied(editor.controller(),
            app::OpenDocument{core::FilePath::parse("C:\\work\\a.txt").value()});
}

// Ctrl+P はタブの後ろに履歴を新しい順に出し、開いているファイルは重ねない。`@` は履歴だけ、`#` は
// タブだけ。履歴を読むのは開くときの 1 回（決定 6・8）。
void verify_palette_history_rows()
{
    Editing editor;
    open_history_editor(editor);
    auto &controller = editor.controller();
    auto frame = controller.apply(app::OpenCommandPalette{});
    const auto opened = palette_of(frame);
    expect(commands_of(opened.choices) ==
               std::vector<std::string>{"tabnext 1", "C:\\docs\\x.txt", "C:\\docs\\y.txt"},
           "Ctrl+P lists the tabs and then the history, newest first, without the open file");
    const auto &row = opened.choices.at(1);
    expect(row.label.text() == "x.txt" && row.kind == core::CommandChoiceKind::open &&
               row.detail.has_value() && row.detail.value().text() == "C:\\docs" &&
               row.origin == std::optional{core::PaletteOrigin::history},
           "a history row has the file name, its folder, the open kind and the history origin");
    expect(editor.history().reads() == 1 && editor.history().writes() == 0,
           "opening the palette reads the history once and writes nothing");
    frame = controller.apply(app::CommandText{"@"});
    expect(commands_of(palette_of(frame).choices) ==
               std::vector<std::string>{"C:\\docs\\x.txt", "C:\\docs\\y.txt"},
           "the at mark lists only the history");
    frame = controller.apply(app::CommandText{"y"});
    expect(commands_of(palette_of(frame).choices) == std::vector<std::string>{"C:\\docs\\y.txt"},
           "a query behind the at mark filters the history");
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
    frame = controller.apply(app::CommandText{"#"});
    expect(commands_of(palette_of(frame).choices) == std::vector<std::string>{"tabnext 1"},
           "the hash mark lists only the tabs");
    expect(editor.history().reads() == 1, "typing in the palette never reads the history again");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    editor.history().serve(HistoryReading{std::unexpected(app::FileHistoryFailure::malformed)});
    frame = controller.apply(app::OpenCommandPalette{});
    expect(commands_of(palette_of(frame).choices) == std::vector<std::string>{"tabnext 1"} &&
               notice_of(frame) == "none",
           "an unreadable history opens the palette with the tabs only and no notice");
}

// 履歴の行を選ぶと開く道を通って開き、失敗は 1 行の知らせ。無いファイルだけ履歴から外す（決定 7）。
void verify_palette_history_open()
{
    Editing editor;
    auto &controller = editor.controller();
    editor.files().hold_at("C:\\docs\\x.txt", Bytes{"x"});
    editor.files().hold_at("C:\\docs\\big.txt",
                           Bytes{std::unexpected(app::FileFailure::too_large)});
    static_cast<void>(controller.apply(InsertText{"one"}));
    editor.history().serve(history_of({"C:\\docs\\x.txt"}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"@"}));
    auto frame = controller.apply(app::SubmitCommand{});
    expect(frame.tabs.size() == 2 && frame.active_tab == 1 && !frame.command_palette.has_value() &&
               frame.lines.front().text == "x" && editor.history().writes() == 0,
           "a history row opens in a new tab, closes the palette and records nothing");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.tabs.size() == 2 && frame.active_tab == 0, "a tab row still switches tabs");
    editor.history().serve(history_of({"C:\\docs\\gone.txt", "C:\\docs\\big.txt"}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"@gone"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.tabs.size() == 2 && frame.active_tab == 0 && !frame.command_palette.has_value() &&
               notice_of(frame) == "開けませんでした: gone.txt" &&
               !frame.document.last_failure.has_value(),
           "a missing file keeps the tabs and shows one notice line without the dialog");
    expect(editor.history().writes() == 1 &&
               history_text(editor.history().read()) == "C:\\docs\\big.txt|",
           "a missing file leaves the history in one write");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{"@big"}));
    frame = controller.apply(app::SubmitCommand{});
    expect(frame.tabs.size() == 2 && notice_of(frame) == "開けませんでした: big.txt" &&
               !frame.document.last_failure.has_value() && editor.history().writes() == 1 &&
               history_text(editor.history().read()) == "C:\\docs\\big.txt|",
           "a file too large to open is only noticed and stays in the history");
}

void verify_tab_folders()
{
    const auto folder = core::tab_folder_for(core::FilePath::parse("C:\\work\\note.txt").value());
    expect(folder.has_value() && folder.value().text() == "C:\\work",
           "the folder is the path without its last name");
    const auto root = core::tab_folder_for(core::FilePath::parse("C:\\note.txt").value());
    expect(root.has_value() && root.value().text() == "C:", "a root file keeps its drive");
    expect(!core::tab_folder_for(std::nullopt).has_value() &&
               !core::tab_folder_for(core::FilePath::parse("note.txt").value()).has_value(),
           "an untitled tab and a bare name have no folder");
}
} // namespace

void verify_command_palette()
{
    verify_palette_choices();
    verify_palette_editing();
    verify_palette_geometry();
    verify_palette_controller();
    verify_palette_unknown_theme();
    verify_palette_input_isolation();
    verify_palette_vim_modes();
    verify_palette_marks();
    verify_listed_choices();
    verify_palette_sources();
    verify_palette_entries_controller();
    verify_palette_notes_geometry();
    verify_palette_hint_view();
    verify_palette_history_rows();
    verify_palette_history_open();
    verify_tab_folders();
}
} // namespace nenenib::tests
