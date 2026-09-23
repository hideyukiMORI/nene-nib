// scope `--vim-macro` の単体テスト（ADR 0042 決定 2・ADR 0046）。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "StoreVimMacro.hpp"
#include "TestSupport.hpp"
#include "VimKey.hpp"
#include "VimMacroRegisters.hpp"
#include "VimMode.hpp"
#include "VimPrefix.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchPattern.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::SelectEditMode;
using nenenib::application::StoreVimMacro;
using nenenib::core::EditMode;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimPrefix;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchPattern;
using nenenib::core::VimState;

// 録画→再生の一気通貫の 1 件（決定 7）。期待値は本物の Vim 9.1 を feedkeys で打った実測
// （Issue #176 の probe 2 節の表）。oracle の :normal! は録画できないので fixture にならない。
struct MacroCase
{
    std::string_view keys;
    std::string_view text;
    std::string_view expected;
    std::size_t line;
    std::size_t column;
    std::string_view register_a;
};

constexpr std::array<MacroCase, 9> recorded_cases{{
    {"qaxq@a", "abcdef", "cdef", 1, 1, "x"},
    {"qa0xjq2@a", "abc\ndef\nghi\njkl", "bc\nef\nhi\njkl", 4, 1, "0xj"},
    {"qaiab<Esc>q@a", "xyz", "aabbxyz", 1, 3, "iab<Esc>"},
    {"qallqqAxq", "abcdefgh", "abdefgh", 1, 3, "llx"},
    {"qaxq@a@@", "abcdefgh", "defgh", 1, 1, "x"},
    {"qallxq@a", "abcde", "abde", 1, 4, "llx"},
    {"qallxq@a.", "abcdefghij", "abdehij", 1, 5, "llx"},
    {"qaxxq@au", "abcdef", "cdef", 1, 1, "xx"},
    {"qaxxqu", "abcdef", "bcdef", 1, 1, "xx"},
}};

[[nodiscard]] const std::vector<VimKey> &macro_keys(const VimState &state, char32_t name)
{
    return state.macros.keys.at(nenenib::core::vim_macro_index(name).value_or(0));
}

void verify_recorded_case(const MacroCase &sample)
{
    Editing editing;
    open_vim_document(editing, std::string(sample.text));
    EditorController &controller = editing.controller();
    vim_replay(controller, sample.keys);
    const std::string name(sample.keys);
    const auto frame = controller.frame();
    expect(whole_vim_body(controller) == sample.expected, (name + ": body").c_str());
    expect(caret_at(frame, sample.line, sample.column), (name + ": caret").c_str());
    expect(macro_keys(controller.vim_state(), U'a') == vim_keys_of(sample.register_a),
           (name + ": register a").c_str());
    expect(!controller.vim_state().macro_recording.has_value(),
           (name + ": the recording is closed").c_str());
}

void verify_macro_round_trips()
{
    for (const MacroCase &sample : recorded_cases)
    {
        verify_recorded_case(sample);
    }
}

// `q` は名前を待ち、名前でない鍵はビープして何も録らない。VISUAL でも録画は同じで、モードは残る。
void verify_macro_waiting()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "q");
    expect(waits_for_prefix(controller.vim_state(), VimPrefix::q), "q waits for a register name");
    vim_replay(controller, "1");
    expect(!controller.vim_state().macro_recording.has_value() &&
               !controller.vim_state().input_wait.has_value(),
           "a digit is no register name; nothing records");
    vim_replay(controller, "@");
    expect(waits_for_prefix(controller.vim_state(), VimPrefix::at), "@ waits for a register name");
    vim_replay(controller, "<Esc>");
    expect(whole_vim_body(controller) == "abcdef", "@<Esc> replays nothing");
    vim_replay(controller, "vqal");
    expect(controller.vim_state().mode == VimMode::visual &&
               controller.vim_state().macro_recording.has_value(),
           "q{name} in VISUAL starts recording and stays in VISUAL");
    vim_replay(controller, "q");
    expect(controller.vim_state().mode == VimMode::visual &&
               macro_keys(controller.vim_state(), U'a') == vim_keys_of("l"),
           "q in VISUAL stops the recording and keeps the selection");
}

// 空のレジスタと直前の無い `@@` は何もしない（決定 3）。
void verify_macro_empty_registers()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "@@x");
    expect(whole_vim_body(controller) == "bcdef" && !controller.vim_state().last_macro.has_value(),
           "@@ without a previous register does nothing");
    vim_replay(controller, "@bx");
    expect(whole_vim_body(controller) == "cdef", "an empty register replays nothing");
}

// 録るのは打った鍵だけ（決定 2）。`@a` と `.` の再生の鍵は積まれず、`@a` `.` の鍵そのものが残る。
// 入力行を開く `/` は積まず、確定した検索が 1 鍵で積まれる。
void verify_macro_records_typed_keys_only()
{
    Editing editing;
    open_vim_document(editing, "alpha beta\nbeta gamma\nbeta");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qaxqqb@a.q");
    expect(macro_keys(controller.vim_state(), U'b') == vim_keys_of("@a."),
           "replayed keys are not recorded; @a and . are");
    vim_replay(controller, "qc/beta<CR>q");
    const std::vector<VimKey> expected{
        VimKey{VimSearchPattern{"beta", VimSearchDirection::forward, std::nullopt}}};
    expect(macro_keys(controller.vim_state(), U'c') == expected,
           "a search is recorded as the one confirmed pattern key");
    vim_replay(controller, "j0@c");
    expect(caret_at(controller.frame(), 3, 1), "the recorded search replays from the caret");
}

// 再生の中の `q` は何もせず次の鍵も待たない（Vim の「レジスタ実行中は q は無効」）。
void verify_macro_q_inside_replay()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, StoreVimMacro{U'a', vim_keys_of("qlx")});
    vim_replay(controller, "@a");
    expect(whole_vim_body(controller) == "acdef" &&
               !controller.vim_state().macro_recording.has_value(),
           "q inside a replay neither records nor waits; l moves and x deletes");
    expect(macro_keys(controller.vim_state(), U'l').empty(), "register l stays empty");
}

// 失敗で残りを捨てる（決定 3）。入れ子の再生の失敗は外側の残りも捨てる。打った鍵は捨てない。
void verify_macro_failure_stops_the_rest()
{
    Editing editing;
    open_vim_document(editing, "ab\ncd");
    EditorController &controller = editing.controller();
    applied(controller, StoreVimMacro{U'a',
                                      {VimKey{VimSearchPattern{"zzz", VimSearchDirection::forward,
                                                               std::nullopt}}}});
    applied(controller, StoreVimMacro{U'b', vim_keys_of("@ax")});
    vim_replay(controller, "@b");
    expect(whole_vim_body(controller) == "ab\ncd", "a failed search in @a stops the x of @b");
    vim_replay(controller, "hx");
    expect(whole_vim_body(controller) == "b\ncd", "a failed typed h does not stop the typed x");
}

// 再帰の上限（決定 4）。終わらない再帰は 100 段で残りを捨て、報せを 1 行出す。
void verify_macro_depth_limit()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, StoreVimMacro{U'a', vim_keys_of("0@a")});
    vim_replay(controller, "@a");
    const auto frame = controller.frame();
    expect(frame.command_message.has_value() &&
               frame.command_message.value().text() == "E132: Macro depth is higher than 100",
           "an endless recursion stops at the depth limit");
    vim_replay(controller, "x");
    expect(whole_vim_body(controller) == "bcdef", "the next typed key runs normally");
}

// 大文字の名前は追記（決定 2）。StoreVimMacro も同じ規則（`:let @A` と同じ）。
void verify_macro_append()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, StoreVimMacro{U'a', vim_keys_of("l")});
    applied(controller, StoreVimMacro{U'A', vim_keys_of("x")});
    expect(macro_keys(controller.vim_state(), U'a') == vim_keys_of("lx"), "A appends to a");
    applied(controller, StoreVimMacro{U'1', vim_keys_of("x")});
    vim_replay(controller, "@a");
    expect(whole_vim_body(controller) == "acdef", "a digit name stores nothing");
}

// 表示値の録画中の名前（決定 8・Issue #180）。`qa` で 'a' を持ち、止める `q` で消える。
// INSERT に入っても録画は続くので値は残る。
void verify_macro_recording_frame()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    expect(!controller.frame().recording.has_value(), "nothing is recorded at first");
    vim_replay(controller, "qa");
    expect(controller.frame().recording == std::optional<char>{'a'}, "qa shows the name a");
    vim_replay(controller, "ix");
    expect(controller.frame().recording == std::optional<char>{'a'}, "INSERT keeps recording");
    vim_replay(controller, "<Esc>q");
    expect(!controller.frame().recording.has_value(), "the stopping q clears the name");
    vim_replay(controller, "qA");
    expect(controller.frame().recording == std::optional<char>{'A'},
           "an appending recording shows the typed capital like Vim");
}

// 通常モードでは Vim の鍵が流れないので、録画中でも名前を出さない（決定 8・Issue #180）。
// Vim へ戻れば同じ録画がまた見える。
void verify_macro_recording_hidden_in_ordinary()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qb");
    applied(controller, SelectEditMode{EditMode::ordinary});
    expect(!controller.frame().recording.has_value(), "ordinary mode shows no recording");
    applied(controller, SelectEditMode{EditMode::vim});
    expect(controller.frame().recording == std::optional<char>{'b'},
           "back in Vim the same recording shows again");
}

[[nodiscard]] bool macro_fixture(const VimFixture &fixture) noexcept
{
    return fixture.macro.has_value();
}
} // namespace

void verify_vim_macro_contracts()
{
    verify_macro_round_trips();
    verify_macro_waiting();
    verify_macro_empty_registers();
    verify_macro_records_typed_keys_only();
    verify_macro_q_inside_replay();
    verify_macro_failure_stops_the_rest();
    verify_macro_depth_limit();
    verify_macro_append();
    verify_macro_recording_frame();
    verify_macro_recording_hidden_in_ordinary();
}

void verify_vim_macro_scope()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (macro_fixture(fixture))
        {
            expect(fixture.name.starts_with("macro-"), "every register fixture is a macro- case");
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 20, "the scope replays the 20 macro fixtures");
    verify_vim_macro_contracts();
}
} // namespace nenenib::tests
