// scope `--vim-random` の単体テスト（Issue #870・ADR 0042 決定 2）。固定の種の生成器で鍵の語を
// 連ね、小さな文書の上で vim_replay に流して、語ごとに壊れていないことを見る。Vim と同じかは
// oracle の領分なので見ない。見るのは次の 5 つだけ（Issue #870 の受け入れ条件）。
//   1. 落ちない・止まらない（ASan / UBSan と CTest の TIMEOUT・再生の深さの上限 100）
//   2. キャレットと選択が本文の範囲の中（桁はコードポイントの単位なので境目は桁が守る）
//   3. 本文が妥当な UTF-8 で、改行の形が開いたときのまま。CRLF の文書ではどの `\n` も直前に
//      `\r` を持つ（ADR 0036 の決定 2・ADR 0055。literal CR は `\r` が増えるだけなので
//      例外にならない）
//   4. 鍵列の後に `<Esc>` を打ち、`u` を打った鍵の数より 1 つ多く流すと最初の本文に戻る
//      （undo の 1 単位は少なくとも 1 鍵から出来る・戻すものが無くなった `u` で止まる）
//   5. モードは閉じた enum の値で、Vim のまま。ファイル・設定・タブ・窓に触れていない
// 生成器は std::mt19937_64 の固定の種で、分布の型は使わず剰余で範囲に畳む（決定的）。
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "LineEnding.hpp"
#include "LineNumber.hpp"
#include "Scopes.hpp"
#include "SelectionPresence.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "Utf8.hpp"
#include "VimMode.hpp"
#include "VimTestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorController;
using nenenib::application::EditorFrame;
using nenenib::core::code_point_count;
using nenenib::core::EditMode;
using nenenib::core::LineEnding;
using nenenib::core::LineNumber;
using nenenib::core::SelectionPresence;
using nenenib::core::TextBuffer;
using nenenib::core::validate_utf8;
using nenenib::core::VimMode;
using Generator = std::mt19937_64;
using Words = std::vector<std::string>;
// 文書と語の列。破れは「どの不変量か」と「何語目か」（語の数なら undo かポートの検査）。
using Trial = std::pair<std::string, Words>;
using Break = std::pair<std::string_view, std::size_t>;

// 既定実行に載せる回数（Debug・ASan で 1 秒以内・2026-10-27 に約 0.5〜0.7 秒）。1 鍵が約 1 ms
// かかるので 3 本にとどめる。50 倍の走り（150 本・2 つの種）は commit しない（Issue #870）。
constexpr std::size_t random_trials = 3;
constexpr std::uint64_t random_seed = 870;
// 本文がこれを超えたら残りの語を打たずに undo の往復へ進む（回数の掛け算で育つ文書の歯止め）。
constexpr std::size_t largest_text = 2048;

[[nodiscard]] std::size_t below(Generator &generator, std::size_t bound)
{
    return static_cast<std::size_t>(generator() % bound);
}

template <std::size_t N>
[[nodiscard]] std::string pick(Generator &generator, const std::array<std::string_view, N> &table)
{
    return std::string(table.at(below(generator, N)));
}

// 文書の行の材料。`+` と `*` を入れない（本文を鍵として実行する `@a` が `"+` `"*` を作らない）。
constexpr std::array<std::string_view, 12> document_lines{
    "",
    "abc def ghi",
    "  indented (x) [y] {z} <w>",
    "日本語のテキスト、です。",
    "\tTab\there\t",
    "café é ü",
    "\"quoted\" 'single' `back`",
    "<p>tag <b>x</b></p>",
    "One. Two! Three?",
    "x",
    "a\xcc\x81 e\xcc\x88 👍🏽",
    "    ",
};

// INSERT で打つ文字の材料（`+` `*` `"` を入れない）。
constexpr std::array<std::string_view, 18> inserted_pieces{
    "x",    "ab",   " ",    "\t",     "あ",      "é",    "(",      ")",      "'",
    "<CR>", "<NL>", "<BS>", "<Left>", "<Right>", "<Up>", "<Down>", "<Home>", "<End>",
};

constexpr std::array<std::string_view, 53> motions{
    "h",    "j",      "k",  "l",  "w",  "b",       "e",    "W",      "B",     "E",      "ge",
    "gE",   "0",      "$",  "^",  "gg", "G",       "H",    "M",      "L",     "%",      "{",
    "}",    "(",      ")",  ";",  ",",  "<Space>", "<BS>", "<Home>", "<End>", "<Left>", "<Right>",
    "<Up>", "<Down>", "gj", "gk", "g0", "g$",      "gm",   "gM",     "g_",    "|",      "+",
    "-",    "n",      "N",  "*",  "#",  "``",      "''",   "`.",     "'.",
};

constexpr std::array<std::string_view, 12> scrolls{
    "<C-d>", "<C-u>", "<C-f>", "<C-b>", "<PageUp>", "<PageDown>",
    "zt",    "zz",    "zb",    "z<CR>", "z.",       "z-",
};

constexpr std::array<std::string_view, 28> objects{
    "iw", "aw", "iW", "aW", "i\"", "a\"", "i'", "a'", "i`", "a`", "i(", "a(", "ib", "ab",
    "i{", "a{", "iB", "aB", "i[",  "a[",  "i<", "a<", "it", "at", "ip", "ap", "is", "as",
};

constexpr std::array<std::string_view, 8> operators{"d", "c", "y", ">", "<", "g~", "gu", "gU"};
constexpr std::array<std::string_view, 8> doubled{"dd", "cc",  "yy",  ">>",
                                                  "<<", "g~~", "guu", "gUU"};
constexpr std::array<std::string_view, 17> edits{
    "x", "X", "D", "C", "Y", "p", "P", "J", "gJ", "~", "s", "S", "u", "<C-r>", ".", "u", ".",
};
constexpr std::array<std::string_view, 6> insert_entries{"i", "a", "I", "A", "o", "O"};
constexpr std::array<std::string_view, 10> targets{"a", "e",  "x", " ", "(",
                                                   ")", "日", "é", "'", "\t"};
constexpr std::array<std::string_view, 13> registers{"a", "b", "c", "z", "A",  "B", "0",
                                                     "1", "2", "9", "-", "\"", "_"};
constexpr std::array<std::string_view, 3> marks{"a", "b", "c"};
constexpr std::array<std::string_view, 9> patterns{"a",    "e",    "x\\|a", "^ ", "日本",
                                                   "[ab]", "\\<T", "é",     "\\("};
constexpr std::array<std::string_view, 5> commands{
    ":noh<CR>",           ":set hlsearch<CR>",    ":set nohlsearch<CR>",
    ":set incsearch<CR>", ":set noincsearch<CR>",
};
constexpr std::array<std::string_view, 7> visual_ends{"d", "x", "y", "~", "u", "U", "<Esc>"};
constexpr std::array<std::string_view, 3> visual_entries{"v", "V", "<C-v>"};

[[nodiscard]] std::string counted(Generator &generator)
{
    const std::size_t kind = below(generator, 6);
    if (kind == 0)
    {
        return std::to_string(1 + below(generator, 9));
    }
    if (kind == 1)
    {
        return std::to_string(10 + below(generator, 30));
    }
    return "";
}

// 本文を増やす語（貼り付け・挿入・`.`）の回数は小さくして、文書が育ちすぎないようにする。
[[nodiscard]] std::string small_count(Generator &generator)
{
    return below(generator, 6) == 0 ? std::to_string(2 + below(generator, 2)) : std::string{};
}

[[nodiscard]] std::string motion_word(Generator &generator)
{
    const std::size_t kind = below(generator, 10);
    if (kind == 0)
    {
        return counted(generator) +
               pick(generator, std::array<std::string_view, 4>{"f", "F", "t", "T"}) +
               pick(generator, targets);
    }
    if (kind == 1)
    {
        return pick(generator, std::array<std::string_view, 2>{"'", "`"}) + pick(generator, marks);
    }
    if (kind == 2)
    {
        return counted(generator) + pick(generator, scrolls);
    }
    return counted(generator) + pick(generator, motions);
}

[[nodiscard]] std::string operator_word(Generator &generator)
{
    const std::string count = counted(generator);
    const std::size_t kind = below(generator, 4);
    if (kind == 0)
    {
        return count + pick(generator, doubled);
    }
    const std::string op = pick(generator, operators);
    const std::string escape = op == "c" ? std::string{"<Esc>"} : std::string{};
    if (kind == 1)
    {
        return count + op + pick(generator, objects) + escape;
    }
    return count + op + motion_word(generator) + escape;
}

[[nodiscard]] std::string inserted_text(Generator &generator)
{
    std::string text;
    const std::size_t pieces = below(generator, 6);
    for (std::size_t index = 0; index < pieces; ++index)
    {
        text += pick(generator, inserted_pieces);
    }
    return text;
}

// INSERT に入る語。たいていは `<Esc>` で閉じ、ときどき開いたまま次の語へ渡す（Issue #870）。
[[nodiscard]] std::string insert_word(Generator &generator)
{
    const std::string entry =
        below(generator, 6) == 0 ? std::string{"cc"} : pick(generator, insert_entries);
    const std::string body = small_count(generator) + entry + inserted_text(generator);
    return below(generator, 5) == 0 ? body : body + "<Esc>";
}

[[nodiscard]] std::string edit_word(Generator &generator)
{
    const std::size_t kind = below(generator, 6);
    if (kind == 0)
    {
        return counted(generator) + "r" + pick(generator, targets);
    }
    if (kind == 1)
    {
        return "m" + pick(generator, marks);
    }
    return small_count(generator) + pick(generator, edits);
}

[[nodiscard]] std::string visual_word(Generator &generator)
{
    std::string word = pick(generator, visual_entries);
    const std::size_t moves = below(generator, 4);
    for (std::size_t index = 0; index < moves; ++index)
    {
        word += below(generator, 3) == 0
                    ? pick(generator, objects)
                    : pick(generator, std::array<std::string_view, 3>{"o", "O", ""}) +
                          motion_word(generator);
    }
    const std::size_t end = below(generator, 9);
    if (end == 0)
    {
        return word + "r" + pick(generator, targets);
    }
    if (end == 1)
    {
        return word + "c" + inserted_text(generator) + "<Esc>";
    }
    if (end == 2)
    {
        return word + small_count(generator) +
               pick(generator, std::array<std::string_view, 2>{">", "<"});
    }
    return word + pick(generator, visual_ends);
}

[[nodiscard]] std::string register_word(Generator &generator)
{
    return "\"" + pick(generator, registers) + small_count(generator) +
           pick(generator,
                std::array<std::string_view, 8>{"yy", "dd", "p", "P", "x", "yw", "dw", "yiw"});
}

[[nodiscard]] std::string search_word(Generator &generator)
{
    const std::size_t kind = below(generator, 4);
    const std::string direction = pick(generator, std::array<std::string_view, 2>{"/", "?"});
    if (kind == 0)
    {
        return direction + pick(generator, patterns) + "<Esc>";
    }
    return direction + pick(generator, patterns) + "<CR>";
}

[[nodiscard]] std::string command_word(Generator &generator)
{
    return pick(generator, commands);
}

// マクロの中身は INSERT を閉じてから `q` で止める。再生は `@{r}` `@@` `{N}@{r}`。
[[nodiscard]] std::string macro_word(Generator &generator)
{
    const std::string name = pick(generator, std::array<std::string_view, 4>{"a", "b", "c", "A"});
    if (below(generator, 2) == 0)
    {
        const std::size_t count = below(generator, 3);
        return (count == 0 ? std::string{} : std::to_string(count + 1)) + "@" +
               (below(generator, 3) == 0 ? std::string{"@"} : name);
    }
    std::string word = "q" + name;
    const std::size_t inner = 1 + below(generator, 3);
    for (std::size_t index = 0; index < inner; ++index)
    {
        word += below(generator, 2) == 0 ? motion_word(generator) : edit_word(generator);
    }
    return word + "<Esc>q";
}

using WordMaker = std::string (*)(Generator &);
constexpr std::array<WordMaker, 16> word_makers{
    motion_word,   motion_word, motion_word, operator_word, operator_word, operator_word,
    insert_word,   insert_word, edit_word,   edit_word,     edit_word,     visual_word,
    register_word, search_word, macro_word,  command_word,
};

[[nodiscard]] std::string random_document(Generator &generator)
{
    const std::string newline = below(generator, 2) == 0 ? std::string{"\n"} : std::string{"\r\n"};
    const std::size_t lines = below(generator, 7);
    std::string text;
    for (std::size_t index = 0; index < lines; ++index)
    {
        if (index > 0)
        {
            text += newline;
        }
        text += pick(generator, document_lines);
    }
    // LF の文書の literal CR（ADR 0036）。最初の行の後ろに置くので判別は LF のまま。
    if (newline == "\n" && lines > 1 && below(generator, 4) == 0)
    {
        text += "a\rb";
    }
    return below(generator, 3) == 0 ? text + newline : text;
}

[[nodiscard]] Trial random_trial(Generator &generator)
{
    Trial trial{random_document(generator), Words{}};
    const std::size_t words = 20 + below(generator, 41);
    for (std::size_t index = 0; index < words; ++index)
    {
        trial.second.push_back(word_makers.at(below(generator, word_makers.size()))(generator));
    }
    return trial;
}

// 不変量 2: キャレットの桁が行のコードポイントの数 + 1 を、選択の終わりの桁が + 2 を超えない。
// 選択だけ 1 つ多いのは、行単位の選択（と行末を越える `$` や矩形の VISUAL）が
// 改行の分の 1 桁を塗るため。
[[nodiscard]] bool within_text(const EditorFrame &frame, const TextBuffer &text)
{
    const auto caret = frame.caret.position;
    if (caret.line.value < 1 || caret.line.value > text.line_count() || caret.column.value < 1 ||
        caret.column.value > code_point_count(text.line_text(caret.line)) + 1)
    {
        return false;
    }
    for (const auto &line : frame.lines)
    {
        if (line.selection.presence == SelectionPresence::absent)
        {
            continue;
        }
        if (line.number.value > text.line_count() || line.selection.begin.value < 1 ||
            line.selection.end.value < line.selection.begin.value ||
            line.selection.end.value > code_point_count(text.line_text(line.number)) + 2)
        {
            return false;
        }
    }
    return true;
}

// 不変量 3: 妥当な UTF-8 で、改行の形が開いたときのまま。CRLF の文書に孤立した `\n` が無い。
[[nodiscard]] bool well_formed(const TextBuffer &text, LineEnding ending)
{
    const std::string bytes = text.text();
    if (!validate_utf8(bytes).has_value() || text.line_ending() != ending)
    {
        return false;
    }
    if (ending == LineEnding::lf)
    {
        return true;
    }
    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        if (bytes[index] == '\n' && (index == 0 || bytes[index - 1] != '\r'))
        {
            return false;
        }
    }
    return true;
}

// 不変量 5: モードは閉じた enum の値（網羅は switch が守る）で、Vim のまま、タブと窓は 1 つ。
[[nodiscard]] bool closed_mode(const EditorFrame &frame)
{
    bool known = false;
    switch (frame.vim_mode)
    {
    case VimMode::normal:
    case VimMode::insert:
    case VimMode::visual:
    case VimMode::visual_line:
    case VimMode::visual_block:
        known = true;
        break;
    }
    return known && frame.mode == EditMode::vim && frame.tabs.size() == 1 && !frame.closing &&
           !frame.close_request.has_value() && !frame.operation_request.has_value();
}

[[nodiscard]] std::optional<std::string_view> broken_invariant(EditorController &controller,
                                                               LineEnding ending)
{
    const EditorFrame frame = controller.frame();
    if (!closed_mode(frame))
    {
        return "the mode stays a Vim mode of the closed enum in one tab";
    }
    if (!well_formed(controller.text(), ending))
    {
        return "the text stays valid UTF-8 with the opened line ending";
    }
    if (!within_text(frame, controller.text()))
    {
        return "the caret and the selection stay inside the text";
    }
    return std::nullopt;
}

// 鍵列の後に `<Esc>` で INSERT を閉じ、打った鍵の数より 1 つ多い `u` を :normal! の意味で流す
// （戻すものが無くなった `u` の失敗で残りを打ち切る・1 鍵ごとに frame を作らない）。
// 見るのは「戻せたところまでで最初の本文に戻る」ことだけ。`<C-r>` や `.` を含む列で 1 鍵の
// undo が 2 単位に割れる破れは、余分な `u` が吸収するのでこの検査では見逃す。
[[nodiscard]] bool undone_to_start(EditorController &controller, const std::string &start,
                                   std::size_t keys)
{
    vim_replay(controller, "<Esc><Esc>");
    vim_normal(controller, std::string(keys + 1, 'u'));
    return controller.text().text() == start;
}

[[nodiscard]] bool ports_untouched(Editing &editing)
{
    return editing.files().reads() == 1 && editing.files().written().empty() &&
           editing.settings().writes() == 0;
}

// 破れの最初の 1 つ。語の後の不変量・undo の往復・ポートの順に見る。
[[nodiscard]] std::optional<Break> broken_in(const Trial &trial)
{
    Editing editing;
    open_vim_document(editing, trial.first);
    EditorController &controller = editing.controller();
    const LineEnding ending = controller.text().line_ending();
    const std::string start = controller.text().text();
    std::size_t keys = 0;
    for (std::size_t index = 0; index < trial.second.size(); ++index)
    {
        if (controller.text().size_bytes() > largest_text)
        {
            break;
        }
        vim_replay(controller, trial.second.at(index));
        keys += vim_keys_of(trial.second.at(index)).size();
        const auto broken = broken_invariant(controller, ending);
        if (broken.has_value())
        {
            return Break{broken.value(), index};
        }
    }
    if (!undone_to_start(controller, start, keys))
    {
        return Break{"undo returns to the first text", trial.second.size()};
    }
    if (!ports_untouched(editing))
    {
        return Break{"no file, setting, tab or window is touched", trial.second.size()};
    }
    return std::nullopt;
}

// 語を 1 つずつ外して、同じ不変量が破れる限り外したままにする（Issue #870 の最小化）。
[[nodiscard]] Trial minimized(Trial trial, std::string_view invariant)
{
    for (std::size_t index = trial.second.size(); index > 0; --index)
    {
        Trial shorter = trial;
        shorter.second.erase(shorter.second.begin() + static_cast<std::ptrdiff_t>(index - 1));
        const auto broken = broken_in(shorter);
        if (broken.has_value() && broken.value().first == invariant)
        {
            trial = std::move(shorter);
        }
    }
    return trial;
}

[[nodiscard]] std::string escaped(std::string_view text)
{
    std::string out;
    for (const char byte : text)
    {
        if (byte == '\n')
        {
            out += "\\n";
        }
        else if (byte == '\r')
        {
            out += "\\r";
        }
        else if (byte == '\t')
        {
            out += "\\t";
        }
        else
        {
            out += byte;
        }
    }
    return out;
}

[[nodiscard]] std::string described(std::string_view label, const Trial &trial,
                                    std::string_view invariant)
{
    std::string keys;
    for (const std::string &word : trial.second)
    {
        keys += word + " ";
    }
    return std::string(label) + ": " + std::string(invariant) + " / text \"" +
           escaped(trial.first) + "\" / words " + escaped(keys);
}

void verify_trial(std::string_view label, const Trial &trial)
{
    const auto broken = broken_in(trial);
    if (!broken.has_value())
    {
        expect(true, "a random key sequence keeps every invariant");
        return;
    }
    const Trial smallest = minimized(trial, broken.value().first);
    expect(false, described(label, smallest, broken.value().first).c_str());
}

// 再帰するマクロ（`qa@aq` の後の `@a`）は深さの上限 100 で止まり、次の鍵は普通に走る（ADR 0046）。
void verify_random_recursive_macro()
{
    Editing editing;
    open_vim_document(editing, "abc def\nghi");
    EditorController &controller = editing.controller();
    vim_replay(controller, "qa@aq");
    vim_replay(controller, "@a");
    const auto frame = controller.frame();
    expect(frame.command_message.has_value() &&
               frame.command_message.value().text() == "E132: Macro depth is higher than 100",
           "a recursive macro made from random words stops at the depth limit");
    expect(!broken_invariant(controller, LineEnding::lf).has_value(),
           "the invariants hold after the depth limit");
    vim_replay(controller, "x");
    expect(controller.text().text() == "bc def\nghi", "the next typed key runs normally");
    expect(undone_to_start(controller, "abc def\nghi", 8), "undo returns to the first text");
}

void verify_random_sequences()
{
    Generator generator{random_seed};
    for (std::size_t index = 0; index < random_trials; ++index)
    {
        const Trial trial = random_trial(generator);
        verify_trial("random trial " + std::to_string(index), trial);
    }
}
} // namespace

void verify_vim_random_scope()
{
    verify_random_recursive_macro();
    verify_random_sequences();
}
} // namespace nenenib::tests
