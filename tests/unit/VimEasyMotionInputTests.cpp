// scope `--vim-easymotion-input` の単体テスト（ADR 0163 の決定 8 の application の分・#1779）。
// 窓と同じ意図（VimKeyPress・CommandText・CancelCommand・SubmitCommand・合図・VisibleLines・
// ScrollLines）で流し、`\\f` の 1 文字の入力行・IME の確定の 1 文字・結合文字・Esc と Enter の
// 取消・VISUAL の伸び・`.` に載らない・LineView.labels の写しと桁と全角・印の間の画面と本文の変化で
// 捨てる・合図では捨てない・改行で終わる本文を見る。本物の Vim に無いので oracle は無い。
#include "CancelCommand.hpp"
#include "Column.hpp"
#include "CommandText.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "ImeStance.hpp"
#include "InputLinePrompt.hpp"
#include "InputLineView.hpp"
#include "LineView.hpp"
#include "MotionLabel.hpp"
#include "Scopes.hpp"
#include "ScrollLines.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimKeyPress.hpp"
#include "VimMode.hpp"
#include "VimSpecialKey.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
using core::Column;
using core::MotionLabel;

constexpr char32_t backslash = 0x5C;

// 窓の 1 打鍵と同じ意図で Vim の鍵を流す（意図の出口が入力行を開く）。
app::EditorFrame typed(app::EditorController &controller, std::u32string_view keys)
{
    for (const char32_t code : keys)
    {
        static_cast<void>(
            controller.apply_frame(app::VimKeyPress{core::VimKey{core::VimCharacter{code}}}));
    }
    return controller.frame();
}

app::EditorFrame leader_typed(app::EditorController &controller, std::u32string_view motion)
{
    std::u32string keys{backslash, backslash};
    keys += motion;
    return typed(controller, keys);
}

app::EditorFrame text_typed(app::EditorController &controller, std::string_view utf8)
{
    static_cast<void>(controller.apply_frame(app::CommandText{std::string(utf8)}));
    return controller.frame();
}

[[nodiscard]] bool jump_line_shown(const app::EditorFrame &frame)
{
    return frame.command_line.has_value() &&
           frame.command_line.value().prompt == core::InputLinePrompt::jump_character;
}

[[nodiscard]] bool engine_rests(const app::EditorController &controller)
{
    return !controller.vim_state().input_wait.has_value() &&
           !controller.vim_state().easy_motion.has_value();
}

[[nodiscard]] std::vector<MotionLabel> labels_on(const app::EditorFrame &frame, std::size_t line)
{
    return frame.lines.at(line).labels;
}

[[nodiscard]] bool no_labels(const app::EditorFrame &frame)
{
    for (const auto &line : frame.lines)
    {
        if (!line.labels.empty())
        {
            return false;
        }
    }
    return true;
}

// `\\f` は IME を使う人に任せる 1 文字の入力行を開き、確定した文字で印を出す（決定 4・6）。
void verify_character_line()
{
    Editing editing;
    open_vim_document(editing, "abcabc");
    auto &controller = editing.controller();
    const auto waiting = leader_typed(controller, U"f");
    expect(jump_line_shown(waiting) && waiting.ime == app::ImeStance::as_left &&
               waiting.command_line.value_or(core::InputLineView{}).text.empty(),
           "\\f opens the one-character line with the IME left to the user");
    const auto labeled = text_typed(controller, "c");
    expect(!jump_line_shown(labeled) &&
               labels_on(labeled, 0) ==
                   std::vector<MotionLabel>{{Column{3}, "a", 1}, {Column{6}, "s", 1}},
           "the character closes the line and labels the targets on the line");
    const auto jumped = typed(controller, U"s");
    expect(caret_at(jumped, 1, 6) && no_labels(jumped) && engine_rests(controller),
           "a label key jumps and the labels go away");
}

// IME の確定は最初の 1 文字だけ・結合文字は手前の文字に含む（決定 4・ADR 0053）。
void verify_committed_characters()
{
    Editing wide;
    open_vim_document(wide, "あいあう");
    static_cast<void>(leader_typed(wide.controller(), U"f"));
    const auto first = text_typed(wide.controller(), "あい");
    expect(caret_at(first, 1, 3) && !jump_line_shown(first) && engine_rests(wide.controller()),
           "the first character of the committed text is the target (one target jumps)");
    Editing combined;
    open_vim_document(combined, "xe\xCC\x81 e");
    static_cast<void>(leader_typed(combined.controller(), U"f"));
    const auto marked = text_typed(combined.controller(), "e\xCC\x81");
    expect(labels_on(marked, 0) ==
               std::vector<MotionLabel>{{Column{2}, "a", 1}, {Column{5}, "s", 1}},
           "a combined character targets its base character");
}

// Esc と Enter と空の行の Backspace は取消で、engine の待ちも消える（決定 4）。
void verify_cancelled_line()
{
    Editing escaped;
    open_vim_document(escaped, "abcabc");
    static_cast<void>(leader_typed(escaped.controller(), U"f"));
    static_cast<void>(escaped.controller().apply_frame(app::CancelCommand{}));
    const auto after_escape = typed(escaped.controller(), U"x");
    expect(!jump_line_shown(after_escape) && whole_vim_body(escaped.controller()) == "bcabc",
           "Esc cancels the wait and the next key works as usual");
    Editing entered;
    open_vim_document(entered, "abcabc");
    static_cast<void>(leader_typed(entered.controller(), U"f"));
    const auto after_enter = entered.controller().apply_frame(app::SubmitCommand{});
    expect(!jump_line_shown(after_enter) && engine_rests(entered.controller()),
           "Enter without a character cancels the wait");
}

// VISUAL は選択を伸ばし、跳びは `.` に載らない（決定 5）。
void verify_visual_and_repeat()
{
    Editing visual;
    open_vim_document(visual, "ab cd ef");
    static_cast<void>(typed(visual.controller(), U"v"));
    static_cast<void>(leader_typed(visual.controller(), U"f"));
    static_cast<void>(text_typed(visual.controller(), "e"));
    const auto extended = visual.controller().frame();
    const auto selection = extended.lines.at(0).selection;
    expect(extended.vim_mode == core::VimMode::visual && selection.begin == Column{1} &&
               selection.end == Column{8},
           "VISUAL extends the selection to the target");
    Editing repeated;
    open_vim_document(repeated, "xab cd ef");
    static_cast<void>(typed(repeated.controller(), U"x"));
    static_cast<void>(leader_typed(repeated.controller(), U"w"));
    static_cast<void>(typed(repeated.controller(), U"s."));
    expect(whole_vim_body(repeated.controller()) == "ab cd f",
           ". repeats the x before the jump at the target");
}

// 全角の跳び先は 2 升（決定 6）。
void verify_wide_cells()
{
    Editing editing;
    open_vim_document(editing, "あいあう");
    static_cast<void>(typed(editing.controller(), U"l"));
    static_cast<void>(leader_typed(editing.controller(), U"s"));
    const auto labeled = text_typed(editing.controller(), "あ");
    const auto labels = labels_on(labeled, 0);
    expect(labels.size() == 2 && labels.at(0) == MotionLabel{Column{3}, "a", 2} &&
               labels.at(1) == MotionLabel{Column{1}, "s", 2},
           "wide targets take two cells and the nearest after the caret takes a");
}

// 印の間に画面か本文が変われば捨て、合図では捨てない（V-1777 の申し送り）。
void verify_discarded_labels()
{
    Editing signalled;
    open_vim_document(signalled, "ab cd ef");
    static_cast<void>(leader_typed(signalled.controller(), U"w"));
    const auto kept = signalled.controller().apply_frame(app::WorkCompleted{});
    expect(!no_labels(kept) && signalled.controller().vim_state().easy_motion.has_value(),
           "a signal keeps the labels");
    Editing resized;
    open_vim_document(resized, "ab cd ef");
    static_cast<void>(leader_typed(resized.controller(), U"w"));
    const auto after_resize = resized.controller().apply_frame(app::VisibleLines{12});
    expect(no_labels(after_resize) && engine_rests(resized.controller()),
           "a new window size drops the labels");
    Editing scrolled;
    open_vim_document(scrolled, "a\nb\nc\nd");
    static_cast<void>(leader_typed(scrolled.controller(), U"j"));
    const auto after_scroll = scrolled.controller().apply_frame(app::ScrollLines{1});
    expect(no_labels(after_scroll) && engine_rests(scrolled.controller()),
           "scrolling drops the labels");
}

// 改行で終わる本文は最後の空の行に跳び先を作らない（ADR 0188）。
void verify_final_line_break()
{
    Editing editing;
    open_vim_document(editing, "a\nb\n");
    const auto jumped = leader_typed(editing.controller(), U"j");
    expect(caret_at(jumped, 2, 1) && engine_rests(editing.controller()),
           "\\\\j jumps to the only line below without the empty last line");
}
// INSERT の Ctrl-o から開いた 1 文字の入力行を Esc で取り消したら INSERT へ戻る（ADR 0182 の
// 決定 8・M3 の申し送り・#1782）。
void verify_paused_insert()
{
    Editing editing;
    open_vim_document(editing, "abc");
    auto &controller = editing.controller();
    static_cast<void>(typed(controller, U"i"));
    static_cast<void>(
        controller.apply_frame(app::VimKeyPress{core::VimKey{core::VimSpecialKey::control_o}}));
    const auto waiting = leader_typed(controller, U"f");
    static_cast<void>(controller.apply_frame(app::CancelCommand{}));
    const auto resumed = controller.frame();
    expect(jump_line_shown(waiting) && !jump_line_shown(resumed) &&
               resumed.vim_mode == core::VimMode::insert,
           "Esc on the line opened from INSERT's Ctrl-o returns to INSERT");
}

// 文字の確定で跳んだ後も、INSERT の Ctrl-o の 1 命令として INSERT へ戻る（#1782 の追い）。
void verify_paused_insert_jump()
{
    Editing editing;
    open_vim_document(editing, "abcd");
    auto &controller = editing.controller();
    static_cast<void>(typed(controller, U"i"));
    static_cast<void>(
        controller.apply_frame(app::VimKeyPress{core::VimKey{core::VimSpecialKey::control_o}}));
    static_cast<void>(leader_typed(controller, U"f"));
    const auto jumped = text_typed(controller, "c");
    expect(!jump_line_shown(jumped) && jumped.vim_mode == core::VimMode::insert &&
               caret_at(jumped, 1, 3),
           "a jump from INSERT's Ctrl-o returns to INSERT at the target");
}
} // namespace

void verify_vim_easymotion_input_contracts()
{
    verify_character_line();
    verify_committed_characters();
    verify_cancelled_line();
    verify_visual_and_repeat();
    verify_wide_cells();
    verify_discarded_labels();
    verify_final_line_break();
    verify_paused_insert();
    verify_paused_insert_jump();
}
} // namespace nenenib::tests
