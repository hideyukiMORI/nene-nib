// scope `--vim-search` の単体テスト（ADR 0042 決定 2）。
#include "Appearance.hpp"
#include "Column.hpp"
#include "Editing.hpp"
#include "EditorController.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "ExEvaluationFailure.hpp"
#include "ExFailure.hpp"
#include "ExResult.hpp"
#include "InputLinePrompt.hpp"
#include "InputLineView.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextPosition.hpp"
#include "VimMatchRequest.hpp"
#include "VimMode.hpp"
#include "VimPattern.hpp"
#include "VimPatternFailure.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"
#include "VimSearch.hpp"
#include "VimSearchDirection.hpp"
#include "VimSearchHit.hpp"
#include "VimSearchNoticeKind.hpp"
#include "VimState.hpp"
#include "VimTestSupport.hpp"
#include "VimWordMotion.hpp"

#include "../vim/VimFixtures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
using nenenib::application::EditorFrame;
using nenenib::core::Column;
using nenenib::core::LineNumber;
using nenenib::core::Offset;
using nenenib::core::TextBuffer;
using nenenib::core::TextPosition;
using nenenib::core::vim_find_match;
using nenenib::core::vim_word_at;
using nenenib::core::VimMatchRequest;
using nenenib::core::VimMode;
using nenenib::core::VimPattern;
using nenenib::core::VimPatternFailure;
using nenenib::core::VimRegister;
using nenenib::core::VimRegisterKind;
using nenenib::core::VimSearchDirection;
using nenenib::core::VimSearchHit;

namespace core = nenenib::core;

// ---------------------------------------------------------------- 検索（Issue #100・ADR 0032）

// 照合器の答えは「行の中の一致の始まり」。パターンが部分集合の外なら検査そのものを落とす。
[[nodiscard]] std::optional<std::size_t> matched_begin(std::string_view pattern,
                                                       std::string_view line, std::size_t from)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    expect(parsed.has_value(), "the measured pattern is inside the supported subset");
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    const auto match = parsed.value().matched(line, from);
    return match.has_value() ? std::optional<std::size_t>{match.value().begin} : std::nullopt;
}

[[nodiscard]] std::optional<VimSearchHit> searched(std::string_view text, std::size_t from,
                                                   std::string_view pattern,
                                                   VimSearchDirection direction)
{
    const auto buffer = TextBuffer::from_utf8(text);
    const auto parsed = VimPattern::parse(pattern, direction);
    expect(buffer.has_value() && parsed.has_value(), "the searched body and pattern are valid");
    if (!buffer.has_value() || !parsed.has_value())
    {
        return std::nullopt;
    }
    const auto hit =
        vim_find_match(buffer.value(), parsed.value(),
                       VimMatchRequest{buffer.value().position_of(Offset{from}), direction, 1});
    return hit.has_value() ? std::optional<VimSearchHit>{hit.value()} : std::nullopt;
}

// 着いた位置を本文のバイト位置で読む（期待値をバイトで書くため）。
[[nodiscard]] Offset landed(std::string_view text, const VimSearchHit &hit)
{
    const auto buffer = TextBuffer::from_utf8(text);
    return buffer.has_value() ? buffer.value().offset_of(hit.position) : Offset{0};
}

[[nodiscard]] bool found_at(std::string_view text, std::size_t from, std::string_view pattern,
                            std::size_t expected)
{
    const auto hit = searched(text, from, pattern, VimSearchDirection::forward);
    return hit.has_value() && landed(text, hit.value()) == Offset{expected};
}

[[nodiscard]] bool found_back_at(std::string_view text, std::size_t from, std::string_view pattern,
                                 std::size_t expected)
{
    const auto hit = searched(text, from, pattern, VimSearchDirection::backward);
    return hit.has_value() && landed(text, hit.value()) == Offset{expected};
}

[[nodiscard]] bool rejects(std::string_view pattern, VimPatternFailure failure)
{
    const auto parsed = VimPattern::parse(pattern, VimSearchDirection::forward);
    return !parsed.has_value() && parsed.error() == failure;
}

// 決定 4 の部分集合。期待値は固定 Vim 9.1 の 218 ケース（out/issue100-oracle）から取った桁。
void verify_vim_pattern_subset()
{
    const std::string_view three = "one two three";
    expect(matched_begin("^four", "four five six", 0) == std::optional<std::size_t>{0},
           "the caret anchors at the start of the line");
    expect(!matched_begin("o^n", three, 0).has_value(),
           "the caret in the middle is just a character");
    expect(matched_begin("three$", three, 0) == std::optional<std::size_t>{8},
           "the dollar anchors at the end of the line");
    expect(!matched_begin("three$four", three, 0).has_value(),
           "the dollar in the middle is a character");
    expect(matched_begin("t.o", three, 0) == std::optional<std::size_t>{4},
           "a dot is one character");
    expect(matched_begin("f.*e", "four five six", 0) == std::optional<std::size_t>{0},
           "a star is greedy and backs off until the rest fits");
    expect(matched_begin("fo*bar", "foo foobar barfoo foo", 0) == std::optional<std::size_t>{4},
           "a star repeats the atom before it");
    expect(matched_begin("fz*oo", "foo foobar barfoo foo", 0) == std::optional<std::size_t>{0},
           "a star allows zero of the atom before it");
    const std::string_view mixed = "a1b2c3";
    expect(matched_begin("[abc]", mixed, 0) == std::optional<std::size_t>{0}, "a character set");
    expect(matched_begin("[a-c]1", mixed, 0) == std::optional<std::size_t>{0}, "a set range");
    expect(matched_begin("[^a-z]", mixed, 0) == std::optional<std::size_t>{1}, "a negated set");
    expect(matched_begin("[0-9]*c", mixed, 0) == std::optional<std::size_t>{3}, "a repeated set");
    expect(matched_begin("[", "[bracket] {brace}", 0) == std::optional<std::size_t>{0},
           "an unclosed set is the bracket itself");
    expect(matched_begin("\\[bracket", "[bracket] {brace}", 0) == std::optional<std::size_t>{0},
           "an escaped bracket is the character");
    expect(matched_begin("\\d", mixed, 0) == std::optional<std::size_t>{1}, "a digit class");
    expect(matched_begin("\\D", mixed, 0) == std::optional<std::size_t>{0}, "a non-digit class");
    expect(matched_begin("\\w", mixed, 0) == std::optional<std::size_t>{0}, "a word byte class");
    expect(matched_begin("\\W", "x.y.z", 0) == std::optional<std::size_t>{1}, "a non-word class");
    expect(matched_begin("\\s", "[bracket] {brace}", 0) == std::optional<std::size_t>{9},
           "a blank class");
    expect(matched_begin("\\S", " x", 0) == std::optional<std::size_t>{1}, "a non-blank class");
    expect(matched_begin("a\\*b", "a*b c", 0) == std::optional<std::size_t>{0},
           "an escaped star is the character");
    expect(matched_begin("a\\/b", "a/b c", 0) == std::optional<std::size_t>{0},
           "an escaped separator is the character");
    expect(matched_begin("a\\\\b", "a\\b c", 0) == std::optional<std::size_t>{0},
           "an escaped backslash is the character");
    expect(matched_begin("x\\.y", "x.y.z", 0) == std::optional<std::size_t>{0},
           "an escaped dot is the character");
    expect(matched_begin("foo", "Foo foo", 0) == std::optional<std::size_t>{4},
           "the search is case sensitive (noignorecase)");
}

// 語の境界は iskeyword ではなく文字の種類の表で切る（決定 4 の追記・ADR 0031 の決定 2）。
void verify_vim_pattern_word_boundaries()
{
    const std::string_view foos = "foo foobar barfoo foo";
    expect(matched_begin("\\<foo", foos, 0) == std::optional<std::size_t>{0}, "a word start");
    expect(matched_begin("\\<foo", foos, 1) == std::optional<std::size_t>{4},
           "a word start skips the middle of a word");
    expect(matched_begin("foo\\>", foos, 0) == std::optional<std::size_t>{0},
           "a word end holds where a blank follows");
    expect(matched_begin("foo\\>", foos, 1) == std::optional<std::size_t>{14},
           "a word end skips the middle of foobar");
    expect(matched_begin("\\<foo\\>", foos, 0) == std::optional<std::size_t>{0},
           "both anchors hold on a whole word");
    expect(matched_begin("\\<foo\\>", foos, 1) == std::optional<std::size_t>{18},
           "both anchors skip foobar and barfoo");
    expect(matched_begin("\\<ab\\>", "a_b ab", 0) == std::optional<std::size_t>{4},
           "the underscore is a word byte, so the first three bytes are one word");
    expect(matched_begin("\\<1\\>", "a1 1 a1", 0) == std::optional<std::size_t>{3},
           "digits are word bytes");
    // 種類が変われば境界になる。ADR 0032 の補足が「一致しない」と書いた例は実測では一致する。
    const std::string_view kana_kanji =
        "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe6\xbc\xa2\xe5\xad\x97";
    expect(matched_begin("\\<\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\\>", kana_kanji, 0) ==
               std::optional<std::size_t>{0},
           "kana followed by kanji is a class change, so the word ends there");
    expect(matched_begin("\\<\xe6\xbc\xa2\xe5\xad\x97", kana_kanji, 0) ==
               std::optional<std::size_t>{9},
           "the same class change also starts a word");
}

// 未対応の構文は黙って別の意味にせず、閉じた失敗で拒否する（決定 4）。
void verify_vim_pattern_rejections()
{
    expect(rejects("\\(foo\\)", VimPatternFailure::group), "a group is refused");
    expect(rejects("foo\\|bar", VimPatternFailure::branch), "a branch is refused");
    expect(rejects("fo\\{2}", VimPatternFailure::quantifier), "a brace count is refused");
    expect(rejects("fo\\+", VimPatternFailure::quantifier), "one or more is refused");
    expect(rejects("fo\\=", VimPatternFailure::quantifier), "an optional atom is refused");
    expect(rejects("fo\\?", VimPatternFailure::quantifier), "its other spelling too");
    expect(rejects("\\v(foo)", VimPatternFailure::magic), "very magic is refused");
    expect(rejects("\\Mfoo", VimPatternFailure::magic), "nomagic is refused");
    expect(rejects("\\cfoo", VimPatternFailure::ignore_case), "a case switch is refused");
    expect(rejects("\\Cfoo", VimPatternFailure::ignore_case), "both case switches are refused");
    expect(rejects("\\%(foo", VimPatternFailure::escape), "a percent escape is refused");
    expect(rejects("\\_foo", VimPatternFailure::escape), "a multiline escape is refused");
    expect(rejects("alpha\\r$", VimPatternFailure::escape), "a carriage return escape is refused");
    expect(rejects("foo\\", VimPatternFailure::escape), "a trailing backslash is refused");
    expect(rejects("~", VimPatternFailure::previous_substitute),
           "the previous substitute is refused");
    expect(rejects("foo/e", VimPatternFailure::offset), "a forward offset is refused");
    const auto backward = VimPattern::parse("foo?e", VimSearchDirection::backward);
    expect(!backward.has_value() && backward.error() == VimPatternFailure::offset,
           "a backward offset is refused at its own separator");
    const auto question = VimPattern::parse("a?b", VimSearchDirection::forward);
    expect(question.has_value() && question.value().matched("a?b c", 0).has_value(),
           "a question mark is a plain character in a forward search");
}

// 走査の規則（決定 4 の追記）。必要な桁に足りない一致は終端まで飛ばし、行の長さで打ち切る。
void verify_vim_search_scan()
{
    expect(found_at("aaaa", 0, "aa", 2), "four a characters find the overlap at the third column");
    expect(found_at("aaaaaa", 2, "aa", 4), "the scan restarts from the head of the line");
    const auto overlap = searched("ababa", 0, "aba", VimSearchDirection::forward);
    expect(overlap.has_value() && landed("ababa", overlap.value()) == Offset{0} &&
               overlap.value().wrapped,
           "skipping to the end of the match passes over the overlap and wraps");
    expect(found_at("abc", 0, ".*", 0), "an always-matching pattern does not move");
    expect(found_at("abc\ndef", 0, ".*", 4), "it does move to the next line");
    expect(found_at("abc\ndef", 0, "^a*", 4), "a zero-length match on the next line");
    expect(found_at("abc", 0, "a*", 1), "a zero-length match steps one character");
    expect(found_at("abc", 1, "a*", 2), "and the next one steps again");
    expect(found_at("alpha beta gamma", 0, "$", 16), "the dollar matches at the end of the line");
    expect(found_at("xa xa", 0, "xa", 3), "the second match on the same line");
    expect(found_at("xa xa", 3, "xa", 0), "and then it wraps to the first");
    expect(found_back_at("aaaa", 3, "a", 2), "backwards takes the last match before the cursor");
    expect(found_back_at("aaaa", 3, "aa", 2), "overlapping matches count backwards too");
    expect(found_back_at("xa xa", 3, "xa", 0), "a match at the cursor is not a backward match");
}

// 折り返しと向き（決定 3・4）。wrapscan は既定どおり有効で、越えたときだけ報せが出る。
void verify_vim_search_wrap()
{
    const std::string_view body = "alpha beta\nbeta gamma\ndelta beta";
    expect(found_at(body, 0, "beta", 6), "the first match after the caret");
    expect(found_at(body, 6, "beta", 11), "the next match is on the next line");
    const auto wrapped = searched(body, 31, "alpha", VimSearchDirection::forward);
    expect(wrapped.has_value() && landed(body, wrapped.value()) == Offset{0} &&
               wrapped.value().wrapped,
           "a forward search wraps from the bottom and says so");
    const auto plain = searched(body, 0, "beta", VimSearchDirection::forward);
    expect(plain.has_value() && !plain.value().wrapped, "a match below the caret does not wrap");
    expect(found_back_at(body, 31, "beta", 28), "a backward search stays on the line");
    const auto back = searched(body, 0, "gamma", VimSearchDirection::backward);
    expect(back.has_value() && landed(body, back.value()) == Offset{16} && back.value().wrapped,
           "a backward search wraps from the top and says so");
    expect(!searched(body, 0, "zzz", VimSearchDirection::forward).has_value(),
           "nothing is found for a pattern that is not there");
    expect(!searched(body, 0, "three.four", VimSearchDirection::forward).has_value(),
           "a match never crosses a line");
    const auto only = searched("solo word", 0, "solo", VimSearchDirection::forward);
    expect(only.has_value() && landed("solo word", only.value()) == Offset{0} &&
               only.value().wrapped,
           "the only match is the one under the caret, found by wrapping");
}

// UTF-8 と CRLF（決定 4・6）。CRLF は fixture にできないのでここで守る。
void verify_vim_search_encoding()
{
    const std::string_view kana = "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86 \xe3\x81\x8b\xe3\x81\x8d"
                                  "\xe3\x81\x8f\n\xe6\xbc\xa2\xe5\xad\x97 \xe3\x81\x82\xe3\x81\x84"
                                  "\xe3\x81\x86";
    expect(found_at(kana, 0, "\xe3\x81\x82.\xe3\x81\x86", 27),
           "a dot matches one multibyte character");
    expect(found_at(kana, 0, "\xe6\xbc\xa2\xe5\xad\x97", 20), "a multibyte literal");
    expect(found_at(kana, 0, "[\xe3\x81\x82\xe3\x81\x8b]", 10), "a multibyte character set");
    expect(found_at(kana, 0, "\xe3\x81\x8f$", 16), "the dollar after a multibyte character");
    expect(found_at(kana, 0, "\\<\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\\>", 27),
           "word anchors around a multibyte word");
    // CRLF の本文では行の内容の終わりが CR の前なので、錨は CR に当たらない（保存形は変わらない）。
    expect(found_at("alpha\r\nbeta", 0, "beta", 7), "a search over CRLF finds the next line");
    const auto anchored = searched("alpha\r\nbeta", 0, "alpha$", VimSearchDirection::forward);
    expect(anchored.has_value() && landed("alpha\r\nbeta", anchored.value()) == Offset{0},
           "the dollar sits before the carriage return");
}

[[nodiscard]] std::string word_under(std::string_view text, std::size_t at)
{
    const auto buffer = TextBuffer::from_utf8(text);
    expect(buffer.has_value(), "the word body is valid UTF-8");
    if (!buffer.has_value())
    {
        return {};
    }
    const auto range = vim_word_at(buffer.value(), Offset{at});
    if (!range.has_value())
    {
        return {};
    }
    return buffer.value().text_range(range.value().begin, range.value().end);
}

// `*` / `#` が使う語（決定 3）。空白と記号の上では同じ行の次の語で、行に語が無ければ無い。
void verify_vim_search_word()
{
    expect(word_under("foo bar\nbaz foo", 0) == "foo", "the word under the caret");
    expect(word_under("foo foobar barfoo foo", 1) == "foo",
           "the caret inside a word takes all of it");
    expect(word_under("alpha beta gamma", 5) == "beta", "a blank takes the next word on the line");
    expect(word_under("a.b x a.b", 1) == "b", "punctuation takes the next word too");
    expect(word_under("[bracket] {brace}", 0) == "bracket", "and so does a bracket");
    expect(word_under("   \nfoo", 0).empty(), "a blank line has no word (it does not cross lines)");
    expect(word_under("beta gamma beta  ", 16).empty(), "trailing blanks have no word after them");
    expect(word_under("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe6\xbc\xa2\xe5\xad\x97", 0) ==
               "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86",
           "the word is the run of one character class");
    expect(word_under("a1_b c a1_b", 0) == "a1_b", "digits and underscores are one word");
    expect(word_under("foo-bar x foo-bar", 3) == "bar", "the hyphen takes the word after it");
}

[[nodiscard]] bool message_is(const EditorFrame &frame, std::string_view text)
{
    return frame.command_message.has_value() && frame.command_message.value().text() == text;
}

[[nodiscard]] bool register_is(const EditorController &controller, std::string_view text,
                               std::string_view kind)
{
    return controller.vim_state().unnamed_register.text == text &&
           vim_register_kind(controller.vim_state().unnamed_register) == kind;
}

// 決定 3 の到達位置・向き・回数。
void verify_vim_search_keys()
{
    const std::string body = "alpha beta\nbeta gamma\ndelta beta";
    Editing forward;
    open_vim_document(forward, body);
    EditorController &first = forward.controller();
    vim_replay(first, "/beta<CR>");
    expect(caret_at(first.frame(), 1, 7) && vim_body(first.frame()) == body,
           "a forward search moves to the next match and leaves the body alone");
    vim_replay(first, "n");
    expect(caret_at(first.frame(), 2, 1), "n keeps the direction");
    vim_replay(first, "N");
    expect(caret_at(first.frame(), 1, 7), "N takes the opposite direction");
    vim_replay(first, "/<CR>");
    expect(caret_at(first.frame(), 2, 1), "an empty pattern reuses the previous one");
    Editing backward;
    open_vim_document(backward, body);
    EditorController &second = backward.controller();
    vim_replay(second, "G$?beta<CR>");
    expect(caret_at(second.frame(), 3, 7), "a backward search moves to the previous match");
    vim_replay(second, "n");
    expect(caret_at(second.frame(), 2, 1), "n after ? keeps going backwards");
    vim_replay(second, "N");
    expect(caret_at(second.frame(), 3, 7), "N after ? goes forwards");
    Editing counted;
    open_vim_document(counted, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &third = counted.controller();
    vim_replay(third, "3/aaa<CR>");
    expect(caret_at(third.frame(), 1, 1), "a count takes the third match, which wraps");
    Editing repeated;
    open_vim_document(repeated, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = repeated.controller();
    vim_replay(fourth, "/aaa<CR>3n");
    expect(caret_at(fourth.frame(), 2, 5), "a count on n multiplies the same way");
}

// 決定 3 のオペレータ（exclusive）と VISUAL の端点。
void verify_vim_search_operators()
{
    Editing removed;
    open_vim_document(removed, "alpha beta gamma");
    EditorController &first = removed.controller();
    vim_replay(first, "d/gamma<CR>");
    expect(vim_body(first.frame()) == "gamma" && register_is(first, "alpha beta ", "v") &&
               caret_at(first.frame(), 1, 1),
           "an operator takes the range up to the match (exclusive)");
    vim_replay(first, "u");
    expect(vim_body(first.frame()) == "alpha beta gamma", "and it is one undo unit");
    Editing yanked;
    open_vim_document(yanked, "alpha beta gamma");
    EditorController &second = yanked.controller();
    vim_replay(second, "y/gamma<CR>");
    expect(vim_body(second.frame()) == "alpha beta gamma" &&
               register_is(second, "alpha beta ", "v"),
           "a yank through a search leaves the body alone");
    Editing changed;
    open_vim_document(changed, "alpha beta gamma");
    EditorController &third = changed.controller();
    vim_replay(third, "c/gamma<CR>ZZ<Esc>");
    expect(vim_body(third.frame()) == "ZZgamma", "a change through a search opens INSERT");
    Editing linewise;
    open_vim_document(linewise, "foo bar\nbaz qux");
    EditorController &fourth = linewise.controller();
    vim_replay(fourth, "d/baz<CR>");
    expect(vim_body(fourth.frame()) == "baz qux" && register_is(fourth, "foo bar\n", "V"),
           "a range that ends in column 1 from the indent becomes linewise");
    Editing shortened;
    open_vim_document(shortened, "foo bar\nbaz qux");
    EditorController &fifth = shortened.controller();
    vim_replay(fifth, "4ld/baz<CR>");
    expect(vim_body(fifth.frame()) == "foo \nbaz qux" && register_is(fifth, "bar", "v"),
           "and from the middle of the line it ends at the end of the previous line");
    Editing counted;
    open_vim_document(counted, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &sixth = counted.controller();
    vim_replay(counted.controller(), "2d/aaa<CR>");
    expect(register_is(sixth, "aaa bbb\nccc aaa\nddd ", "v"),
           "the operator count survives the input line");
    Editing empty;
    open_vim_document(empty, "foo bar");
    EditorController &seventh = empty.controller();
    vim_replay(seventh, "d/foo<CR>");
    expect(vim_body(seventh.frame()) == "foo bar" && register_is(seventh, "", ""),
           "a range that wraps onto itself changes neither the body nor the register");
    Editing visual;
    open_vim_document(visual, "alpha beta gamma");
    EditorController &eighth = visual.controller();
    vim_replay(eighth, "v/gamma<CR>");
    expect(eighth.frame().vim_mode == VimMode::visual, "VISUAL stays VISUAL while searching");
    vim_replay(eighth, "d");
    expect(vim_body(eighth.frame()) == "amma", "and the search moved the end of the selection");
    Editing visual_line;
    open_vim_document(visual_line, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &ninth = visual_line.controller();
    vim_replay(ninth, "V/gamma<CR>d");
    expect(vim_body(ninth.frame()) == "delta beta", "a line VISUAL takes whole lines");
}

// 決定 3 の `*` / `#`。語の先頭から探すが、オペレータの範囲は元のキャレットから。
void verify_vim_search_word_keys()
{
    Editing star;
    open_vim_document(star, "foo bar\nbaz foo\nfoo qux");
    EditorController &first = star.controller();
    vim_replay(first, "*");
    expect(caret_at(first.frame(), 2, 5), "the star searches the word under the caret");
    vim_replay(first, "n");
    expect(caret_at(first.frame(), 3, 1), "and n keeps that pattern");
    Editing hash;
    open_vim_document(hash, "foo bar\nbaz foo\nfoo qux");
    EditorController &second = hash.controller();
    vim_replay(second, "G#");
    expect(caret_at(second.frame(), 2, 5), "the hash searches backwards");
    Editing bounded;
    open_vim_document(bounded, "foo foobar barfoo foo");
    EditorController &third = bounded.controller();
    vim_replay(third, "*");
    expect(caret_at(third.frame(), 1, 19),
           "the word is anchored, so foobar and barfoo are skipped");
    Editing pending;
    open_vim_document(pending, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = pending.controller();
    vim_replay(fourth, "d*");
    expect(vim_body(fourth.frame()) == "aaa\nddd aaa eee" &&
               register_is(fourth, "aaa bbb\nccc ", "v"),
           "an operator before the star takes the range to the match");
    Editing anchored;
    open_vim_document(anchored, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fifth = anchored.controller();
    vim_replay(fifth, "G$d#");
    expect(vim_body(fifth.frame()) == "aaa bbb\nccc aaa\nddd aaa e" &&
               register_is(fifth, "ee", "v"),
           "the range ends at the original caret, not at the start of the word");
}

// 決定 5 の報せ。Vim にある文言は Vim のまま、未対応構文の拒否だけが本実装のもの。
void verify_vim_search_messages()
{
    Editing missing;
    open_vim_document(missing, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &first = missing.controller();
    vim_replay(first, "/zzz<CR>");
    expect(message_is(first.frame(), "E486: Pattern not found: zzz") &&
               caret_at(first.frame(), 1, 1),
           "a pattern that is not there says so and does not move");
    vim_replay(first, "n");
    expect(message_is(first.frame(), "E486: Pattern not found: zzz"),
           "a failed search is still remembered, so n repeats the same failure");
    vim_replay(first, "x");
    expect(!first.frame().command_message.has_value(), "the next key clears the message");
    Editing fresh;
    open_vim_document(fresh, "alpha beta");
    EditorController &second = fresh.controller();
    vim_replay(second, "n");
    expect(message_is(second.frame(), "E35: No previous regular expression"),
           "n without a previous pattern says so");
    vim_replay(second, "/<CR>");
    expect(message_is(second.frame(), "E35: No previous regular expression"),
           "and so does an empty pattern");
    Editing wordless;
    open_vim_document(wordless, "   \nfoo");
    EditorController &third = wordless.controller();
    vim_replay(third, "*");
    expect(message_is(third.frame(), "E348: No string under cursor"),
           "the star on a blank line says there is no word");
    Editing wrapped;
    open_vim_document(wrapped, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &fourth = wrapped.controller();
    vim_replay(fourth, "G$/alpha<CR>");
    expect(message_is(fourth.frame(), "search hit BOTTOM, continuing at TOP") &&
               caret_at(fourth.frame(), 1, 1),
           "a forward search that wraps says so");
    Editing backwards;
    open_vim_document(backwards, "alpha beta\nbeta gamma\ndelta beta");
    EditorController &fifth = backwards.controller();
    vim_replay(fifth, "?gamma<CR>");
    expect(message_is(fifth.frame(), "search hit TOP, continuing at BOTTOM") &&
               caret_at(fifth.frame(), 2, 6),
           "and so does a backward one");
    Editing refused;
    open_vim_document(refused, "foo foobar");
    EditorController &sixth = refused.controller();
    vim_replay(sixth, "/\\(foo\\)<CR>");
    expect(message_is(sixth.frame(), "Unsupported pattern item: \\( \\)") &&
               caret_at(sixth.frame(), 1, 1),
           "an unsupported item is refused instead of meaning something else");
    vim_replay(sixth, "/foo/e<CR>");
    expect(message_is(sixth.frame(), "Search offset is not supported"),
           "and so is a search offset");
}

// 決定 1 の入力行と取消。入力中に本文は変わらず、Esc と空の Backspace で取消になる。
void verify_vim_search_input()
{
    Editing editing;
    open_vim_document(editing, "alpha beta gamma");
    EditorController &controller = editing.controller();
    vim_replay(controller, "2l/fo");
    const auto typing = controller.frame();
    expect(typing.command_line.has_value() &&
               typing.command_line.value().prompt == nenenib::core::InputLinePrompt::search_forward,
           "the search input line is drawn with its own prompt");
    const auto shown = typing.command_line.value_or(nenenib::core::InputLineView{});
    expect(shown.text == "fo" && shown.completions.empty(),
           "the typed pattern is in the input line and there are no completions");
    expect(vim_body(typing) == "alpha beta gamma" && caret_at(typing, 1, 3),
           "typing a pattern changes neither the body nor the caret");
    vim_replay(controller, "<Esc>");
    expect(!controller.frame().command_line.has_value() && caret_at(controller.frame(), 1, 3),
           "Esc closes the input line without searching");
    expect(!controller.vim_state().last_search.has_value(), "a cancelled search is not remembered");
    vim_replay(controller, "/<BS>");
    expect(!controller.frame().command_line.has_value() && caret_at(controller.frame(), 1, 3),
           "Backspace on an empty input line cancels too");
    vim_replay(controller, "d/<Esc>x");
    expect(vim_body(controller.frame()) == "alha beta gamma",
           "a cancelled search drops the pending operator and the next key acts alone");
    Editing backwards;
    open_vim_document(backwards, "alpha beta gamma");
    EditorController &second = backwards.controller();
    vim_replay(second, "?al");
    expect(second.frame().command_line.value_or(nenenib::core::InputLineView{}).prompt ==
               nenenib::core::InputLinePrompt::search_backward,
           "the backward search has the other prompt");
}

// 決定 3 の `.`。検索は鍵 1 つとして記録に乗るので、追加の仕掛けは要らない（ADR 0030）。
void verify_vim_search_dot()
{
    Editing operated;
    open_vim_document(operated, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &first = operated.controller();
    vim_replay(first, "d/bbb<CR>j0.");
    expect(vim_body(first.frame()) == "ccc aaa\nddd aaa eee" && register_is(first, "bbb\n", "V"),
           ". replays the search as one key");
    Editing starred;
    open_vim_document(starred, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &second = starred.controller();
    vim_replay(second, "*dn.");
    expect(vim_body(second.frame()) == "aaa eee" && register_is(second, "aaa bbb\nccc ", "v"),
           ". replays an operator over n");
    Editing pending;
    open_vim_document(pending, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &third = pending.controller();
    vim_replay(third, "d*.");
    expect(vim_body(third.frame()) == "aaa eee" && register_is(third, "aaa\nddd ", "v"),
           ". replays the star as the key it is");
    Editing plain;
    open_vim_document(plain, "aaa bbb\nccc aaa\nddd aaa eee");
    EditorController &fourth = plain.controller();
    vim_replay(fourth, "x/aaa<CR>.");
    expect(vim_body(fourth.frame()) == "aa bbb\nccc aa\nddd aaa eee",
           "a plain search is a move, so . still replays the last change");
}

void verify_vim_search_fixtures()
{
    // 検索の 135 件と、鍵の分類の表・次キー待ちを共有する境界の代表（`.`・r・f/t・gg）。
    constexpr std::array<std::string_view, 6> boundaries{
        "replace-char-one",    "replace-char-count", "char-search-f-count",
        "char-search-t-first", "line-jump-gg-count", "dot-remove-word"};
    std::size_t selected = 0;
    for (const VimFixture &fixture : nenenib::tests::vim_fixtures)
    {
        if (fixture.name.starts_with("search-") ||
            std::ranges::find(boundaries, fixture.name) != boundaries.end())
        {
            verify_vim_fixture(fixture);
            ++selected;
        }
    }
    expect(selected == 141, "the scope replays 135 search fixtures and 6 shared boundaries");
}

// 次の一致を求める 1 本の経路（ADR 0041 の決定 1）。確定の検索の鍵と incsearch の preview が
// 同じ関数を呼ぶので、位置は行と桁（桁は文字で 1 から）で返り、回数と折り返しもここで決まる。
[[nodiscard]] std::expected<VimSearchHit, core::VimSearchNoticeKind>
match_found(std::string_view text, std::string_view pattern, const VimMatchRequest &request)
{
    const auto buffer = TextBuffer::from_utf8(text);
    const auto parsed = VimPattern::parse(pattern, request.direction);
    expect(buffer.has_value() && parsed.has_value(), "the matched body and pattern are valid");
    if (!buffer.has_value() || !parsed.has_value())
    {
        return std::unexpected(core::VimSearchNoticeKind::pattern_not_found);
    }
    return vim_find_match(buffer.value(), parsed.value(), request);
}

[[nodiscard]] TextPosition at_position(std::size_t line, std::size_t column)
{
    return TextPosition{LineNumber{line}, Column{column}};
}

void expect_match(const std::expected<VimSearchHit, core::VimSearchNoticeKind> &hit,
                  TextPosition expected, bool wrapped, const char *what)
{
    expect(hit.has_value() && hit.value().position == expected, what);
    expect(hit.has_value() && hit.value().wrapped == wrapped, what);
}

void expect_no_match(const std::expected<VimSearchHit, core::VimSearchNoticeKind> &hit,
                     const char *what)
{
    expect(!hit.has_value() && hit.error() == core::VimSearchNoticeKind::pattern_not_found, what);
}

void verify_vim_find_match()
{
    using enum VimSearchDirection;
    const std::string_view body = "alpha beta\nbeta gamma\ndelta beta";
    const std::string_view kana = "\xe3\x81\x82\xe3\x81\x84 \xe3\x81\x8b\n\xe6\xbc\xa2";
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 1}), at_position(1, 7),
                 false, "forward finds the first match after the caret");
    expect_match(match_found(body, "beta", {at_position(1, 7), forward, 1}), at_position(2, 1),
                 false, "a match under the caret is skipped and the next line is found");
    expect_match(match_found("xa xa", "xa", {at_position(1, 1), forward, 1}), at_position(1, 4),
                 false, "the next match on the same line");
    expect_match(match_found(body, "beta", {at_position(3, 7), backward, 1}), at_position(2, 1),
                 false, "backward finds the last match before the caret");
    expect_match(match_found(body, "beta", {at_position(3, 10), backward, 1}), at_position(3, 7),
                 false, "backward stays on the line when a match is before the caret");
    expect_match(match_found(body, "alpha", {at_position(3, 7), forward, 1}), at_position(1, 1),
                 true, "forward wraps from the bottom to the top");
    expect_match(match_found(body, "gamma", {at_position(1, 1), backward, 1}), at_position(2, 6),
                 true, "backward wraps from the top to the bottom");
    expect_match(match_found("solo word", "solo", {at_position(1, 1), forward, 1}),
                 at_position(1, 1), true, "the only match is reached again by wrapping");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 2}), at_position(2, 1),
                 false, "a count of two moves twice");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 3}), at_position(3, 7),
                 false, "a count of three reaches the last line");
    expect_match(match_found(body, "beta", {at_position(1, 1), forward, 4}), at_position(1, 7),
                 true, "a count that passes the end wraps and says so");
    expect_match(match_found(body, "beta", {at_position(3, 7), backward, 2}), at_position(1, 7),
                 false, "a backward count of two moves twice");
    expect_match(match_found("solo word", "solo", {at_position(1, 1), forward, 2}),
                 at_position(1, 1), true, "a count keeps wrapping back to the only match");
    expect_match(match_found(kana, "\xe3\x81\x8b", {at_position(1, 1), forward, 1}),
                 at_position(1, 4), false, "the column counts characters, not bytes");
    expect_match(match_found(kana, "\xe6\xbc\xa2", {at_position(1, 4), forward, 1}),
                 at_position(2, 1), false, "a multibyte match on the next line");
    expect_match(match_found("alpha\r\nbeta", "beta", {at_position(1, 1), forward, 1}),
                 at_position(2, 1), false, "CRLF lines are searched by their content");
    expect_no_match(match_found(body, "zzz", {at_position(1, 1), forward, 1}),
                    "a pattern that is not there is pattern_not_found");
    expect_no_match(match_found(body, "zzz", {at_position(1, 1), backward, 3}),
                    "a count does not hide a missing pattern");
    expect_no_match(match_found(body, "three.four", {at_position(1, 1), forward, 1}),
                    "a match never crosses a line");
    expect_no_match(match_found("", "a", {at_position(1, 1), forward, 1}),
                    "an empty body has no match");
    expect_no_match(match_found("", "a", {at_position(1, 1), backward, 2}),
                    "an empty body has no match backwards either");
}

// `:set incsearch` / `:set noincsearch`（ADR 0041 の決定 6）。hlsearch と同じ経路で評価し、
// 設定にも強調にも触れない。既定は on で、Vim の鍵を食べ終えても持ち越す。
void verify_ex_incsearch_commands()
{
    const auto settings = core::default_editor_settings();
    const auto on = core::evaluate_ex("set incsearch", settings, Appearance::dark);
    expect(on.has_value() && on.value().incsearch == std::optional<bool>{true} &&
               !on.value().settings.has_value() && !on.value().highlight.has_value() &&
               on.value().message.text() == "incsearch=on",
           ":set incsearch turns the preview on without touching the settings");
    const auto off = core::evaluate_ex("set noincsearch", settings, Appearance::dark);
    expect(off.has_value() && off.value().incsearch == std::optional<bool>{false} &&
               off.value().message.text() == "incsearch=off",
           ":set noincsearch turns it off");
    const auto again = core::evaluate_ex("set incsearch", settings, Appearance::dark);
    expect(again.has_value() && again.value().incsearch == std::optional<bool>{true},
           ":set incsearch turns it back on");
    const auto typo = core::evaluate_ex("set incserch", settings, Appearance::dark);
    expect(!typo.has_value() &&
               typo.error() == core::ExEvaluationFailure{core::ExFailure::unknown_option},
           "a misspelled option is the existing unknown option failure");
    const auto theme = core::evaluate_ex("colorscheme dracula", settings, Appearance::dark);
    expect(theme.has_value() && !theme.value().incsearch.has_value(),
           "commands that are not about incsearch leave it alone");
    const auto completions = core::command_completions("set noi");
    expect(completions.size() == 1 && completions.front() == "set noincsearch",
           "the option is offered as a completion");
    auto state =
        nenenib::core::vim_resting_state(VimRegister{std::string{}, VimRegisterKind::characters});
    expect(state.incsearch, "incsearch is on by default");
    state.incsearch = false;
    const auto rested = nenenib::core::vim_resting_from(
        state, VimRegister{std::string{}, VimRegisterKind::characters});
    expect(!rested.incsearch, "a finished key keeps incsearch as it was");
}
} // namespace

void verify_vim_search_contracts()
{
    verify_vim_find_match();
    verify_ex_incsearch_commands();
    verify_vim_pattern_subset();
    verify_vim_pattern_word_boundaries();
    verify_vim_pattern_rejections();
    verify_vim_search_scan();
    verify_vim_search_wrap();
    verify_vim_search_encoding();
    verify_vim_search_word();
    verify_vim_search_keys();
    verify_vim_search_operators();
    verify_vim_search_word_keys();
    verify_vim_search_messages();
    verify_vim_search_input();
    verify_vim_search_dot();
}

void verify_vim_search_scope()
{
    verify_vim_search_fixtures();
    verify_vim_search_contracts();
}
} // namespace nenenib::tests
