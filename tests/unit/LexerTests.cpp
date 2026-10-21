// scope `--lexer` の単体テスト（#815・段 2 の ADR 0156 の決定 1〜6・#830 の JSON と Python・
// #833 の Markdown と JS / TS）。core の lex_line が 1 行を役割の区間に分け、行をまたぐ状態
// （ブロックコメント・raw string・フェンス・テンプレート）を値で次の行へ渡すこと。
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
// Python の 3 連引用符の中（#830・delimiter はその 3 文字）。
constexpr LexState triple_dq = raw("\"\"\"");
constexpr LexState triple_sq = raw("'''");
// Markdown のフェンスの中（#833・delimiter はフェンスの文字列）と JS のテンプレートの中。
constexpr LexState fence3 = raw("```");
constexpr LexState fence4 = raw("````");
constexpr LexState fence16 = raw("````````````````");
constexpr LexState tilde3 = raw("~~~");
constexpr LexState tilde4 = raw("~~~~");
constexpr LexState tpl = raw("`");

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

// 表の 1 行を language で読み、区間・出る状態・形を確かめる。lex_state も同じ状態を返す。
LexLine verify_lexed(Language language, const LexCase &sample)
{
    LexLine lexed = core::lex_line(language, sample.in, sample.line);
    const bool spans_match = lexed.spans == painted(sample.paint);
    const bool out_matches = lexed.out == sample.out;
    if (!spans_match || !out_matches)
    {
        const std::string actual = paint_of(lexed.spans);
        std::fprintf(stderr, "lexer case: \"%.*s\"\n  painted: \"%s\" (kind %d)\n",
                     static_cast<int>(sample.line.size()), sample.line.data(), actual.c_str(),
                     static_cast<int>(lexed.out.kind));
    }
    expect(spans_match, "lex_line paints the expected spans");
    expect(out_matches, "lex_line leaves the expected state");
    expect(core::lex_state(language, sample.in, sample.line) == lexed.out,
           "lex_state leaves the same state as lex_line");
    expect(well_formed(lexed.spans, sample.line),
           "spans are ascending, disjoint and inside the line");
    return lexed;
}

// 表の 1 行を C++ と C で読む。C と C++ は同じ字句器。
void verify_case(const LexCase &sample)
{
    const LexLine cpp = verify_lexed(Language::cpp, sample);
    const LexLine c = core::lex_line(Language::c, sample.in, sample.line);
    expect(c.spans == cpp.spans && c.out == cpp.out, "C and C++ share one lexer");
    expect(core::lex_state(Language::c, sample.in, sample.line) == cpp.out,
           "lex_state leaves the same state for C");
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

// JSON（#830・Vim の json.vim）。入れ子・エスケープ・数字の形・裸のキー・閉じない文字列・UTF-8 の
// キー・`\r` の末尾・どの状態で入っても行頭から読んで plain で出る。
constexpr std::array<LexCase, 44> json_cases{{
    {"{", plain, "o", plain},
    {"}", plain, "o", plain},
    {"{}", plain, "oo", plain},
    {"[ ]", plain, "o o", plain},
    {"{\"a\": 1}", plain, "ossso no", plain},
    {"\"key\": \"value\",", plain, "ssssso ssssssso", plain},
    {"\"a\"\"b\"", plain, "sssSss", plain},
    {"\"a\", \"b\"", plain, "ssso sss", plain},
    {"\"esc \\\" quote\"", plain, "ssssssssssssss", plain},
    {"\"back\\\\\"", plain, "ssssssss", plain},
    {"\"\\u00e9\"", plain, "ssssssss", plain},
    {"\"open", plain, "eeeee", plain},
    {"\"ends with \\\"", plain, "eeeeeeeeeeeee", plain},
    {"{\"a\": \"open", plain, "ossso eeeee", plain},
    {"12", plain, "nn", plain},
    {"-12.5e+3", plain, "nnnnnnnn", plain},
    {"0.001", plain, "nnnnn", plain},
    {"1E-10", plain, "nnnnn", plain},
    {"[1, -2, 3.5]", plain, "ono nno nnno", plain},
    {"- 1", plain, "  n", plain},
    {"1.5.5", plain, "nnnnn", plain},
    {"12abc", plain, "nn", plain},
    {"true", plain, "vvvv", plain},
    {"false", plain, "vvvvv", plain},
    {"null", plain, "vvvv", plain},
    {"[true,false,null]", plain, "ovvvvovvvvvovvvvo", plain},
    {"True", plain, "", plain},
    {"nullable", plain, "", plain},
    {"bareKey: 1", plain, "       o n", plain},
    {"{key: \"v\"}", plain, "o   o ssso", plain},
    {"{\"a\": {\"b\": [1, 2]}}", plain, "ossso ossso ono nooo", plain},
    {"],", plain, "oo", plain},
    {"}, {", plain, "oo o", plain},
    {"\"名前\": \"値\"", plain, "sssso sss", plain},
    {"\"emoji 🖋\": 1", plain, "ssssssssso n", plain},
    {"\"a\"\r", plain, "sss", plain},
    {"{\"a\": 1}\r", plain, "ossso no", plain},
    {"", plain, "", plain},
    {"   ", plain, "", plain},
    {"/* c */", plain, "", plain},
    {"// x", plain, "", plain},
    {"\"a\" */", block, "sss", plain},
    {"1", raw_xyz, "n", plain},
    {"\"\"\"", triple_dq, "ssE", plain},
}};

// Python の文字列とコメントと数字（#830・Vim の python.vim）。接頭辞・3 連引用符の行またぎ（開く・
// 中・閉じる・同じ行で開閉）・閉じない文字列・UTF-8・`\r` の末尾。
constexpr std::array<LexCase, 42> python_string_cases{{
    {"# comment", plain, "ccccccccc", plain},
    {"x = 1  # note", plain, "  o n  cccccc", plain},
    {"\"# not comment\"", plain, "sssssssssssssss", plain},
    {"'single'", plain, "ssssssss", plain},
    {"\"double\"", plain, "ssssssss", plain},
    {"'it\\'s'", plain, "sssssss", plain},
    {"\"open", plain, "eeeee", plain},
    {"'open \\'", plain, "eeeeeeee", plain},
    {"r\"raw\\d\"", plain, "ssssssss", plain},
    {"b'bytes'", plain, "ssssssss", plain},
    {"f\"{x}\"", plain, "ssssss", plain},
    {"rb\"x\"", plain, "sssss", plain},
    {"Rb'x'", plain, "sssss", plain},
    {"BR\"x\"", plain, "sssss", plain},
    {"u\"x\"", plain, "ssss", plain},
    {"ub\"x\"", plain, "  sss", plain},
    {"x\"y\"", plain, " sss", plain},
    {"''", plain, "ss", plain},
    {"\"\" + \"\"", plain, "ss o ss", plain},
    {"\"\"\"doc\"\"\"", plain, "sssssssss", plain},
    {"\"\"\"opens", plain, "ssssssss", triple_dq},
    {"inside", triple_dq, "ssssss", triple_dq},
    {"", triple_dq, "", triple_dq},
    {"ends\"\"\" + 1", triple_dq, "sssssss o n", plain},
    {"x = '''opens", plain, "  o ssssssss", triple_sq},
    {"still \"\"\" inside", triple_sq, "ssssssssssssssss", triple_sq},
    {"'''", triple_sq, "sss", plain},
    {"r\"\"\"raw", plain, "sssssss", triple_dq},
    {"a\\\"\"\" b", triple_dq, "sssssss", triple_dq},
    {"\"\"\"a\"\"\" \"\"\"b\"\"\"", plain, "sssssss sssssss", plain},
    {"f'''{x}''' # c", plain, "ssssssssss ccc", plain},
    {"0x_FF_FF", plain, "nnnnnnnn", plain},
    {"0o17 0b1010", plain, "nnnn nnnnnn", plain},
    {"1_000_000", plain, "nnnnnnnnn", plain},
    {"1.5e3 1.5e-3", plain, "nnnnn nnnnnn", plain},
    {"1j + .5", plain, "nn o nn", plain},
    {"3.14j", plain, "nnnnn", plain},
    {"0xFFe+1", plain, "nnnnnon", plain},
    {"s = \"日本語\"", plain, "  o sssss", plain},
    {"# コメント", plain, "cccccc", plain},
    {"x = 1\r", plain, "  o n", plain},
    {"x = 1", block, "  o n", plain},
}};

// Python の識別子と演算子（#830）。予約語と定数の境（None は constant）・型・関数（`(` の前と
// def / class の名前）・デコレータ・`lambda`・演算子の連なり。
constexpr std::array<LexCase, 27> python_word_cases{{
    {"if x is None:", plain, "kk   kk vvvvo", plain},
    {"True and False or not NotImplemented", plain, "vvvv kkk vvvvv kk kkk vvvvvvvvvvvvvv", plain},
    {"Ellipsis ...", plain, "vvvvvvvv ooo", plain},
    {"def f(x):", plain, "kkk fo oo", plain},
    {"class Widget(Base):", plain, "kkkkk ffffffo    oo", plain},
    {"class Point:", plain, "kkkkk fffffo", plain},
    {"async def run():", plain, "kkkkk kkk fffooo", plain},
    {"print(len(xs))", plain, "fffffofffo  oo", plain},
    {"int(x) + str(y)", plain, "ttto o o ttto o", plain},
    {"x: list[int] = []", plain, " o ttttottto o oo", plain},
    {"@decorator", plain, "kkkkkkkkkk", plain},
    {"@functools.lru_cache(maxsize=None)", plain, "kkkkkkkkkkkkkkkkkkkko       ovvvvo", plain},
    {"    @property", plain, "    kkkkkkkkk", plain},
    {"a @ b", plain, "  o", plain},
    {"a @= b", plain, "  oo", plain},
    {"lambda x: x", plain, "kkkkkk  o", plain},
    {"def f() -> int:", plain, "kkk foo oo ttto", plain},
    {"a //= b ** 2 != c", plain, "  ooo   oo n oo", plain},
    {"x := y[1:2]", plain, "  oo  onono", plain},
    {"obj.attr.method()", plain, "   o    offffffoo", plain},
    {"match x:", plain, "kkkkk  o", plain},
    {"case _:", plain, "kkkk  o", plain},
    {"yield from gen()", plain, "kkkkk kkkk fffoo", plain},
    {"x = \\", plain, "  o", plain},
    {"value = 0  # type: int", plain, "      o n  ccccccccccc", plain},
    {"$x ?", plain, "", plain},
    {"9abc", plain, "nnnn", plain},
}};

// Markdown（#833・Vim の markdown.vim）。見出し 6 段・強調の組み合わせ・行内コード・フェンスの
// 開く / 中 / 閉じる / `~~~` / 長さの違い・リンクと画像・引用・箇条書きと番号・水平線・
// HTML のタグ・UTF-8・`\r`・ほかの言語の状態で入っても行頭から読む。
constexpr std::array<LexCase, 89> markdown_cases{{
    {"# Title", plain, "kkkkkkk", plain},
    {"## Section", plain, "kkkkkkkkkk", plain},
    {"### Third", plain, "kkkkkkkkk", plain},
    {"#### Fourth", plain, "kkkkkkkkkkk", plain},
    {"##### Fifth", plain, "kkkkkkkkkkk", plain},
    {"###### Sixth", plain, "kkkkkkkkkkkk", plain},
    {"####### seven", plain, "", plain},
    {"#hashtag", plain, "", plain},
    {"#", plain, "k", plain},
    {"  ## indented", plain, "  kkkkkkkkkkk", plain},
    {"**bold**", plain, "vvvvvvvv", plain},
    {"__bold__", plain, "vvvvvvvv", plain},
    {"*it*", plain, "vvvv", plain},
    {"_it_", plain, "vvvv", plain},
    {"a **b** c *d* e", plain, "  vvvvv   vvv", plain},
    {"snake_case_name", plain, "", plain},
    {"** not bold**", plain, "", plain},
    {"*unclosed", plain, "", plain},
    {"**", plain, "", plain},
    {"[a](", plain, "", plain},
    {"![", plain, "", plain},
    {"a \\", plain, "", plain},
    {"**a*b**", plain, "vvvvvvv", plain},
    {"_a_b_", plain, "vvv", plain},
    {"2*3*4", plain, " vvv", plain},
    {"`code`", plain, "ssssss", plain},
    {"``a ` b``", plain, "sssssssss", plain},
    {"text `open", plain, "", plain},
    {"a `x` and `y`", plain, "  sss     sss", plain},
    {"``x``", plain, "sssss", plain},
    {"[link](http://x)", plain, "ffffffffffffffff", plain},
    {"![alt](a.png)", plain, "fffffffffffff", plain},
    {"see [a](b) and [c](d)", plain, "    ffffff     ffffff", plain},
    {"[not a link] here", plain, "", plain},
    {"[a] (b)", plain, "", plain},
    {"> quote", plain, "ccccccc", plain},
    {">", plain, "c", plain},
    {"  > nested quote **b**", plain, "  cccccccccccccccccccc", plain},
    {"- item", plain, "o", plain},
    {"* item", plain, "o", plain},
    {"+ item", plain, "o", plain},
    {"1. first", plain, "oo", plain},
    {"10) tenth", plain, "ooo", plain},
    {"  - nested *em*", plain, "  o        vvvv", plain},
    {"-item", plain, "", plain},
    {"1.5 apples", plain, "", plain},
    {"---", plain, "ccc", plain},
    {"***", plain, "ccc", plain},
    {"___", plain, "ccc", plain},
    {"- - -", plain, "ccccc", plain},
    {"--", plain, "", plain},
    {"<div>", plain, "kkkkk", plain},
    {"a <span>x</span> b", plain, "  kkkkkk kkkkkkk", plain},
    {"<!-- c -->", plain, "kkkkkkkkkk", plain},
    {"a < b > c", plain, "", plain},
    {"\\*not em\\*", plain, "", plain},
    {"", plain, "", plain},
    {"   ", plain, "", plain},
    {"```", plain, "sss", fence3},
    {"```cpp", plain, "ssssss", fence3},
    {"  ~~~~ info", plain, "sssssssssss", tilde4},
    {"~~~ `x`", plain, "sssssss", tilde3},
    {"    ```", plain, "", plain},
    {"``` a ` b", plain, "", plain},
    {"````````````````", plain, "ssssssssssssssss", fence16},
    {"`````````````````", plain, "", plain},
    {"int x = 1;", fence3, "ssssssssss", fence3},
    {"", fence3, "", fence3},
    {"# not heading", fence3, "sssssssssssss", fence3},
    {"**not em**", fence3, "ssssssssss", fence3},
    {"``` x", fence3, "sssss", fence3},
    {"~~~", fence3, "sss", fence3},
    {"```", fence4, "sss", fence4},
    {"```", fence3, "sss", plain},
    {"````", fence3, "ssss", plain},
    {"   ```", fence3, "ssssss", plain},
    {"```\r", fence3, "ssss", plain},
    {"~~~~  ", tilde3, "ssssss", plain},
    {"~~~", tilde4, "sss", tilde4},
    {"# 見出し", plain, "kkkkk", plain},
    {"**日本語**", plain, "vvvvvvv", plain},
    {"[リンク](url)", plain, "ffffffffff", plain},
    {"本文 `コード` 本文", plain, "   sssss", plain},
    {"> 引用", plain, "cccc", plain},
    {"text\r", plain, "", plain},
    {"- item\r", plain, "o", plain},
    {"---\r", plain, "cccc", plain},
    {"# x", block, "kkk", plain},
    {"**b**", raw_xyz, "vvvvv", plain},
}};

// JavaScript と TypeScript（#833・Vim の javascript.vim / typescript.vim）で同じ答えになる行。
// コメントの行またぎ・文字列とテンプレートの行またぎ・正規表現と除算の見分け（`a++ / 2` は直前が
// 演算子なので正規表現に読む限界）・数字の形・予約語と定数と型・関数・演算子の連なり・UTF-8・境。
constexpr std::array<LexCase, 80> javascript_cases{{
    {"// comment", plain, "cccccccccc", plain},
    {"x = 1; // c", plain, "  o no cccc", plain},
    {"/* block */", plain, "ccccccccccc", plain},
    {"/* opens", plain, "cccccccc", block},
    {"still", block, "ccccc", block},
    {"ends */ x", block, "ccccccc", plain},
    {"", block, "", block},
    {"a /* b */ c", plain, "  ccccccc", plain},
    {"'single'", plain, "ssssssss", plain},
    {"\"double\"", plain, "ssssssss", plain},
    {"'it\\'s'", plain, "sssssss", plain},
    {"'open", plain, "eeeee", plain},
    {"\"a\" + 'b'", plain, "sss o sss", plain},
    {"`tpl ${x} end`", plain, "ssssssssssssss", plain},
    {"`opens", plain, "ssssss", tpl},
    {"middle ${y}", tpl, "sssssssssss", tpl},
    {"ends` + 1", tpl, "sssss o n", plain},
    {"", tpl, "", tpl},
    {"`a\\`b`", plain, "ssssss", plain},
    {"x = /ab+c/gi;", plain, "  o sssssssso", plain},
    {"a / b / c", plain, "  o   o", plain},
    {"(a) / 2", plain, "o o o n", plain},
    {"f(/x/)", plain, "fossso", plain},
    {"return /re/.test(s);", plain, "kkkkkk ssssoffffo oo", plain},
    {"x = a / b;", plain, "  o   o  o", plain},
    {"x /= 2;", plain, "  oo no", plain},
    {"if (/^a/.test(s)) {}", plain, "kk ossssoffffo oo oo", plain},
    {"const r = [/a/, /b[/]c/];", plain, "kkkkk   o ossso sssssssoo", plain},
    {"typeof /x/", plain, "kkkkkk sss", plain},
    {"case /x/:", plain, "kkkk ssso", plain},
    {"} /x/", plain, "o sss", plain},
    {"arr[0] / 2", plain, "   ono o n", plain},
    {"'s' / 2", plain, "sss o n", plain},
    {"x = /unclosed", plain, "  o o", plain},
    {"a++ / 2", plain, " oo o n", plain},
    {"/", plain, "o", plain},
    {"/ 2", plain, "o n", plain},
    {"/* */ / 2", plain, "ccccc o n", plain},
    {"a /* c */ / 2", plain, "  ccccccc o n", plain},
    {"*/ / 2", block, "cc o n", plain},
    {"</div>", plain, "oO   o", plain},
    {"]/2", plain, "oOn", plain},
    {"x.return / 2", plain, " o       o n", plain},
    {"x = /[\\]/]+/;", plain, "  o sssssssso", plain},
    {"0xFF", plain, "nnnn", plain},
    {"0b1010", plain, "nnnnnn", plain},
    {"0o17", plain, "nnnn", plain},
    {"1_000_000", plain, "nnnnnnnnn", plain},
    {"10n", plain, "nnn", plain},
    {"1e3 + 1.5e-3", plain, "nnn o nnnnnn", plain},
    {"0x1e+2", plain, "nnnnon", plain},
    {".5", plain, "nn", plain},
    {"x.y", plain, " o", plain},
    {"const x = null;", plain, "kkkkk   o vvvvo", plain},
    {"let a = undefined", plain, "kkk   o vvvvvvvvv", plain},
    {"if (NaN || Infinity) return true;", plain, "kk ovvv oo vvvvvvvvo kkkkkk vvvvo", plain},
    {"let n = 1;", plain, "kkk   o no", plain},
    {"void 0", plain, "kkkk n", plain},
    {"this.get(x)", plain, "kkkkofffo o", plain},
    {"obj.default", plain, "   o", plain},
    {"$el.on('x', fn)", plain, "   offossso   o", plain},
    {"async function run() {}", plain, "kkkkk kkkkkkkk fffoo oo", plain},
    {"function(x) {}", plain, "kkkkkkkko o oo", plain},
    {"a === b", plain, "  ooo", plain},
    {"a?.b ?? c", plain, " oo  oo", plain},
    {"x => x ** 2", plain, "  oo   oo n", plain},
    {"y ?\?= z >>>= 1", plain, "  ooo   oooo n", plain},
    {"f(...args)", plain, "foooo    o", plain},
    {"a &&= b || !c", plain, "  ooo   oo o", plain},
    {"const s = '日本語'; // コメント", plain, "kkkkk   o ssssso ccccccc", plain},
    {"/* 説明 */ x", plain, "cccccccc", plain},
    {"`テンプレート`", plain, "ssssssss", plain},
    {"x = /日本/u", plain, "  o sssss", plain},
    {"#count = 1", plain, "       o n", plain},
    {"${", plain, " o", plain},
    {"${", tpl, "ss", tpl},
    {"x = \\", plain, "  o", plain},
    {"x = 1;\r", plain, "  o no", plain},
    {"a", raw_xyz, "", plain},
    {"x", triple_dq, "", plain},
}};

// TS の予約語と型は JavaScript では区間なし（`static` は JS の予約語・型は TS だけで引く）。
constexpr std::array<LexCase, 7> javascript_only_cases{{
    {"type T = string;", plain, "       o       o", plain},
    {"let n: number = 1;", plain, "kkk  o        o no", plain},
    {"const string = 1;", plain, "kkkkk        o no", plain},
    {"interface A {}", plain, "            oo", plain},
    {"x as unknown", plain, "", plain},
    {"enum E { A }", plain, "       o   o", plain},
    {"public static override m() {}", plain, "       kkkkkk          foo oo", plain},
}};

// TypeScript だけで keyword になる予約語と type になる型。
constexpr std::array<LexCase, 10> typescript_only_cases{{
    {"type T = string;", plain, "kkkk   o tttttto", plain},
    {"let n: number = 1;", plain, "kkk  o tttttt o no", plain},
    {"const string = 1;", plain, "kkkkk tttttt o no", plain},
    {"interface A {}", plain, "kkkkkkkkk   oo", plain},
    {"x as unknown", plain, "  kk ttttttt", plain},
    {"enum E { A }", plain, "kkkk   o   o", plain},
    {"public static override m() {}", plain, "kkkkkk kkkkkk kkkkkkkk foo oo", plain},
    {"readonly a: keyof T;", plain, "kkkkkkkk  o kkkkk  o", plain},
    {"declare module 'x' {}", plain, "kkkkkkk kkkkkk sss oo", plain},
    {"x satisfies Y; a is B", plain, "  kkkkkkkkk  o   kk", plain},
}};

void verify_table(std::span<const LexCase> cases)
{
    for (const LexCase &sample : cases)
    {
        verify_case(sample);
    }
}

void verify_language_table(Language language, std::span<const LexCase> cases)
{
    for (const LexCase &sample : cases)
    {
        static_cast<void>(verify_lexed(language, sample));
    }
}

// 字句器を持たない言語は、どの状態で入っても区間なしで状態をそのまま返す（決定 4）。
void verify_other_languages()
{
    constexpr std::array<Language, 9> others{
        Language::plain, Language::html,       Language::yaml,  Language::toml, Language::xml,
        Language::css,   Language::powershell, Language::shell, Language::cmake};
    for (const Language language : {Language::c, Language::cpp, Language::json, Language::python,
                                    Language::markdown, Language::javascript, Language::typescript})
    {
        expect(core::has_lexer(language),
               "C, C++, JSON, Python, Markdown, JavaScript and TypeScript have a lexer");
    }
    for (const Language language : others)
    {
        expect(!core::has_lexer(language), "the other languages have no lexer");
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
// 閉じの無い `[` と `<a` を 20,000 文字並べた 1 行（#836 のレビュー R1）。区間は無く、走査は
// 閉じの控えで O(n)（二乗なら Debug で目に見えて遅くなる）。
void verify_markdown_worst_lines()
{
    std::string brackets(20'000U, '[');
    std::string tags;
    for (std::size_t index = 0; index < 10'000U; ++index)
    {
        tags += "<a";
    }
    for (const std::string &line : {brackets, tags})
    {
        const LexLine lexed = core::lex_line(Language::markdown, plain, line);
        expect(lexed.spans.empty() && lexed.out == plain,
               "a long markdown line of unclosed openers paints nothing");
    }
    brackets += "](x)";
    const LexLine closed = core::lex_line(Language::markdown, plain, brackets);
    expect(closed.spans ==
               std::vector{ColorSpan{Column{1U}, Column{20'005U}, SyntaxRole::function}},
           "the first opener reaches the far close of a long markdown line");
}

// フェンスとテンプレートとブロックコメントも、状態を値で次の行へ渡す（#833）。
void verify_threading_of(Language language, std::span<const std::string_view> lines,
                         std::span<const LexKind> expected)
{
    LexState state{};
    std::vector<LexKind> kinds;
    for (const std::string_view line : lines)
    {
        const LexState spans_state = core::lex_line(language, state, line).out;
        expect(core::lex_state(language, state, line) == spans_state,
               "lex_state threads the same state as lex_line");
        state = spans_state;
        kinds.push_back(state.kind);
    }
    expect(std::ranges::equal(kinds, expected), "states thread through markdown and script lines");
}

void verify_markdown_and_script_threading()
{
    constexpr std::array<std::string_view, 5> markdown{"text", "~~~~js", "let x = 1;", "~~~~",
                                                       "# h"};
    constexpr std::array<LexKind, 5> markdown_kinds{
        LexKind::plain, LexKind::raw_string, LexKind::raw_string, LexKind::plain, LexKind::plain};
    verify_threading_of(Language::markdown, markdown, markdown_kinds);
    constexpr std::array<std::string_view, 4> script{"const a = `x", "${y}", "z` / 2 /* c",
                                                     "d */ x"};
    constexpr std::array<LexKind, 4> script_kinds{LexKind::raw_string, LexKind::raw_string,
                                                  LexKind::block_comment, LexKind::plain};
    verify_threading_of(Language::javascript, script, script_kinds);
    verify_threading_of(Language::typescript, script, script_kinds);
}
} // namespace

void verify_lexer_scope()
{
    verify_table(comment_and_string_cases);
    verify_table(number_and_keyword_cases);
    verify_table(type_and_operator_cases);
    verify_table(mixed_cases);
    verify_table(review_cases);
    verify_language_table(Language::json, json_cases);
    verify_language_table(Language::python, python_string_cases);
    verify_language_table(Language::python, python_word_cases);
    verify_language_table(Language::markdown, markdown_cases);
    verify_language_table(Language::javascript, javascript_cases);
    verify_language_table(Language::typescript, javascript_cases);
    verify_language_table(Language::javascript, javascript_only_cases);
    verify_language_table(Language::typescript, typescript_only_cases);
    verify_other_languages();
    verify_columns();
    verify_state_threading();
    verify_markdown_and_script_threading();
    verify_markdown_worst_lines();
}
} // namespace nenenib::tests
