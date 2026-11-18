// Issue #1008 / #1824・ADR 0169 の決定 1・2・6。INSERT の補完の語の表と候補の順。
// 候補の順の期待値は Windows の Vim 9.1 の実測（`-u NONE -es` で `:normal! i<C-n>…` を流した結果・
// 道具 D:/NeNeNib/scripts/20261118/1008/probe.py）。面と鍵は #1008 の段 B。
#include "Appearance.hpp"
#include "CompletionCandidates.hpp"
#include "CompletionDirection.hpp"
#include "CompletionSource.hpp"
#include "DeleteDirection.hpp"
#include "DeleteText.hpp"
#include "DeleteUnit.hpp"
#include "Document.hpp"
#include "EditMode.hpp"
#include "Editing.hpp"
#include "EditorState.hpp"
#include "HistoryAction.hpp"
#include "HistoryDirection.hpp"
#include "InsertText.hpp"
#include "LineEnding.hpp"
#include "NewTab.hpp"
#include "Scopes.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimTestSupport.hpp"
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
constexpr std::size_t property_seeds = 6;
constexpr std::size_t property_steps = 1200;
constexpr std::size_t controller_steps = 240;

// 語の文字の種類が変わる所・幅 0 の code point（結合文字 U+0301・タイ文字の結合 U+0E31・幅 0 の
// 空白 U+200B・ZWJ・VS16）・改行の形（LF・CRLF・孤立した CR）・3 バイトで語の文字のタイ文字
// U+0E01・64 バイトを超える語と幅 0 の列（窓を倍にして読み直す道・窓の端が code point の途中）を
// 混ぜる（PR #1832 のレビューの ReviewProbe の形）。
constexpr std::array<std::string_view, 33> fragments{
    "a",
    "b",
    "f",
    "o",
    "_",
    "1",
    " ",
    ".",
    "\n",
    "\r\n",
    "\r",
    "\t",
    "日",
    "本",
    "あ",
    "カ",
    "\xF0\x9F\x98\x80",
    "\xCC\x81",
    "\xE2\x80\x8B",
    "\xE2\x80\x8D",
    "\xEF\xB8\x8F",
    "\xC3\xA9",
    "\xE0\xB8\x81",
    "\xE0\xB8\xB1",
    "\xE1\xB8\x81",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81"
    "\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81"
    "\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81\xE0\xB8\x81",
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81"
    "\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81",
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80"
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80"
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80\xF0\x9F\x98\x80"
    "\xF0\x9F\x98\x80\xF0\x9F\x98\x80",
    "ab",
    "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E",
    "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy",
    "foo"};

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

// 1 MiB を超える文書は表を持たない（設計席の追補 2026-11-18）。候補は窓の語と他のタブの表だけ。
void verify_size_limit()
{
    const std::string filler(core::word_table_byte_limit, '.');
    const std::string text = "wfar " + filler + " wnear w";
    const core::TextBuffer buffer = buffer_of(text);
    expect(!core::WordTable::of(buffer).has_value(), "a document over 1 MiB gets no word table");
    const core::WordTable other = held(buffer_of("wother"));
    const std::array<const core::WordTable *, 1> others{&other};
    const auto list =
        core::completion_list(buffer, Offset{text.size()}, CompletionDirection::forward,
                              core::CompletionTables{nullptr, others});
    expect(listed(list) == std::vector<std::string>{"wnear", "wother"},
           "without its table the document offers the words in the window and other tabs");
    expect(list.prefix.end == Offset{text.size()} && list.prefix.begin == Offset{text.size() - 1},
           "the prefix range is reported");
}

// 開く・切り替える道は表を作らない。表を作るのは最初に補完を使ったときだけ。
void verify_lazy_table()
{
    const auto opened = app::EditorState::create(core::Appearance::dark, core::EditMode::ordinary)
                            .with_opened(buffer_of("alpha beta"), app::Document{});
    expect(!opened.word_table().has_value(), "opening a document builds no word table");
    const auto built = opened.with_word_table();
    const auto &table = built.word_table();
    expect(table.has_value() && table.value().count("alpha") == 1,
           "the table is built on demand from the text");
    const auto switched = built.with_new_tab().with_switched(0);
    expect(switched.word_table().has_value(), "a parked table comes back with its tab");
    const auto reopened = built.with_opened(buffer_of("gamma"), app::Document{});
    expect(!reopened.word_table().has_value(), "opening another document drops the old table");
    const std::string paste(core::word_table_edit_limit + 1, 'a');
    const core::Edit big{Offset{0}, std::string{}, paste, Offset{0}};
    expect(!built.with_words_edited(big).word_table().has_value(),
           "an edit over 64 KiB drops the table instead of updating it");
    const core::Edit small{Offset{0}, std::string{}, "zeta ", Offset{0}};
    expect(!opened.with_words_edited(small).word_table().has_value(),
           "editing a document without a table costs nothing and builds none");
}

// 本文の code point の境の列（0 と末尾を含む）。
[[nodiscard]] std::vector<std::size_t> boundaries_of(std::string_view text)
{
    std::vector<std::size_t> bounds{0};
    for (std::size_t index = 1; index <= text.size(); ++index)
    {
        if (index == text.size() || (static_cast<unsigned char>(text[index]) & 0xC0U) != 0x80U)
        {
            bounds.push_back(index);
        }
    }
    return bounds;
}

// 1 回の編集。code point の境で切った範囲を消し、断片を 0〜4 個入れる。
[[nodiscard]] core::Edit random_edit(Random &random, const std::string &text)
{
    const std::vector<std::size_t> bounds = boundaries_of(text);
    const std::size_t first = below(random, bounds.size());
    const std::size_t last = std::min(bounds.size() - 1, first + below(random, 7));
    std::string typed;
    for (std::size_t count = below(random, 5); count > 0; --count)
    {
        typed += fragments.at(below(random, fragments.size()));
    }
    const std::size_t at = bounds.at(first);
    return core::Edit{Offset{at}, text.substr(at, bounds.at(last) - at), std::move(typed),
                      Offset{at}};
}

[[nodiscard]] core::TextBuffer seeded_text(Random &random, bool crlf)
{
    std::string text;
    for (std::size_t index = 0; index < 120; ++index)
    {
        text += fragments.at(below(random, fragments.size()));
    }
    auto buffer = crlf ? core::TextBuffer::from_utf8(text, core::LineEnding::crlf)
                       : core::TextBuffer::from_utf8(text);
    if (!buffer.has_value())
    {
        std::abort();
    }
    return std::move(buffer).value();
}

// 1 つの seed の編集の列。5 回に 1 回は直前の編集を戻す（undo の形の逆向きの編集）。
[[nodiscard]] bool property_case(std::uint64_t seed)
{
    Random random{property_seed + seed};
    core::TextBuffer buffer = seeded_text(random, seed % 2 == 0);
    core::WordTable table = held(buffer);
    std::vector<core::Edit> done;
    for (std::size_t step = 0; step < property_steps; ++step)
    {
        core::Edit edit = random_edit(random, buffer.text());
        if (!done.empty() && below(random, 5) == 0)
        {
            const core::Edit last = done.back();
            done.pop_back();
            edit = core::Edit{last.at, last.inserted, last.removed, last.at};
        }
        else
        {
            done.push_back(edit);
        }
        table = table.edited(buffer, edit);
        buffer =
            buffer.replaced(edit.at, Offset{edit.at.value + edit.removed.size()}, edit.inserted);
        if (words_of(table) != words_of(held(buffer)))
        {
            return false;
        }
    }
    return true;
}

// 性質の試験（ADR 0169 の決定 6）: ランダムな編集の列の後、増分で直した表は本文から作り直した表と
// 同じ（LF と CRLF の本文・undo の形の編集）。
void verify_incremental_property()
{
    bool agreed = true;
    for (std::uint64_t seed = 0; seed < property_seeds && agreed; ++seed)
    {
        agreed = property_case(seed);
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
    expect(forward_is("foobar fofooqux", 9, {"fooqux", "foobar"}) &&
               forward_is("bar ffoo", 5, {"foo"}) && forward_is("ab ababab", 5, {"abab", "ab"}) &&
               backward_is("zz fofooqux foobar", 5, {"foobar", "fooqux"}),
           "the rest of the typed word after the caret is matched once as a word start");
    expect(forward_is("foxfoo x", 2, {}) && forward_is("fozz x fozz", 9, {"fozz"}),
           "the rest after the caret must start with the prefix");
    expect(forward_is("aa\xCC\x81 aab2 aa", 12, {"aab2"}),
           "a prefix that ends inside a character with a combining mark does not match");
}

// 空の接頭辞の `\<\k\k` は 2 文字を食い、次の照合はその後ろから（Vim の実測）。
void verify_empty_prefix_chain()
{
    const std::string text = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE\xE8\xA3\x9C"
                             "\xE5\xAE\x8C\xE3\x81\xAF\xE4\xBE\xBF\xE5\x88\xA9\xE3\x81\xA7"
                             "\xE3\x81\x99 ";
    const std::vector<std::string> forward{"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "\xE3\x81\xAE",
                                           "\xE3\x81\xAF", "\xE3\x81\xA7\xE3\x81\x99"};
    expect(forward_is(text, text.size(), forward),
           "a word that starts inside the two characters of the previous match is not offered");
    const std::vector<std::string> backward{forward.rbegin(), forward.rend()};
    expect(backward_is(text, text.size(), backward), "the same chain holds for Ctrl-p");
    expect(forward_is("\xE6\x97\xA5_a ", 6, {"\xE6\x97\xA5"}),
           "a one-character word swallows the first character of the next word");
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
    static_cast<void>(controller.insert_completion(CompletionDirection::forward));
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

// 他のタブ（帯の位置 1・空）から空の接頭辞で引いた候補（他のタブの表の語・語の順の 100 個まで）。
[[nodiscard]] std::vector<std::string> parked_words(EditorController &controller)
{
    applied(controller, app::SwitchTab{1});
    std::vector<std::string> words =
        listed(controller.insert_completion(CompletionDirection::forward));
    applied(controller, app::SwitchTab{0});
    return words;
}

[[nodiscard]] std::vector<std::string> rebuilt_words(std::string_view text)
{
    std::vector<std::string> words;
    for (const core::WordCount &entry :
         held(buffer_of(text)).prefixed("", core::completion_candidate_limit))
    {
        words.push_back(entry.word);
    }
    return words;
}

// 1 歩の操作（打つ・消す・undo・redo）。
void random_step(EditorController &controller, Random &random)
{
    constexpr std::array<std::string_view, 8> typed{
        "ab", "foo", " ", "\n", "x", "\xE6\x97\xA5\xE6\x9C\xAC", "1", "."};
    const std::size_t choice = below(random, 6);
    if (choice == 0)
    {
        applied(controller,
                app::DeleteText{core::DeleteDirection::backward, core::DeleteUnit::character});
    }
    else if (choice == 1 || choice == 2)
    {
        applied(controller, app::HistoryAction{choice == 1 ? core::HistoryDirection::undo
                                                           : core::HistoryDirection::redo});
    }
    else
    {
        applied(controller, app::InsertText{std::string{typed.at(below(random, typed.size()))}});
    }
}

// controller の性質の試験（レビューの推奨 3）: 打つ・消す・undo・redo の列の後、状態の表は本文から
// 作り直した表と同じ。大きな貼り付けで捨てた後は表が無く、次に補完を使うと作り直す。
void verify_controller_property()
{
    Random random{property_seed};
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    applied(controller, app::NewTab{});
    applied(controller, app::SwitchTab{0});
    static_cast<void>(controller.insert_completion(CompletionDirection::forward));
    bool agreed = true;
    for (std::size_t step = 1; step <= controller_steps && agreed; ++step)
    {
        random_step(controller, random);
        if (step % 20 == 0)
        {
            agreed = parked_words(controller) == rebuilt_words(whole_vim_body(controller));
        }
    }
    expect(agreed, "the table in the state equals the table rebuilt from the text");
    applied(controller, app::InsertText{std::string(core::word_table_edit_limit + 1, 'q') + " "});
    expect(parked_words(controller).empty(), "a large paste drops the table");
    static_cast<void>(controller.insert_completion(CompletionDirection::forward));
    expect(parked_words(controller) == rebuilt_words(whole_vim_body(controller)),
           "the next completion rebuilds the table from the text");
}
} // namespace

void verify_insert_completion_contracts()
{
    verify_table();
    verify_size_limit();
    verify_incremental_property();
    verify_fold();
    verify_lazy_table();
    verify_vim_order();
    verify_empty_prefix_chain();
    verify_non_ascii();
    verify_limit();
    verify_scan_window();
    verify_other_tables();
    verify_controller_tabs();
    verify_controller_property();
}

void verify_insert_completion_scope()
{
    verify_insert_completion_contracts();
}
} // namespace nenenib::tests
