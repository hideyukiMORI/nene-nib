// scope `--vim-macro` の単体テスト（ADR 0042 決定 2・ADR 0046）。
#include "CancelCommand.hpp"
#include "CommandEdit.hpp"
#include "CommandText.hpp"
#include "DisplayLine.hpp"
#include "EditCommand.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "Selection.hpp"
#include "StoreVimRegister.hpp"
#include "SubmitCommand.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimBlockEdit.hpp"
#include "VimCharacter.hpp"
#include "VimClipboardText.hpp"
#include "VimEditorView.hpp"
#include "VimEffect.hpp"
#include "VimInsertAt.hpp"
#include "VimInsertBlock.hpp"
#include "VimInsertString.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimMoveTo.hpp"
#include "VimNamedRegisters.hpp"
#include "VimNoEffect.hpp"
#include "VimNumberedRegisters.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterText.hpp"
#include "VimReplay.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchPattern.hpp"
#include "VimSelect.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimStep.hpp"
#include "VimTestSupport.hpp"
#include "VimViewport.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::SelectEditMode;
using nenenib::application::StoreVimRegister;
using nenenib::core::collapsed_at;
using nenenib::core::EditMode;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::Selection;
using nenenib::core::TextBuffer;
using nenenib::core::vim_clipboard_loaded;
using nenenib::core::vim_reads_clipboard;
using nenenib::core::vim_register_of_clipboard;
using nenenib::core::vim_step;
using nenenib::core::VimBlockEdit;
using nenenib::core::VimEditorView;
using nenenib::core::VimInsertAt;
using nenenib::core::VimInsertBlock;
using nenenib::core::VimInsertString;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimMoveTo;
using nenenib::core::VimNoEffect;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimReplay;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchPattern;
using nenenib::core::VimSelect;
using nenenib::core::VimSpecialKey;
using nenenib::core::VimState;
using nenenib::core::VimStep;
using nenenib::core::VimViewport;

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

[[nodiscard]] const VimRegister &named_register(const VimState &state, char32_t name)
{
    return state.registers.registers.at(nenenib::core::vim_register_index(name).value_or(0))
        .value();
}

// レジスタの本文を再生と同じ写しで鍵列に戻した値（ADR 0048 の決定 7）。
[[nodiscard]] std::vector<VimKey> macro_keys(const VimState &state, char32_t name)
{
    return nenenib::core::vim_keys_of_text(named_register(state, name).text);
}

// 文字単位の本文を置く意図（`:let @a = "…"` に当たる）。
[[nodiscard]] StoreVimRegister stored_text(char name, std::string_view keys)
{
    return StoreVimRegister{name, VimRegister{nenenib::core::vim_register_text(vim_keys_of(keys)),
                                              VimRegisterKind::characters}};
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
    vim_replay(controller, "-");
    expect(!controller.vim_state().macro_recording.has_value() &&
               !controller.vim_state().input_wait.has_value(),
           "- is no recording name; nothing records");
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

// `q` の後の Esc は鳴らさずに打ち消し、後ろの鍵が効く（#1369・ADR 0048 の追補 17〜19）。oracle は
// `q` を fixture に入れられないので、同じ run_vim で測った値（Vim 9.1・`-es -u NONE`・
// D:/NeNeNib/outputs/20261111/1369-q-escape/result.json）を :normal! の意味（vim_normal）で流す。
using QEscapeCase =
    std::tuple<std::string_view, std::string_view, std::string_view, std::size_t, std::size_t>;
constexpr std::array<QEscapeCase, 6> q_escape_cases{{
    {"abcdef", "q<Esc>x", "bcdef", 1, 1},
    {"abcdef", "3q<Esc>x", "bcdef", 1, 1},
    {"abcdef", "vq<Esc>ld", "cdef", 1, 1},
    {"abcdef\nab\nabcdef", "$jq<Esc>j", "abcdef\nab\nabcdef", 3, 6},
    {"abc", "\"ayllyl\"aq<Esc>p", "abbc", 1, 3},
    {"abcdef", "xq<Esc>.", "cdef", 1, 1},
}};

void verify_macro_escape_cancels()
{
    for (const auto &[text, keys, body, line, column] : q_escape_cases)
    {
        Editing editing;
        open_vim_document(editing, std::string(text));
        EditorController &controller = editing.controller();
        vim_normal(controller, keys);
        const auto frame = controller.frame();
        const std::string name = "q<Esc> cancels and the next key runs: " + std::string(keys);
        expect(whole_vim_body(controller) == body && frame.caret.position.line.value == line &&
                   vim_byte_column(std::string(body), frame.caret.position) == column &&
                   !controller.vim_state().macro_recording.has_value() &&
                   !controller.vim_state().input_wait.has_value(),
               name.c_str());
    }
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
    expect(named_register(controller.vim_state(), U'c').text == "/beta\r",
           "a search is recorded as the one confirmed pattern key and stored as /beta<CR>");
    vim_replay(controller, "j0@c");
    expect(caret_at(controller.frame(), 3, 1), "the recorded search replays from the caret");
}

// `:` の行も打った鍵として録る（#1528）。期待値は Vim 9.1 で feedkeys(…, 'xt') を打った実測
// （`qa:set so=2<CR>jq` の a は `:set so=2^Mj`・`<BS>` `<Left>` `<Del>` `<Right>` `<Home>` `<End>`
// は `<80>kb` `<80>kl` `<80>kD` `<80>kr` `<80>kh` `<80>@7`・`<Tab>` は文字の Tab・`<Up>` は
// `<80>ku`・ 取消の `<Esc>`
// も積む）。窓が送る入力行の意図（CommandText・EditCommand・SubmitCommand・ CancelCommand）と
// fixture の harness の鍵の口（vim_replay）の両方を見る。
void verify_macro_records_ex_line()
{
    using nenenib::application::CancelCommand;
    using nenenib::application::CommandText;
    using nenenib::application::EditCommand;
    using nenenib::application::SubmitCommand;
    using nenenib::core::CommandEdit;
    Editing editing;
    open_vim_document(editing, "abc\ndef\nghi\njkl");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qa:");
    applied(controller, CommandText{"set so=3"});
    applied(controller, EditCommand{CommandEdit::backspace});
    applied(controller, CommandText{"2"});
    applied(controller, EditCommand{CommandEdit::left});
    applied(controller, EditCommand{CommandEdit::right});
    applied(controller, EditCommand{CommandEdit::home});
    applied(controller, EditCommand{CommandEdit::end});
    applied(controller, SubmitCommand{});
    vim_replay(controller, "jq");
    expect(named_register(controller.vim_state(), U'a').text ==
               ":set so=3\xC2\x80kb2\xC2\x80kl\xC2\x80kr\xC2\x80kh\xC2\x80@7\rj",
           "the window's input-line intents are recorded as the typed keys");
    expect(controller.frame().settings.scrolloff == 2, "the recorded line ran while recording");

    vim_replay(controller, "qb:noh");
    applied(controller, CancelCommand{});
    vim_replay(controller, "q");
    expect(named_register(controller.vim_state(), U'b').text == ":noh\x1b",
           "a cancelling Esc is recorded too");

    vim_replay(controller, "qc:set so=1<CR>q");
    expect(named_register(controller.vim_state(), U'c').text == ":set so=1\r",
           "the harness keys record the same way");

    vim_replay(controller, "qd:");
    applied(controller, EditCommand{CommandEdit::history_previous});
    applied(controller, EditCommand{CommandEdit::complete_next});
    applied(controller, EditCommand{CommandEdit::complete_previous});
    applied(controller, CancelCommand{});
    vim_replay(controller, "q");
    expect(named_register(controller.vim_state(), U'd').text == ":\xC2\x80ku\t\x1b",
           "history Up is <Up>, Tab is a Tab and Shift+Tab is not recorded");

    vim_replay(controller, "qe/def<CR>q");
    expect(named_register(controller.vim_state(), U'e').text == "/def\r",
           "a search is still the one confirmed pattern key");
}

// VISUAL の `:` の行も録る（#1528・レビュー V-1547 の反例・Vim 9.1 の feedkeys の実測
// D:/NeNeNib/scripts/20261111/c10/p6.vim）: `qavj:set so=2<CR>xq` の a は `vj:set
// so=2^Mx`。録画中の
// `:'<,'>set so=2` は E481 で、打った `x` は流れて 1 行目が `bc` になる。`@a` は同じ E481 で
// 打ち切られ、`x` は流れない（本文は `bc` `def` `ghi` のまま）。
void verify_macro_records_visual_ex_line()
{
    Editing editing;
    open_vim_document(editing, "abc\ndef\nghi");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qavj:set so=2<CR>xq");
    expect(named_register(controller.vim_state(), U'a').text == "vj:set so=2\rx",
           "the VISUAL : line is recorded as the typed keys");
    expect(whole_vim_body(controller) == "bc\ndef\nghi", "the typed x after E481 still runs");
    vim_replay(controller, "@a");
    expect(whole_vim_body(controller) == "bc\ndef\nghi" &&
               controller.vim_state().mode == VimMode::normal,
           "@a stops at E481 and its x does not run");
}

// 再生と fixture の鍵の `<Up>` は窓と同じ履歴の前（Vim 9.1 の実測: 打った `:set so=4<CR>`
// `:set so=1<CR>` の後の `let @c = ":\<Up>\<Up>\r"` と `@c` で so=4）。録画の中の `:` の行は
// 再生しても録画に積まない（#1528）。
void verify_macro_replays_ex_line()
{
    Editing editing;
    open_vim_document(editing, "abc\ndef\nghi\njkl");
    EditorController &controller = editing.controller();
    vim_replay(controller, ":set so=4<CR>:set so=1<CR>");
    applied(controller, StoreVimRegister{'c', VimRegister{":\xC2\x80ku\xC2\x80ku\r",
                                                          VimRegisterKind::characters}});
    vim_replay(controller, "@c");
    expect(controller.frame().settings.scrolloff == 4, "<Up> in a replayed : line recalls history");

    applied(controller, stored_text('a', ":set so=2<CR>j"));
    vim_replay(controller, "qb@aq");
    expect(named_register(controller.vim_state(), U'b').text == "@a" &&
               controller.frame().settings.scrolloff == 2 && caret_at(controller.frame(), 2, 1),
           "a : line replayed while recording runs and only @a is recorded");
}

// 再生の中の `q` は何もせず次の鍵も待たない（Vim の「レジスタ実行中は q は無効」）。
void verify_macro_q_inside_replay()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', "qlx"));
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
    applied(controller, stored_text('a', "/zzz<CR>"));
    applied(controller, stored_text('b', "@ax"));
    vim_replay(controller, "@b");
    expect(whole_vim_body(controller) == "ab\ncd", "a failed search in @a stops the x of @b");
    vim_replay(controller, "hx");
    expect(whole_vim_body(controller) == "b\ncd", "a failed typed h does not stop the typed x");
}

// 行に収まらない `3rz` は置き換える文字を 1 鍵読んでから失敗する（nv_replace・#634）。
// 録画には打った鍵が残り、`.` の記録は前の変更のまま。再生は失敗で残りを捨てる。
void verify_macro_records_failed_replace()
{
    Editing editing;
    open_vim_document(editing, "ab\ncd\nef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "xqa3rzjq");
    expect(whole_vim_body(controller) == "b\ncd\nef" &&
               macro_keys(controller.vim_state(), U'a') == vim_keys_of("3rzj"),
           "a failed 3rz changes nothing and is recorded as typed");
    expect(dot_record_is(controller.vim_state(), std::nullopt, "x"),
           "a failed 3rz keeps the last change for .");
    vim_replay(controller, ".");
    expect(whole_vim_body(controller) == "b\nd\nef", ". repeats the x before the failed 3rz");
    vim_replay(controller, "@a");
    expect(caret_at(controller.frame(), 2, 1), "the replayed 3rz fails and drops the recorded j");
}

// 再帰の上限（決定 4）。終わらない再帰は 100 段で残りを捨て、報せを 1 行出す。
void verify_macro_depth_limit()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', "0@a"));
    vim_replay(controller, "@a");
    const auto frame = controller.frame();
    expect(frame.command_message.has_value() &&
               frame.command_message.value().text() == "E132: Macro depth is higher than 100",
           "an endless recursion stops at the depth limit");
    vim_replay(controller, "x");
    expect(whole_vim_body(controller) == "bcdef", "the next typed key runs normally");
}

// 鍵の数の上限（#1351・施主決定 D75）。製品の上限 100,000 を Debug で流すと 1 件 80 秒
// かかるので、limit_replay_keys で小さい上限を置いて境を確かめる（製品の値は static_assert）。
// c が `3@ax` なら、`@c` が流す鍵は `3` `@` `a` と a の 3 周（`0` を 3 回）と最後の `x` で、
// `x` は 7 鍵目。上限 7 なら流れ、上限 6 なら捨てて 1 行知らせる（Vim は止まらず `x` まで
// 流すので本文が `b` になる・ここが Vim と違う最小の形）。時計は読まない。mapped なら表に
// `nnoremap Q x` を置いて c を `3@aQ` にし、表に行があるときの 2 本目のループでも同じ境を見る
// （`Q` は右辺 `x` の 1 歩で 7 歩目）。
[[nodiscard]] std::pair<std::string, std::optional<std::string>> limited_replay(std::size_t limit,
                                                                                bool mapped)
{
    Editing editing;
    open_vim_document(editing, "ab");
    EditorController &controller = editing.controller();
    controller.limit_replay_keys(limit);
    if (mapped)
    {
        static_cast<void>(run_ex(controller, "nnoremap Q x"));
    }
    applied(controller, stored_text('a', "0"));
    applied(controller, stored_text('c', mapped ? "3@aQ" : "3@ax"));
    vim_replay(controller, "@c");
    const auto message = controller.frame().command_message;
    return {whole_vim_body(controller),
            message.has_value() ? std::optional<std::string>{std::string{message.value().text()}}
                                : std::nullopt};
}

void verify_macro_key_limit()
{
    static_assert(nenenib::core::vim_replay_key_limit == 100000U);
    const std::optional<std::string> expected_notice{
        "再生を 100,000 鍵で止めました（残りは捨てました）"};
    for (const bool mapped : {false, true})
    {
        const auto [inside, quiet] = limited_replay(7U, mapped);
        expect(inside == "b" && !quiet.has_value(), "the replayed key at the limit still runs");
        const auto [over, notice] = limited_replay(6U, mapped);
        expect(over == "ab" && notice == expected_notice,
               "the replayed key past the limit is dropped with one notice line");
    }
    Editing editing;
    open_vim_document(editing, "ab");
    EditorController &controller = editing.controller();
    controller.limit_replay_keys(6U);
    applied(controller, stored_text('a', "0"));
    vim_replay(controller, "999999999@ax");
    expect(whole_vim_body(controller) == "b", "a typed key after the stopped replay runs normally");
}

// 巨大な回数の `@`（#1341・ADR 0046 の決定 3 の追補）。鍵の列を回数ぶん並べず 1 周ずつ積むので、
// 鍵が失敗した周で止まり、積んだ周の数は回数に比例しない（時計は読まない）。b は `xl`。
[[nodiscard]] std::size_t huge_rounds(std::string text, std::string_view macro,
                                      std::string_view keys, std::string_view body)
{
    Editing editing;
    open_vim_document(editing, std::move(text));
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', macro));
    applied(controller, stored_text('b', "xl"));
    const std::size_t before = controller.replay_rounds();
    vim_replay(controller, keys);
    expect(whole_vim_body(controller) == body, "the huge count stops at the failing round");
    return controller.replay_rounds() - before;
}

// 周の最後の鍵を流す前に次の周を積むので、数は流した周より 1 つ多い。`xl` の `l` は 3 周目の
// 行末で失敗する（`x` は空の行でも失敗しないので、`x` だけのマクロは Vim も回数ぶん回る・実測）。
void verify_macro_huge_count_stops()
{
    expect(huge_rounds("abcdef\nghi", "xl", "999999999@a", "bdf\nghi") == 4U,
           "999999999@a of xl stops at the line end in the third round");
    expect(huge_rounds("a\nb\nc", "j", "999999999@a", "a\nb\nc") == 4U,
           "999999999@a of j stops at the last line");
    expect(huge_rounds("abcdef", "xl", "@a999999999@@", "bdf") == 4U,
           "999999999@@ repeats the rounds of the last macro");
    // 入れ子の `2@b` は a の周ごとに b の 2 周を積む。a の 2 周目の b の 1 周目で `l` が失敗する
    // （a の 3 周・b の 4 周を積む）。
    expect(huge_rounds("abcdef", "2@b", "999999999@a", "bdf") == 7U,
           "a nested 2@b inside 999999999@a stops with the outer rounds");
}

// 周を 1 つずつ積んでも、流れる順は回数ぶん繋いだ鍵の列と同じ（入れ子の再生は周の続きより前）。
void verify_macro_rounds_keep_the_order()
{
    const std::array<std::array<std::string_view, 3>, 3> cases{{
        {"@bl", "4@a", "xlxlxlxl"},
        {"lx", "3@a", "lxlxlx"},
        {"2@bj", "2@a", "xxjxxj"},
    }};
    for (const auto &[macro, keys, flat] : cases)
    {
        Editing rounds;
        open_vim_document(rounds, "abcdefgh\nijklmnop\nqrstuvwx");
        applied(rounds.controller(), stored_text('a', macro));
        applied(rounds.controller(), stored_text('b', "x"));
        vim_replay(rounds.controller(), keys);
        Editing typed;
        open_vim_document(typed, "abcdefgh\nijklmnop\nqrstuvwx");
        vim_replay(typed.controller(), flat);
        expect(whole_vim_body(rounds.controller()) == whole_vim_body(typed.controller()) &&
                   rounds.controller().frame().caret.position ==
                       typed.controller().frame().caret.position,
               "the rounds run in the order of the keys repeated");
    }
}

// 大文字の名前は追記（決定 2）。StoreVimRegister も録画の追記と同じ規則（ADR 0048 の決定 4）。
void verify_macro_append()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', "l"));
    applied(controller, stored_text('A', "x"));
    expect(macro_keys(controller.vim_state(), U'a') == vim_keys_of("lx"), "A appends to a");
    vim_replay(controller, "@a");
    expect(whole_vim_body(controller) == "acdef", "@a runs the appended lx");
}

[[nodiscard]] const VimRegister &numbered_register(const VimState &state, std::size_t index)
{
    return state.numbered.registers.at(index).value();
}

// `q{0-9}` はその数字へ文字単位で録る（ADR 0050 の決定 6・probe 2 の Q10）。追記も繰り下がりも
// 無く、無名は変えない。`q-` は録らずにビープし（`"-` は `x` が置いた `b` のまま）、次の `l` は
// ふつうの移動になる。
void verify_macro_numbered_recording()
{
    Editing editing;
    open_vim_document(editing, "abcdef\nsecond");
    EditorController &controller = editing.controller();
    vim_replay(controller, "yyq0lq");
    const VimState &zero = controller.vim_state();
    expect(numbered_register(zero, 0).text == "l" &&
               numbered_register(zero, 0).kind == VimRegisterKind::characters &&
               zero.unnamed_register.value().text == "abcdef\n" &&
               zero.unnamed_register.value().kind == VimRegisterKind::lines,
           "q0lq records l into 0 and leaves the unnamed yank");
    vim_replay(controller, "jddq1lq");
    const VimState &one = controller.vim_state();
    expect(numbered_register(one, 1).text == "l" && numbered_register(one, 2).text.empty() &&
               one.unnamed_register.value().text == "second\n",
           "q1lq replaces 1 without shifting 1 into 2 and leaves the unnamed delete");
    vim_replay(controller, "q9xq");
    expect(numbered_register(controller.vim_state(), 9).text == "x" &&
               whole_vim_body(controller) == "acdef",
           "q9xq records x into 9");
    vim_replay(controller, "q-l");
    expect(!controller.vim_state().macro_recording.has_value() &&
               controller.vim_state().small_delete.value().text == "b" &&
               caret_at(controller.frame(), 1, 3),
           "q- refuses and the next l moves");
}

// 数字と `-` へ本文を置く意図は置き換え（ADR 0050 の決定 7・`:let @0 = "…"`）。`@0` `@9` `@-`
// は置いた本文を鍵として実行し、`@@` は数字も `-` も覚える（決定 6）。
void verify_macro_numbered_stored()
{
    Editing editing;
    open_vim_document(editing, "abcdefgh");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('0', "l"));
    applied(controller, stored_text('0', "x"));
    applied(controller, stored_text('9', "lx"));
    applied(controller, stored_text('-', "lrZ"));
    const VimState &stored = controller.vim_state();
    expect(numbered_register(stored, 0).text == "x" && numbered_register(stored, 9).text == "lx" &&
               stored.small_delete.value().text == "lrZ" &&
               stored.small_delete.value().kind == VimRegisterKind::characters,
           "0 9 and - are replaced");
    vim_replay(controller, "@-");
    expect(whole_vim_body(controller) == "aZcdefgh" &&
               controller.vim_state().last_macro == std::optional<char>{'-'},
           "@- runs lrZ and @@ remembers -");
    vim_replay(controller, "@@");
    expect(whole_vim_body(controller) == "aZZdefgh", "@@ after @- runs - again");
    vim_replay(controller, "0@0");
    expect(whole_vim_body(controller) == "ZZdefgh", "@0 runs x");
    vim_replay(controller, "@9");
    expect(whole_vim_body(controller) == "Zdefgh", "@9 runs lx");
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

// 鍵列 ↔ 本文の往復（ADR 0048 の決定 7）。特殊鍵の全列挙子と文字（ASCII・日本語・U+0080 単独）
// は戻り、`\x08` は backspace に読み、確定した検索は `/` か `?` ＋ pattern ＋ `\r` の片道。
void verify_register_text_round_trip()
{
    const std::vector<VimKey> keys{
        VimKey{nenenib::core::VimCharacter{U'a'}},
        VimKey{nenenib::core::VimCharacter{U'\u65E5'}},
        VimKey{nenenib::core::VimCharacter{U'\u0080'}},
        VimKey{nenenib::core::VimCharacter{U'z'}},
        VimKey{nenenib::core::VimCharacter{U'\n'}},
        VimKey{VimSpecialKey::escape},
        VimKey{VimSpecialKey::enter},
        VimKey{VimSpecialKey::backspace},
        VimKey{VimSpecialKey::arrow_left},
        VimKey{VimSpecialKey::arrow_right},
        VimKey{VimSpecialKey::arrow_up},
        VimKey{VimSpecialKey::arrow_down},
        VimKey{VimSpecialKey::control_r},
        VimKey{VimSpecialKey::home},
        VimKey{VimSpecialKey::end},
        VimKey{VimSpecialKey::page_up},
        VimKey{VimSpecialKey::page_down},
        VimKey{VimSpecialKey::control_d},
        VimKey{VimSpecialKey::control_u},
        VimKey{VimSpecialKey::control_f},
        VimKey{VimSpecialKey::control_b},
        VimKey{VimSpecialKey::control_v},
    };
    expect(nenenib::core::vim_keys_of_text(nenenib::core::vim_register_text(keys)) == keys,
           "every special key and character survives the round trip");
    expect(nenenib::core::vim_register_text(vim_keys_of("iab<Esc>")) == "iab\x1b",
           "Esc is the byte 0x1b");
    expect(nenenib::core::vim_register_text(vim_keys_of("<CR>")) == "\r", "CR is the byte 0x0d");
    expect(nenenib::core::vim_register_text(vim_keys_of("<Home><End>")) == "\xC2\x80kh\xC2\x80@7",
           "Home and End are U+0080 followed by Vim's two termcap letters");
    const std::vector<VimKey> searches{
        VimKey{VimSearchPattern{"ab", VimSearchDirection::forward, std::nullopt}},
        VimKey{VimSearchPattern{"cd", VimSearchDirection::backward, std::nullopt}}};
    expect(nenenib::core::vim_register_text(searches) == "/ab\r?cd\r",
           "a confirmed search is the prefix, the pattern and CR");
    expect(nenenib::core::vim_keys_of_text("a\x08") ==
               std::vector<VimKey>{VimKey{nenenib::core::VimCharacter{U'a'}},
                                   VimKey{VimSpecialKey::backspace}},
           "Ctrl-H reads as backspace");
    expect(nenenib::core::vim_keys_of_text("\xC2\x80zz") ==
               std::vector<VimKey>{VimKey{nenenib::core::VimCharacter{U'\u0080'}},
                                   VimKey{nenenib::core::VimCharacter{U'z'}},
                                   VimKey{nenenib::core::VimCharacter{U'z'}}},
           "U+0080 without a known key after it is a character");
}

// 録画は本文として置かれる（ADR 0048 の決定 6）。種類は文字単位、特殊鍵は Vim のバイト。
// `qA` は行単位の本文の末尾の改行の手前へ繋ぎ種類を保つ（probe Q11 の `abcdefx\n`）。
void verify_recording_is_text()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qaxq");
    expect(named_register(controller.vim_state(), U'a').text == "x" &&
               named_register(controller.vim_state(), U'a').kind == VimRegisterKind::characters,
           "qaxq stores the characterwise text x");
    vim_replay(controller, "qbi<Home><Esc>q");
    expect(named_register(controller.vim_state(), U'b').text == "i\xC2\x80kh\x1b",
           "special keys are stored as Vim's bytes");
    applied(controller, StoreVimRegister{'c', VimRegister{"abcdef\n", VimRegisterKind::lines}});
    vim_replay(controller, "qCxq");
    expect(named_register(controller.vim_state(), U'c').text == "abcdefx\n" &&
               named_register(controller.vim_state(), U'c').kind == VimRegisterKind::lines,
           "qA appends before the trailing newline of a linewise register and keeps the kind");
    vim_replay(controller, "qdq");
    expect(named_register(controller.vim_state(), U'd').text.empty() &&
               named_register(controller.vim_state(), U'd').kind == VimRegisterKind::characters,
           "an empty recording stores an empty characterwise text");
}

// 再生は窓の鍵と同じ口を通る（ADR 0048 の決定 8・9）。本文の `/ab\r` は入力行を経て確定し、
// INSERT の中の `/cd\r` は文字のまま入り、`lines` の末尾の `\n` は `j` と同じ 1 行の移動になる。
// 期待値は本物の Vim 9.1 で setreg の後に feedkeys("@a", 'xt') を打った実測。
void verify_replay_through_input_line()
{
    Editing searching;
    open_vim_document(searching, "one ab two\nab end");
    EditorController &search = searching.controller();
    applied(search, stored_text('a', "/ab<CR>x"));
    vim_replay(search, "@a");
    expect(whole_vim_body(search) == "one b two\nab end" && caret_at(search.frame(), 1, 5) &&
               !search.command_line_active(),
           "the stored /ab<CR> confirms through the input line and x acts on the match");

    Editing inserting;
    open_vim_document(inserting, "hello");
    EditorController &insert = inserting.controller();
    applied(insert, stored_text('a', "iab/cd<CR><Esc>"));
    vim_replay(insert, "@a");
    expect(whole_vim_body(insert) == "ab/cd\nhello" && caret_at(insert.frame(), 2, 1) &&
               insert.vim_state().mode == VimMode::normal,
           "a / inside INSERT is typed as text, not a search");

    Editing lines;
    open_vim_document(lines, "jj\naaa\nbbb\nccc\nddd");
    EditorController &linewise = lines.controller();
    applied(linewise, StoreVimRegister{'a', VimRegister{"jj\n", VimRegisterKind::lines}});
    vim_replay(linewise, "j@a");
    expect(whole_vim_body(linewise) == "jj\naaa\nbbb\nccc\nddd" && caret_at(linewise.frame(), 5, 1),
           "the trailing newline of a lines register moves down one more line");
}

// 再生の中の入力行の鍵（ADR 0048 の決定 8 の改訂）。`<Esc>` は取消ではなく確定（Vim の
// c_<Esc>）で、矢印・Home / End・`<BS>` は窓と同じ EditCommand の写しで入力行を編集する。
// keys はレジスタ a に置く本文（Vim のバイト・`<80>` は U+0080）で、再生の後も a は変わらない。
// 期待値は本物の Vim 9.1 で setreg の後に feedkeys("@a", 'xt') を打った実測。
constexpr std::array<MacroCase, 4> input_line_cases{{
    {"/ab\x1bx", "one ab", "one b", 1, 5, "/ab\x1bx"},
    {"/ac\xC2\x80klb\rx", "one ac abc", "one ac bc", 1, 8, "/ac\xC2\x80klb\rx"},
    {"/b\xC2\x80kha\xC2\x80@7z\rx", "xb ab abz", "xb ab bz", 1, 7, "/b\xC2\x80kha\xC2\x80@7z\rx"},
    {"/ab\xC2\x80kbc\rx", "one ab ac", "one ab c", 1, 8, "/ab\xC2\x80kbc\rx"},
}};

// 録画と `"` の読み書きが同じ表を通る（ADR 0048 の決定 6・11）。期待値は本物の Vim 9.1 で
// feedkeys(…, 'xt') を打った実測（録画は `:normal!` では観測できない）。`<NL>` は本文の改行。
constexpr std::array<MacroCase, 4> recording_register_cases{{
    {"qaxq\"ap", "abcdef", "bxcdef", 1, 2, "x"},
    {"qaiab<Esc>q\"ap", "xyz", "abiab\x1bxyz", 1, 6, "iab<Esc>"},
    {"\"ayyqAxq", "abcdef", "bcdef", 1, 1, "abcdefx<NL>"},
    {"qaxq\"Ayy", "abcdef", "bcdef", 1, 1, "x<NL>bcdef<NL>"},
}};

// 上の表と同じ順の a の種類と無名レジスタの本文。
using RegisterOutcome = std::pair<VimRegisterKind, std::string_view>;
constexpr std::array<RegisterOutcome, 4> recording_register_outcomes{{
    {VimRegisterKind::characters, "a"},
    {VimRegisterKind::characters, ""},
    {VimRegisterKind::lines, "a"},
    {VimRegisterKind::lines, "x\nbcdef\n"},
}};

void verify_replayed_input_line(const MacroCase &sample)
{
    Editing editing;
    open_vim_document(editing, std::string(sample.text));
    EditorController &controller = editing.controller();
    applied(controller, StoreVimRegister{'a', VimRegister{std::string(sample.keys),
                                                          VimRegisterKind::characters}});
    vim_replay(controller, "@a");
    const std::string name(sample.expected);
    expect(whole_vim_body(controller) == sample.expected, (name + ": body").c_str());
    expect(caret_at(controller.frame(), sample.line, sample.column), (name + ": caret").c_str());
    expect(named_register(controller.vim_state(), U'a').text == sample.register_a,
           (name + ": register a").c_str());
    expect(!controller.command_line_active(), (name + ": the input line is closed").c_str());
}

void verify_replay_input_line_keys()
{
    for (const MacroCase &sample : input_line_cases)
    {
        verify_replayed_input_line(sample);
    }
    Editing typed;
    open_vim_document(typed, "one ab");
    EditorController &window = typed.controller();
    vim_replay(window, "/ab<Esc>x");
    expect(whole_vim_body(window) == "ne ab" && !window.command_line_active(),
           "a typed Esc still cancels the input line");
}

// `qa<Left>q"ap`: 録った `<Left>` は U+0080 `kl` の本文として貼られ、描画は Vim と同じ `<80>kl`。
void verify_recorded_left_pasted()
{
    Editing editing;
    open_vim_document(editing, "abc");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qa");
    static_cast<void>(controller.press_vim_key(VimKey{VimSpecialKey::arrow_left}));
    vim_replay(controller, "q\"ap");
    const VimState &state = controller.vim_state();
    expect(whole_vim_body(controller) == "a\xC2\x80klbc" && caret_at(controller.frame(), 1, 4),
           "qa<Left>q\"ap pastes U+0080 kl after the caret");
    expect(named_register(state, U'a').text == "\xC2\x80kl" &&
               named_register(state, U'a').kind == VimRegisterKind::characters,
           "the recorded Left is the characterwise text U+0080 kl");
    expect(nenenib::core::display_line(whole_vim_body(controller)).text == "a<80>klbc",
           "a pasted Left is drawn as <80>kl like Vim");
}

void verify_recording_meets_registers()
{
    for (std::size_t index = 0; index < recording_register_cases.size(); ++index)
    {
        const MacroCase &sample = recording_register_cases.at(index);
        verify_recorded_case(sample);
        Editing editing;
        open_vim_document(editing, std::string(sample.text));
        vim_replay(editing.controller(), sample.keys);
        const VimState &state = editing.controller().vim_state();
        const RegisterOutcome &outcome = recording_register_outcomes.at(index);
        expect(named_register(state, U'a').kind == outcome.first &&
                   state.unnamed_register.value().text == outcome.second,
               (std::string(sample.keys) + ": register kinds").c_str());
    }
    verify_recorded_left_pasted();
}

// `@"` は無名レジスタの本文を鍵として実行し、`@@` はそれを繰り返す（ADR 0048 の決定 6）。
// 未使用のレジスタと空の本文はビープして何もしない。
void verify_macro_unnamed_and_uninitialized()
{
    Editing editing;
    open_vim_document(editing, "lxabc");
    EditorController &controller = editing.controller();
    vim_replay(controller, "2yl@\"");
    expect(whole_vim_body(controller) == "labc", "@\" runs the unnamed text lx");
    expect(controller.vim_state().last_macro == std::optional<char>{'"'}, "@@ remembers @\"");
    vim_replay(controller, "@@");
    expect(whole_vim_body(controller) == "lbc",
           "@@ after @\" runs the unnamed register again, which x has made x");
    vim_replay(controller, "@e");
    expect(whole_vim_body(controller) == "lbc", "an uninitialized register replays nothing");
    vim_replay(controller, "qfq@f");
    expect(whole_vim_body(controller) == "lbc", "an empty register replays nothing");
}

// 矩形が絡む `"A` の追記は書かずにビープする（ADR 0048 の決定 4・Vim は繋ぐ・後続）。矩形で取って
// 行単位のレジスタへ追記しても、矩形のレジスタへ行を追記しても、名前つきも無名も変わらない。
// VISUAL の中の拒否は選択を保ったまま残る。
void verify_register_block_append_refused()
{
    Editing editing;
    open_vim_document(editing, "abcd\nefgh");
    EditorController &controller = editing.controller();
    vim_replay(controller, "\"ayy<C-v>j\"Ay");
    const VimState &into_lines = controller.vim_state();
    expect(named_register(into_lines, U'a').text == "abcd\n" &&
               named_register(into_lines, U'a').kind == VimRegisterKind::lines &&
               into_lines.unnamed_register.value().text == "abcd\n" &&
               into_lines.mode == VimMode::visual_block,
           "a block yank appended to a lines register is refused and the block selection stays");
    vim_replay(controller, "<Esc>gg<C-v>j\"by\"Byy");
    const VimState &into_block = controller.vim_state();
    expect(named_register(into_block, U'b').kind == VimRegisterKind::block &&
               named_register(into_block, U'b').text == "a\ne" &&
               into_block.unnamed_register.value().kind == VimRegisterKind::block &&
               whole_vim_body(controller) == "abcd\nefgh" && into_block.mode == VimMode::normal,
           "a line yank appended to a block register is refused and both registers stay");
}

// ---------------------------------------------------------------- `"+` `"*`（ADR 0051）の core
// の側

// 本文とキャレットを固定して core の vim_step だけへ鍵を流す（決定 9 のうち controller を要らない
// 契約）。効果は本文へ写さないので、本文を変えない鍵の列か、最後の 1 鍵の効果だけを見る。
[[nodiscard]] VimStep core_steps(const VimState &state, const TextBuffer &text,
                                 const Selection &selection, std::string_view keys)
{
    const VimEditorView view{text, selection, VimViewport{LineNumber{1}, vim_visible_lines, 0}};
    VimStep step{state, VimNoEffect{}};
    for (const VimKey &key : vim_keys_of(keys))
    {
        step = vim_step(step.next, view, key);
    }
    return step;
}

[[nodiscard]] TextBuffer buffer_of(std::string_view text)
{
    auto buffer = TextBuffer::from_utf8(text);
    expect(buffer.has_value(), "the clipboard sample parses");
    return std::move(buffer).value();
}

[[nodiscard]] bool register_is(const VimRegister &value, std::string_view text,
                               VimRegisterKind kind)
{
    return value.text == text && value.kind == kind;
}

[[nodiscard]] bool register_unused(const VimRegister &value)
{
    return value.kind == VimRegisterKind::uninitialized;
}

// `p` `P` の VimInsertAt を本文へ写した結果（写し方は controller の LF 文書と同じ）。
[[nodiscard]] std::string put_into(std::string body, const VimStep &step)
{
    const auto *const insert = std::get_if<VimInsertAt>(&step.effect);
    if (insert == nullptr)
    {
        return {};
    }
    body.insert(insert->at.value, insert->utf8);
    return body;
}

// OS の本文 → 写しの表（決定 4・probe の B3）。本文・畳んだ本文・種類。
constexpr std::array<std::tuple<std::string_view, std::string_view, VimRegisterKind>, 9>
    clipboard_texts{{
        {"abc", "abc", VimRegisterKind::characters},
        {"abc\r\n", "abc\n", VimRegisterKind::lines},
        {"abc\n", "abc\n", VimRegisterKind::lines},
        {"ab\r\ncd", "ab\ncd", VimRegisterKind::characters},
        {"ab\r\ncd\r\n", "ab\ncd\n", VimRegisterKind::lines},
        {"ab\rcd", "ab\rcd", VimRegisterKind::characters},
        {"abc\r\n\r\n", "abc\n\n", VimRegisterKind::lines},
        {"\r\n", "\n", VimRegisterKind::lines},
        {"", "", VimRegisterKind::characters},
    }};

void verify_clipboard_text_table()
{
    for (const auto &[os_text, folded, kind] : clipboard_texts)
    {
        expect(register_is(vim_register_of_clipboard(os_text), folded, kind),
               "the clipboard text folds CRLF and a trailing newline makes it linewise");
        expect(!vim_register_of_clipboard(os_text).width.has_value(),
               "the clipboard text is never a block");
    }
}

// `:set clipboard=unnamed` の読む述語（ADR 0051 の追補（#848）の決定 D）。レジスタを選ばず、
// オペレータも次の鍵の待ちも無い NORMAL / VISUAL の `p` `P` だけが無名へ読む側。
void verify_unnamed_clipboard_predicate(const VimState &empty, const TextBuffer &text,
                                        const Selection &caret)
{
    VimState on = empty;
    on.clipboard_unnamed = true;
    const VimKey put{nenenib::core::VimCharacter{U'p'}};
    const VimKey put_before{nenenib::core::VimCharacter{U'P'}};
    const VimKey down{nenenib::core::VimCharacter{U'j'}};
    expect(!nenenib::core::vim_reads_unnamed_clipboard(empty, put), "the sync is off by default");
    expect(nenenib::core::vim_reads_unnamed_clipboard(on, put) && vim_reads_clipboard(on, put) &&
               nenenib::core::vim_reads_unnamed_clipboard(on, put_before),
           "p and P read the clipboard into the unnamed register");
    expect(!vim_reads_clipboard(on, down), "other keys never read the clipboard");
    expect(nenenib::core::vim_reads_unnamed_clipboard(core_steps(on, text, caret, "3").next, put),
           "p after a count reads it");
    expect(nenenib::core::vim_reads_unnamed_clipboard(core_steps(on, text, caret, "v").next, put) &&
               nenenib::core::vim_reads_unnamed_clipboard(core_steps(on, text, caret, "V").next,
                                                          put_before),
           "characterwise and linewise VISUAL p P read it (#1418)");
    expect(
        nenenib::core::vim_reads_unnamed_clipboard(core_steps(on, text, caret, "<C-v>").next, put),
        "blockwise VISUAL p reads it too (#1461)");
    for (const std::string_view keys :
         {"d", "i", "\"a", "\"+", "f", "m", "@", "r", "g", "z", "'", "`", "q"})
    {
        expect(!nenenib::core::vim_reads_unnamed_clipboard(core_steps(on, text, caret, keys).next,
                                                           put),
               "p as an operand, an INSERT letter or after a register name is not a put");
    }
    expect(vim_reads_clipboard(core_steps(on, text, caret, "\"+").next, put),
           "\"+p still reads into the \"+ copy");
}

// 読む時機の述語（決定 3）。`"+` `"*` を選んでいるか `@` の名前を待っているときだけ真。`q` の
// 待ちと、後勝ちで別の名前へ選び直した `"+"a` は読まない。
void verify_clipboard_read_predicate()
{
    const TextBuffer text = buffer_of("one");
    const Selection caret = collapsed_at(Offset{0});
    const VimState empty = empty_vim_state();
    const VimKey put{nenenib::core::VimCharacter{U'p'}};
    expect(!vim_reads_clipboard(empty, put), "a resting state does not read the clipboard");
    expect(vim_reads_clipboard(core_steps(empty, text, caret, "\"+").next, put), "\"+ reads it");
    expect(vim_reads_clipboard(core_steps(empty, text, caret, "\"*").next, put), "\"* reads it");
    expect(vim_reads_clipboard(core_steps(empty, text, caret, "3\"+2").next, put),
           "the count digits after \"+ keep reading it");
    expect(vim_reads_clipboard(core_steps(empty, text, caret, "@").next, put),
           "waiting for the @ name reads it");
    expect(!vim_reads_clipboard(core_steps(empty, text, caret, "q").next, put),
           "waiting for the q name does not read it");
    expect(!vim_reads_clipboard(core_steps(empty, text, caret, "\"a").next, put),
           "\"a does not read it");
    expect(!vim_reads_clipboard(core_steps(empty, text, caret, "\"+\"a").next, put),
           "the later \"a replaces \"+");
    verify_unnamed_clipboard_predicate(empty, text, caret);
    const VimStep refused = core_steps(empty, text, caret, "q+");
    expect(refused.failure.has_value() && !refused.next.macro_recording.has_value(),
           "q+ is not a recording name");
    const VimState stored = nenenib::core::vim_register_stored(
        empty, '+', VimRegister{"x", VimRegisterKind::characters});
    expect(register_unused(stored.clipboard.value()), "vim_register_stored does not take +");
}

// 写しを置いてから貼る（決定 5・probe の B3 と B4）。前置きの鍵・OS の本文・貼る鍵・貼った本文。
// 本文は `X1` `X2` の 2 行でキャレットは 1 行 1 桁。
constexpr std::array<
    std::tuple<std::string_view, std::string_view, std::string_view, std::string_view>, 12>
    clipboard_puts{{
        {"\"+", "abc", "p", "Xabc1\nX2"},
        {"\"+", "abc\r\n", "p", "X1\nabc\nX2"},
        {"\"+", "abc\n", "p", "X1\nabc\nX2"},
        {"\"+", "ab\r\ncd", "p", "Xab\ncd1\nX2"},
        {"\"+", "ab\r\ncd\r\n", "p", "X1\nab\ncd\nX2"},
        {"\"+", "ab\rcd", "p", "Xab\rcd1\nX2"},
        {"\"+", "abc\r\n\r\n", "p", "X1\nabc\n\nX2"},
        {"\"*", "abc\r\n", "p", "X1\nabc\nX2"},
        {"\"*", "ab\r\ncd", "p", "Xab\ncd1\nX2"},
        {"3\"+", "abc", "p", "Xabcabcabc1\nX2"},
        {"\"+3", "abc", "p", "Xabcabcabc1\nX2"},
        {"\"+", "abc", "P", "abcX1\nX2"},
    }};

[[nodiscard]] VimStep clipboard_put(std::string_view prefix, std::string_view os_text,
                                    std::string_view put)
{
    const TextBuffer text = buffer_of("X1\nX2");
    const Selection caret = collapsed_at(Offset{0});
    const VimStep chosen = core_steps(empty_vim_state(), text, caret, prefix);
    return core_steps(vim_clipboard_loaded(chosen.next, vim_register_of_clipboard(os_text)), text,
                      caret, put);
}

void verify_clipboard_put()
{
    for (const auto &[prefix, os_text, put, expected] : clipboard_puts)
    {
        const VimStep step = clipboard_put(prefix, os_text, put);
        expect(put_into("X1\nX2", step) == expected, "\"+p pastes the clipboard text as read");
        expect(!step.clipboard.has_value() && !step.failure.has_value(),
               "\"+p does not write the clipboard");
        expect(register_unused(step.next.clipboard.value()), "the copy is gone after the command");
    }
    expect(dot_record_is(clipboard_put("\"+", "abc", "p").next, std::nullopt, "\"+p"),
           "\"+p is recorded for . with its name, which reads the clipboard again");
    const TextBuffer text = buffer_of("X1\nX2");
    const Selection caret = collapsed_at(Offset{0});
    const VimStep unread = core_steps(empty_vim_state(), text, caret, "\"+p");
    expect(unread.failure.has_value() && std::holds_alternative<VimNoEffect>(unread.effect),
           "\"+p without a copy is refused like an empty register");
    const VimStep empty = clipboard_put("\"+", "", "p");
    expect(empty.failure.has_value() && std::holds_alternative<VimNoEffect>(empty.effect),
           "\"+p of an empty text is refused");
    const VimStep held = clipboard_put("\"+", "abc", "3");
    expect(held.next.clipboard.value().text == "abc", "the copy stays while the command is typed");
    const auto escaped = core_steps(held.next, text, caret, "<Esc>");
    expect(register_unused(escaped.next.clipboard.value()), "Esc drops the copy");
}

// `@+` `@*` と `@@`（決定 5・probe の B8）。`@` の待ちに置いた写しを鍵列にして再生する。
[[nodiscard]] VimStep clipboard_replay(const VimState &state, std::string_view os_text,
                                       std::string_view name)
{
    const TextBuffer text = buffer_of("abc");
    const Selection caret = collapsed_at(Offset{0});
    const VimStep waiting = core_steps(state, text, caret, "@");
    return core_steps(vim_clipboard_loaded(waiting.next, vim_register_of_clipboard(os_text)), text,
                      caret, name);
}

[[nodiscard]] bool replays(const VimStep &step, std::string_view keys)
{
    const auto *const replay = std::get_if<VimReplay>(&step.effect);
    return replay != nullptr && replay->keys == vim_keys_of(keys);
}

void verify_clipboard_replay()
{
    const VimStep plus = clipboard_replay(empty_vim_state(), "x", "+");
    expect(replays(plus, "x") && plus.next.last_macro == std::optional<char>{'+'},
           "@+ runs the clipboard text and @@ remembers +");
    const VimStep again = clipboard_replay(plus.next, "l", "@");
    expect(replays(again, "l"), "@@ after @+ reads the clipboard again");
    const VimStep star = clipboard_replay(empty_vim_state(), "x\r\n", "*");
    expect(replays(star, "x<NL>") && star.next.last_macro == std::optional<char>{'*'},
           "@* runs a linewise clipboard text with its newline as <NL>");
    const VimStep empty = clipboard_replay(empty_vim_state(), "", "+");
    expect(empty.failure.has_value() && std::holds_alternative<VimNoEffect>(empty.effect),
           "@+ of an empty text is refused");
}

// `"+` への書き（決定 6・probe の B1）。表へは置かず VimStep.clipboard に返し、`"1` の規則と
// `"-` の規則と無名は名前つきと同じ（`"0` は変えない・1 行の中の削除は `"-` に入らない）。
void verify_clipboard_written()
{
    const TextBuffer text = buffer_of("one\ntwo");
    const Selection caret = collapsed_at(Offset{0});
    const VimState empty = empty_vim_state();
    for (const std::string_view keys : {std::string_view{"\"+yy"}, std::string_view{"\"*yy"}})
    {
        const VimStep yank = core_steps(empty, text, caret, keys);
        expect(yank.clipboard.has_value() &&
                   register_is(yank.clipboard.value(), "one\n", VimRegisterKind::lines),
               "\"+yy writes the line to the clipboard");
        expect(register_is(yank.next.unnamed_register.value(), "one\n", VimRegisterKind::lines) &&
                   register_unused(numbered_register(yank.next, 0)) &&
                   register_unused(yank.next.clipboard.value()),
               "\"+yy fills the unnamed register but not \"0 or the copy");
    }
    const VimStep line = core_steps(empty, text, caret, "\"+dd");
    expect(line.clipboard.has_value() &&
               register_is(line.clipboard.value(), "one\n", VimRegisterKind::lines) &&
               register_is(numbered_register(line.next, 1), "one\n", VimRegisterKind::lines) &&
               register_is(line.next.unnamed_register.value(), "one\n", VimRegisterKind::lines) &&
               register_unused(numbered_register(line.next, 0)) &&
               register_unused(line.next.small_delete.value()),
           "\"+dd writes the clipboard, \"1 and the unnamed register");
    const VimStep character = core_steps(empty, text, caret, "\"+x");
    expect(character.clipboard.has_value() &&
               register_is(character.clipboard.value(), "o", VimRegisterKind::characters) &&
               register_is(character.next.unnamed_register.value(), "o",
                           VimRegisterKind::characters) &&
               register_unused(numbered_register(character.next, 1)) &&
               register_unused(character.next.small_delete.value()),
           "\"+x writes the clipboard and the unnamed register but not \"- or \"1");
    const VimStep change = core_steps(empty, text, caret, "\"+cw");
    expect(change.clipboard.has_value() &&
               register_is(change.clipboard.value(), "one", VimRegisterKind::characters) &&
               change.next.mode == VimMode::insert &&
               register_unused(change.next.small_delete.value()),
           "\"+cw writes the clipboard and enters INSERT");
    const VimStep word = core_steps(empty, buffer_of("hello world"), caret, "\"+yw");
    expect(word.clipboard.has_value() &&
               register_is(word.clipboard.value(), "hello ", VimRegisterKind::characters),
           "\"+yw writes the word with its trailing blank");
    const VimStep plain = core_steps(empty, text, caret, "yy");
    expect(!plain.clipboard.has_value() &&
               register_is(numbered_register(plain.next, 0), "one\n", VimRegisterKind::lines),
           "yy without \"+ does not write the clipboard");
    const VimStep blackhole = core_steps(empty, text, caret, "\"_dd");
    expect(!blackhole.clipboard.has_value(), "\"_dd does not write the clipboard");
}

// 矩形の `"+y`（決定 6・7）。OS へ出す本文は engine の値のまま矩形の種類と幅を持ち、行を
// 改行で繋いだ本文にするのは controller。
void verify_clipboard_block_written()
{
    const TextBuffer text = buffer_of("abcd\nefgh");
    VimState block = empty_vim_state();
    block.mode = VimMode::visual_block;
    const VimStep yank = core_steps(block, text, Selection{Offset{0}, Offset{6}}, "\"+y");
    expect(yank.clipboard.has_value() &&
               register_is(yank.clipboard.value(), "ab\nef", VimRegisterKind::block) &&
               yank.next.mode == VimMode::normal,
           "a block \"+y writes the block text to the clipboard");
}

// 回数で本文を増やす編集は、編集の後の文書が開ける上限（1 GiB・ADR 0159 の決定 7）を超えると
// 確保の前に too_large で断る（#1341）。断った鍵は本文を変えず、`@` の残りも捨てる。
void verify_huge_count_put_refused()
{
    const TextBuffer text = buffer_of("abc\ndef");
    const Selection caret = collapsed_at(Offset{0});
    const std::array<std::string_view, 3> refused{"yiw999999999p", "yiw999999999P", "yy999999999p"};
    for (const std::string_view keys : refused)
    {
        const VimStep step = core_steps(empty_vim_state(), text, caret, keys);
        expect(step.failure == std::optional{nenenib::core::VimRepeatFailure::too_large} &&
                   std::holds_alternative<VimNoEffect>(step.effect),
               "a put past the document limit is refused before it is built");
    }
    // 矩形は選択を写す controller で確かめる（2 行 x 幅 2 の 999999999 個は 1 行 2 GB）。
    const std::array<std::string_view, 2> blocks{"<C-v>jly999999999p", "<C-v>jly999999999P"};
    for (const std::string_view keys : blocks)
    {
        Editing block;
        open_vim_document(block, "abc\ndef");
        vim_replay(block.controller(), keys);
        expect(whole_vim_body(block.controller()) == "abc\ndef",
               "a block put past the document limit is refused");
    }
    const VimStep small = core_steps(empty_vim_state(), text, caret, "yiw3p");
    expect(!small.failure.has_value(), "a put inside the limit is not refused");
    Editing editing;
    open_vim_document(editing, "abc\ndef");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', "yiw999999999px"));
    vim_replay(controller, "@a");
    expect(whole_vim_body(controller) == "abc\ndef", "a refused put stops the rest of @a");
}

// INSERT の回数の反復と `o` の回数も同じ上限。1 周目の本文は残り、残りの周は確保しない。
void verify_huge_count_insert_refused()
{
    const std::array<std::array<std::string_view, 2>, 2> cases{{
        {"999999999ixy<Esc>", "xyabc\ndef"},
        {"999999999oab<Esc>", "abc\nab\ndef"},
    }};
    for (const auto &[keys, body] : cases)
    {
        Editing editing;
        open_vim_document(editing, "abc\ndef");
        vim_replay(editing.controller(), keys);
        expect(whole_vim_body(editing.controller()) == body,
               "the insert count past the document limit keeps only the first round");
    }
}

// 文書の上限を小さな値に置いた core の vim_step（#1341）。1 GiB を確保せずに上限の境を見る。
// 効果は本文へ写さないので、最後の鍵の失敗だけを見る。
// 本文とキャレット。
using LimitedText = std::pair<std::string_view, Offset>;

[[nodiscard]] std::optional<nenenib::core::VimRepeatFailure>
limited_failure(const VimState &state, LimitedText text, std::string_view keys, std::size_t limit)
{
    const TextBuffer buffer = buffer_of(text.first);
    const Selection selection = collapsed_at(text.second);
    VimEditorView view{buffer, selection, VimViewport{LineNumber{1}, vim_visible_lines, 0}};
    view.document_limit = limit;
    VimStep step{state, VimNoEffect{}};
    for (const VimKey &key : vim_keys_of(keys))
    {
        step = vim_step(step.next, view, key);
    }
    return step.failure;
}

// 効果の本文は LF で数え controller が判定の後で文書の改行へ写すので、CRLF の文書では改行を
// 2 バイトに数える（#1341）。同じ回数が LF の文書では通り、CRLF の文書では断られる境。
void verify_huge_count_crlf_boundary()
{
    const std::optional<nenenib::core::VimRepeatFailure> refused{
        nenenib::core::VimRepeatFailure::too_large};
    const VimState rest = empty_vim_state();
    // 空行の `yy` は "\n"。上限 304: LF は 4 バイトの文書に 300 個、CRLF は 6 バイトに 149 個。
    expect(!limited_failure(rest, {"a\n\nb", Offset{2}}, "yy300p", 304).has_value() &&
               limited_failure(rest, {"a\n\nb", Offset{2}}, "yy301p", 304) == refused,
           "a line put fills the limit of an LF document exactly");
    expect(!limited_failure(rest, {"a\r\n\r\nb", Offset{3}}, "yy149p", 304).has_value() &&
               limited_failure(rest, {"a\r\n\r\nb", Offset{3}}, "yy150p", 304) == refused,
           "a line put counts two bytes for each CRLF newline");
    // INSERT の回数の残りの周（"\n" を 51 回）。上限 104 で LF の 3 バイトの文書は 101 まで、CRLF
    // の 4 バイトの文書は 2 バイト × 50 まで。
    expect(!limited_failure(rest, {"a\nb", Offset{0}}, "52i<CR><Esc>", 104).has_value() &&
               limited_failure(rest, {"a\r\nb", Offset{0}}, "52i<CR><Esc>", 104) == refused,
           "the insert count counts two bytes for each CRLF newline");
    // `o` の回数の残りの周（"\nab"）。上限 103 で LF は 3 バイト × 33、CRLF は 4 バイト × 24 まで。
    expect(!limited_failure(rest, {"a\nb", Offset{0}}, "34oab<Esc>", 103).has_value() &&
               limited_failure(rest, {"a\r\nb", Offset{0}}, "34oab<Esc>", 103) == refused,
           "the o count counts two bytes for each CRLF newline");
    // 矩形の `p`（幅 2 の 2 行・25 個）。1 行は手前の空白の上界 max(桁 2, tabstop 8) と改行と本文
    // 50 バイトで、上限 125 では LF の 7 バイトの文書は 118 で通り、CRLF の 8 バイトの文書は 120 で
    // 断る。
    const VimState block = nenenib::core::vim_resting_state(
        VimRegister{"ab\ncd", VimRegisterKind::block, nenenib::core::VimBlockWidth{2}});
    expect(!limited_failure(block, {"abc\ndef", Offset{0}}, "25p", 125).has_value() &&
               limited_failure(block, {"abc\r\ndef", Offset{0}}, "25p", 125) == refused,
           "a block put counts the newline of the document for each line");
}

// VISUAL の回数つきの `>` と矩形の `>` も同じ上限（#1341）。1 行に足すのは多くとも回数 + 9 バイト
// （shiftwidth 8・tabstop 8）で、上限 103 の 3 バイトの文書は 91 段まで。`<` は増やさない。
void verify_huge_count_shift_refused()
{
    const std::optional<nenenib::core::VimRepeatFailure> refused{
        nenenib::core::VimRepeatFailure::too_large};
    const VimState rest = empty_vim_state();
    expect(!limited_failure(rest, {"a\nb", Offset{0}}, "V91>", 103).has_value() &&
               limited_failure(rest, {"a\nb", Offset{0}}, "V92>", 103) == refused,
           "a counted visual > past the document limit is refused");
    expect(!limited_failure(rest, {"a\nb", Offset{0}}, "<C-v>91>", 103).has_value() &&
               limited_failure(rest, {"a\nb", Offset{0}}, "<C-v>92>", 103) == refused,
           "a counted block > past the document limit is refused");
    expect(!limited_failure(rest, {"\ta\nb", Offset{1}}, "V92<", 103).has_value(),
           "a counted visual < never grows the document");
}

// 小さな上限の試験の中で効果を本文と選択へ写す（controller の写しの小さな替え玉・#1341）。矩形の
// INSERT と矩形の `p` が通る効果だけ。本文は LF。
[[nodiscard]] std::pair<TextBuffer, Selection>
limited_applied(const TextBuffer &text, const Selection &selection,
                const nenenib::core::VimEffect &effect)
{
    if (const auto *moved = std::get_if<VimMoveTo>(&effect))
    {
        return {text, collapsed_at(moved->caret)};
    }
    if (const auto *selected = std::get_if<VimSelect>(&effect))
    {
        return {text, selected->selection};
    }
    if (const auto *typed = std::get_if<VimInsertString>(&effect))
    {
        return {text.insert(selection.caret, typed->utf8),
                collapsed_at(Offset{selection.caret.value + typed->utf8.size()})};
    }
    if (const auto *repeat = std::get_if<VimInsertAt>(&effect))
    {
        return {text.insert(repeat->at, repeat->utf8), collapsed_at(repeat->caret)};
    }
    if (const auto *inserted = std::get_if<VimInsertBlock>(&effect))
    {
        std::vector<VimBlockEdit> edits = inserted->edits;
        std::ranges::sort(edits, [](const VimBlockEdit &one, const VimBlockEdit &other)
                          { return other.range.begin < one.range.begin; });
        TextBuffer next = text;
        for (const VimBlockEdit &edit : edits)
        {
            next = next.replaced(edit.range.begin, edit.range.end, edit.utf8);
        }
        return {next, collapsed_at(inserted->caret)};
    }
    return {text, selection};
}

// 断ったか（どれかの鍵が too_large）と、効果を写した後の本文。
using LimitedRun = std::pair<bool, std::string>;

// 小さな上限で鍵を流し、効果を本文へ写す（#1341）。実際の大きさは写した後の本文で数える。
[[nodiscard]] LimitedRun limited_run(std::string_view body, std::string_view keys,
                                     std::size_t limit)
{
    const std::optional<nenenib::core::VimRepeatFailure> too_large{
        nenenib::core::VimRepeatFailure::too_large};
    TextBuffer text = buffer_of(body);
    Selection selection = collapsed_at(Offset{0});
    VimState state = empty_vim_state();
    bool refused = false;
    for (const VimKey &key : vim_keys_of(keys))
    {
        VimEditorView view{text, selection, VimViewport{LineNumber{1}, vim_visible_lines, 0}};
        view.document_limit = limit;
        VimStep step = vim_step(state, view, key);
        refused = refused || step.failure == too_large;
        auto [next_text, next_selection] = limited_applied(text, selection, step.effect);
        text = std::move(next_text);
        selection = next_selection;
        state = std::move(step.next);
    }
    return {refused, text.text()};
}

// 断らなかった鍵の後の本文は上限に収まる（見積もりが上界）。keys の `%` に 1 から counts までの
// 回数を入れて流す。
[[nodiscard]] bool limited_estimates_hold(std::string_view body, std::string_view keys,
                                          std::size_t limit, std::size_t counts)
{
    bool held = true;
    for (std::size_t count = 1; count <= counts; ++count)
    {
        std::string typed(keys);
        typed.replace(typed.find('%'), 1, std::to_string(count));
        const auto [refused, after] = limited_run(body, typed, limit);
        held = held && (refused || after.size() <= limit);
    }
    return held;
}

// 矩形の `I` `A` の回数は、写す本文 × 行の数で上限を見る（PR #1348 の再レビュー M1）。1 行は写す
// 本文と多くとも max(桁, tabstop) の空白なので、上限 300 で `a` の 3 行（Esc で 6 バイト）は
// 3 × (N + 8) ≤ 294 の N = 90 まで、10 行（Esc で 20 バイト）は 10 × (N + 8) ≤ 280 の N = 20 まで。
// 断った鍵は 1 周目の本文だけを全部の行へ写す。
void verify_huge_count_block_insert_refused()
{
    const std::string_view three = "a\na\na";
    const std::string_view ten = "a\na\na\na\na\na\na\na\na\na";
    expect(!limited_run(three, "<C-v>G90Ix<Esc>", 300).first &&
               limited_run(three, "<C-v>G91Ix<Esc>", 300) == LimitedRun{true, "xa\nxa\nxa"},
           "a counted block I over 3 lines is refused past the copied lines");
    expect(!limited_run(ten, "<C-v>G20Ix<Esc>", 300).first &&
               limited_run(ten, "<C-v>G21Ix<Esc>", 300).first,
           "a counted block I over 10 lines is refused past the copied lines");
    // 1 行目だけなら収まる回数（10 行の 50 個は 1 行目で 49 バイト・全部で 519 バイト）も断る。
    expect(limited_run(ten, "<C-v>G50Ix<Esc>", 300).first,
           "the copied lines count against the limit, not only the first line");
    const std::array<std::string_view, 4> forms{"<C-v>G%Ix<Esc>", "2l<C-v>G%Ix<Esc>",
                                                "5l<C-v>G%Ayz<Esc>", "l<C-v>Glcw%<Esc>"};
    for (const std::string_view keys : forms)
    {
        expect(limited_estimates_hold("abcdef\na\n\tbc\nabcdefgh", keys, 200, 60),
               "a block insert that is not refused stays inside the limit");
    }
}

// 矩形の `p` の 1 行の余分は多くとも max(桁, tabstop) と改行（PR #1348 の再レビュー M2）。桁の途中
// で割る Tab（`abc` `\tx` `\ty` の `0p` は 2 行目が +9 バイト・Vim と同じ）・全角の途中・行末より
// 右・足す行のどれも、断らなかった貼り付けの後の本文は上限に収まる。
void verify_huge_count_block_put_estimate()
{
    const std::array<std::array<std::string_view, 2>, 4> cases{{
        {"abc\n\tx\n\ty", "<C-v>jjly0%p"},
        {"abc\n\xE5\x85\xA8x\n\xE5\x85\xA8y", "<C-v>jjly0%p"},
        {"abcdef\na\nb", "<C-v>jjly$%p"},
        {"abcdef\nab\ncd", "<C-v>jjlyG$%p"},
    }};
    for (const auto &[body, keys] : cases)
    {
        bool held = true;
        for (std::size_t limit = body.size(); limit < body.size() + 80; ++limit)
        {
            held = held && limited_estimates_hold(body, keys, limit, 4);
        }
        expect(held, "a block put that is not refused stays inside the limit");
    }
}

// 断った鍵は E1240 を 1 行出し、鳴らさない（#1341・ADR 0153 の決定 2）。`@` の中で断っても同じ。
void verify_huge_count_notice()
{
    for (const std::string_view keys : {std::string_view{"yiw999999999p"}, std::string_view{"@a"}})
    {
        Editing editing;
        open_vim_document(editing, "abc\ndef");
        EditorController &controller = editing.controller();
        applied(controller, stored_text('a', "yiw999999999px"));
        vim_replay(controller, keys);
        const auto frame = controller.frame();
        expect(frame.command_message.has_value() &&
                   frame.command_message.value().text() == "E1240: Resulting text too long" &&
                   editing.bell().rings() == 0U,
               "a put refused at the document limit shows E1240 without the bell");
        // `@a` の残りの `x` も流れない（再生の残りを捨てる）。
        expect(whole_vim_body(controller) == "abc\ndef",
               "a refused put leaves the text and the rest of the macro does not run");
    }
}

// 録画を経る fixture と `"` の fixture は同じレジスタの表を通るので 1 scope（ADR 0048 の決定 11）。
[[nodiscard]] bool macro_fixture(const VimFixture &fixture) noexcept
{
    return fixture.macro.has_value() || fixture.name.starts_with("register-");
}
} // namespace

void verify_vim_macro_contracts()
{
    verify_macro_round_trips();
    verify_macro_waiting();
    verify_macro_escape_cancels();
    verify_macro_empty_registers();
    verify_macro_records_typed_keys_only();
    verify_macro_records_ex_line();
    verify_macro_records_visual_ex_line();
    verify_macro_replays_ex_line();
    verify_macro_q_inside_replay();
    verify_macro_failure_stops_the_rest();
    verify_macro_records_failed_replace();
    verify_macro_depth_limit();
    verify_macro_key_limit();
    verify_macro_huge_count_stops();
    verify_macro_rounds_keep_the_order();
    verify_huge_count_put_refused();
    verify_huge_count_insert_refused();
    verify_huge_count_crlf_boundary();
    verify_huge_count_shift_refused();
    verify_huge_count_block_insert_refused();
    verify_huge_count_block_put_estimate();
    verify_huge_count_notice();
    verify_macro_append();
    verify_macro_numbered_recording();
    verify_macro_numbered_stored();
    verify_macro_recording_frame();
    verify_macro_recording_hidden_in_ordinary();
    verify_register_text_round_trip();
    verify_recording_is_text();
    verify_macro_unnamed_and_uninitialized();
    verify_replay_through_input_line();
    verify_register_block_append_refused();
    verify_replay_input_line_keys();
    verify_recording_meets_registers();
    verify_clipboard_text_table();
    verify_clipboard_read_predicate();
    verify_clipboard_put();
    verify_clipboard_replay();
    verify_clipboard_written();
    verify_clipboard_block_written();
}

void verify_vim_macro_scope()
{
    std::size_t selected = 0;
    bool named = true;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        // macro 欄はほかの族の fixture も持つ（#1246 の insert-page-edge-・#1232 の
        // viewport-ctrl-e-・#1325 の del-key-・#1337 の dot-register-count-・#1341 の
        // huge-count-macro-・#1369 の prefix-escape-・#1363 の viewport-insert-follow-replay-・
        // #1448 の mark-insert-left-・#1440 の change-mark-macro-）。macro- には #1528 の macro-ex-
        // を含む。 名前で守るのは逆の向き: macro- の fixture は必ず macro 欄を持つ（#1324）。
        named &= !fixture.name.starts_with("macro-") || fixture.macro.has_value();
        if (macro_fixture(fixture))
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 186, "the scope replays the 49 macro, 108 register and 29 other "
                            "fixtures with a macro register");
    expect(named, "every macro- fixture carries a macro register");
    verify_vim_macro_contracts();
}
} // namespace nenenib::tests
