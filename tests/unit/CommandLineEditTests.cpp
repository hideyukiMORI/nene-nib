// scope `--command-line-edit` の単体テスト（#1550・#1643・ADR 0032 の追補）。
// 1. core: c_CTRL-U はキャレットの前だけを消し、c_CTRL-W は INSERT の `Ctrl-w` と同じ
//    vim_word_erase_begin で語の前を消す。Ctrl+P の面はどちらでも変わらない。Tab を文字として
//    入れるのは Vim の `/` `?` の行だけ（通常モードの「検索: 」は今のまま断る）。
// 2. 窓の意図: Vim の `:` `/` `?` の行で Ctrl+U / Ctrl+W が効き、通常モードの「検索: 」「行: 」と
//    Ctrl+P の面では何もしない（Windows の標準のまま・D82）。`/` の Tab は文字の Tab で、`:` の
//    Tab は補完のまま。
// 3. 録画（`q`）は Vim 9.1 の feedkeys(…, 'xt') の実測どおり `^U` `^W` を録り、`@` で同じに
//    流れる。map の右辺の `<C-u>` `<C-w>` `<Tab>` も Vim 9.1 の `:normal`（! なし）の実測どおり
//    （D:/NeNeNib/outputs/20261113/1550-impl/vim-measure.md）。
#include "BeginFind.hpp"
#include "BeginGoToLine.hpp"
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "InputLineView.hpp"
#include "InputText.hpp"
#include "Offset.hpp"
#include "OpenCommandPalette.hpp"
#include "Scopes.hpp"
#include "SearchLine.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "VimNamedRegisters.hpp"
#include "VimRegister.hpp"
#include "VimSearchDirection.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using app::EditorController;
using core::CommandEdit;

constexpr std::string_view five_lines = "a1 a\na2 a\na3 a\na4 a\na5 a";
constexpr std::string_view three_lines = "a1 a\na2 a\na3 a";

[[nodiscard]] core::InputText input_at(std::string text, std::size_t caret)
{
    return core::InputText{std::move(text), core::Offset{caret}};
}

[[nodiscard]] std::string shown_text(const app::EditorFrame &frame)
{
    return frame.command_line.has_value() ? frame.command_line.value().text
                                          : std::string{"<closed>"};
}

[[nodiscard]] std::string register_text(const EditorController &controller, char32_t name)
{
    const auto &held =
        controller.vim_state().registers.registers.at(core::vim_register_index(name).value_or(0));
    return held.value().text;
}

void verify_core_edits()
{
    const auto cut = core::edited_input(input_at("xyzd", 3), CommandEdit::erase_to_start);
    expect(cut.text == "d" && cut.caret.value == 0,
           "c_CTRL-U removes only the text before the caret (Vim keeps d of :xyzd<Left>)");
    const auto start = core::edited_input(input_at("3", 0), CommandEdit::erase_to_start);
    expect(start.text == "3" && start.caret.value == 0, "c_CTRL-U at the line start does nothing");
    const auto word = core::edited_input(input_at("s/a/XYZ abc", 11), CommandEdit::erase_word);
    expect(word.text == "s/a/XYZ " && word.caret.value == 8, "c_CTRL-W removes the word");
    const auto range = core::edited_input(input_at("'<,'>", 5), CommandEdit::erase_word);
    expect(range.text.empty(), "c_CTRL-W removes the whole '<,'> symbol run at once");
    const auto blanks = core::edited_input(input_at("foo  ", 5), CommandEdit::erase_word);
    expect(blanks.text.empty(), "c_CTRL-W takes the blanks and the word before them");
    const std::string kanji_kana = "漢字かな";
    const auto kana =
        core::edited_input(input_at(kanji_kana, kanji_kana.size()), CommandEdit::erase_word);
    expect(kana.text == "漢字", "kana and kanji are different classes (the INSERT table)");
    const auto middle = core::edited_input(input_at("xx s", 2), CommandEdit::erase_word);
    expect(middle.text == " s" && middle.caret.value == 0,
           "c_CTRL-W keeps the text after the caret");
    const auto none = core::edited_input(input_at("abc", 0), CommandEdit::erase_word);
    expect(none.text == "abc", "c_CTRL-W at the line start does nothing");
}

void verify_core_tab()
{
    const auto forward = core::SearchLine::opened(core::VimSearchDirection::forward).inserted("\t");
    const auto backward =
        core::SearchLine::opened(core::VimSearchDirection::backward).inserted("a\tb");
    expect(forward.has_value() && forward.value().text() == "\t" && backward.has_value(),
           "the Vim / and ? lines take a Tab as a character (#1643)");
    expect(!core::SearchLine::opened_find("").inserted("\t").has_value() &&
               !core::SearchLine::opened_go_to_line().inserted("\t").has_value(),
           "the ordinary find and go-to-line lines still refuse a Tab");
    expect(!core::SearchLine::opened(core::VimSearchDirection::forward).inserted("\n").has_value(),
           "other control characters are still refused on the search line");
    expect(!core::inserted_input(core::InputText{}, "\t").has_value(),
           "the shared insertion still refuses a Tab");
}

// 面（Ctrl+P）の入力は Ctrl+U / Ctrl+W で変わらない（D82・Windows の標準）。
void verify_palette_unchanged()
{
    Editing editing;
    open_vim_document(editing, std::string{five_lines});
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply_frame(app::CommandText{"abc def"}));
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_word}));
    const auto frame = controller.apply_frame(app::EditCommand{CommandEdit::erase_to_start});
    expect(shown_text(frame) == "abc def" && controller.command_palette_active(),
           "Ctrl+W and Ctrl+U leave the palette input as it is");
}

// 通常モードの「検索: 」「行: 」は Ctrl+U / Ctrl+W と Tab で変わらない（D82）。
void verify_ordinary_lines_unchanged()
{
    Editing editing;
    open_vim_document(editing, std::string{five_lines});
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::SelectEditMode{core::EditMode::ordinary}));
    static_cast<void>(controller.apply_frame(app::BeginFind{}));
    static_cast<void>(controller.apply_frame(app::CommandText{"foo bar"}));
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_word}));
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_to_start}));
    const auto find = controller.apply_frame(app::EditCommand{CommandEdit::complete_next});
    expect(shown_text(find) == "foo bar", "the find line ignores Ctrl+W, Ctrl+U and Tab");
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
    static_cast<void>(controller.apply_frame(app::BeginGoToLine{}));
    static_cast<void>(controller.apply_frame(app::CommandText{"12"}));
    const auto line = controller.apply_frame(app::EditCommand{CommandEdit::erase_to_start});
    expect(shown_text(line) == "12", "the go-to-line line ignores Ctrl+U");
}

// 窓の意図で打つ Vim の `:` の行（Ctrl+W・Ctrl+U は EditCommand で届く）。
void verify_window_ex_line()
{
    Editing editing;
    open_vim_document(editing, std::string{five_lines});
    auto &controller = editing.controller();
    vim_replay(controller, "vj:");
    const auto opened = controller.frame();
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_to_start}));
    static_cast<void>(controller.apply_frame(app::CommandText{"s/a/X/"}));
    const auto frame = controller.apply_frame(app::SubmitCommand{});
    expect(shown_text(opened) == "'<,'>" &&
               whole_vim_body(controller) == "X1 a\na2 a\na3 a\na4 a\na5 a",
           "Ctrl+U on the VISUAL : line clears the range and :s runs on the current line");
    expect(!frame.command_line.has_value(), "the line ran and closed");

    vim_replay(controller, ":");
    static_cast<void>(controller.apply_frame(app::CommandText{"s/a/XYZ abc"}));
    const auto erased = controller.apply_frame(app::EditCommand{CommandEdit::erase_word});
    expect(shown_text(erased) == "s/a/XYZ ", "Ctrl+W on the : line removes the word");
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
}

// `/` の Tab は文字の Tab（#1643）、Shift+Tab は何もしない。`:` の Tab は補完のまま。
void verify_window_tab()
{
    Editing editing;
    open_vim_document(editing, "x\tb\tc");
    auto &controller = editing.controller();
    vim_replay(controller, "/");
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::complete_next}));
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::complete_previous}));
    const auto typed = controller.apply_frame(app::CommandText{"c"});
    expect(shown_text(typed) == "\tc", "Tab on the / line is a Tab; Shift+Tab adds nothing");
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
    vim_replay(controller, "x");
    expect(whole_vim_body(controller) == "x\tbc", "the search lands on the Tab before c");

    vim_replay(controller, ":colo");
    const auto completed = controller.apply_frame(app::EditCommand{CommandEdit::complete_next});
    expect(shown_text(completed).starts_with("colorscheme"),
           "Tab on the : line is still completion");
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
}

// 録画（Vim 9.1 の feedkeys(…, 'xt') の実測: `qavj:\<C-u>s/a/X/\<CR>q` は `vj:^Us/a/X/^M`、
// `qb:s/a/Y q\<C-w>Z/\<CR>q` は `:s/a/Y q^WZ/^M`・本文は 1 行目が `X1 a` → `X1 Y Z`）。
void verify_recording()
{
    Editing editing;
    open_vim_document(editing, std::string{three_lines});
    auto &controller = editing.controller();
    vim_replay(controller, "qavj:");
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_to_start}));
    static_cast<void>(controller.apply_frame(app::CommandText{"s/a/X/"}));
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
    vim_replay(controller, "q");
    expect(register_text(controller, U'a') == "vj:\x15s/a/X/\r",
           "Ctrl+U on the recorded : line is recorded as ^U");
    expect(whole_vim_body(controller) == "X1 a\na2 a\na3 a", "the recorded line ran");

    vim_replay(controller, "qb:");
    static_cast<void>(controller.apply_frame(app::CommandText{"s/a/Y q"}));
    static_cast<void>(controller.apply_frame(app::EditCommand{CommandEdit::erase_word}));
    static_cast<void>(controller.apply_frame(app::CommandText{"Z/"}));
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
    vim_replay(controller, "q");
    expect(register_text(controller, U'b') == ":s/a/Y q\x17Z/\r",
           "Ctrl+W on the recorded : line is recorded as ^W");
    expect(whole_vim_body(controller) == "X1 Y Z\na2 a\na3 a", "the second recorded line ran");

    vim_replay(controller, "j@a");
    expect(whole_vim_body(controller) == "X1 Y Z\nX2 a\na3 a",
           "@a replays ^U through the same input-line key path");
}

// map の右辺（Vim 9.1 の `:normal`（! なし）の実測: `nnoremap Q :abc<C-u>s/a/X/<CR>` の `jQ` は
// 2 行目が `X2 a`、`nnoremap Q :s/a/X q<C-w>Y/<CR>` の `Q` は 1 行目が `X Y1 a`、
// `nnoremap Q /a2<C-w>a3<CR>` と `nnoremap Q /<Tab><C-u>a3<CR>` の `Q` はどちらも 3 行目）。
void mapped(EditorController &controller, std::string line)
{
    vim_replay(controller, ":");
    static_cast<void>(controller.apply_frame(app::CommandText{std::move(line)}));
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
}

// 1 行: map の行・打つ鍵・本文・キャレットの行・説明。
using MappedCase =
    std::tuple<std::string_view, std::string_view, std::string_view, std::size_t, const char *>;
constexpr std::array<MappedCase, 5> mapped_cases{{
    {"nnoremap Q :abc<C-u>s/a/X/<CR>", "jQ", "a1 a\nX2 a\na3 a", 2,
     "<C-u> in a map right side clears the : line"},
    {"nnoremap Q :s/a/X q<C-w>Y/<CR>", "Q", "X Y1 a\na2 a\na3 a", 1,
     "<C-w> in a map right side removes the word"},
    {"nnoremap Q /a2<C-w>a3<CR>", "Q", three_lines, 3, "<C-w> in a mapped search line"},
    {"nnoremap Q /<Tab><C-u>a3<CR>", "Q", three_lines, 3,
     "<Tab> then <C-u> in a mapped search line"},
    {"xnoremap Q :<C-u>s/a/X/<CR>", "vjQ", "X1 a\na2 a\na3 a", 1,
     "<C-u> in a VISUAL map clears the range"},
}};

void verify_map_right_side()
{
    for (const auto &[map, keys, body, line, what] : mapped_cases)
    {
        Editing editing;
        open_vim_document(editing, std::string{three_lines});
        auto &controller = editing.controller();
        mapped(controller, std::string{map});
        vim_replay(controller, keys);
        expect(whole_vim_body(controller) == body && caret_at(controller.frame(), line, 1), what);
    }
}
} // namespace

void verify_command_line_edit_scope()
{
    verify_core_edits();
    verify_core_tab();
    verify_palette_unchanged();
    verify_ordinary_lines_unchanged();
    verify_window_ex_line();
    verify_window_tab();
    verify_recording();
    verify_map_right_side();
}
} // namespace nenenib::tests
