// scope `--input-line-limit` の単体テスト（#1568・ADR 0022 の追補 #1568）。入力行（`:`・`/` `?`・
// 通常モードの「検索: 」「置換後: 」「行: 」・Ctrl+P の面）の上限は core::input_line_limit の 1
// つ。
// 上限を超えた入力は行に持ったまま「あふれた」行にし、確定は丸ごと断る（切れた命令を実行しない）。
// 1. core: あふれ・Backspace で戻す・持てる最大を超える差し込みの印（lost）・描画の窓・Ex の評価の
//    上限と、長い引数で知らせの 1 行が上限の内に収まること（例外で止まらない）。
// 2. 打った鍵と再生（:normal! の意味）: 300 バイトの pattern の `/` が当たる・あふれた行の確定は
//    本文・レジスタ・マーク・未保存の印を変えずに断って再生を打ち切る・Backspace で戻せば確定
//    できる・lost の行は戻しても断る。
// 3. 貼り付けと IME の確定も同じ規則・検索の履歴に長い行が入り、あふれた行は入らない・`:set` と
//    `:w <長いパス>` の長い引数。
// 4. 確定と同じ口: 通常モードの「検索: 」の F3 / Shift+F3（Ctrl+G / Ctrl+T も同じ意図）・
//    「置換後: 」の Enter と Ctrl+Enter・面の候補のクリックも、あふれた行は同じ 1 か所で断る。
//    incsearch の preview はあふれた行では照合せず、起点のまま（レビュー V-1570 の M1・M2・L3）。
#include "ActivateCommandChoice.hpp"
#include "BeginFind.hpp"
#include "BeginReplace.hpp"
#include "CancelCommand.hpp"
#include "CommandInput.hpp"
#include "CommandLine.hpp"
#include "CommandText.hpp"
#include "CommitText.hpp"
#include "DisplayText.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "FontSize.hpp"
#include "InputLineView.hpp"
#include "InputText.hpp"
#include "Offset.hpp"
#include "OffsetRange.hpp"
#include "OpenCommandPalette.hpp"
#include "PasteCommand.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SearchHop.hpp"
#include "SearchLine.hpp"
#include "SelectEditMode.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"
#include "VimCharacter.hpp"
#include "VimKeyPress.hpp"
#include "VimSearchDirection.hpp"
#include "VimTestSupport.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using core::CommandEdit;
using core::input_line_hold;
using core::input_line_limit;

constexpr std::string_view four_lines = "one\ntwo\nthree\nfour\n";

[[nodiscard]] std::string repeated(std::string_view unit, std::size_t count)
{
    std::string text;
    for (std::size_t index = 0; index < count; ++index)
    {
        text += unit;
    }
    return text;
}

[[nodiscard]] std::string message_of(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string();
}

[[nodiscard]] std::string limit_message()
{
    return std::string(core::ex_failure_message(core::ExFailure::too_long).text());
}

void verify_core_overflow()
{
    auto line = core::CommandLine::empty().inserted(std::string(input_line_limit, 'x')).value();
    expect(!line.overflowed() && line.text().size() == input_line_limit,
           "a line of exactly the limit is not overflowed");
    line = line.inserted("y").value();
    expect(line.overflowed() && line.text().size() == input_line_limit + 1,
           "one more byte is kept and marks the line overflowed");
    line = line.edited(CommandEdit::backspace);
    expect(!line.overflowed() && line.text() == std::string(input_line_limit, 'x'),
           "a backspace back within the limit clears the overflow");
    expect(!core::CommandLine::empty().inserted("\n") &&
               !core::CommandLine::empty().inserted("\xFF"),
           "control characters and broken UTF-8 are still closed failures");
    auto search = core::SearchLine::opened(core::VimSearchDirection::forward)
                      .inserted(repeated("界", 1366))
                      .value();
    expect(search.overflowed() && search.text().size() == 4098,
           "the search line shares the same overflow rule");
    search = search.edited(CommandEdit::backspace);
    expect(!search.overflowed() && search.text().size() == 4095,
           "a backspace removes one code point and returns within the limit");
    expect(core::SearchLine::opened_find(std::string(input_line_limit + 1, 'f')).text().empty(),
           "a preset beyond the limit opens the find line empty");
}

// 持てる最大（上限の 2 倍）を超える差し込みは捨てて lost を立て、戻しても消えない。
void verify_core_hold()
{
    auto held = core::CommandLine::empty().inserted(std::string(input_line_hold, 'a')).value();
    expect(held.overflowed() && held.text().size() == input_line_hold,
           "the line holds up to twice the limit");
    held = held.inserted("b").value();
    expect(held.overflowed() && held.text().size() == input_line_hold,
           "an insert past the hold keeps the text and marks it lost");
    for (std::size_t index = 0; index < input_line_hold; ++index)
    {
        held = held.edited(CommandEdit::backspace);
    }
    expect(held.text().empty() && held.overflowed(),
           "a lost line stays overflowed even when backspaced to empty");
    const auto pasted = core::CommandLine::empty().inserted(std::string(input_line_hold + 1, 'p'));
    expect(pasted.has_value() && pasted.value().text().empty() && pasted.value().overflowed(),
           "one paste longer than the hold is dropped whole and marks the line lost");
}

void verify_core_window()
{
    const std::string text = repeated("界", 2000);
    const auto window = core::input_line_window(text, core::Offset{3000}, core::Offset{3000});
    expect(window.begin.value == 3000 - core::input_line_window_before + 1 &&
               window.end.value == 3000 + core::input_line_window_after - 1,
           "the window keeps whole code points around the caret");
    expect(core::is_boundary(text, window.begin) && core::is_boundary(text, window.end),
           "both ends of the window are code point boundaries");
    const auto whole = core::input_line_window("abc", core::Offset{1}, core::Offset{2});
    expect(whole.begin.value == 0 && whole.end.value == 3, "a short line is laid out whole");
}

// 受けたときの知らせが表示の 1 行に収まるか（断るのは閉じた失敗なので常に真）。
[[nodiscard]] bool shown_within_display(std::string_view text)
{
    const auto result =
        core::evaluate_ex(text, core::default_editor_settings(), core::Appearance::dark);
    return !result.has_value() ||
           result.value().message.text().size() <= core::DisplayText::maximum_bytes;
}

// 命令の候補の全部に長い引数を付けて評価する。
void verify_long_candidates(std::string_view filler, std::size_t count)
{
    for (const auto &command : core::ex_command_candidates())
    {
        const auto text = command + repeated(filler, count);
        expect(text.size() > input_line_limit || shown_within_display(text),
               "a long argument is shown within the display limit");
    }
}

// 入力行の上限まで長い命令でも、評価は例外で止まらず、知らせは表示の 1 行に収まる（#495 の後続）。
void verify_core_evaluation()
{
    const auto settings = core::default_editor_settings();
    const auto too_long = core::ExEvaluationFailure{core::ExFailure::too_long};
    const auto fits = "1" + std::string(input_line_limit - 1, ' ');
    expect(core::evaluate_ex(fits, settings, core::Appearance::dark).has_value(),
           "a command of exactly the limit is evaluated");
    const auto over = core::evaluate_ex(fits + " ", settings, core::Appearance::dark);
    expect(!over && over.error() == too_long, "one more byte than the limit is too long");
    for (const std::string_view filler : {"a", "0", "界", " ", ":h12", "9"})
    {
        verify_long_candidates(filler, 90);
        verify_long_candidates(filler, 1000);
    }
    for (const auto &text : {std::string(4000, '7'), ".,.+" + std::string(4000, '0') + "1"})
    {
        const auto result = core::evaluate_ex(text, settings, core::Appearance::dark);
        expect(result.has_value() &&
                   result.value().message.text().size() <= core::DisplayText::maximum_bytes,
               "a long line number is shown clipped");
    }
}

// 300 バイトの pattern は切れずに当たる（256 バイトで切れると 2 行目に当たる）。
void verify_long_search()
{
    Editing editing;
    open_vim_document(editing,
                      "b\n" + std::string(299, 'y') + "q\n" + std::string(299, 'y') + "z\n");
    auto &controller = editing.controller();
    vim_normal(controller, "/" + std::string(299, 'y') + "z<CR>");
    const auto frame = controller.frame();
    expect(caret_at(frame, 3, 1) && !frame.command_line.has_value() && message_of(frame).empty(),
           "a 300-byte search pattern finds the whole pattern");
}

// あふれた `:` の行の確定は丸ごと断る。本文・レジスタ・マーク・未保存の印は変わらず、後ろの
// 鍵（`x`）は流れない。
void verify_overflow_refused()
{
    Editing editing;
    open_vim_document(editing, std::string(four_lines));
    auto &controller = editing.controller();
    vim_normal(controller, "yy");
    const auto marks = controller.vim_marks();
    const auto held = controller.vim_state().unnamed_register.value().text;
    vim_normal(controller, ":3" + std::string(input_line_limit, ' ') + "<CR>x");
    const auto frame = controller.frame();
    expect(whole_vim_body(controller) == four_lines && caret_at(frame, 1, 1),
           "an overflowed command line is not run, not even truncated");
    expect(controller.vim_marks() == marks &&
               controller.vim_state().unnamed_register.value().text == held &&
               frame.document.save_state == core::SaveState::saved,
           "the refusal leaves marks, registers and the saved state");
    expect(!frame.command_line.has_value() && message_of(frame) == limit_message() &&
               editing.bell().rings() == 1,
           "the refusal closes the line, says so once and rings");
    vim_normal(controller, "/" + std::string(input_line_limit + 1, 'o') + "<CR>x");
    expect(whole_vim_body(controller) == four_lines && caret_at(controller.frame(), 1, 1),
           "an overflowed search line is refused and stops the replay too");
}

// あふれた後に Backspace で上限の内へ戻せば、ふつうに確定できる。持てる最大を超えて捨てた行は、
// 戻しても断る。
void verify_overflow_backspace()
{
    Editing editing;
    open_vim_document(editing, std::string(four_lines));
    auto &controller = editing.controller();
    vim_normal(controller,
               ":3" + std::string(input_line_limit, ' ') + repeated("<BS>", 1) + "<CR>x");
    expect(whole_vim_body(controller) == "one\ntwo\nhree\nfour\n" &&
               message_of(controller.frame()).empty(),
           "a backspace back within the limit lets the command run");
    Editing lost;
    open_vim_document(lost, std::string(four_lines));
    vim_normal(lost.controller(), ":3" + std::string(input_line_hold, ' ') +
                                      repeated("<BS>", input_line_hold - 8) + "<CR>x");
    expect(whole_vim_body(lost.controller()) == four_lines &&
               caret_at(lost.controller().frame(), 1, 1),
           "a line that dropped input is refused even after backspacing");
}

// 貼り付けと IME の確定も打った鍵と同じ規則（入口ごとに書かない）。
void verify_paste_and_commit()
{
    Editing editing;
    open_vim_document(editing, std::string(four_lines));
    auto &controller = editing.controller();
    vim_replay(controller, ":3");
    editing.clipboard().hold(std::string(input_line_limit, ' '));
    auto frame = controller.apply_frame(app::PasteCommand{});
    expect(frame.command_line.has_value() && message_of(frame) == limit_message(),
           "a paste past the limit is kept and the line says it overflowed");
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(caret_at(frame, 1, 1) && !frame.command_line.has_value() &&
               message_of(frame) == limit_message(),
           "Enter on a pasted overflow is refused");
    vim_replay(controller, ":3");
    static_cast<void>(controller.apply_frame(app::PasteCommand{}));
    frame = controller.apply_frame(app::EditCommand{CommandEdit::backspace});
    expect(message_of(frame).empty(), "returning within the limit clears the notice");
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(caret_at(frame, 3, 1), "the pasted command runs once it fits");
    vim_replay(controller, ":1");
    static_cast<void>(controller.apply_frame(app::CommitText{std::string(input_line_limit, ' ')}));
    frame = controller.apply_frame(app::SubmitCommand{});
    expect(caret_at(frame, 3, 1) && message_of(frame) == limit_message(),
           "an IME commit past the limit overflows the same way");
}

// 検索の履歴には上限の内の長い行が入り、あふれた行は入らない（呼び出すと切れた行になる）。
void verify_search_history()
{
    Editing editing;
    open_vim_document(editing, std::string(four_lines));
    auto &controller = editing.controller();
    const std::string pattern(3000, 'q');
    vim_replay(controller, "/" + pattern + "<CR>");
    vim_replay(controller, "/" + std::string(input_line_limit + 1, 'w') + "<CR>");
    static_cast<void>(controller.apply_frame(app::VimKeyPress{core::VimCharacter{U'/'}}));
    const auto frame = controller.apply_frame(app::EditCommand{CommandEdit::history_previous});
    expect(frame.command_line.has_value() && frame.command_line.value().text == pattern,
           "Up recalls the long pattern whole and skips the refused one");
}

// `:set` と `:w` の長い引数（256 バイトで切らない）。
void verify_long_arguments()
{
    Editing editing;
    open_vim_document(editing, std::string(four_lines));
    auto &controller = editing.controller();
    auto frame = run_ex(controller, "set fontsize=" + std::string(300, '0') + "13");
    expect(editing.settings().writes() == 1 &&
               editing.settings()
                       .written()
                       .value_or(core::default_editor_settings())
                       .font_size.points() > 12.5F &&
               editing.settings()
                       .written()
                       .value_or(core::default_editor_settings())
                       .font_size.points() < 13.5F &&
               frame.command_message.has_value() &&
               frame.command_message.value().text().size() <= core::DisplayText::maximum_bytes,
           "a long set value is read whole and shown clipped");
    const std::string name = repeated("長い名前", 30) + ".txt";
    frame = run_ex(controller, "w " + name);
    expect(editing.files().resolve_input() == name && editing.files().written_path() == name &&
               message_of(frame) == "Written",
           "a write target over 260 characters is passed whole");
}

// 通常モードの文書を開く（「検索: 」「置換後: 」は通常モードの入力行）。
void open_ordinary(Editing &editing, std::string text)
{
    open_vim_document(editing, std::move(text));
    static_cast<void>(
        editing.controller().apply_frame(app::SelectEditMode{core::EditMode::ordinary}));
}

// あふれた「検索: 」で F3 / Shift+F3 を押しても、確定と同じに断る。キャレット・語・最後の検索は
// 変わらない（Ctrl+G / Ctrl+T は同じ SearchHop の意図）。
void verify_find_hop_refused(core::VimSearchDirection relative, const char *what)
{
    Editing editing;
    open_ordinary(editing, "x\n" + std::string(input_line_limit + 1, 'a') + "\n");
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::BeginFind{}));
    static_cast<void>(
        controller.apply_frame(app::CommandText{std::string(input_line_limit + 1, 'a')}));
    const auto frame = controller.apply_frame(app::SearchHop{relative});
    expect(caret_at(frame, 1, 1) && !frame.command_line.has_value() &&
               message_of(frame) == limit_message() &&
               !controller.vim_state().last_search.has_value(),
           what);
    const auto reopened = controller.apply_frame(app::BeginFind{});
    expect(reopened.command_line.has_value() && reopened.command_line.value().text.empty(),
           "the refused hop does not remember the word");
}

// `foo` を打った後に持てる最大を超えて貼ると lost になる。F3 は打った語と違う `foo` で探さない。
void verify_find_hop_lost()
{
    Editing editing;
    open_ordinary(editing, "x\nfoo\n");
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::BeginFind{}));
    static_cast<void>(controller.apply_frame(app::CommandText{"foo"}));
    static_cast<void>(controller.apply_frame(app::CommandText{std::string(9000, 'b')}));
    const auto frame = controller.apply_frame(app::SearchHop{core::VimSearchDirection::forward});
    expect(caret_at(frame, 1, 1) && !frame.command_line.has_value() &&
               message_of(frame) == limit_message() &&
               !controller.vim_state().last_search.has_value(),
           "F3 on a lost find line does not search the kept word");
}

// 「置換後: 」を開いてあふれた置換語を打つ。
void overflow_replacement(app::EditorController &controller)
{
    static_cast<void>(controller.apply_frame(app::BeginReplace{}));
    static_cast<void>(controller.apply_frame(app::CommandText{"beta"}));
    static_cast<void>(controller.apply_frame(app::SubmitCommand{}));
    static_cast<void>(
        controller.apply_frame(app::CommandText{std::string(input_line_limit + 1, 'B')}));
}

// 「置換後: 」の Enter と Ctrl+Enter と F3 も、あふれた行は本文を変えずに断る。
void verify_replace_refused()
{
    for (const bool all : {false, true})
    {
        Editing editing;
        open_ordinary(editing, "beta\nbeta\n");
        auto &controller = editing.controller();
        overflow_replacement(controller);
        const auto frame = controller.apply_frame(app::SubmitCommand{all});
        expect(whole_vim_body(controller) == "beta\nbeta\n" && !frame.command_line.has_value() &&
                   message_of(frame) == limit_message() &&
                   frame.document.save_state == core::SaveState::saved,
               all ? "Ctrl+Enter on an overflowed replacement is refused"
                   : "Enter on an overflowed replacement is refused");
    }
    Editing hop;
    open_ordinary(hop, "beta\nbeta\n");
    overflow_replacement(hop.controller());
    const auto frame =
        hop.controller().apply_frame(app::SearchHop{core::VimSearchDirection::forward});
    expect(caret_at(frame, 1, 1) && !frame.command_line.has_value() &&
               message_of(frame) == limit_message(),
           "F3 on an overflowed replacement line is refused like Enter");
}

// incsearch の preview はあふれた行では照合せず起点のまま。上限ちょうどでは当たりを見せる。
// Vim の `/` の Ctrl-G は当たりが無いので動かず、Esc は起点へ戻る。当たりの桁は 1 から数える。
void verify_preview_on_overflow()
{
    Editing editing;
    open_vim_document(editing, "x\n" + std::string(input_line_limit + 1, 'a') + "\n");
    auto &controller = editing.controller();
    // 1 鍵ずつ流すと preview が 4096 回照合するので、pattern は 1 回の差し込みで入れる。
    vim_replay(controller, "/");
    auto frame = controller.apply_frame(app::CommandText{std::string(input_line_limit, 'a')});
    expect(current_match_is(frame, 1, MatchSpan{1, input_line_limit + 1}) && caret_at(frame, 1, 1),
           "a pattern of exactly the limit is previewed");
    frame = controller.apply_frame(app::CommandText{"a"});
    expect(current_match_is(frame, 1, std::nullopt) && frame.command_line.has_value() &&
               caret_at(frame, 1, 1),
           "an overflowed pattern is not matched and the preview returns to the origin");
    frame = controller.apply_frame(app::SearchHop{core::VimSearchDirection::forward});
    expect(current_match_is(frame, 1, std::nullopt) && frame.command_line.has_value() &&
               caret_at(frame, 1, 1),
           "Ctrl-G on an overflowed vim search line does nothing");
    frame = controller.apply_frame(app::EditCommand{CommandEdit::backspace});
    expect(current_match_is(frame, 1, MatchSpan{1, input_line_limit + 1}),
           "backspacing within the limit previews again");
    static_cast<void>(controller.apply_frame(app::CommandText{"a"}));
    frame = controller.apply_frame(app::CancelCommand{});
    expect(!frame.command_line.has_value() && caret_at(frame, 1, 1),
           "Esc from an overflowed line returns to the origin");
}

// 面の候補のクリックも Enter と同じ 1 か所で断る（打った値の候補を lost の面で実行しない）。
void verify_palette_click_refused()
{
    Editing editing;
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(app::OpenCommandPalette{}));
    static_cast<void>(controller.apply_frame(app::CommandText{":set fontsize=18"}));
    static_cast<void>(controller.apply_frame(app::CommandText{std::string(9000, ' ')}));
    const auto frame = controller.apply_frame(app::ActivateCommandChoice{0});
    expect(editing.settings().writes() == 0 && !frame.command_line.has_value() &&
               message_of(frame) == limit_message(),
           "a click on a lost palette is refused like Enter");
}
} // namespace

void verify_input_line_limit_scope()
{
    verify_core_overflow();
    verify_core_hold();
    verify_core_window();
    verify_core_evaluation();
    verify_long_search();
    verify_overflow_refused();
    verify_overflow_backspace();
    verify_paste_and_commit();
    verify_search_history();
    verify_long_arguments();
    verify_find_hop_refused(core::VimSearchDirection::forward,
                            "F3 on an overflowed find line is refused like Enter");
    verify_find_hop_refused(core::VimSearchDirection::backward,
                            "Shift+F3 on an overflowed find line is refused like Enter");
    verify_find_hop_lost();
    verify_replace_refused();
    verify_preview_on_overflow();
    verify_palette_click_refused();
}
} // namespace nenenib::tests
