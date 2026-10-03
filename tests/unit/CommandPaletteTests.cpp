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
#include "CompositionView.hpp"
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
#include "FileFolder.hpp"
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
#include "UnlistedExtensions.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"

#include <algorithm>
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
    // 設定候補の点数は長さ、同点は文字列の順。ls、wq、xit の順になる（ADR 0066）。
    expect(all.front().command == "ls" && all.at(1).command == "wq" && all.at(2).command == "xit",
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
    const auto count = palette.count();
    palette = palette.edited(CommandEdit::complete_previous);
    expect(palette.selected() == count - 1, "previous wraps backwards");
    palette = palette.edited(CommandEdit::complete_next);
    expect(palette.selected() == 0 && palette.input().text() == ":",
           "next wraps without changing query");
    palette = palette.selected_at(4).inserted("drac").value();
    expect(palette.selected() == 0 && palette.count() == 1, "filter resets the selected row");
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
    expect(palette.selected() == 0 && palette.count() == 0, "no results can be navigated safely");
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
    expect(frame.command_line.value_or(core::InputLineView{}).text == "界" &&
               !frame.command_composition.has_value(),
           "the commit goes into the palette query instead (ADR 0061)");
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
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

// frame の面の窓（ADR 0062 の決定 2）が結果の全体と同じときの実行の列。窓が全体でなければ印
// "<window>" を足し、全件を言う期待に一致させない（候補が上限以下の試験の確かめ）。
[[nodiscard]] std::vector<std::string> whole_commands_of(const app::CommandPaletteView &view)
{
    auto commands = commands_of(view.rows);
    if (view.first != 0 || view.rows.size() != view.total)
    {
        commands.emplace_back("<window>");
    }
    return commands;
}

// 絞り込みの結果の位置の列を候補に写す（全件を読むのは試験だけ・ADR 0062 の決定 1）。
[[nodiscard]] std::vector<core::CommandChoice>
listed_choices_of(const std::vector<core::CommandChoice> &entries, core::PaletteScope scope,
                  std::string_view query)
{
    std::vector<core::CommandChoice> listed;
    for (const std::size_t position : core::listed_positions(entries, scope, query))
    {
        listed.push_back(entries.at(position));
    }
    return listed;
}

// 面の結果の全件（試験だけが読む。controller と frame は件数・1 件・行の範囲を読む）。
[[nodiscard]] std::vector<core::CommandChoice> rows_of(const core::CommandPalette &palette)
{
    return palette.rows(0, palette.count());
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
    expect(query_is("/a", core::PaletteScope::folder, "a") &&
               query_is("/", core::PaletteScope::folder, ""),
           "the slash mark selects the same folder");
    expect(query_is("*a", core::PaletteScope::bookmarks, "a") &&
               query_is("?a", core::PaletteScope::files, "?a"),
           "the star selects bookmarks while unknown marks are plain search text");
    expect(query_is("a#", core::PaletteScope::files, "a#"), "only the first character is a mark");
    expect(core::palette_mark_hint().text() ==
               "# タブ\u3000* ブックマーク\u3000@ 履歴\u3000/ フォルダ\u3000: 設定",
           "the hint is built from the table");
    expect(core::palette_origin_label(core::PaletteOrigin::tab) == "開いているタブ",
           "the tab origin has its label");
    expect(core::palette_origin_label(core::PaletteOrigin::history) == "履歴",
           "the history origin has its label");
    expect(core::palette_origin_label(core::PaletteOrigin::folder) == "同じフォルダ",
           "the folder origin has its label");
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
    expect(commands_of(listed_choices_of(entries, core::PaletteScope::files, "")) == all,
           "files keeps every entry with an origin in list order");
    expect(commands_of(listed_choices_of(entries, core::PaletteScope::tabs, "")) == all,
           "tabs keeps the tab entries in list order");
    expect(listed_choices_of(entries, core::PaletteScope::commands, "").empty(),
           "commands are not listed from the entries");
    expect(commands_of(listed_choices_of(entries, core::PaletteScope::tabs, "NOTE")) ==
               std::vector<std::string>{"tabnext 3", "tabnext 1", "tabnext 4"},
           "name hits come first in score order and a folder hit follows, ignoring case");
    const std::vector<core::CommandChoice> folder_first{
        tab_choice("a.md", 1, "C:\\note"),
        tab_choice("x-note-with-a-rather-long-name.txt", 2, "C:\\work")};
    expect(commands_of(listed_choices_of(folder_first, core::PaletteScope::files, "note")) ==
               std::vector<std::string>{"tabnext 2", "tabnext 1"},
           "a name hit beats a shorter folder-only hit");
    const std::vector<core::CommandChoice> twins{tab_choice("x.txt", 1), tab_choice("a.txt", 2),
                                                 tab_choice("a.txt", 3)};
    expect(commands_of(listed_choices_of(twins, core::PaletteScope::tabs, "a")) ==
               std::vector<std::string>{"tabnext 2", "tabnext 3"},
           "equal scores keep list order");
    expect(listed_choices_of(entries, core::PaletteScope::files, "zq").empty(),
           "an entry that matches neither name nor folder is dropped");
    expect(commands_of(listed_choices_of(entries, core::PaletteScope::files, "work")) ==
               std::vector<std::string>{"tabnext 1"},
           "the folder and the name are searched together");
}

// 照合はコードポイントの境目で行う（ADR 0061 の決定 6）。日本語の query は日本語の名前に当たり、
// 別の文字の継続バイトにまたがる並びには当たらない。ASCII の点は今までと同じ。
void verify_listed_code_points()
{
    const std::vector<core::CommandChoice> names{
        tab_choice("日本語メモ", 1), tab_choice("メモ.txt", 2), tab_choice("めも.md", 3),
        tab_choice("Memo帳.txt", 4), tab_choice("アあ", 5),     tab_choice("memo.txt", 6)};
    expect(commands_of(listed_choices_of(names, core::PaletteScope::files, "メモ")) ==
               std::vector<std::string>{"tabnext 2", "tabnext 1"},
           "a katakana query hits the katakana names, the shorter skip first");
    expect(commands_of(listed_choices_of(names, core::PaletteScope::files, "めも")) ==
               std::vector<std::string>{"tabnext 3"},
           "a hiragana query hits only the hiragana name");
    // め は E3 82 81。「アあ」は E3 82 A2 E3 81 82 で、バイトの部分列としては E3 82 81 を含む。
    expect(listed_choices_of(names, core::PaletteScope::files, "め").size() == 1 &&
               listed_choices_of(names, core::PaletteScope::files, "め").front().command ==
                   "tabnext 3",
           "a character never matches across the bytes of two other characters");
    expect(listed_choices_of(names, core::PaletteScope::files, "ア め").empty(),
           "the spaced query still needs every character on a boundary");
    expect(commands_of(listed_choices_of(names, core::PaletteScope::files, "MEMO帳")) ==
               std::vector<std::string>{"tabnext 4"},
           "ASCII and Japanese mix in one query, ASCII ignoring case");
    expect(commands_of(listed_choices_of(names, core::PaletteScope::files, "帳txt")) ==
               std::vector<std::string>{"tabnext 4"},
           "a Japanese character is followed by ASCII in the same name");
    expect(commands_of(listed_choices_of(names, core::PaletteScope::files, "memo")) ==
               std::vector<std::string>{"tabnext 6", "tabnext 4"},
           "ASCII scores still count bytes: the shorter name comes first");
    expect(choices_for("メモ").empty() &&
               choices_for("drac").front().command == "colorscheme dracula",
           "the Ex commands take the same matcher");
}

// CommandPalette は開いたときの列を持ち、入力の先頭の記号で出どころを切り替える（決定 1・5）。
void verify_palette_sources()
{
    const std::vector<core::CommandChoice> tabs{tab_choice("● note.txt", 1), tab_choice("無題", 2),
                                                tab_choice("Notes.md", 3),
                                                tab_choice("another note", 4)};
    const std::vector<std::string> all{"tabnext 1", "tabnext 2", "tabnext 3", "tabnext 4"};
    const auto empty = core::CommandPalette::opened(tabs, "", 0);
    expect(empty.scope() == core::PaletteScope::files && commands_of(rows_of(empty)) == all &&
               empty.selected() == 0,
           "an empty input lists every entry with an origin");
    const auto listed = core::CommandPalette::opened(tabs, "#", 2);
    expect(listed.scope() == core::PaletteScope::tabs && listed.input().text() == "#" &&
               listed.selected() == 2 && commands_of(rows_of(listed)) == all,
           "the hash input opens on the given row");
    expect(core::CommandPalette::opened(tabs, "#", 9).selected() == 0,
           "a row outside the choices selects the first");
    const auto filtered = listed.inserted("NOTE").value();
    expect(commands_of(rows_of(filtered)) ==
                   std::vector<std::string>{"tabnext 3", "tabnext 4", "tabnext 1"} &&
               filtered.selected() == 0,
           "a query keeps the matching titles in score order, ignoring case");
    expect(listed.edited(core::CommandEdit::complete_next).selected() == 3 &&
               listed.edited(core::CommandEdit::complete_previous).selected() == 1,
           "up and down move the selection over the tab rows");
    const auto erased = listed.edited(core::CommandEdit::backspace);
    expect(erased.input().text().empty() && erased.scope() == core::PaletteScope::files &&
               commands_of(rows_of(erased)) == all,
           "erasing the mark lists every entry");
    for (const auto query : {":", ":drac", ":fz", ":set fontsize=18", ":colorscheme missing"})
    {
        const auto commands = empty.inserted(query).value();
        expect(commands.scope() == core::PaletteScope::commands &&
                   commands_of(rows_of(commands)) == commands_of(choices_for(query)),
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
    expect(commands_of(rows_of(staged.inserted("#").value())) == all, "the entries survive a fill");
}

// 絞り込みの結果は入力が変わったときだけ作り、選択を動かす道は同じ結果を共有する（ADR 0062 の
// 決定 1）。読む口は件数・1 件・行の範囲で、範囲の外は無しか切り詰め。
void verify_palette_result_shared()
{
    using core::CommandEdit;
    const std::vector<core::CommandChoice> tabs{tab_choice("● note.txt", 1), tab_choice("無題", 2),
                                                tab_choice("Notes.md", 3),
                                                tab_choice("another note", 4)};
    const auto palette = core::CommandPalette::opened(tabs, "", 0);
    expect(palette.edited(CommandEdit::complete_next).shares_result_with(palette) &&
               palette.edited(CommandEdit::complete_previous).shares_result_with(palette),
           "up and down share the result");
    expect(palette.selected_at(3).shares_result_with(palette) &&
               palette.selected_at(9).shares_result_with(palette),
           "selecting a row shares the result");
    const auto commands = core::CommandPalette::opened({}, ":", 0);
    expect(commands.edited(CommandEdit::complete_next).shares_result_with(commands) &&
               commands.selected_at(2).shares_result_with(commands),
           "the command rows are shared the same way");
    expect(!palette.inserted("n").value().shares_result_with(palette) &&
               !commands.filled("set fontsize=").value().shares_result_with(commands),
           "typing and filling rebuild the result");
    const auto typed = palette.inserted("n").value();
    expect(!typed.edited(CommandEdit::backspace).shares_result_with(typed) &&
               !typed.edited(CommandEdit::left).shares_result_with(typed) &&
               !typed.edited(CommandEdit::home).shares_result_with(typed),
           "editing the input rebuilds the result");
    const auto last = palette.choice_at(3);
    expect(palette.count() == 4 && last.has_value() && last.value().command == "tabnext 4" &&
               !palette.choice_at(4).has_value(),
           "one row is read by its index and none outside");
    expect(commands_of(palette.rows(1, 2)) == std::vector<std::string>{"tabnext 2", "tabnext 3"} &&
               commands_of(palette.rows(2, 100)) ==
                   std::vector<std::string>{"tabnext 3", "tabnext 4"} &&
               palette.rows(4, 1).empty() && palette.rows(0, 0).empty(),
           "a row range is cut at the end of the result");
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
    for (const auto &choice : opened.rows)
    {
        marked = marked && choice.origin == std::optional{core::PaletteOrigin::tab};
    }
    expect(marked && whole_commands_of(opened) == all,
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
               whole_commands_of(listed) == all,
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
    expect(whole_commands_of(opened) ==
               std::vector<std::string>{"C:\\work\\a.txt", "C:\\docs\\x.txt", "C:\\docs\\y.txt"},
           "Ctrl+P lists the tabs and then the history, newest first, without the open file");
    const auto &row = opened.rows.at(1);
    expect(row.label.text() == "x.txt" && row.kind == core::CommandChoiceKind::open &&
               row.detail.has_value() && row.detail.value().text() == "C:\\docs" &&
               row.origin == std::optional{core::PaletteOrigin::history},
           "a history row has the file name, its folder, the open kind and the history origin");
    expect(editor.history().reads() == 1 && editor.history().writes() == 0,
           "opening the palette reads the history once and writes nothing");
    frame = controller.apply(app::CommandText{"@"});
    expect(whole_commands_of(palette_of(frame)) ==
               std::vector<std::string>{"C:\\docs\\x.txt", "C:\\docs\\y.txt"},
           "the at mark lists only the history");
    frame = controller.apply(app::CommandText{"y"});
    expect(whole_commands_of(palette_of(frame)) == std::vector<std::string>{"C:\\docs\\y.txt"},
           "a query behind the at mark filters the history");
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::backspace}));
    frame = controller.apply(app::CommandText{"#"});
    expect(whole_commands_of(palette_of(frame)) == std::vector<std::string>{"C:\\work\\a.txt"},
           "the hash mark lists only the tabs");
    expect(editor.history().reads() == 1, "typing in the palette never reads the history again");
    static_cast<void>(controller.apply(app::CancelCommand{}));
    editor.history().serve(HistoryReading{std::unexpected(app::FileHistoryFailure::malformed)});
    frame = controller.apply(app::OpenCommandPalette{});
    expect(whole_commands_of(palette_of(frame)) == std::vector<std::string>{"C:\\work\\a.txt"} &&
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

// 面を開いた editor（本文は "body"・履歴は メモ.txt と notes.md・vim なら Vim の NORMAL から）。
void open_palette_over_body(Editing &editor, bool vim)
{
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(InsertText{"body"}));
    editor.history().serve(history_of({"C:\\docs\\メモ.txt", "C:\\docs\\notes.md"}));
    static_cast<void>(controller.apply(SelectEditMode{vim ? EditMode::vim : EditMode::ordinary}));
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
}

// 面が開いている間の変換は面の入力欄のもの。確定は打った文字と同じ道で入り、本文にも engine
// にも行かない（ADR 0061 の決定 3・4）。
void verify_palette_commit(bool vim)
{
    Editing editor;
    open_palette_over_body(editor, vim);
    auto &controller = editor.controller();
    const auto mode = controller.frame().vim_mode;
    static_cast<void>(controller.apply(app::EditCommand{core::CommandEdit::complete_next}));
    auto frame = controller.apply(ComposeText{composed_of("めも", {}, 6)});
    expect(frame.command_composition.has_value() && !frame.composition.has_value() &&
               app::composing(frame),
           "a composition in the palette is shown on the input line only");
    expect(frame.command_composition.value_or(app::CompositionView{}).utf8 == "めも" &&
               frame.command_line.value_or(core::InputLineView{}).text.empty() &&
               palette_of(frame).total == 3 && palette_of(frame).rows.size() == 3 &&
               frame.lines.front().text == "body",
           "the composition neither filters the list nor touches the body");
    frame = controller.apply(CommitText{"メモ"});
    expect(!app::composing(frame) &&
               frame.command_line.value_or(core::InputLineView{}).text == "メモ",
           "the commit goes into the palette query");
    expect(whole_commands_of(palette_of(frame)) == std::vector<std::string>{"C:\\docs\\メモ.txt"} &&
               palette_of(frame).selected == 0,
           "the commit filters the list and the selection returns to the top");
    expect(frame.lines.front().text == "body" && frame.command_palette.has_value() &&
               frame.vim_mode == mode,
           "the body and the Vim engine never see the commit and the palette stays open");
}

// 面を閉じる道（取消・確定）は残っている変換を消す（ADR 0061 の決定 3）。
void verify_palette_closing_composition(bool vim)
{
    Editing editor;
    open_palette_over_body(editor, vim);
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 3)}));
    auto frame = controller.apply(app::CancelCommand{});
    expect(!frame.command_palette.has_value() && !app::composing(frame),
           "closing the palette drops its composition");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 3)}));
    frame = controller.apply(app::SubmitCommand{});
    expect(!frame.command_palette.has_value() && !app::composing(frame),
           "submitting the palette drops its composition");
}

// 確定は undo の単位を作らない。上限を越える確定は入らず知らせが出る（ADR 0061 の決定 3）。
void verify_palette_commit_limits()
{
    Editing editor;
    open_palette_over_body(editor, false);
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(CommitText{"メモ"}));
    static_cast<void>(controller.apply(app::CancelCommand{}));
    auto frame = controller.apply(HistoryAction{HistoryDirection::undo});
    expect(frame.lines.front().text.empty(), "undo still only sees the typed body");
    static_cast<void>(controller.apply(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply(app::CommandText{std::string(250, 'a')}));
    frame = controller.apply(CommitText{"日本語"});
    expect(frame.command_line.value_or(core::InputLineView{}).text == std::string(250, 'a') &&
               frame.command_message.has_value() && !app::composing(frame),
           "a commit past the input limit is refused with a notice");
}

// Ex の行と検索の行は今までどおり変換も確定も捨てる（ADR 0061 の決定 3）。
void verify_input_lines_drop_composition()
{
    Editing editor;
    auto &controller = editor.controller();
    static_cast<void>(controller.apply(SelectEditMode{EditMode::vim}));
    for (const auto entry : {U':', U'/'})
    {
        static_cast<void>(controller.apply(VimKeyPress{core::VimCharacter{entry}}));
        auto frame = controller.apply(ComposeText{composed_of("あ", {}, 3)});
        expect(frame.command_line.has_value() && !app::composing(frame),
               "the Ex and search lines still drop the composition");
        frame = controller.apply(CommitText{"界"});
        expect(frame.command_line.value_or(core::InputLineView{}).text.empty(),
               "and drop the commit");
        static_cast<void>(controller.apply(app::CancelCommand{}));
    }
}

// 記号の案内は変換を始めたら消え、取り消して空に戻ればまた出る。確定して文字が入れば出ない
// （ADR 0060 の決定 9・#264 の差し戻し 1）。
void verify_palette_hint_composing()
{
    Editing editor;
    open_palette_over_body(editor, false);
    auto &controller = editor.controller();
    auto frame = controller.frame();
    expect(palette_of(frame).hint.has_value(), "an opened palette shows the mark hint");
    frame = controller.apply(ComposeText{composed_of("あ", {}, 3)});
    expect(frame.command_composition.has_value() && !palette_of(frame).hint.has_value(),
           "a composition in the palette hides the mark hint");
    frame = controller.apply(CancelComposition{});
    expect(!app::composing(frame) && palette_of(frame).hint.has_value(),
           "a cancelled composition shows the hint again");
    static_cast<void>(controller.apply(ComposeText{composed_of("あ", {}, 3)}));
    frame = controller.apply(CommitText{"メモ"});
    expect(frame.command_line.value_or(core::InputLineView{}).text == "メモ" &&
               !palette_of(frame).hint.has_value(),
           "a committed query keeps the hint hidden");
}

void verify_palette_composition()
{
    for (const auto vim : {false, true})
    {
        verify_palette_commit(vim);
        verify_palette_closing_composition(vim);
    }
    verify_palette_hint_composing();
    verify_palette_commit_limits();
    verify_input_lines_drop_composition();
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

[[nodiscard]] core::CommandChoice origin_choice(std::string_view title, std::size_t number,
                                                core::PaletteOrigin origin)
{
    return core::CommandChoice{fixed_text(title), "tabnext " + std::to_string(number),
                               core::CommandChoiceKind::execute, std::nullopt, origin};
}

[[nodiscard]] core::CommandChoice folder_choice(std::string_view title, std::size_t number)
{
    return origin_choice(title, number, core::PaletteOrigin::folder);
}

// 記号 `/` は同じフォルダの候補だけ、記号なしは印のある候補の全部（ADR 0062 の決定 11）。
void verify_palette_folder_scope()
{
    using core::PaletteOrigin;
    using core::PaletteScope;
    const std::vector<core::CommandChoice> entries{
        origin_choice("a.txt", 1, PaletteOrigin::tab),
        origin_choice("b.txt", 2, PaletteOrigin::history), folder_choice("c.txt", 3),
        folder_choice("d.txt", 4), origin_choice("e.txt", 5, PaletteOrigin::history)};
    expect(commands_of(listed_choices_of(entries, PaletteScope::files, "")) ==
               std::vector<std::string>{"tabnext 1", "tabnext 2", "tabnext 3", "tabnext 4",
                                        "tabnext 5"},
           "no mark lists the tabs, the history and the same folder");
    expect(commands_of(listed_choices_of(entries, PaletteScope::tabs, "")) ==
                   std::vector<std::string>{"tabnext 1"} &&
               commands_of(listed_choices_of(entries, PaletteScope::history, "")) ==
                   std::vector<std::string>{"tabnext 2", "tabnext 5"} &&
               commands_of(listed_choices_of(entries, PaletteScope::folder, "")) ==
                   std::vector<std::string>{"tabnext 3", "tabnext 4"},
           "each mark lists only its own source");
    expect(listed_choices_of(entries, PaletteScope::commands, "").empty(),
           "the commands do not come from the entries");
    const auto palette = core::CommandPalette::opened(entries, "/", 0);
    expect(palette.scope() == PaletteScope::folder &&
               commands_of(rows_of(palette)) == std::vector<std::string>{"tabnext 3", "tabnext 4"},
           "the slash alone lists the same folder in list order");
    expect(commands_of(rows_of(palette.inserted("d").value())) ==
               std::vector<std::string>{"tabnext 4"},
           "the slash query searches only the same folder");
}

[[nodiscard]] std::string upper_ascii(std::string_view text)
{
    std::string upper(text);
    for (char &letter : upper)
    {
        if (letter >= 'a' && letter <= 'z')
        {
            letter = static_cast<char>(letter - ('a' - 'A'));
        }
    }
    return upper;
}

[[nodiscard]] bool lower_ascii_word(std::string_view text)
{
    return !text.empty() &&
           std::ranges::all_of(
               text, [](char letter)
               { return (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9'); });
}

// 表の全部の拡張子は、小文字・大文字・混ぜた形のどれでも出さない。表は小文字の ASCII で重複が無い
// （ADR 0062 の決定 12）。
void verify_unlisted_table()
{
    bool hidden = true;
    bool lowered = true;
    bool unique = true;
    for (std::size_t index = 0; index < core::unlisted_extensions.size(); ++index)
    {
        const std::string_view extension = core::unlisted_extensions.at(index);
        const std::string lower(extension);
        const std::string mixed = upper_ascii(lower.substr(0, 1)) + lower.substr(1);
        hidden = hidden && !core::folder_lists("note." + lower) &&
                 !core::folder_lists("NOTE." + upper_ascii(lower)) &&
                 !core::folder_lists("Note." + mixed);
        lowered = lowered && lower_ascii_word(extension);
        for (std::size_t other = index + 1; other < core::unlisted_extensions.size(); ++other)
        {
            unique = unique && core::unlisted_extensions.at(other) != extension;
        }
    }
    expect(hidden, "every extension in the table is hidden in any case");
    expect(lowered, "the table is written in lower-case ASCII");
    expect(unique, "no extension is written twice");
    expect(core::unlisted_extensions.size() == 70, "the table has the 70 extensions of ADR 0062");
}

// 表に無い拡張子・拡張子の無い名前・`.` で終わる名前は出す。引くのは最後の `.` の後ろ（D33）。
// `.` が先頭の 1 つだけの名前は拡張子なしとして出す（`.png` `.a` も）。後ろにもう 1 つあれば引く。
void verify_unlisted_names()
{
    for (const std::string_view name :
         {"note.txt", "note.md", "Makefile", "note.", ".gitignore", "archive.gz.txt", "note.pngx",
          "png", "メモ.txt", "settings.json", "a.b.c.log", ".png", ".a", ".PNG"})
    {
        expect(core::folder_lists(name), "a name outside the table is listed");
    }
    for (const std::string_view name :
         {"a.tar.gz", "メモ.png", "写真.JPG", "setup.Exe", ".config.png", "..png"})
    {
        expect(!core::folder_lists(name), "the last extension decides a hidden name");
    }
}

[[nodiscard]] std::optional<std::string> folder_text(std::string_view path)
{
    const auto folder = core::folder_of(core::FilePath::parse(path).value());
    if (!folder.has_value())
    {
        return std::nullopt;
    }
    return std::string(folder.value().text());
}

// 列挙に渡すフォルダ（ADR 0062 の決定 13）。最後の区切りの前まで、ルート直下は区切りを残す。
void verify_folder_of()
{
    expect(folder_text("C:\\work\\note.txt") == "C:\\work", "the folder drops the last name");
    expect(folder_text("C:\\note.txt") == "C:\\" && folder_text("C:/note.txt") == "C:/",
           "a root file keeps the separator after the drive");
    expect(folder_text("C:/work/note.txt") == "C:/work", "a slash is a separator too");
    expect(folder_text("\\\\server\\share\\note.txt") == "\\\\server\\share",
           "a UNC file keeps the server and the share");
    expect(folder_text("\\note.txt") == "\\", "a rooted name keeps its separator");
    expect(folder_text("D:\\メモ\\a.txt") == "D:\\メモ",
           "a Japanese folder is cut on the separator");
    expect(folder_text("C:\\work\\") == "C:\\work" && folder_text("C:\\") == "C:\\",
           "a path ending in a separator gives the part before it, the root stays");
    expect(!folder_text("note.txt").has_value() && !folder_text("C:note.txt").has_value(),
           "a path without a separator has no folder");
}

// 入力が空のとき、足した候補は後ろに付き、選択の番号は同じ（ADR 0062 の決定 16）。伸ばすと結果を
// 作り直し、足す分が空なら同じ面。伸ばした後の入力は足した候補も絞り込む。
void verify_palette_extended_tail()
{
    const std::vector<core::CommandChoice> tabs{tab_choice("a.txt", 1), tab_choice("b.txt", 2),
                                                tab_choice("c.txt", 3)};
    const auto palette = core::CommandPalette::opened(tabs, "", 2);
    const auto grown = palette.extended({folder_choice("d.txt", 4), folder_choice("e.txt", 5)});
    expect(commands_of(rows_of(grown)) == std::vector<std::string>{"tabnext 1", "tabnext 2",
                                                                   "tabnext 3", "tabnext 4",
                                                                   "tabnext 5"} &&
               grown.selected() == 2 && grown.input().text().empty(),
           "with no input the added choices follow and the selection stays");
    expect(commands_of(rows_of(palette)) ==
               std::vector<std::string>{"tabnext 1", "tabnext 2", "tabnext 3"},
           "the palette before growing keeps its choices");
    expect(!grown.shares_result_with(palette) && palette.extended({}).shares_result_with(palette),
           "growing rebuilds the result and adding nothing keeps it");
    const auto twice = grown.extended({folder_choice("f.txt", 6)});
    const auto last = twice.choice_at(5);
    expect(twice.count() == 6 && twice.selected() == 2 && last.has_value() &&
               last.value().command == "tabnext 6",
           "growing twice appends again and keeps the selection");
    expect(commands_of(rows_of(grown.inserted("e").value())) ==
                   std::vector<std::string>{"tabnext 5"} &&
               commands_of(rows_of(grown.inserted("/").value())) ==
                   std::vector<std::string>{"tabnext 4", "tabnext 5"},
           "the input filters the added choices after growing");
}

[[nodiscard]] bool selects_command(const core::CommandPalette &palette, std::string_view command)
{
    const auto choice = palette.choice_at(palette.selected());
    return choice.has_value() && choice.value().command == command;
}

// 足した候補が点の順で前に入ると選択の番号は増え、指す候補は同じ（ADR 0062 の決定 16）。
void verify_palette_extended_keeps_choice()
{
    const std::vector<core::CommandChoice> tabs{tab_choice("a-note.txt", 1),
                                                tab_choice("bb-note.txt", 2)};
    const auto palette = core::CommandPalette::opened(tabs, "note", 1);
    expect(palette.selected() == 1 && selects_command(palette, "tabnext 2"),
           "the second hit is selected before growing");
    const auto grown =
        palette.extended({folder_choice("note.txt", 3), folder_choice("zzzz-note.txt", 4),
                          folder_choice("memo.txt", 5)});
    expect(commands_of(rows_of(grown)) ==
                   std::vector<std::string>{"tabnext 3", "tabnext 1", "tabnext 2", "tabnext 4"} &&
               grown.selected() == 2 && selects_command(grown, "tabnext 2"),
           "a better added hit moves the selected index but not the selected choice");
    const auto twice = grown.extended({folder_choice("note.md", 6)});
    expect(commands_of(rows_of(twice)) == std::vector<std::string>{"tabnext 6", "tabnext 3",
                                                                   "tabnext 1", "tabnext 2",
                                                                   "tabnext 4"} &&
               twice.selected() == 3 && selects_command(twice, "tabnext 2"),
           "growing twice still keeps the selected choice");
    expect(twice.edited(core::CommandEdit::complete_next).selected() == 4,
           "moving walks the grown result");
    expect(grown.inserted("t").value().selected() == 0,
           "changing the input still selects the first row");
}

// 伸ばす前に当たりが無ければ先頭。設定のコマンドは件数も選択も変わらない（ADR 0062 の決定 16）。
void verify_palette_extended_edges()
{
    const auto none = core::CommandPalette::opened({tab_choice("a.txt", 1)}, "qz", 0);
    const auto miss = none.extended({folder_choice("b.txt", 2)});
    expect(none.count() == 0 && miss.count() == 0 && miss.selected() == 0,
           "added choices that miss the query leave the result empty");
    const auto hit = miss.extended(
        {folder_choice("x.txt", 3), folder_choice("qz.txt", 4), folder_choice("qqz.txt", 5)});
    expect(commands_of(rows_of(hit)) == std::vector<std::string>{"tabnext 4", "tabnext 5"} &&
               hit.selected() == 0,
           "the first hits after none select the first row");
    const auto folder = core::CommandPalette::opened({tab_choice("a.txt", 1)}, "/", 0)
                            .extended({folder_choice("b.txt", 2), folder_choice("c.txt", 3)});
    expect(commands_of(rows_of(folder)) == std::vector<std::string>{"tabnext 2", "tabnext 3"} &&
               folder.selected() == 0,
           "the slash palette fills from its first row");
    const auto commands = core::CommandPalette::opened({}, ":", 0).selected_at(2);
    const auto grown = commands.extended({folder_choice("a.txt", 1)});
    expect(grown.count() == commands.count() && grown.selected() == 2 &&
               commands_of(rows_of(grown)) == commands_of(rows_of(commands)),
           "the command rows and their selection do not change");
    expect(commands_of(rows_of(grown.edited(core::CommandEdit::backspace))) ==
               std::vector<std::string>{"tabnext 1"},
           "the added choice is listed once the colon is erased");
}

// ui が描く行は frame の窓に入る（ADR 0062 の決定 2・3）。見えている行数 1〜上限・件数 0〜20・
// 選択のすべての組で、ui が読む [見えている先頭, +min(行数, 件数 - 先頭)) が窓
// [窓の先頭, +min(上限, 件数 - 窓の先頭)) に入り、選択はその中にある。行数が上限なら先頭は同じ式。
[[nodiscard]] bool window_covers_rows(std::size_t visible, std::size_t total)
{
    const core::PaletteLayout layout{{}, {}, {}, {}, 1, visible};
    bool covered = true;
    for (std::size_t selected = 0; covered && selected < total; ++selected)
    {
        const auto start = core::palette_first_visible(layout, selected);
        const auto end = start + std::min(visible, total - start);
        const auto first = core::palette_window_first(selected);
        const auto last = first + std::min(core::palette_row_limit, total - first);
        covered = first <= start && end <= last && start <= selected && selected < end;
    }
    return covered;
}

void verify_palette_window_covers_rows()
{
    bool covered = true;
    for (std::size_t visible = 1; visible <= core::palette_row_limit; ++visible)
    {
        for (std::size_t total = 0; total <= 20; ++total)
        {
            covered = covered && window_covers_rows(visible, total);
        }
    }
    expect(covered, "every drawn row lies inside the window carried by the frame");
    const core::PaletteLayout full{{}, {}, {}, {}, 1, core::palette_row_limit};
    bool shared = true;
    for (std::size_t selected = 0; selected <= 20; ++selected)
    {
        shared = shared && core::palette_first_visible(full, selected) ==
                               core::palette_window_first(selected);
    }
    expect(shared && core::palette_row_limit == 8 && core::palette_window_first(7) == 0 &&
               core::palette_window_first(8) == 1 && core::palette_window_first(12) == 5,
           "the window starts where eight visible rows start");
}

// frame に載るのは窓だけ。候補が上限を越えても rows は上限まで、rows の i 番目は結果の first + i
// 番目、選択と件数は結果の全体の数（ADR 0062 の決定 2）。
[[nodiscard]] bool window_follows_tabs(const app::CommandPaletteView &view, std::size_t selected)
{
    const auto first = core::palette_window_first(selected);
    bool same = view.first == first && view.selected == selected && view.total == 12 &&
                view.rows.size() == std::min(core::palette_row_limit, view.total - first);
    for (std::size_t index = 0; same && index < view.rows.size(); ++index)
    {
        same = view.rows.at(index).command == "tabnext " + std::to_string(first + index + 1);
    }
    return same;
}

void verify_palette_view_window()
{
    Editing editor;
    auto &controller = editor.controller();
    for (std::size_t made = 1; made < 12; ++made)
    {
        applied(controller, app::NewTab{});
    }
    applied(controller, app::SwitchTab{0});
    bool follows = window_follows_tabs(palette_of(controller.apply(app::OpenCommandPalette{})), 0);
    for (std::size_t selected = 1; selected < 12; ++selected)
    {
        const auto frame = controller.apply(app::EditCommand{core::CommandEdit::complete_next});
        follows = follows && window_follows_tabs(palette_of(frame), selected);
    }
    expect(follows, "the frame carries at most eight rows of the result around the selection");
    expect(palette_of(controller.frame()).rows.size() == core::palette_row_limit &&
               palette_of(controller.frame()).first == 4,
           "the last selection keeps the eight rows before it");
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
    verify_listed_code_points();
    verify_palette_sources();
    verify_palette_result_shared();
    verify_palette_entries_controller();
    verify_palette_notes_geometry();
    verify_palette_hint_view();
    verify_palette_history_rows();
    verify_palette_history_open();
    verify_palette_composition();
    verify_tab_folders();
    verify_palette_window_covers_rows();
    verify_palette_view_window();
    verify_palette_folder_scope();
    verify_unlisted_table();
    verify_unlisted_names();
    verify_folder_of();
    verify_palette_extended_tail();
    verify_palette_extended_keeps_choice();
    verify_palette_extended_edges();
}
} // namespace nenenib::tests
