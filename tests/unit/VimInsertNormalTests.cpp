// scope `--vim-insert-normal` の単体テスト（#1517・ADR 0182）。INSERT の `Ctrl-o` は INSERT
// を抜けて NORMAL の命令を 1 つ打ち、終わったら INSERT を始め直す印。fixture（接頭辞
// `insert-ctrl-o-`）は oracle の `:normal!`
// の側（打った鍵ではない）を測るので、ここは打った鍵の側を守る（Vim 9.1 の feedkeys(…, "xt") と
// scriptin の実測・docs/quality/vim-insert-ctrl-o-2026-11-12.md）。
// 1. undo は打った鍵で 3 単位（`<C-o>` の前・命令・始め直した後）、空の命令は単位を作らず、
//    再生の中は全体で 1 単位（決定 5）。
// 2. `<C-o>c` `C` と VISUAL の `c` の後の Esc（NORMAL の `s` は Nib に無い） は、打った鍵では
// NORMAL、:normal! では INSERT を
//    始め直す（決定 4・Vim の KeyTyped）。
// 3. 録画は打った `Ctrl-o` を録り、`<C-o>q` で止めた録画は末尾の `Ctrl-o` を外す（決定 7）。
// 4. ステータスの `(insert)` `(insert) VISUAL`・IME の構え（命令を待つ間は閉じ、始め直すと戻る）
//    （決定 1・9）。
// 5. Ex の行（確定と取消）と検索の行（確定と取消）の後・クリックの後に始め直す。全選択・Ctrl+Z・
//    engine の外の道の置き換え（貼り付け・IME の確定）は印を捨てて NORMAL（決定
//    8）。失敗した命令の後も打った鍵は続く（決定 2）。
// 6. 印が残る / 消える鍵の族の表（engine）。vim_resting_from は待ちと due を持ち越し、始め直した
//    後の印は持ち越さない。割り込み・タブの切り替えは消し、入力行の取消は保つ（決定 2・8）。
// 7. `<C-o>.` は INSERT へ入る前の変更、始め直した後の `.` は始め直してから打った分（決定 6）。
#include "Column.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "ImeStance.hpp"
#include "InsertText.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "PlaceCaret.hpp"
#include "Scopes.hpp"
#include "SelectAll.hpp"
#include "Selection.hpp"
#include "SelectionAnchoring.hpp"
#include "StoreVimRegister.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VimCharacter.hpp"
#include "VimEditorView.hpp"
#include "VimInsertRestartPhase.hpp"
#include "VimKey.hpp"
#include "VimKeySource.hpp"
#include "VimMode.hpp"
#include "VimNamedRegisters.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorController;

constexpr std::string_view words = "alpha beta gamma";

// 打った鍵（press_vim_key を 1 つずつ）で流した本文。
[[nodiscard]] std::string typed(std::string_view text, std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, std::string(text));
    vim_replay(editing.controller(), keys);
    return whole_vim_body(editing.controller());
}

// :normal! の側（fixture の再生と同じ press_vim_keys）で流した本文。
[[nodiscard]] std::string normal(std::string_view text, std::string_view keys)
{
    Editing editing;
    open_vim_document(editing, std::string(text));
    vim_normal(editing.controller(), keys);
    return whole_vim_body(editing.controller());
}

void undo_once(EditorController &controller)
{
    vim_replay(controller, "u");
}

void verify_undo_units()
{
    Editing three;
    open_vim_document(three, std::string(words));
    EditorController &controller = three.controller();
    vim_replay(controller, "iab<C-o>xcd<Esc>");
    expect(whole_vim_body(controller) == "abcdlpha beta gamma", "iab<C-o>xcd types around x");
    undo_once(controller);
    expect(whole_vim_body(controller) == "ablpha beta gamma",
           "the first u undoes only what was typed after the restart (Vim feedkeys)");
    undo_once(controller);
    expect(whole_vim_body(controller) == "abalpha beta gamma", "the second u undoes the x");
    undo_once(controller);
    expect(whole_vim_body(controller) == std::string(words),
           "the third u undoes what was typed before Ctrl-o");
    Editing two;
    open_vim_document(two, std::string(words));
    vim_replay(two.controller(), "iab<C-o>lcd<Esc>u");
    expect(whole_vim_body(two.controller()) == "abalpha beta gamma",
           "a motion between makes no unit, so iab<C-o>lcd has two");
    Editing replayed;
    open_vim_document(replayed, std::string(words));
    static_cast<void>(replayed.controller().apply(app::StoreVimRegister{
        'a', core::VimRegister{"iab\x0fxcd\x1b", core::VimRegisterKind::characters}}));
    vim_replay(replayed.controller(), "@a");
    expect(whole_vim_body(replayed.controller()) == "abcdlpha beta gamma",
           "@a replays the keys around Ctrl-o");
    undo_once(replayed.controller());
    expect(whole_vim_body(replayed.controller()) == std::string(words),
           "and one u undoes the whole replay (ADR 0046)");
}

void verify_typed_change()
{
    constexpr std::array<std::array<std::string_view, 4>, 3> cases{{
        {"alpha beta gamma", "iX<C-o>cwY<Esc>Z<Esc>", "XY beta gamma", "XZY beta gamma"},
        {"alpha beta gamma", "wiX<C-o>CY<Esc>Z<Esc>", "alpha XY", "alpha XZY"},
        {"alpha beta gamma", "iX<C-o>vecY<Esc>Z<Esc>", "XY beta gamma", "XZY beta gamma"},
    }};
    for (const auto &[text, keys, as_typed, as_normal] : cases)
    {
        const std::string label(keys);
        expect(typed(text, keys) == as_typed,
               (label + ": typed, the Esc of the change goes back to NORMAL").c_str());
        expect(normal(text, keys) == as_normal,
               (label + ": under :normal!, the Esc of the change restarts INSERT").c_str());
    }
    expect(typed(words, "iX<C-o>iY<Esc>Z<Esc>") == "XYalpha beta gamma",
           "i after Ctrl-o drops the restart, typed or not");
}

[[nodiscard]] std::optional<std::string> register_a(const EditorController &controller)
{
    const auto &value = controller.vim_state().registers.registers.at(0);
    return std::optional{value.value().text};
}

void verify_recording()
{
    Editing recorded;
    open_vim_document(recorded, std::string(words));
    vim_replay(recorded.controller(), "qaiX<C-o>lY<Esc>q");
    expect(register_a(recorded.controller()) == std::optional<std::string>{"iX\x0flY\x1b"},
           "the recording keeps the typed Ctrl-o");
    Editing stopped;
    open_vim_document(stopped, std::string(words));
    vim_replay(stopped.controller(), "qaiX<C-o>qY<Esc>");
    expect(register_a(stopped.controller()) == std::optional<std::string>{"iX"},
           "<C-o>q stops the recording without the trailing Ctrl-o (Vim get_recorded)");
    expect(whole_vim_body(stopped.controller()) == "XYalpha beta gamma",
           "and the q is the one command before INSERT restarts");
}

void verify_label_and_ime()
{
    Editing editing;
    open_vim_document(editing, std::string(words));
    EditorController &controller = editing.controller();
    vim_replay(controller, "iX<C-o>");
    auto frame = controller.frame();
    expect(frame.mode_label == "(insert)" && frame.vim_mode == core::VimMode::normal &&
               frame.ime == app::ImeStance::closed,
           "waiting for the command shows (insert) and closes the IME like NORMAL");
    vim_replay(controller, "v");
    expect(controller.frame().mode_label == "(insert) VISUAL", "Ctrl-o v shows (insert) VISUAL");
    vim_replay(controller, "<Esc>");
    frame = controller.frame();
    expect(frame.mode_label == "INSERT" && frame.vim_mode == core::VimMode::insert &&
               frame.ime == app::ImeStance::as_left,
           "the Esc of VISUAL ends the command and INSERT restarts with the IME as left");
    vim_replay(controller, "<Esc>");
    expect(controller.frame().mode_label == "NORMAL", "one more Esc leaves INSERT");
}

void verify_input_lines()
{
    expect(typed(words, "iX<C-o>:set so=2<CR>Y<Esc>") == "XYalpha beta gamma",
           "an Ex line is one command and INSERT restarts after it");
    expect(typed(words, "iX<C-o>:<Esc>Y<Esc>") == "XYalpha beta gamma",
           "a cancelled Ex line also restarts INSERT");
    expect(typed(words, "iX<C-o>/gam<CR>Y<Esc>") == "Xalpha beta Ygamma",
           "a search restarts INSERT at the match");
    expect(typed(words, "iX<C-o>/gam<Esc>Y<Esc>") == "XYalpha beta gamma",
           "a cancelled search restarts INSERT where it was (Vim scriptin)");
}

void place(EditorController &controller, std::size_t column)
{
    static_cast<void>(controller.apply(
        app::PlaceCaret{core::TextPosition{core::LineNumber{1}, core::Column{column}},
                        core::SelectionAnchoring::collapse}));
}

void verify_click_and_interrupts()
{
    Editing clicked;
    open_vim_document(clicked, std::string(words));
    vim_replay(clicked.controller(), "iX<C-o>");
    place(clicked.controller(), 8);
    expect(clicked.controller().vim_state().mode == core::VimMode::insert,
           "a click after Ctrl-o counts as the command and INSERT restarts");
    vim_replay(clicked.controller(), "Y<Esc>");
    Editing reference;
    open_vim_document(reference, std::string(words));
    vim_replay(reference.controller(), "iX<Esc>");
    place(reference.controller(), 8);
    vim_replay(reference.controller(), "iY<Esc>");
    expect(whole_vim_body(clicked.controller()) == whole_vim_body(reference.controller()),
           "and restarts where the click put the caret");
    Editing undone;
    open_vim_document(undone, std::string(words));
    vim_replay(undone.controller(), "iX<C-o>");
    static_cast<void>(undone.controller().apply(app::HistoryAction{core::HistoryDirection::undo}));
    vim_replay(undone.controller(), "l");
    expect(undone.controller().vim_state().mode == core::VimMode::normal &&
               !core::vim_insert_paused(undone.controller().vim_state()),
           "Ctrl+Z drops the restart and stays in NORMAL");
    Editing selected;
    open_vim_document(selected, std::string(words));
    vim_replay(selected.controller(), "iX<C-o>");
    static_cast<void>(selected.controller().apply(app::SelectAll{}));
    expect(!core::vim_insert_paused(selected.controller().vim_state()),
           "select all drops the restart");
    Editing pasted;
    open_vim_document(pasted, std::string(words));
    vim_replay(pasted.controller(), "iX<C-o>");
    static_cast<void>(pasted.controller().apply(app::InsertText{"Q"}));
    vim_replay(pasted.controller(), "l");
    expect(!core::vim_insert_paused(pasted.controller().vim_state()) &&
               pasted.controller().vim_state().mode == core::VimMode::normal,
           "an edit outside the engine (paste, IME) drops the restart and stays in NORMAL");
}

void verify_failed_command()
{
    expect(typed(words, "AX<C-o>lY<Esc>") == "alpha beta gammaXY",
           "a failed l restarts at the line end and the typed Y follows (Vim feedkeys)");
    expect(typed(words, "iX<C-o>fzY<Esc>") == "XYalpha beta gamma",
           "a failed f restarts INSERT and the typed Y follows");
    expect(normal(words, "iX<C-o>fzY<Esc>") == "Xalpha beta gamma",
           "under :normal! the keys after the failure are dropped");
}

// `iX` の後の `Ctrl-o` の状態（キャレットは X の後ろの a）。
[[nodiscard]] std::pair<core::VimState, core::Selection> paused_state(const core::TextBuffer &text)
{
    const core::Selection caret = core::collapsed_at(core::Offset{1});
    const core::VimEditorView view{text, caret, std::nullopt};
    const core::VimStep entered =
        core::vim_step(empty_vim_state(), view, core::VimKey{core::VimCharacter{U'i'}});
    const core::VimStep paused =
        core::vim_step(entered.next, view, core::VimKey{core::VimSpecialKey::control_o});
    return {paused.next, caret};
}

// 鍵の列を流した後の印の段（空なら印が無い）。view の出どころを流す鍵の出どころにする。
[[nodiscard]] std::optional<core::VimInsertRestartPhase>
phase_after(const core::VimState &state, const core::VimEditorView &view, std::string_view keys)
{
    core::VimState next = state;
    for (const core::VimKey &key : vim_keys_of(keys))
    {
        next = core::vim_step(next, view, key).next;
    }
    if (!next.insert_restart.has_value())
    {
        return std::nullopt;
    }
    return next.insert_restart.value().phase;
}

void verify_mark_table()
{
    const auto text = core::TextBuffer::from_utf8("Xalpha beta gamma").value();
    const auto [state, caret] = paused_state(text);
    expect(state.mode == core::VimMode::normal && core::vim_insert_paused(state) &&
               !core::vim_insert_restart_due(state),
           "Ctrl-o leaves INSERT and waits for one command");
    using Phase = core::VimInsertRestartPhase;
    using Row = std::tuple<std::string_view, core::VimKeySource, std::optional<Phase>>;
    constexpr core::VimKeySource typed_key = core::VimKeySource::typed;
    const std::array<Row, 17> rows{{
        {"l", typed_key, Phase::due},
        {"x", typed_key, Phase::due},
        {"<Esc>", typed_key, Phase::due},
        {"<C-o>", typed_key, Phase::due},
        {"3", typed_key, Phase::waiting},
        {"3l", typed_key, Phase::due},
        {"d", typed_key, Phase::waiting},
        {"\"a", typed_key, Phase::waiting},
        {"v", typed_key, Phase::waiting},
        {"vl<Esc>", typed_key, Phase::due},
        {":", typed_key, Phase::waiting},
        {"/", typed_key, Phase::waiting},
        {"q", typed_key, Phase::waiting},
        {"i", typed_key, std::nullopt},
        {"o", typed_key, std::nullopt},
        {"i", core::VimKeySource::replayed, Phase::waiting},
        {"cw", core::VimKeySource::mapped, Phase::waiting},
    }};
    for (const auto &[keys, source, expected] : rows)
    {
        core::VimEditorView view{text, caret, std::nullopt};
        view.source = source;
        expect(phase_after(state, view, keys) == expected,
               (std::string("the mark after ") + std::string(keys)).c_str());
    }
    const core::VimEditorView typed_view{text, caret, std::nullopt};
    expect(!phase_after(state, typed_view, "cw").has_value(), "a typed cw drops the mark");
}

// 印を持ち越す口・捨てる口と、始め直した後の印（決定 2・8）。
void verify_mark_carriers()
{
    using Phase = core::VimInsertRestartPhase;
    const auto text = core::TextBuffer::from_utf8("Xalpha beta gamma").value();
    const auto [state, caret] = paused_state(text);
    expect(core::vim_resting_from(state, state.unnamed_register).insert_restart.has_value(),
           "vim_resting_from carries the waiting mark");
    expect(core::vim_cancelled_input(state).insert_restart.has_value(),
           "cancelling an input line keeps the mark");
    expect(!core::vim_interrupted(state).insert_restart.has_value(),
           "an interruption drops the mark");
    expect(
        !core::vim_switched_document(state, std::nullopt, std::nullopt).insert_restart.has_value(),
        "switching the tab drops the mark");
    const core::VimEditorView view{text, caret, std::nullopt};
    const core::VimStep restarted = core::vim_restarted_insert(state, view);
    expect(restarted.next.mode == core::VimMode::insert &&
               restarted.next.insert_restart.has_value() &&
               restarted.next.insert_restart.value().phase == Phase::restarted &&
               restarted.next.insert_start.has_value() && restarted.next.insert_start.value().moved,
           "vim_restarted_insert enters INSERT like i with the arrow mark");
    expect(!core::vim_insert_paused(restarted.next) &&
               !core::vim_resting_from(restarted.next, restarted.next.unnamed_register)
                    .insert_restart.has_value(),
           "the restarted mark is not carried out of INSERT");
}

void verify_repeats()
{
    expect(typed(words, "xiab<C-o>.cd<Esc>") == "abcdpha beta gamma",
           "<C-o>. repeats the change before INSERT");
    expect(typed(words, "iX<C-o>lY<Esc>w.") == "XaYlpha Ybeta gamma",
           ". repeats only what was typed after the restart");
}
} // namespace

void verify_vim_insert_normal_scope()
{
    verify_undo_units();
    verify_typed_change();
    verify_recording();
    verify_label_and_ime();
    verify_input_lines();
    verify_click_and_interrupts();
    verify_failed_command();
    verify_mark_table();
    verify_mark_carriers();
    verify_repeats();
}
} // namespace nenenib::tests
