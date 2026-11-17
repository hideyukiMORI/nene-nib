// Issue #1008 / #1824・ADR 0169 の決定 1・2・6。INSERT の補完の語の表と候補の順。
// 候補の順の期待値は Windows の Vim 9.1 の実測（`-u NONE -es` で `:normal! i<C-n>…` を流した結果・
// 道具 D:/NeNeNib/scripts/20261118/1008/probe.py）。面と鍵は #1008 の段 B。
#include "CompletionCandidates.hpp"
#include "CompletionDirection.hpp"
#include "CompletionSource.hpp"
#include "Editing.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "NewTab.hpp"
#include "Scopes.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VisibleLines.hpp"
#include "WordTable.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <limits>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using core::CompletionDirection;
using core::CompletionSource;
using core::Offset;
using Random = std::mt19937_64;

constexpr std::size_t every_word = std::numeric_limits<std::size_t>::max();
constexpr std::uint64_t property_seed = 0x6E69'6231'3030'38ULL;
constexpr std::size_t property_steps = 2500;

// 語の文字の種類が変わる所・幅 0 の code point（結合文字 U+0301・幅 0 の空白 U+200B）・改行の
// 形・64 バイトを超える語の連なり（窓を倍にして読み直す道）を混ぜる。
constexpr std::array<std::string_view, 22> fragments{
    "a",        "b",
    "f",        "o",
    "_",        "1",
    " ",        ".",
    "\n",       "\r\n",
    "\t",       "日",
    "本",       "あ",
    "カ",       "\xF0\x9F\x98\x80",
    "\xCC\x81", "\xE2\x80\x8B",
    "\xC3\xA9", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "ab",       "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return bound == 0 ? 0 : static_cast<std::size_t>(random() % bound);
}

// 上限より小さい本文の表（作れなければ試験の前提が崩れているので止める）。
[[nodiscard]] core::WordTable held(const core::TextBuffer &buffer)
{
    auto table = core::WordTable::of(buffer);
    if (!table.has_value())
    {
        std::abort();
    }
    return table.value();
}

[[nodiscard]] std::vector<std::string> words_of(const core::WordTable &table)
{
    std::vector<std::string> words;
    for (const core::WordCount &entry : table.prefixed("", every_word))
    {
        words.push_back(entry.word + "=" + std::to_string(entry.count));
    }
    return words;
}

[[nodiscard]] std::vector<std::string> listed(const core::CompletionList &list)
{
    std::vector<std::string> words;
    for (const core::CompletionCandidate &candidate : list.candidates)
    {
        words.push_back(candidate.word);
    }
    return words;
}

// text の at（バイト位置）で Ctrl-n / Ctrl-p を押したときの候補（今の文書の表だけ）。
[[nodiscard]] core::CompletionList completed(std::string_view text, std::size_t at,
                                             CompletionDirection direction)
{
    const core::TextBuffer buffer = buffer_of(text);
    const core::WordTable table = held(buffer);
    return core::completion_list(buffer, Offset{at}, direction, core::CompletionTables{&table, {}});
}

[[nodiscard]] bool forward_is(std::string_view text, std::size_t at,
                              const std::vector<std::string> &expected)
{
    return listed(completed(text, at, CompletionDirection::forward)) == expected;
}

[[nodiscard]] bool backward_is(std::string_view text, std::size_t at,
                               const std::vector<std::string> &expected)
{
    return listed(completed(text, at, CompletionDirection::backward)) == expected;
}

void verify_table()
{
    const core::WordTable words =
        held(buffer_of("foo bar foo \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E a _x1 x\n"
                       "\xE6\x97\xA5\xE6\x9C\xAC\xE3\x81\xAF \xF0\x9F\x98\x80\xF0\x9F\x98\x80"));
    expect(words.count("foo") == 2 && words.count("bar") == 1 && words.count("_x1") == 1,
           "the table counts every occurrence of an ASCII keyword run");
    expect(words.count("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E") == 1 &&
               words.count("\xE6\x97\xA5\xE6\x9C\xAC") == 1,
           "a non-ASCII word is a run of one character class (kanji stop before hiragana)");
    expect(words.count("a") == 0 && words.count("x") == 0 && words.count("\xE3\x81\xAF") == 0,
           "one-character words are not in the table (Vim's \\<\\k\\k never offers them)");
    expect(words.count("\xF0\x9F\x98\x80\xF0\x9F\x98\x80") == 1,
           "emoji are their own word class like `w`");
    const auto prefixed = words.prefixed("f", every_word);
    expect(prefixed.size() == 1 && prefixed.front().word == "foo" && prefixed.front().count == 2,
           "prefixed lists the words that start with the prefix");
}

void verify_size_limit()
{
    const std::string large(core::word_table_byte_limit + 1, 'a');
    const core::TextBuffer buffer = buffer_of(large);
    expect(!core::WordTable::of(buffer).has_value(),
           "a document over 16 MiB gets no word table (ADR 0169 decision 1)");
    const auto list = core::completion_list(buffer, Offset{large.size()},
                                            CompletionDirection::forward, core::CompletionTables{});
    expect(list.candidates.empty(), "without its table the document offers no body words");
    expect(list.prefix.end == Offset{large.size()} && list.prefix.begin.value < large.size(),
           "the prefix range is still reported for the other sources");
}

// 1 回の編集。断片の境 [first, last) を消し、断片の列 inserted を入れる。
[[nodiscard]] core::Edit random_edit(Random &random, std::vector<std::string_view> &model)
{
    const std::size_t first = below(random, model.size() + 1);
    const std::size_t last =
        first + below(random, std::min<std::size_t>(model.size() - first, 6) + 1);
    std::size_t at = 0;
    for (std::size_t index = 0; index < first; ++index)
    {
        at += model.at(index).size();
    }
    std::string removed;
    for (std::size_t index = first; index < last; ++index)
    {
        removed += model.at(index);
    }
    std::vector<std::string_view> inserted;
    std::string typed;
    for (std::size_t count = below(random, 5); count > 0; --count)
    {
        inserted.push_back(fragments.at(below(random, fragments.size())));
        typed += inserted.back();
    }
    model.erase(model.begin() + static_cast<std::ptrdiff_t>(first),
                model.begin() + static_cast<std::ptrdiff_t>(last));
    model.insert(model.begin() + static_cast<std::ptrdiff_t>(first), inserted.begin(),
                 inserted.end());
    return core::Edit{Offset{at}, std::move(removed), std::move(typed), Offset{at}};
}

// 性質の試験（ADR 0169 の決定 6）: ランダムな編集の列の後、増分で直した表は本文から作り直した表と
// 同じ。増減の列を畳む道も通る。
void verify_incremental_property()
{
    Random random{property_seed};
    std::vector<std::string_view> model;
    for (std::size_t index = 0; index < 120; ++index)
    {
        model.push_back(fragments.at(below(random, fragments.size())));
    }
    std::string text;
    for (const std::string_view fragment : model)
    {
        text += fragment;
    }
    core::TextBuffer buffer = buffer_of(text);
    core::WordTable table = held(buffer);
    bool agreed = true;
    for (std::size_t step = 0; step < property_steps && agreed; ++step)
    {
        const core::Edit edit = random_edit(random, model);
        table = table.edited(buffer, edit);
        buffer =
            buffer.replaced(edit.at, Offset{edit.at.value + edit.removed.size()}, edit.inserted);
        agreed = words_of(table) == words_of(held(buffer));
    }
    expect(agreed, "the incrementally edited table equals the table rebuilt from the text");
}

// 新しい語を 1 つずつ足すと増減の列が上限を超え、base へ畳まれる。畳んだ後も作り直した表と同じ。
void verify_fold()
{
    core::TextBuffer buffer = buffer_of("");
    core::WordTable table = held(buffer);
    bool folded = false;
    for (std::size_t index = 0; index <= core::WordTable::recent_word_limit; ++index)
    {
        const std::string word = std::format("w{} ", index);
        const core::Edit edit{Offset{buffer.size_bytes()}, std::string{}, word,
                              Offset{buffer.size_bytes()}};
        const std::size_t before = table.recent_size();
        table = table.edited(buffer, edit);
        folded = folded || (before > 0 && table.recent_size() == 0);
        buffer = buffer.insert(edit.at, word);
    }
    expect(folded && words_of(table) == words_of(held(buffer)),
           "the recent changes are folded into the base table and the counts stay exact");
}

// 候補の順は Vim の実測（ADR 0169 の決定 2・6）。
void verify_vim_order()
{
    expect(forward_is("a1 a2 a1 a a3 a2", 10, {"a3", "a2", "a1"}),
           "Ctrl-n lists the words after the caret first, then wraps to the top");
    expect(forward_is("a2 a1 a a2 a3", 7, {"a2", "a3", "a1"}) &&
               backward_is("a2 a1 a a2 a3", 7, {"a1", "a2", "a3"}),
           "Ctrl-p walks back from the caret and wraps from the end (not Ctrl-n reversed)");
    expect(forward_is("b1\nb2 b b3\nb4", 7, {"b3", "b4", "b1", "b2"}) &&
               backward_is("b1\nb2 b b3\nb4", 7, {"b2", "b1", "b4", "b3"}),
           "the order crosses lines in both directions");
    expect(forward_is("foo foobar foo", 14, {"foo", "foobar"}),
           "a word equal to the prefix elsewhere is a candidate; the typed one is not");
    expect(forward_is("fo", 2, {}), "the word being typed is never its own candidate");
    expect(forward_is("foobar fooqux fozz", 16, {"foobar", "fooqux"}) &&
               forward_is("fozz x fozz", 9, {"fozz"}),
           "in the middle of a word the typed word is skipped but its other occurrences count");
    expect(forward_is("ab a b ac a", 11, {"ab", "ac"}),
           "a one-character prefix offers only words of two or more characters");
    expect(forward_is("ab a b cd ", 10, {"ab", "cd"}),
           "after a blank the prefix is empty and every word of two characters is offered");
    expect(forward_is("ab cd", 3, {"cd", "ab"}) && backward_is("ab cd", 3, {"ab", "cd"}) &&
               backward_is("ab ef cd", 6, {"ef", "ab", "cd"}),
           "with an empty prefix the word starting at the caret counts as after the caret");
    expect(forward_is("a\xE6\x97\xA5 ab a", 8, {"a", "ab"}),
           "a one-character word followed by another keyword class matches \\<a\\k");
}

void verify_non_ascii()
{
    const std::string kanji = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xE6\x97\xA5\xE6\x9C\xAC"
                              "\xE3\x81\xAF \xE6\x97\xA5 \xE6\x97\xA5";
    expect(forward_is(kanji, kanji.size(),
                      {"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "\xE6\x97\xA5\xE6\x9C\xAC"}),
           "a kanji prefix completes kanji runs only");
    const std::string mixed = "\xE6\x97\xA5\xE6\x9C\xAC a\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E "
                              "\xE6\x97\xA5\xE6\x9C\xAC\xE4\xBA\xBA x\xE6\x97\xA5\xE6\x9C\xAC";
    const auto list = completed(mixed, mixed.size(), CompletionDirection::forward);
    expect(listed(list) == std::vector<std::string>{"\xE6\x97\xA5\xE6\x9C\xAC",
                                                    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",
                                                    "\xE6\x97\xA5\xE6\x9C\xAC\xE4\xBA\xBA"},
           "the prefix is the run of the class before the caret and a class change starts a word");
    expect(list.prefix.begin == Offset{mixed.size() - 6} && list.prefix.end == Offset{mixed.size()},
           "the prefix range covers only the same-class run before the caret");
    const std::string emoji = "\xF0\x9F\x98\x80\xF0\x9F\x98\x80 ab \xF0\x9F\x98\x80";
    expect(forward_is(emoji, emoji.size(), {"\xF0\x9F\x98\x80\xF0\x9F\x98\x80"}),
           "emoji complete like any other word class");
}

void verify_limit()
{
    std::string text;
    for (std::size_t index = 0; index < 150; ++index)
    {
        text += std::format("w{:03} ", index);
    }
    text += "w";
    const auto list = completed(text, text.size(), CompletionDirection::forward);
    expect(list.candidates.size() == core::completion_candidate_limit &&
               list.candidates.front().word == "w000" && list.candidates.back().word == "w099",
           "the list stops at 100 candidates in Vim's order");
}

// 窓に収まらない文書: 窓の中は Vim の順、窓の外の語は表から語の順で足す。打っている語は表の
// 数から 1 つ引く。
void verify_scan_window()
{
    const std::string filler(core::completion_scan_bytes + 1024, '.');
    const std::string text = "wzfar wzz2 " + filler + " wznear wzz";
    const std::size_t caret = text.size() - 1;
    const auto list = completed(text, caret, CompletionDirection::forward);
    expect(listed(list) == std::vector<std::string>{"wznear", "wzfar", "wzz2"},
           "words beyond the scan window come from the table after the near ones");
    expect(list.candidates.at(1).source == CompletionSource::buffer,
           "words from the document's own table are marked as the buffer");
}

void verify_other_tables()
{
    const core::TextBuffer buffer = buffer_of("alpha al");
    const core::WordTable own = held(buffer);
    const core::WordTable first = held(buffer_of("alps beta"));
    const core::WordTable second = held(buffer_of("alpha alpaca"));
    const std::array<const core::WordTable *, 2> others{&first, &second};
    const auto list = core::completion_list(buffer, Offset{8}, CompletionDirection::forward,
                                            core::CompletionTables{&own, others});
    expect(listed(list) == std::vector<std::string>{"alpha", "alps", "alpaca"},
           "other tabs follow the document in tab order and duplicates appear once");
    expect(list.candidates.at(0).source == CompletionSource::buffer &&
               list.candidates.at(1).source == CompletionSource::other_buffers &&
               list.candidates.at(2).source == CompletionSource::other_buffers,
           "each candidate carries where it came from");
}

// controller の編集の 3 か所（replace・undo・redo）が脇に置いたタブの表まで直している。
void verify_controller_tabs()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::InsertText{"alpha beta"});
    applied(controller, app::NewTab{});
    applied(controller, app::InsertText{"al"});
    const auto typed = controller.insert_completion(CompletionDirection::forward);
    expect(listed(typed) == std::vector<std::string>{"alpha"} &&
               typed.candidates.front().source == CompletionSource::other_buffers,
           "the parked tab's words are offered after typing there");
    applied(controller, app::SwitchTab{0});
    applied(controller, app::HistoryAction{core::HistoryDirection::undo});
    applied(controller, app::SwitchTab{1});
    expect(controller.insert_completion(CompletionDirection::forward).candidates.empty(),
           "undo in the other tab removes its words from the table");
    applied(controller, app::SwitchTab{0});
    applied(controller, app::HistoryAction{core::HistoryDirection::redo});
    applied(controller, app::SwitchTab{1});
    expect(listed(controller.insert_completion(CompletionDirection::forward)) ==
               std::vector<std::string>{"alpha"},
           "redo puts the words back");
}
} // namespace

void verify_insert_completion_contracts()
{
    verify_table();
    verify_size_limit();
    verify_incremental_property();
    verify_fold();
    verify_vim_order();
    verify_non_ascii();
    verify_limit();
    verify_scan_window();
    verify_other_tables();
    verify_controller_tabs();
}

void verify_insert_completion_scope()
{
    verify_insert_completion_contracts();
}
} // namespace nenenib::tests
