// scope `--vim-window` の単体テスト（ADR 0042 決定 2・ADR 0148 の決定 7）。`Ctrl-w` の 2 打鍵は
// oracle の :normal! で窓を観測できないので fixture にならず、ここの契約で守る。
#include "Editing.hpp"
#include "EditorController.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimInsertString.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNoEffect.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterText.hpp"
#include "VimRemoveRange.hpp"
#include "VimReplay.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTabs.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"
#include "VimWindowCommand.hpp"

#include <array>
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
using nenenib::core::collapsed_at;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::vim_step;
using nenenib::core::VimEditorView;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPrefix;
using nenenib::core::VimReplay;
using nenenib::core::VimState;
using nenenib::core::VimStep;
using nenenib::core::VimViewport;
using nenenib::core::VimWindowCommand;

// 鍵の記法の `|` は Ctrl-w（`<C-w>` は fixture の記法の表に無い・表は oracle の 1 か所）。
constexpr char32_t control_w_mark = U'|';

[[nodiscard]] std::vector<VimKey> window_keys(std::string_view keys)
{
    std::vector<VimKey> result;
    for (const VimKey &key : vim_keys_of(keys))
    {
        const auto *const character = std::get_if<nenenib::core::VimCharacter>(&key);
        result.push_back(character != nullptr && character->code == control_w_mark
                             ? VimKey{VimSpecialKey::control_w}
                             : key);
    }
    return result;
}

// 鍵を順に engine へ流し、最後の 1 鍵の結果を返す。本文と選択は鍵のあいだで変えない（engine は
// 効果を返すだけで、窓の命令は本文にも選択にも触れない）。
[[nodiscard]] VimStep window_step_in(std::string_view body, Offset caret, const VimState &start,
                                     std::string_view keys)
{
    const auto text = TextBuffer::from_utf8(body);
    const auto &buffer = text.value();
    const Selection selection = collapsed_at(caret);
    const VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, vim_visible_lines, 0},
                             nenenib::core::VimKeySource::typed, nenenib::core::VimTabs{0, 1}};
    VimStep last{start, VimNoEffect{}};
    for (const VimKey &key : window_keys(keys))
    {
        last = vim_step(last.next, view, key);
    }
    return last;
}

[[nodiscard]] VimStep window_step_of(const VimState &start, std::string_view keys)
{
    return window_step_in("abc def\nghi", Offset{4}, start, keys);
}

[[nodiscard]] VimStep window_step_of(std::string_view keys)
{
    return window_step_of(empty_vim_state(), keys);
}

[[nodiscard]] bool commands(const VimStep &step, VimWindowCommand command, VimMode mode)
{
    const auto *const effect = std::get_if<VimWindowCommand>(&step.effect);
    return effect != nullptr && *effect == command && !step.failure.has_value() &&
           step.next.mode == mode && !step.next.count.has_value() &&
           !step.next.pending.has_value() && !step.next.input_wait.has_value();
}

[[nodiscard]] bool refused(const VimStep &step, VimMode mode)
{
    return std::holds_alternative<VimNoEffect>(step.effect) && step.failure.has_value() &&
           step.next.mode == mode && !step.next.count.has_value() &&
           !step.next.input_wait.has_value();
}

using WindowCase = std::pair<std::string_view, VimWindowCommand>;

constexpr std::array<WindowCase, 15> window_cases{{
    {"|s", VimWindowCommand::split_horizontal},
    {"|S", VimWindowCommand::split_horizontal},
    {"|v", VimWindowCommand::split_vertical},
    {"|<C-v>", VimWindowCommand::split_vertical},
    {"|w", VimWindowCommand::next},
    {"|W", VimWindowCommand::next},
    {"|p", VimWindowCommand::next},
    {"||", VimWindowCommand::next},
    {"|h", VimWindowCommand::left},
    {"|j", VimWindowCommand::down},
    {"|k", VimWindowCommand::up},
    {"|l", VimWindowCommand::right},
    {"|c", VimWindowCommand::close},
    {"|q", VimWindowCommand::close},
    {"|o", VimWindowCommand::only},
}};

// 2 打鍵目の表（決定 7）。NORMAL の 1 打鍵目は待ちを置くだけで効果は無い。
void verify_window_table()
{
    const VimStep waiting = window_step_of("|");
    expect(waits_for_prefix(waiting.next, VimPrefix::window_command) &&
               std::holds_alternative<VimNoEffect>(waiting.effect) && !waiting.failure.has_value(),
           "control-w waits for the window command");
    for (const auto &[keys, command] : window_cases)
    {
        expect(commands(window_step_of(keys), command, VimMode::normal),
               (std::string(keys) + ": the second key names the window command").c_str());
    }
    expect(commands(window_step_of("3|w"), VimWindowCommand::next, VimMode::normal),
           "a count before control-w is dropped");
}

// 表に無い 2 打鍵目は失敗し、Esc は待ちを消すだけ。オペレータの後ろの Ctrl-w は失敗する。
void verify_window_refusals()
{
    expect(refused(window_step_of("|x"), VimMode::normal),
           "a second key outside the table fails without an effect");
    expect(refused(window_step_of("|<CR>"), VimMode::normal),
           "and so does a special key outside the table");
    const VimStep escaped = window_step_of("|<Esc>");
    expect(std::holds_alternative<VimNoEffect>(escaped.effect) && !escaped.failure.has_value() &&
               !escaped.next.input_wait.has_value(),
           "escape after control-w clears the wait without failing");
    const VimStep operated = window_step_of("d|");
    expect(refused(operated, VimMode::normal) && !operated.next.pending.has_value(),
           "control-w after an operator fails and cancels the operator");
    const VimStep counted = window_step_of("2d|");
    expect(refused(counted, VimMode::normal) && !counted.next.pending.has_value(),
           "and so does a counted operator, dropping the count");
    const VimStep visual_escaped = window_step_of("v|<Esc>");
    expect(std::holds_alternative<VimNoEffect>(visual_escaped.effect) &&
               !visual_escaped.failure.has_value() && visual_escaped.next.mode == VimMode::visual &&
               !visual_escaped.next.input_wait.has_value(),
           "escape after control-w in VISUAL clears only the wait and keeps VISUAL");
}

// ほかの待ちの後ろの Ctrl-w（Issue #684 のレビュー I2）。`f` は 0x17 を文字として探し、`z` は
// 表に無いので失敗し、`g` は待ちを静かに打ち消す。
void verify_window_after_other_waits()
{
    const VimStep found = window_step_in("a\x17"
                                         "b",
                                         Offset{0}, empty_vim_state(), "f|");
    const auto *const moved = std::get_if<nenenib::core::VimMoveTo>(&found.effect);
    expect(moved != nullptr && moved->caret == Offset{1} && !found.failure.has_value() &&
               !found.next.input_wait.has_value(),
           "f then control-w finds the control-w character");
    expect(refused(window_step_of("f|"), VimMode::normal),
           "and fails when the line has no control-w character");
    expect(refused(window_step_of("z|"), VimMode::normal), "z then control-w fails");
    const VimStep cancelled = window_step_of("g|");
    expect(std::holds_alternative<VimNoEffect>(cancelled.effect) &&
               !cancelled.failure.has_value() && !cancelled.next.input_wait.has_value() &&
               cancelled.next.mode == VimMode::normal,
           "g then control-w cancels the wait quietly");
}

// VISUAL でも効き、VISUAL は解除しない（枠を移るときに畳むのは controller）。INSERT
// では窓の命令ではなく語を消す（#1040）。
void verify_window_modes()
{
    expect(commands(window_step_of("v|w"), VimWindowCommand::next, VimMode::visual) &&
               commands(window_step_of("V|v"), VimWindowCommand::split_vertical,
                        VimMode::visual_line) &&
               commands(window_step_of("<C-v>|s"), VimWindowCommand::split_horizontal,
                        VimMode::visual_block),
           "control-w works in every VISUAL mode and keeps it");
    expect(refused(window_step_of("v|x"), VimMode::visual), "and fails outside the table there");
    const VimStep inserting = window_step_of("i|");
    expect(std::holds_alternative<nenenib::core::VimRemoveRange>(inserting.effect) &&
               !inserting.failure.has_value() && inserting.next.mode == VimMode::insert &&
               !inserting.next.input_wait.has_value(),
           "control-w in INSERT erases a word and does not wait");
    const VimStep typed = window_step_of("i|w");
    expect(typed.next.mode == VimMode::insert &&
               std::holds_alternative<nenenib::core::VimInsertString>(typed.effect),
           "and the next key is typed as text");
}

// `.` の記録に残らない（本文を変えない）。直前の変更はそのまま `.` で繰り返せる。
void verify_window_not_repeated()
{
    const VimStep removed = window_step_of("x");
    const VimStep after = window_step_of(removed.next, "|w");
    expect(dot_record_is(after.next, std::nullopt, "x") && !after.next.recording.has_value(),
           "a window command leaves the last change as it was");
    const VimStep visual = window_step_of(removed.next, "v|w");
    expect(dot_record_is(visual.next, std::nullopt, "x") && !visual.next.recording.has_value(),
           "also in VISUAL");
}

// マクロの録画には鍵として入り、`@a` が同じ鍵を再生する（決定 7）。
void verify_window_macro()
{
    const VimStep recorded = window_step_of("qa|wq");
    const auto &stored = recorded.next.registers.registers.at(0).value();
    expect(stored.text == "\x17w", "the recording keeps control-w and its second key");
    expect(nenenib::core::vim_keys_of_text(stored.text) == window_keys("|w"),
           "and reads back as the same keys");
    const VimStep replayed = window_step_of(recorded.next, "@a");
    const auto *const replay = std::get_if<VimReplay>(&replayed.effect);
    expect(replay != nullptr && replay->keys == window_keys("|w"),
           "@a replays control-w and its second key");
    expect(commands(window_step_of(recorded.next, "|w"), VimWindowCommand::next, VimMode::normal),
           "and the replayed keys give the window command");
}

// `"ap` はマクロの鍵列を文字として貼るので、Ctrl-w は 0x17 の 1 文字になる（Ctrl-r の 0x12
// と同じ）。
void verify_window_register_put()
{
    Editing editing;
    open_vim_document(editing, "abc");
    EditorController &controller = editing.controller();
    static_cast<void>(controller.press_vim_keys(window_keys("qa|wq\"ap")));
    expect(whole_vim_body(controller) == "a\x17wbc",
           "\"ap puts control-w as one control character");
}

// controller の写しは #682 の意図が入るまで何もしない。鍵は本文もキャレットも動かさず、失敗した
// 2 打鍵目は鍵の列の後ろを打ち切る（Issue #230 と同じ）。
void verify_window_controller()
{
    Editing editing;
    open_vim_document(editing, "abc def");
    EditorController &controller = editing.controller();
    static_cast<void>(controller.press_vim_keys(window_keys("ll|v")));
    const auto frame = controller.frame();
    expect(whole_vim_body(controller) == "abc def" && caret_at(frame, 1, 3) &&
               !controller.vim_state().input_wait.has_value(),
           "a window command through the controller changes neither the text nor the caret");
    static_cast<void>(controller.press_vim_keys(window_keys("|xx")));
    expect(whole_vim_body(controller) == "abc def", "a refused second key drops the keys after it");
    static_cast<void>(controller.press_vim_keys(window_keys("i|<Esc>")));
    expect(whole_vim_body(controller) == "c def" && controller.vim_state().mode == VimMode::normal,
           "control-w in INSERT erases the word before the caret instead of a window command");
}
} // namespace

void verify_vim_window_contracts()
{
    verify_window_table();
    verify_window_refusals();
    verify_window_after_other_waits();
    verify_window_modes();
    verify_window_not_repeated();
    verify_window_macro();
    verify_window_register_put();
    verify_window_controller();
}

void verify_vim_window_scope()
{
    verify_vim_window_contracts();
}
} // namespace nenenib::tests
