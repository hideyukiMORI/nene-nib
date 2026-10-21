// scope `--lexer` の単体テスト（#815・段 2 の ADR 0156 の決定 1〜6）。core の lex_line が C / C++
// の 1 行を役割の区間に分け、行をまたぐ状態（ブロックコメント・raw string）を値で次の行へ渡すこと。
// 期待の区間は「塗り」の文字列で書く: 行の code point 1 つに 1 文字で、c comment・k keyword・
// s string・n number・t type・f function・v constant・o operators・e error・空白は区間なし。
// 大文字は同じ役割の区間をそこで新しく始める（隣り合う 2 つの文字列など）。塗りが行より短ければ
// 残りは区間なし。桁は SelectionSpan と同じ code point の序数（行頭が Column{1}）。
#include "ColorSpan.hpp"
#include "Column.hpp"
#include "Language.hpp"
#include "LexKind.hpp"
#include "LexLine.hpp"
#include "LexState.hpp"
#include "Lexer.hpp"
#include "Scopes.hpp"
#include "SyntaxRole.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::ColorSpan;
using core::Column;
using core::Language;
using core::LexKind;
using core::LexLine;
using core::LexState;
using core::SyntaxRole;

struct LexCase
{
    std::string_view line;
    LexState in;
    std::string_view paint;
    LexState out;
};

constexpr LexState plain{};
constexpr LexState block{.kind = LexKind::block_comment, .delimiter = {}, .delimiter_length = 0U};

[[nodiscard]] constexpr LexState raw(std::string_view delimiter)
{
    LexState state{.kind = LexKind::raw_string,
                   .delimiter = {},
                   .delimiter_length = static_cast<std::uint8_t>(delimiter.size())};
    std::ranges::copy(delimiter, state.delimiter.begin());
    return state;
}

constexpr LexState raw_xyz = raw("xyz");
constexpr LexState raw_empty = raw("");
constexpr LexState raw_sixteen = raw("abcdefghijklmnop");

[[nodiscard]] std::optional<SyntaxRole> role_of(char letter)
{
    constexpr std::array<std::pair<char, SyntaxRole>, 9> letters{{{'c', SyntaxRole::comment},
                                                                  {'k', SyntaxRole::keyword},
                                                                  {'s', SyntaxRole::string},
                                                                  {'n', SyntaxRole::number},
                                                                  {'t', SyntaxRole::type},
                                                                  {'f', SyntaxRole::function},
                                                                  {'v', SyntaxRole::constant},
                                                                  {'o', SyntaxRole::operators},
                                                                  {'e', SyntaxRole::error}}};
    const char lower = static_cast<char>(letter | '\x20');
    for (const auto &[mark, role] : letters)
    {
        if (lower == mark && letter != ' ')
        {
            return role;
        }
    }
    return std::nullopt;
}

// 塗りの文字列を区間の列に写す。
[[nodiscard]] std::vector<ColorSpan> painted(std::string_view paint)
{
    std::vector<ColorSpan> spans;
    for (std::size_t index = 0; index < paint.size(); ++index)
    {
        const std::optional<SyntaxRole> role = role_of(paint[index]);
        if (!role.has_value())
        {
            continue;
        }
        const Column column{index + 1U};
        const bool fresh = paint[index] >= 'A' && paint[index] <= 'Z';
        if (!fresh && !spans.empty() && spans.back().end == column &&
            spans.back().role == role.value())
        {
            spans.back().end = Column{index + 2U};
            continue;
        }
        spans.push_back(ColorSpan{column, Column{index + 2U}, role.value()});
    }
    return spans;
}

[[nodiscard]] std::size_t code_points(std::string_view line)
{
    return static_cast<std::size_t>(
        std::ranges::count_if(line, [](char value) { return (value & '\xC0') != '\x80'; }));
}

// 区間は空でなく、昇順で重ならず、行の中に収まる。
[[nodiscard]] bool well_formed(const std::vector<ColorSpan> &spans, std::string_view line)
{
    std::size_t floor = 1U;
    for (const ColorSpan &span : spans)
    {
        if (span.begin.value < floor || !(span.begin < span.end))
        {
            return false;
        }
        floor = span.end.value;
    }
    return floor <= code_points(line) + 1U;
}

// 失敗の報告のために、区間の列を塗りの文字列へ戻す（区間の始まりは大文字）。
[[nodiscard]] std::string paint_of(const std::vector<ColorSpan> &spans)
{
    constexpr std::string_view letters = "cksntfvoew";
    std::string paint;
    for (const ColorSpan &span : spans)
    {
        paint.resize(span.end.value - 1U, ' ');
        const char letter = letters[static_cast<std::size_t>(span.role)];
        for (std::size_t column = span.begin.value; column < span.end.value; ++column)
        {
            paint.at(column - 1U) =
                column == span.begin.value ? static_cast<char>(letter - ('a' - 'A')) : letter;
        }
    }
    return paint;
}

// 表の 1 行を C++ と C で読み、区間・出る状態・形を確かめる。
void verify_case(const LexCase &sample)
{
    const LexLine cpp = core::lex_line(Language::cpp, sample.in, sample.line);
    const LexLine c = core::lex_line(Language::c, sample.in, sample.line);
    const bool spans_match = cpp.spans == painted(sample.paint);
    const bool out_matches = cpp.out == sample.out;
    if (!spans_match || !out_matches)
    {
        const std::string actual = paint_of(cpp.spans);
        std::fprintf(stderr, "lexer case: \"%.*s\"\n  painted: \"%s\"\n",
                     static_cast<int>(sample.line.size()), sample.line.data(), actual.c_str());
    }
    expect(spans_match, "lex_line paints the expected spans");
    expect(out_matches, "lex_line leaves the expected state");
    expect(c.spans == cpp.spans && c.out == cpp.out, "C and C++ share one lexer");
    expect(core::lex_state(Language::cpp, sample.in, sample.line) == cpp.out &&
               core::lex_state(Language::c, sample.in, sample.line) == cpp.out,
           "lex_state leaves the same state as lex_line");
    expect(well_formed(cpp.spans, sample.line),
           "spans are ascending, disjoint and inside the line");
}

// コメント 10・文字列と raw string と行またぎ 18。
constexpr std::array<LexCase, 28> comment_and_string_cases{{
    {"// hello", plain, "cccccccc", plain},
    {"int x; // note", plain, "ttt  o ccccccc", plain},
    {"/* a */ int", plain, "ccccccc ttt", plain},
    {"/* open", plain, "ccccccc", block},
    {"still inside", block, "cccccccccccc", block},
    {"end */ x = 1;", block, "cccccc   o no", plain},
    {"", block, "", block},
    {"/**/ /*/ x */", plain, "cccc cccccccc", plain},
    {"a /* b */ /* c", plain, "  ccccccc cccc", block},
    {"**/", block, "ccc", plain},
    {"\"abc\"", plain, "sssss", plain},
    {"\"a\\\"b\" x", plain, "ssssss", plain},
    {"\"open", plain, "eeeee", plain},
    {"'x'", plain, "sss", plain},
    {"'\\''", plain, "ssss", plain},
    {"'ab", plain, "eee", plain},
    {"\"\\", plain, "ee", plain},
    {"L\"wide\" u8\"x\" u'c'", plain, "sssssss sssss ssss", plain},
    {"\"a\" \"b\"", plain, "sss sss", plain},
    {"\"a\"\"b\"", plain, "sssSss", plain},
    {"R\"(a \"b\" c)\"", plain, "ssssssssssss", plain},
    {"R\"xy(a)\" b)xy\" + 1", plain, "ssssssssssssss o n", plain},
    {"auto s = R\"xyz(first", plain, "kkkk   o sssssssssss", raw_xyz},
    {"middle )xy\" still", raw_xyz, "sssssssssssssssss", raw_xyz},
    {")xyz\"; int a;", raw_xyz, "ssssso ttt  o", plain},
    {"x = R\"(", plain, "  o sss", raw_empty},
    {"R\"abcdefghijklmnop(x", plain, "ssssssssssssssssssss", raw_sixteen},
    {"R\"abcdefghijklmnopq(x", plain, "eeeeeeeeeeeeeeeeeeeee", plain},
}};

// 数字 9・キーワードとプリプロセッサ 12。
constexpr std::array<LexCase, 21> number_and_keyword_cases{{
    {"0x1F", plain, "nnnn", plain},
    {"0b1010u", plain, "nnnnnnn", plain},
    {"017", plain, "nnn", plain},
    {"12.5e-3f", plain, "nnnnnnnn", plain},
    {"1'000'000ull", plain, "nnnnnnnnnnnn", plain},
    {"x1 = .5;", plain, "   o nno", plain},
    {"0x1e+2", plain, "nnnnon", plain},
    {"1e+10L", plain, "nnnnnn", plain},
    {"a[0]", plain, " ono", plain},
    {"if (ready) return;", plain, "kk o     o kkkkkko", plain},
    {"while(true)", plain, "kkkkkovvvvo", plain},
    {"  #define MAX 10", plain, "  kkkkkkkkkkkkkk", plain},
    {"# pragma once // x", plain, "kkkkkkkkkkkkk cccc", plain},
    {"#if defined(X) /* c */ && 1", plain, "kkkkkkkkkkkkkk ccccccc kkkk", plain},
    {"#define S \"str\"", plain, "kkkkkkkkk sssss", plain},
    {"#include <stdio.h>", plain, "kkkkkkkk sssssssss", plain},
    {"#include \"nib.hpp\" // c", plain, "kkkkkkkk sssssssss cccc", plain},
    {"#include <a.h", plain, "kkkkkkkkkkkkk", plain},
    {"#define X /* open", plain, "kkkkkkkkk ccccccc", block},
    {"template <typename T> class A final {};", plain, "kkkkkkkk okkkkkkkk  o kkkkk   kkkkk ooo",
     plain},
    {"static_assert(sizeof(int) == 4);", plain, "kkkkkkkkkkkkkokkkkkkottto oo noo", plain},
}};

// 型と関数と定数 9・演算子 7。
constexpr std::array<LexCase, 16> type_and_operator_cases{{
    {"unsigned long long n = 0;", plain, "tttttttt tttt tttt   o no", plain},
    {"std::size_t count(void);", plain, "   ootttttt fffffottttoo", plain},
    {"return nullptr == p || false;", plain, "kkkkkk vvvvvvv oo   oo vvvvvo", plain},
    {"print_value(NULL);", plain, "fffffffffffovvvvoo", plain},
    {"foo (x)", plain, "    o o", plain},
    {"char8_t c = u8'a';", plain, "ttttttt   o ssssso", plain},
    {"MAX_SIZE(3)", plain, "ffffffffono", plain},
    {"std::vector<int> v;", plain, "   oo      ottto  o", plain},
    {"wchar_t *p = nullptr;", plain, "ttttttt o  o vvvvvvvo", plain},
    {"a->b::c", plain, " oo oo", plain},
    {"x <<= 2;", plain, "  ooo no", plain},
    {"a <=> b", plain, "  ooo", plain},
    {"i++ + --j", plain, " oo o oo", plain},
    {"f(a, b...);", plain, "fo o  ooooo", plain},
    {"x=y/z;", plain, " o o o", plain},
    {"a ? b : c", plain, "  o   o", plain},
}};

// 混在 6・UTF-8 5・境 6。
constexpr std::array<LexCase, 17> mixed_cases{{
    {"for (int i = 0; i < n; ++i) { sum += v[i]; } // done", plain,
     "kkk ottt   o no   o  o oo o o     oo  o oo o ccccccc", plain},
    {"auto f = [](int x) -> bool { return x > 0; };", plain,
     "kkkk   o ooottt  o oo tttt o kkkkkk   o no oo", plain},
    {"x = \"s\" /* c */ + 'q';", plain, "  o sss ccccccc o ssso", plain},
    {"if (p != nullptr && *p == '\\0') { return 0x00; }", plain,
     "kk o  oo vvvvvvv oo o  oo sssso o kkkkkk nnnno o", plain},
    {"class Foo : public Bar { virtual void run() override; };", plain,
     "kkkkk     o kkkkkk     o kkkkkkk tttt fffoo kkkkkkkko oo", plain},
    {"int a = b /* open", plain, "ttt   o   ccccccc", block},
    {"// 日本語のコメント", plain, "ccccccccccc", plain},
    {"\"日本\" x", plain, "ssss", plain},
    {"/* 漢字 */ int 値 = 1;", plain, "cccccccc ttt   o no", plain},
    {"printf(\"é %d\", 1);", plain, "ffffffosssssso noo", plain},
    {"x = '🖋' + 1;", plain, "  o sss o no", plain},
    {"", plain, "", plain},
    {"/", plain, "o", plain},
    {"/*", plain, "cc", block},
    {"#define LONG \\", plain, "kkkkkkkkkkkkkk", plain},
    {"  continued", plain, "", plain},
    {"#", plain, "k", plain},
}};

// #820 のレビューの追補 11（R1 の閉じない `'`・I9 の抜け・Q3 の末尾の `\r`）。
constexpr std::array<LexCase, 11> review_cases{{
    {"#error can't", plain, "kkkkkkkkkkkk", plain},
    {"#if 'a' == 97", plain, "kkk sss kkkkk", plain},
    {"0x1'000ull", plain, "nnnnnnnnnn", plain},
    {"1.e5", plain, "nnnn", plain},
    {"x->y(", plain, " oofo", plain},
    {"operator()(", plain, "kkkkkkkkooo", plain},
    {"u8R\"x(a)x\" b", plain, "ssssssssss", plain},
    {"*/ x", plain, "oo", plain},
    {"a//b", plain, " ccc", plain},
    {"#  define X", plain, "kkkkkkkkkkk", plain},
    {"int x;\r", plain, "ttt  o", plain},
}};

void verify_table(std::span<const LexCase> cases)
{
    for (const LexCase &sample : cases)
    {
        verify_case(sample);
    }
}

// 字句器を持たない言語は、どの状態で入っても区間なしで状態をそのまま返す（決定 4）。
void verify_other_languages()
{
    constexpr std::array<Language, 14> others{
        Language::plain, Language::markdown,   Language::html,       Language::python,
        Language::json,  Language::javascript, Language::typescript, Language::yaml,
        Language::toml,  Language::xml,        Language::css,        Language::powershell,
        Language::shell, Language::cmake};
    for (const Language language : others)
    {
        for (const LexState &in : {plain, block, raw_xyz})
        {
            const LexLine lexed = core::lex_line(language, in, "int x = \"s\"; /* c");
            expect(lexed.spans.empty(), "a language without a lexer paints nothing");
            expect(lexed.out == in, "a language without a lexer keeps the state");
            expect(core::lex_state(language, in, "/* c") == in,
                   "lex_state keeps the state for a language without a lexer");
        }
    }
}

// 桁は code point の序数で、バイトではない（決定 2）。区間の境を数で確かめる。
void verify_columns()
{
    const LexLine lexed = core::lex_line(Language::cpp, plain, "printf(\"é %d\", 1);");
    const std::vector<ColorSpan> expected{
        ColorSpan{Column{1U}, Column{7U}, SyntaxRole::function},
        ColorSpan{Column{7U}, Column{8U}, SyntaxRole::operators},
        ColorSpan{Column{8U}, Column{14U}, SyntaxRole::string},
        ColorSpan{Column{14U}, Column{15U}, SyntaxRole::operators},
        ColorSpan{Column{16U}, Column{17U}, SyntaxRole::number},
        ColorSpan{Column{17U}, Column{19U}, SyntaxRole::operators}};
    expect(lexed.spans == expected, "columns count code points, not bytes");
    const LexLine emoji = core::lex_line(Language::cpp, plain, "/*🖋*/x");
    expect(emoji.spans == std::vector{ColorSpan{Column{1U}, Column{6U}, SyntaxRole::comment}},
           "a four-byte code point is one column");
}

// 状態は値で、行をまたいで渡すと同じ答え（行ごとに読み直しても同じ）。
void verify_state_threading()
{
    constexpr std::array<std::string_view, 5> lines{
        "int a; /* one", "two", "three */ auto s = R\"q(", "body", ")q\"; return;"};
    LexState state{};
    std::vector<LexKind> kinds;
    for (const std::string_view line : lines)
    {
        state = core::lex_line(Language::cpp, state, line).out;
        kinds.push_back(state.kind);
    }
    expect(kinds == std::vector{LexKind::block_comment, LexKind::block_comment, LexKind::raw_string,
                                LexKind::raw_string, LexKind::plain},
           "states thread through lines as values");
    expect(core::lex_delimiter(raw("q")) == "q" && raw("q") == raw("q") && !(raw("q") == raw("r")),
           "raw states compare by their delimiter");
    expect(LexState{} == plain, "the default state is plain");
}
} // namespace

void verify_lexer_scope()
{
    verify_table(comment_and_string_cases);
    verify_table(number_and_keyword_cases);
    verify_table(type_and_operator_cases);
    verify_table(mixed_cases);
    verify_table(review_cases);
    verify_other_languages();
    verify_columns();
    verify_state_threading();
}
} // namespace nenenib::tests
