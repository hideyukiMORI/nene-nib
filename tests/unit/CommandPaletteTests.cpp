// scope `--command-palette` の単体テスト（ADR 0042 決定 2）。
#include "ActivateCommandChoice.hpp"
#include "BuiltinTheme.hpp"
#include "BuiltinThemes.hpp"
#include "CancelCommand.hpp"
#include "CancelComposition.hpp"
#include "CommandChoiceKind.hpp"
#include "CommandEdit.hpp"
#include "CommandPalette.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "ComposeText.hpp"
#include "DevicePixels.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InputLineView.hpp"
#include "InsertText.hpp"
#include "LayoutRect.hpp"
#include "OpenCommandPalette.hpp"
#include "PaletteLayout.hpp"
#include "PasteCommand.hpp"
#include "Scopes.hpp"
#include "SelectAll.hpp"
#include "SelectEditMode.hpp"
#include "SettingsFailure.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "ThemeChoice.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"

#include <cstddef>
#include <string>

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
}
} // namespace nenenib::tests
