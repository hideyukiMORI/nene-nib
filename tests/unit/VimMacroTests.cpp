// scope `--vim-macro` の単体テスト（ADR 0042 決定 2・ADR 0046）。
#include "DisplayLine.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "StoreVimRegister.hpp"
#include "TestSupport.hpp"
#include "VimCharacter.hpp"
#include "VimKey.hpp"
#include "VimMode.hpp"
#include "VimNamedRegisters.hpp"
#include "VimPrefix.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimRegisterText.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchPattern.hpp"
#include "VimSpecialKey.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"

#include "../vim/VimFixtures.hpp"

#include <array>
#include <cstddef>
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
using nenenib::application::SelectEditMode;
using nenenib::application::StoreVimRegister;
using nenenib::core::EditMode;
using nenenib::core::VimKey;
using nenenib::core::VimMode;
using nenenib::core::VimPrefix;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchPattern;
using nenenib::core::VimSpecialKey;
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

[[nodiscard]] const VimRegister &named_register(const VimState &state, char32_t name)
{
    return state.registers.registers.at(nenenib::core::vim_register_index(name).value_or(0));
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
    expect(named_register(controller.vim_state(), U'c').text == "/beta\r",
           "a search is recorded as the one confirmed pattern key and stored as /beta<CR>");
    vim_replay(controller, "j0@c");
    expect(caret_at(controller.frame(), 3, 1), "the recorded search replays from the caret");
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

// 大文字の名前は追記（決定 2）。StoreVimRegister も録画の追記と同じ規則（ADR 0048 の決定 4）。
void verify_macro_append()
{
    Editing editing;
    open_vim_document(editing, "abcdef");
    EditorController &controller = editing.controller();
    applied(controller, stored_text('a', "l"));
    applied(controller, stored_text('A', "x"));
    expect(macro_keys(controller.vim_state(), U'a') == vim_keys_of("lx"), "A appends to a");
    applied(controller, stored_text('1', "x"));
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
                   state.unnamed_register.text == outcome.second,
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
               into_lines.unnamed_register.text == "abcd\n" &&
               into_lines.mode == VimMode::visual_block,
           "a block yank appended to a lines register is refused and the block selection stays");
    vim_replay(controller, "<Esc>gg<C-v>j\"by\"Byy");
    const VimState &into_block = controller.vim_state();
    expect(named_register(into_block, U'b').kind == VimRegisterKind::block &&
               named_register(into_block, U'b').text == "a\ne" &&
               into_block.unnamed_register.kind == VimRegisterKind::block &&
               whole_vim_body(controller) == "abcd\nefgh" && into_block.mode == VimMode::normal,
           "a line yank appended to a block register is refused and both registers stay");
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
    verify_macro_empty_registers();
    verify_macro_records_typed_keys_only();
    verify_macro_q_inside_replay();
    verify_macro_failure_stops_the_rest();
    verify_macro_depth_limit();
    verify_macro_append();
    verify_macro_recording_frame();
    verify_macro_recording_hidden_in_ordinary();
    verify_register_text_round_trip();
    verify_recording_is_text();
    verify_macro_unnamed_and_uninitialized();
    verify_replay_through_input_line();
    verify_register_block_append_refused();
    verify_replay_input_line_keys();
    verify_recording_meets_registers();
}

void verify_vim_macro_scope()
{
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (macro_fixture(fixture))
        {
            expect(fixture.name.starts_with("macro-") || fixture.name.starts_with("register-"),
                   "every selected fixture is a macro- or register- case");
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 45, "the scope replays the 20 macro and 25 register fixtures");
    verify_vim_macro_contracts();
}
} // namespace nenenib::tests
