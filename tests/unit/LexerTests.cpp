// scope `--lexer` の単体テスト（#815・段 2 の ADR 0156 の決定 1〜6・#830 の JSON と Python・
// #833 の Markdown と JS / TS・#838 の HTML / XML・CSS・YAML・TOML・#855 の GDScript と Ini と
// gdresource・#857 の C#・Rust・Go・#860 の PHP・SQL・Lua・Dockerfile・#862 の PowerShell・shell・
// CMake）。core の lex_line
// が 1 行を
// 役割の区間に分け、行をまたぐ状態（ブロックコメント・raw string・フェンス・テンプレート・script
// の中・CDATA・ブロックスカラー）を値で次の行へ渡すこと。
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
// #838: HTML の script / style の中（delimiter は閉じタグの頭）・XML の CDATA の中・YAML の
// ブロックスカラーの中（delimiter はキーの桁の 2 桁）。
constexpr LexState in_script = raw("</script");
constexpr LexState in_style = raw("</style");
constexpr LexState in_cdata = raw("]]>");
constexpr LexState block00 = raw("00");
constexpr LexState block02 = raw("02");
constexpr LexState block04 = raw("04");
constexpr LexState block99 = raw("99");
// #855: gdresource と GDScript の行をまたぐ `"…"` の中（delimiter は `"`）。
constexpr LexState in_quote = raw("\"");
// #856: GDScript の行をまたぐ `'…'` とノードのパス `$"…"` の中（delimiter は閉じの前に `$`）。
constexpr LexState in_squote = raw("'");
constexpr LexState in_node = raw("$\"");
// #857: C# の逐語的文字列の中（delimiter は `@`・補間は `$@`）と、開きが引用符 4
// つの生の文字列の中。
constexpr LexState cs_verbatim = raw("@");
constexpr LexState cs_interp_verbatim = raw("$@");
constexpr LexState quad_dq = raw("\"\"\"\"");

// #857: Rust の入れ子のコメントの中（delimiter は深さの 10 進）。
[[nodiscard]] constexpr LexState nested(std::string_view depth)
{
    LexState state = raw(depth);
    state.kind = LexKind::block_comment;
    return state;
}

constexpr LexState nest1 = nested("1");
constexpr LexState nest2 = nested("2");
// #857: Rust の生の文字列の中（delimiter は `r` と `#` の列）。
constexpr LexState raw_r1 = raw("r#");
constexpr LexState raw_r2 = raw("r##");

// #860: plain の delimiter を持つ状態（PHP の中 `php`・Dockerfile の行の続き `\`）と、PHP の
// ヒアドキュメント（閉じの識別子）・Lua の長い括弧（`=` の列）・Dockerfile のヒアドキュメント
// （閉じの語・`<<-` は頭に `-`）。PHP の引用符の文字列は in_quote と in_squote。
[[nodiscard]] constexpr LexState plain_with(std::string_view delimiter)
{
    LexState state = raw(delimiter);
    state.kind = LexKind::plain;
    return state;
}

constexpr LexState php_code = plain_with("php");
constexpr LexState here_eot = raw("EOT");
constexpr LexState here_now = raw("NOW");
constexpr LexState here_sql = raw("SQL");
constexpr LexState lua_c2 = nested("==");
constexpr LexState lua_s1 = raw("=");
constexpr LexState continued = plain_with("\\");
// #861: `RUN` `COPY` `ADD` の続き（ヒアドキュメントを開ける）。
constexpr LexState run_continued = plain_with("\\<");
constexpr LexState here_eof = raw("EOF");
constexpr LexState here_dash = raw("-SCRIPT");
constexpr LexState here_a = raw("A");

// #862: PowerShell のヒア文字列の中（delimiter は開きの `@"` `@'`・引用符の文字列は in_quote と
// in_squote）、shell の `$'…'` の中とヒアドキュメント（delimiter は頭の印 `<`・`<<-` は `-` と
// 閉じの語）。CMake の括弧のコメントと括弧の引数は block・lua_c2・raw_empty、`"…"` は in_quote。
constexpr LexState ps_here_dq = raw("@\"");
constexpr LexState ps_here_sq = raw("@'");
constexpr LexState sh_ansi = raw("$'");
constexpr LexState here_eof_sh = raw("<EOF");
constexpr LexState here_dash_sh = raw("-EOF");
constexpr LexState here_end_sh = raw("<END");
constexpr LexState here_x_sh = raw("<X");

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
constexpr std::array<LexCase, 81> javascript_cases{{
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
    {"/[/x/", plain, "oOsss", plain},
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

// #838: HTML と XML で同じ答えの行（タグ・属性・コメントの行またぎ・実体参照・宣言・UTF-8 の
// 本文・閉じない引用符は行末まで string・行をまたぐタグの属性は区間なし（後続）・ほかの言語の
// 状態は行頭から）。
constexpr std::array<LexCase, 40> html_common_cases{{
    {"<html>", plain, "kkkkkK", plain},
    {"</html>", plain, "kkkkkkK", plain},
    {"<div class=\"box\" id='main'>", plain, "kkkk ttttt sssss tt ssssssk", plain},
    {"<br/>", plain, "kkkKk", plain},
    {"<p>a</p><p>b</p>", plain, "kkK kkkKKkK kkkK", plain},
    {"<td colspan = \"2\" >", plain, "kkk ttttttt   sss k", plain},
    {"<ns:tag a:b=\"c\">", plain, "kkkkkkk ttt sssk", plain},
    {"<a href=\"a>b\">link</a>", plain, "kk tttt sssssk    kkkK", plain},
    {"<a title=\"open", plain, "kk ttttt sssss", plain},
    {"<a title='x' data-x=1>", plain, "kk ttttt sss tttttt sk", plain},
    {"<option selected value=x>", plain, "kkkkkkk tttttttt ttttt sk", plain},
    {"<div", plain, "kkkk", plain},
    {"  class=\"x\">", plain, "", plain},
    {"a < b and c > d", plain, "", plain},
    {"<1abc> <-x>", plain, "", plain},
    {"<!DOCTYPE html>", plain, "kkkkkkkkkkkkkkk", plain},
    {"<!-- comment -->", plain, "cccccccccccccccc", plain},
    {"<!---->x", plain, "ccccccc", plain},
    {"text <!-- open", plain, "     ccccccccc", block},
    {"still comment <b>", block, "ccccccccccccccccc", block},
    {"end --> <b>bold</b>", block, "ccccccc kkK    kkkK", plain},
    {"-->", block, "ccc", plain},
    {"a <!-- one --> b <!-- two", plain, "  cccccccccccc   cccccccc", block},
    {"&amp; &lt; &#123; &#x1F; &nbsp;", plain, "vvvvv vvvv vvvvvv vvvvvv vvvvvv", plain},
    {"& alone &; &toolongname1;", plain, "", plain},
    {"Tom &amp Jerry", plain, "", plain},
    {"<p>こんにちは &amp; 世界</p>", plain, "kkK      vvvvv   kkkK", plain},
    {"<img alt=\"ロゴ\" src=logo.png />", plain, "kkkk ttt ssss ttt ssssssss kk", plain},
    {"", plain, "", plain},
    {"", block, "", block},
    {"plain text only", plain, "", plain},
    {"\t<li>\tx</li>", plain, " kkkK  kkkkK", plain},
    {"<e attr='single'/>", plain, "kk tttt sssssssskk", plain},
    {"<a b=\"1\"c='2'>", plain, "kk t ssst sssk", plain},
    {"<x =y>", plain, "kk  sk", plain},
    {"<a\tb\t=\t\"c\">", plain, "kk t   sssk", plain},
    {"<p>1 &lt; 2</p> <!-- note -->", plain, "kkK  vvvv  kkkK ccccccccccccc", plain},
    {"<!-->x <!--->y", plain, "ccccc  cccccc", plain},
    {"<日本 属性=\"値\">テキスト</日本>", plain, "kkk tt sssk    kkkkK", plain},
    {"int x; /* c", raw_xyz, "", plain},
}};

// HTML だけ: script / style の中は区間なしで閉じタグまで（大小を問わない）・CDATA と `<?` は
// XML だけの字句。
constexpr std::array<LexCase, 22> html_only_cases{{
    {"<input type=\"checkbox\" checked>", plain, "kkkkkk tttt ssssssssss tttttttk", plain},
    {"<!doctype html PUBLIC \"-//W3C//DTD\">", plain, "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
     plain},
    {"<script>var a = \"<b>\";</script>", plain, "kkkkkkkK              kkkkkkkkK", plain},
    {"<script type=\"module\">", plain, "kkkkkkk tttt ssssssssk", in_script},
    {"if (a < b && c > d) { x(); }", in_script, "", in_script},
    {"  const s = \"<div class='x'>\";", in_script, "", in_script},
    {"</script>", in_script, "kkkkkkkkK", plain},
    {"x(); </SCRIPT> <p>", in_script, "     kkkkkkkkK kkK", plain},
    {"y </scripts> z", in_script, "", in_script},
    {"<style>", plain, "kkkkkkK", in_style},
    {"p { color: red; }", in_style, "", in_style},
    {"</style> <p>", in_style, "kkkkkkkK kkK", plain},
    {"<STYLE media=\"all\">a{}</Style>", plain, "kkkkkk ttttt sssssk   kkkkkkkK", plain},
    {"<script src=\"a.js\"/>", plain, "kkkkkkk ttt sssssskk", plain},
    {"<scripts>x</scripts>", plain, "kkkkkkkkK kkkkkkkkkK", plain},
    {"<script a=\">", plain, "kkkkkkk t ss", plain},
    {"<script src=x/>", plain, "kkkkkkk ttt skk", plain},
    {"<script", plain, "kkkkkkk", plain},
    {"<![CDATA[ x ]]>", plain, "kkkkkkkkkkkkkkk", plain},
    {"<?xml version=\"1.0\"?>", plain, "", plain},
    {"end ]]> <b/>", in_cdata, "        kkKk", plain},
    {"<script>", plain, "kkkkkkkK", in_script},
}};

// XML だけ: `<?xml … ?>`・CDATA の行またぎ・名前空間・script の中も普通に読む。
constexpr std::array<LexCase, 17> xml_only_cases{{
    {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>", plain, "kkkkk ttttttt sssss tttttttt ssssssskk",
     plain},
    {"<?xml-stylesheet href=\"a.xsl\"?>", plain, "kkkkkkkkkkkkkkkk tttt ssssssskk", plain},
    {"<root xmlns:x=\"urn:x\">", plain, "kkkkk ttttttt sssssssk", plain},
    {"<x:item id=\"1\"/>", plain, "kkkkkkk tt ssskk", plain},
    {"<![CDATA[ a < b ]]>", plain, "sssssssssssssssssss", plain},
    {"<data><![CDATA[ open", plain, "kkkkkKssssssssssssss", in_cdata},
    {"inside <not> a &amp; tag", in_cdata, "ssssssssssssssssssssssss", in_cdata},
    {"end ]]> <b/>", in_cdata, "sssssss kkKk", plain},
    {"]]>", in_cdata, "sss", plain},
    {"", in_cdata, "", in_cdata},
    {"<script>if (a < b)</script>", plain, "kkkkkkkK          kkkkkkkkK", plain},
    {"<a:b.c-d e=\"f\">", plain, "kkkkkkkk t sssk", plain},
    {"<!DOCTYPE note SYSTEM \"Note.dtd\">", plain, "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk", plain},
    {"<note>日本語のテキスト</note>", plain, "kkkkkK        kkkkkkK", plain},
    {"x </script> y", in_script, "  kkkkkkkkK", plain},
    {"<![CDATA[]]><![CDATA[x", plain, "ssssssssssssSsssssssss", in_cdata},
    {"<?pi?>", plain, "kkkkKk", plain},
}};

// CSS: セレクタ・プロパティと値・単位・色・at-rule・`!important`・コメントの行またぎ・文字列。
constexpr std::array<LexCase, 42> css_cases{{
    {"body {", plain, "     o", plain},
    {"}", plain, "o", plain},
    {"  color: red;", plain, "  ttttto    o", plain},
    {"  margin: 0 auto;", plain, "  tttttto n     o", plain},
    {"  padding: 12px 1.5em 50% .5rem;", plain, "  ttttttto nnnn nnnnn nnn nnnnno", plain},
    {"  width: -10px;", plain, "  ttttto nnnnno", plain},
    {"  background: #fff;", plain, "  tttttttttto vvvvo", plain},
    {"  border-color: #a0b0c0 #A0B0C0FF #abcd;", plain, "  tttttttttttto vvvvvvv vvvvvvvvv vvvvvo",
     plain},
    {"  color: #12345;", plain, "  ttttto       o", plain},
    {"#main-nav, .item > a:hover {", plain, "         o          o      o", plain},
    {"a:hover {", plain, " o      o", plain},
    {"a { color: blue; font-size: 2em }", plain, "  o ttttto     o ttttttttto nnn o", plain},
    {"@media screen and (max-width: 600px) {", plain, "kkkkkk                      o nnnnn  o",
     plain},
    {"@import url(\"theme.css\");", plain, "kkkkkkk     sssssssssss o", plain},
    {"@charset 'utf-8';", plain, "kkkkkkkk ssssssso", plain},
    {"  color: red !important;", plain, "  ttttto     kkkkkkkkkko", plain},
    {"  color: red ! IMPORTANT;", plain, "  ttttto     kkkkkkkkkkko", plain},
    {"  color: red !default;", plain, "  ttttto             o", plain},
    {"/* comment */", plain, "ccccccccccccc", plain},
    {"p { /* open", plain, "  o ccccccc", block},
    {"still inside", block, "cccccccccccc", block},
    {"end */ color: red;", block, "cccccc      o    o", plain},
    {"*/", block, "cc", plain},
    {"  content: \"a \\\" b\";", plain, "  ttttttto sssssssso", plain},
    {"  content: 'open", plain, "  ttttttto eeeee", plain},
    {"  font-family: \"Noto Sans JP\", sans-serif;", plain,
     "  ttttttttttto sssssssssssssso           o", plain},
    {"  --main-color: #333;", plain, "  tttttttttttto vvvvo", plain},
    {"  -webkit-transition: all 0.3s;", plain, "  tttttttttttttttttto     nnnno", plain},
    {"  z-index: 10;", plain, "  ttttttto nno", plain},
    {"h1, h2 { margin: 0 }", plain, "  o    o tttttto n o", plain},
    {"div::before { content: \"→\" }", plain, "   oo       o ttttttto sss o", plain},
    {"  line-height: 1.5;", plain, "  ttttttttttto nnno", plain},
    {"  transform: rotate(45deg);", plain, "  ttttttttto        nnnnn o", plain},
    {"  width: calc(100% - 2px);", plain, "  ttttto      nnnn   nnn o", plain},
    {"", plain, "", plain},
    {"", block, "", block},
    {"p{color:red}", plain, " ottttto   o", plain},
    {"  content: \"見出し\";", plain, "  ttttttto ssssso", plain},
    {"x /* a */ y /* b", plain, "  ccccccc   cccc", block},
    {"int x; /* c", raw_xyz, "     o cccc", block},
    {"  margin: +1px -.5em;", plain, "  tttttto nnnn nnnnno", plain},
    {"  grid-template: 1fr / 2fr;", plain, "  ttttttttttttto nnn   nnno", plain},
}};

// YAML: キー・リスト・文字列・定数・数・アンカー・区切り・flow・ブロックスカラーの開く / 中 /
// 空行 / 戻る（delimiter はキーの桁・`- run: |` の兄弟キーで戻る・タグとアンカーの後ろの印）。
constexpr std::array<LexCase, 56> yaml_cases{{
    {"key: value", plain, "ttt", plain},
    {"name: \"nene-nib\"", plain, "tttt  ssssssssss", plain},
    {"single: 'it''s'", plain, "tttttt  sssssss", plain},
    {"open: \"never closed", plain, "tttt  sssssssssssss", plain},
    {"  nested: 12", plain, "  tttttt  nn", plain},
    {"ratio: -1.5e+3", plain, "ttttt  nnnnnnn", plain},
    {"hex: 0xFF", plain, "ttt  nnnn", plain},
    {"octal: 0o17", plain, "ttttt  nnnn", plain},
    {"inf: .inf", plain, "ttt  nnnn", plain},
    {"version: 1.2.3", plain, "ttttttt", plain},
    {"date: 2001-12-14", plain, "tttt", plain},
    {"flag: true", plain, "tttt  vvvv", plain},
    {"off: False", plain, "ttt  vvvvv", plain},
    {"none: ~", plain, "tttt  v", plain},
    {"nothing: null", plain, "ttttttt  vvvv", plain},
    {"yes: YES", plain, "ttt  vvv", plain},
    {"text: true story", plain, "tttt", plain},
    {"- item", plain, "o", plain},
    {"- 42", plain, "o nn", plain},
    {"  - - nested", plain, "  o o", plain},
    {"- name: x", plain, "o tttt", plain},
    {"-not-a-list: 1", plain, "ttttttttttt  n", plain},
    {"# comment", plain, "ccccccccc", plain},
    {"key: value # trailing", plain, "ttt        cccccccccc", plain},
    {"url: http://example.com/#anchor", plain, "ttt", plain},
    {"base: &base", plain, "tttt  kkkkk", plain},
    {"ref: *base", plain, "ttt  kkkkk", plain},
    {"<<: *base", plain, "tt  kkkkk", plain},
    {"---", plain, "kkk", plain},
    {"--- # doc", plain, "kkk ccccc", plain},
    {"...", plain, "kkk", plain},
    {"list: [1, two, \"3\", true]", plain, "tttt  ono    o ssso vvvvo", plain},
    {"map: {a: 1, b: x}", plain, "ttt  ot  no t   o", plain},
    {"tag: !!str 123", plain, "ttt        nnn", plain},
    {"日本語: テキスト", plain, "ttt", plain},
    {"note: 日本語の値", plain, "tttt", plain},
    {"script: |", plain, "tttttt  o", block00},
    {"  line one # not comment", block00, "ssssssssssssssssssssssss", block00},
    {"", block00, "", block00},
    {"  ", block00, "ss", block00},
    {"next: 1", block00, "tttt  n", plain},
    {"  folded: >-", plain, "  tttttt  oo", block02},
    {"    deeper text", block02, "sssssssssssssss", block02},
    {"  sibling: 2", block02, "  ttttttt  n", plain},
    {"- |", plain, "o o", block00},
    {"- run: |", plain, "o ttt  o", block02},
    {"    echo hi", block02, "sssssssssss", block02},
    {"  env: x", block02, "  ttt", plain},
    {"- - k: |", plain, "o o t  o", block04},
    {"key: !!str |", plain, "ttt        o", block00},
    {"key: &a |", plain, "ttt  kk o", block00},
    {"key: |2 # c", plain, "ttt  oo ccc", block00},
    {"key: |x", plain, "ttt", plain},
    {"a: b: c", plain, "t", plain},
    {"int x; /* c", raw_xyz, "", plain},
    {"    still block", block99, "", plain},
}};

// TOML: テーブル・キー・文字列 4 種・3 連の行またぎ・数と日時・配列・インラインテーブル。
// 複数行の配列の中の `[a]` だけの行は見出しに塗る（限界）。`+infx` は数でない。
constexpr std::array<LexCase, 49> toml_cases{{
    {"[package]", plain, "kkkkkkkkk", plain},
    {"[[bin]]", plain, "kkkkkkk", plain},
    {"[dependencies.serde]", plain, "kkkkkkkkkkkkkkkkkkkk", plain},
    {"[ \"quoted key\" . sub ]  # c", plain, "kkkkkkkkkkkkkkkkkkkkkk  ccc", plain},
    {"[1, 2],", plain, "ono noo", plain},
    {"name = \"nene-nib\"", plain, "tttt o ssssssssss", plain},
    {"version = '0.1.0'", plain, "ttttttt o sssssss", plain},
    {"bare_key-2 = 1", plain, "tttttttttt o n", plain},
    {"\"quoted\" = true", plain, "tttttttt o vvvv", plain},
    {"a.b.c = false", plain, "totot o vvvvv", plain},
    {"site.\"google.com\" = 1", plain, "ttttotttttttttttt o n", plain},
    {"1234 = \"x\"", plain, "tttt o sss", plain},
    {"-key = 1", plain, "tttt o n", plain},
    {"int = +99", plain, "ttt o nnn", plain},
    {"neg = -17", plain, "ttt o nnn", plain},
    {"under = 1_000", plain, "ttttt o nnnnn", plain},
    {"hex = 0xDEAD_BEEF", plain, "ttt o nnnnnnnnnnn", plain},
    {"flt = 6.626e-34", plain, "ttt o nnnnnnnnn", plain},
    {"inf = -inf", plain, "ttt o nnnn", plain},
    {"nan = nan", plain, "ttt o nnn", plain},
    {"odt = 1979-05-27T07:32:00-08:00", plain, "ttt o nnnnnnnnnnnnnnnnnnnnnnnnn", plain},
    {"ldt = 1979-05-27 07:32:00", plain, "ttt o nnnnnnnnnnnnnnnnnnn", plain},
    {"ld = 1979-05-27", plain, "tt o nnnnnnnnnn", plain},
    {"lt = 07:32:00", plain, "tt o nnnnnnnn", plain},
    {"arr = [ 1, 2, 3 ]", plain, "ttt o o no no n o", plain},
    {"nested = [[1, 2], [\"a\"]]", plain, "tttttt o oono noo osssoo", plain},
    {"inline = { x = 1, y.z = \"w\" }", plain, "tttttt o o t o no tot o sss o", plain},
    {"str = \"esc \\\" quote\"", plain, "ttt o ssssssssssssss", plain},
    {"lit = 'C:\\path'", plain, "ttt o sssssssss", plain},
    {"bad = \"open", plain, "ttt o eeeee", plain},
    {"# comment", plain, "ccccccccc", plain},
    {"key = \"value\" # trailing", plain, "ttt o sssssss cccccccccc", plain},
    {"ml = \"\"\"", plain, "tt o sss", triple_dq},
    {"inside \\\"\"\" still", triple_dq, "sssssssssssssssss", triple_dq},
    {"end\"\"\" # c", triple_dq, "ssssss ccc", plain},
    {"lit = '''", plain, "ttt o sss", triple_sq},
    {"raw \\ text", triple_sq, "ssssssssss", triple_sq},
    {"x'''''", triple_sq, "ssssss", plain},
    {"one = \"\"\"a\"\"\"", plain, "ttt o sssssss", plain},
    {"日本 = \"値\"", plain, "   o sss", plain},
    {"\"キー\" = '値'", plain, "tttt o sss", plain},
    {"", plain, "", plain},
    {"  1,", plain, "  no", plain},
    {"]", plain, "o", plain},
    {"  [a]", plain, "  kkk", plain},
    {"x = +inf", plain, "t o nnnn", plain},
    {"x = +infx", plain, "t o", plain},
    {"word", plain, "", plain},
    {"int x; /* c", raw_xyz, "", plain},
}};

// GDScript（#855）: 宣言・注釈・型の注釈・ノードのパス・StringName と NodePath・3 連引用符の
// 行またぎ・数の形・UTF-8 のコメント。`%` は式の始まりでだけノード、`@` は行のどこでも注釈。
// #856: 引用符 1 つの文字列とノードのパスも行をまたぎ（Godot 4 の tokenizer）、生の文字列 `r"…"`
// は `r` を含めて string（`\` は次の 1 バイトを飛ばすので `r"\"` は閉じない）。
constexpr std::array<LexCase, 76> gdscript_cases{{
    {"extends CharacterBody2D", plain, "kkkkkkk", plain},
    {"class_name Player", plain, "kkkkkkkkkk", plain},
    {"@tool", plain, "kkkkk", plain},
    {"@export var speed: int = 3", plain, "kkkkkkk kkk      o ttt o n", plain},
    {"@export_range(0, 100) var hp_max := 100", plain, "kkkkkkkkkkkkkono nnno kkk        oo nnn",
     plain},
    {"@onready var n = $Path/To", plain, "kkkkkkkk kkk   o vvvvvvvv", plain},
    {"@onready var label := %Label as Label", plain, "kkkkkkkk kkk       oo vvvvvv kk", plain},
    {"var hp: Health = null", plain, "kkk   o tttttt o vvvv", plain},
    {"var arr: Array[int] = []", plain, "kkk    o tttttottto o oo", plain},
    {"const MAX_SPEED: float = 1.5e3", plain, "kkkkk          o ttttt o nnnnn", plain},
    {"signal health_changed(value: int)", plain, "kkkkkk               o     o ttto", plain},
    {"enum {A, B}", plain, "kkkk o o  o", plain},
    {"enum State { IDLE, RUN = 2 }", plain, "kkkk       o     o     o n o", plain},
    {"func _ready() -> void:", plain, "kkkk ffffffoo oo tttto", plain},
    {"func hit(body: Area2D, power := 2) -> Player:", plain,
     "kkkk fffo    o tttttto       oo no oo tttttto", plain},
    {"static func create() -> Player:", plain, "kkkkkk kkkk ffffffoo oo tttttto", plain},
    {"var d = {a: b, \"k\": 1}", plain, "kkk   o o o  o ssso no", plain},
    {"    match state:", plain, "    kkkkk      o", plain},
    {"        State.IDLE when hp > 0:", plain, "             o     kkkk    o no", plain},
    {"    await get_tree().create_timer(1.5).timeout", plain,
     "    kkkkk         ooo            onnnoo", plain},
    {"    if a % b == 0 and not self.dead:", plain, "    kk   o   oo n kkk kkk kkkko    o", plain},
    {"    return %Label", plain, "    kkkkkk vvvvvv", plain},
    {"    get_node(%\"Unique Name\").show()", plain, "            ovvvvvvvvvvvvvvoo    oo", plain},
    {"    $\"../Sibling\".hide()", plain, "    vvvvvvvvvvvvvo    oo", plain},
    {"    $%Unique/Child.visible = true", plain, "    vvvvvvvvvvvvvvo        o vvvv", plain},
    {"    print(\"%d\" %count)", plain, "         ossss o     o", plain},
    {"    var name := &\"enemy\"", plain, "    kkk      oo ssssssss", plain},
    {"    var path := ^\"Path/To:prop\"", plain, "    kkk      oo sssssssssssssss", plain},
    {"    var flags = 0xFF_FF | 0b1010_0101", plain, "    kkk       o nnnnnnn o nnnnnnnnnnn",
     plain},
    {"    var big = 1_000_000 + 1e5 - .5", plain, "    kkk     o nnnnnnnnn o nnn o nn", plain},
    {"    var x = PI * TAU / INF + NAN", plain, "    kkk   o vv o vvv o vvv o vvv", plain},
    {"    for i in range(10):", plain, "    kkk   kk      onnoo", plain},
    {"    while true: pass", plain, "    kkkkk vvvvo kkkk", plain},
    {"    elif body is Node2D: break", plain, "    kkkk      kk       o kkkkk", plain},
    {"    else: continue", plain, "    kkkko kkkkkkkk", plain},
    {"    super._ready(); breakpoint", plain, "    kkkkko      ooo kkkkkkkkkk", plain},
    {"    assert(hp > 0, \"hp\")", plain, "    kkkkkko   o no sssso", plain},
    {"class Inner extends Node:", plain, "kkkkk       kkkkkkk tttto", plain},
    {"    var doc := \"\"\"multi", plain, "    kkk     oo ssssssss", triple_dq},
    {"line \"quoted\" inside", triple_dq, "ssssssssssssssssssss", triple_dq},
    {"end\"\"\" # done", triple_dq, "ssssss cccccc", plain},
    {"var q = '''", plain, "kkk   o sss", triple_sq},
    {"x''' + 'y'", triple_sq, "ssss o sss", plain},
    {"# コメント 日本語", plain, "cccccccccc", plain},
    {"## doc comment", plain, "cccccccccccccc", plain},
    {"var t = \"日本語\" # 注", plain, "kkk   o sssss ccc", plain},
    {"var bad = \"open", plain, "kkk     o sssss", in_quote},
    {"more\" # c", in_quote, "sssss ccc", plain},
    {"var s = 'it", plain, "kkk   o sss", in_squote},
    {"x' + 1", in_squote, "ss o n", plain},
    {"var s = \"a\\", plain, "kkk   o sss", in_quote},
    {"\"", plain, "s", in_quote},
    {"\"\"\"", plain, "sss", triple_dq},
    {"&\"", plain, "ss", in_quote},
    {"^\"", plain, "ss", in_quote},
    {"$", plain, "v", plain},
    {"%", plain, "o", plain},
    {"@", plain, "o", plain},
    {"(", plain, "o", plain},
    {"\"\"\"x\"\"\"", plain, "sssssss", plain},
    {"a\\''' b'''", triple_sq, "ssssssssss", plain},
    {"&\"\"\"", plain, "ssss", triple_dq},
    {"$\"open", plain, "vvvvvv", in_node},
    {"rest\" .x", in_node, "vvvvv o", plain},
    {"x = %\"Unique", plain, "  o vvvvvvvv", in_node},
    {"var re = r\"\\d+\"", plain, "kkk    o ssssss", plain},
    {"r'x'", plain, "ssss", plain},
    {"r\"\"\"raw", plain, "sssssss", triple_dq},
    {"r\"\\\"", plain, "ssss", in_quote},
    {"var esc = \"a\\\"b\" + 'it\\'s'", plain, "kkk     o ssssss o sssssss", plain},
    {"    tween.tween_property(self, \"position\", Vector2(1, 2), 0.5)", plain,
     "         o              okkkko sssssssssso tttttttono noo nnno", plain},
    {"x @ y", plain, "  o", plain},
    {"", plain, "", plain},
    {"still /* x", block, "      oo", plain},
    {"int x; xyz", raw_xyz, "ttt  o", plain},
}};

// Ini（#855・dosini）: 見出し・キーと値・`;` `#` の行頭のコメント・空白の入ったキー・値の数と
// 文字列。値の中の `;` はコメントでなく（Vim と同じ）、行をまたがない。
constexpr std::array<LexCase, 47> dosini_cases{{
    {"[section]", plain, "kkkkkkkkk", plain},
    {"  [Indented Section] trailing", plain, "  kkkkkkkkkkkkkkkkkk", plain},
    {"[unclosed", plain, "kkkkkkkkk", plain},
    {"[preset.0]", plain, "kkkkkkkkkk", plain},
    {"key=value", plain, "ttto", plain},
    {"key = value", plain, "ttt o", plain},
    {"my key = some value", plain, "tttttt o", plain},
    {"count = 42", plain, "ttttt o nn", plain},
    {"ratio=0.75", plain, "tttttonnnn", plain},
    {"half = .5", plain, "tttt o nn", plain},
    {"exp = 1e10", plain, "ttt o nnnn", plain},
    {"small = 2E-3", plain, "ttttt o nnnn", plain},
    {"padded = 12  ", plain, "tttttt o nn", plain},
    {"zero = 007", plain, "tttt o nnn", plain},
    {"minus = -1", plain, "ttttt o", plain},
    {"version = 1.2.3", plain, "ttttttt o", plain},
    {"under = 1_000", plain, "ttttt o", plain},
    {"dot = 1.", plain, "ttt o", plain},
    {"name = \"quoted value\"", plain, "tttt o ssssssssssssss", plain},
    {"mixed = a \"b\" c \"d", plain, "ttttt o   sss", plain},
    {"empty_filter=\"\"", plain, "ttttttttttttoss", plain},
    {"; comment", plain, "ccccccccc", plain},
    {"# comment", plain, "ccccccccc", plain},
    {"   ; indented comment", plain, "   cccccccccccccccccc", plain},
    {"\t# tab comment", plain, " ccccccccccccc", plain},
    {";;;;;;;;;;;;;;;;;;;", plain, "ccccccccccccccccccc", plain},
    {"key = value ; not a comment", plain, "ttt o", plain},
    {"path=C:\\Windows\\System32", plain, "tttto", plain},
    {"url = http://x/?a=b", plain, "ttt o", plain},
    {"= novalue", plain, "o", plain},
    {"novalue", plain, "", plain},
    {"empty =", plain, "ttttt o", plain},
    {"日本語キー = 値", plain, "ttttt o", plain},
    {"名前=\"値\"", plain, "ttosss", plain},
    {"[PHP]", plain, "kkkkk", plain},
    {"extension=php_mbstring.dll", plain, "ttttttttto", plain},
    {"memory_limit = 128M", plain, "tttttttttttt o", plain},
    {"error_reporting = E_ALL & ~E_DEPRECATED", plain, "ttttttttttttttt o", plain},
    {"runnable=true", plain, "tttttttto", plain},
    {"a.b-c d = 1", plain, "ttttttt o n", plain},
    {"", plain, "", plain},
    {"    ", plain, "", plain},
    {"a = \"x", in_quote, "t o", plain},
    {"[", plain, "k", plain},
    {"=", plain, "o", plain},
    {"k = \"", plain, "t o", plain},
    {"int x; /* c", raw_xyz, "", plain},
}};

// Godot の設定（#855・gdresource）: project.godot と .tscn の形。見出しの名前と閉じは keyword・
// 属性名とキーは type・呼び出しの名前は type・`"…"` は行をまたぐ・`;` から行末はコメント。
// 限界（#856 で固定）: 見出しの属性の文字列が行をまたぐと次の行の `]` は閉じに取れず、複数行の
// 配列の中で `[SubResource(` から始まる行は見出しとして塗る。
constexpr std::array<LexCase, 60> gdresource_cases{{
    {"; Engine configuration file.", plain, "cccccccccccccccccccccccccccc", plain},
    {"config_version=5", plain, "tttttttttttttton", plain},
    {"[application]", plain, "kkkkkkkkkkkkk", plain},
    {"config/name=\"Super Game\"", plain, "tttttttttttossssssssssss", plain},
    {"run/main_scene=\"res://main.tscn\"", plain, "ttttttttttttttosssssssssssssssss", plain},
    {"config/features=PackedStringArray(\"4.3\", \"GL Compatibility\")", plain,
     "tttttttttttttttottttttttttttttttt sssss  ssssssssssssssssss", plain},
    {"[display]", plain, "kkkkkkkkk", plain},
    {"window/size/viewport_width=320", plain, "ttttttttttttttttttttttttttonnn", plain},
    {"[autoload]", plain, "kkkkkkkkkk", plain},
    {"Game=\"*res://autoload/game.gd\"", plain, "ttttosssssssssssssssssssssssss", plain},
    {"move_left={", plain, "ttttttttto", plain},
    {"\"deadzone\": 0.5,", plain, "ssssssssss  nnn", plain},
    {"\"events\": [Object(InputEventKey,\"pressed\":false,\"device\":-1)]", plain,
     "ssssssss   tttttt               sssssssss vvvvv ssssssss  n", plain},
    {"}", plain, "", plain},
    {"2d_physics/layer_1=\"player\"", plain, "ttttttttttttttttttossssssss", plain},
    {"[gd_scene load_steps=3 format=3 uid=\"uid://cvx1a2b3c4d5\"]", plain,
     "kkkkkkkkk tttttttttton tttttton tttossssssssssssssssssssk", plain},
    {"[ext_resource type=\"Script\" path=\"res://player.gd\" id=\"1_abcde\"]", plain,
     "kkkkkkkkkkkkk ttttossssssss ttttosssssssssssssssss ttosssssssssk", plain},
    {"[sub_resource type=\"RectangleShape2D\" id=\"Shape_x1\"]", plain,
     "kkkkkkkkkkkkk ttttossssssssssssssssss ttossssssssssk", plain},
    {"size = Vector2(32, 48)", plain, "tttt o ttttttt nn  nn", plain},
    {"[node name=\"Player\" type=\"CharacterBody2D\"]", plain,
     "kkkkk ttttossssssss ttttosssssssssssssssssk", plain},
    {"script = ExtResource(\"1_abcde\")", plain, "tttttt o ttttttttttt sssssssss", plain},
    {"speed = 120.5", plain, "ttttt o nnnnn", plain},
    {"position = Vector2i(500, 350)", plain, "tttttttt o tttttttt nnn  nnn", plain},
    {"modulate = Color(1, 1, 1, 0.5)", plain, "tttttttt o ttttt n  n  n  nnn", plain},
    {"visible = false", plain, "ttttttt o vvvvv", plain},
    {"metadata/_edit_lock_ = true", plain, "tttttttttttttttttttt o vvvv", plain},
    {"resource = null", plain, "tttttttt o vvvv", plain},
    {"[node name=\"Shape\" type=\"CollisionShape2D\" parent=\".\" groups=[\"solid\", \"wall\"]]",
     plain, "kkkkk ttttosssssss ttttossssssssssssssssss ttttttosss tttttto sssssss  ssssss k",
     plain},
    {"[node name=\"Hud\" parent=\".\" instance=ExtResource(\"3\")] ; c", plain,
     "kkkkk ttttosssss ttttttosss ttttttttottttttttttt sss k ccc", plain},
    {"shape = SubResource(\"Shape_x1\")", plain, "ttttt o ttttttttttt ssssssssss", plain},
    {"data = {", plain, "tttt o", plain},
    {"\"key\": Vector3(0, 1.5, -2)", plain, "sssss  ttttttt n  nnn   n", plain},
    {"script/source = \"extends Node", plain, "ttttttttttttt o sssssssssssss", in_quote},
    {"func _ready():", in_quote, "ssssssssssssss", in_quote},
    {"\tprint(\\\"hi\\\")", in_quote, "ssssssssssssss", in_quote},
    {"\" ; after", in_quote, "s ccccccc", plain},
    {"path = \"C:\\\\dir\\\\\"", plain, "tttt o sssssssssss", plain},
    {"[connection signal=\"body_entered\" from=\"Area\" to=\".\" method=\"_on_body\"]", plain,
     "kkkkkkkkkkk ttttttossssssssssssss ttttossssss ttosss ttttttossssssssssk", plain},
    {"0:0/0 = 0", plain, "ttttt o n", plain},
    {"0:0/0/physics_layer_0/polygon_0/points = PackedVector2Array(-8, -8, 8, -8)", plain,
     "tttttttttttttttttttttttttttttttttttttt o tttttttttttttttttt  n   n  n   n", plain},
    {"action = &\"jump\"", plain, "tttttt o sssssss", plain},
    {"target = ^\"../Player\"", plain, "tttttt o ssssssssssss", plain},
    {"flags = 0x1F", plain, "ttttt o nnnn", plain},
    {"value = 1e+06", plain, "ttttt o nnnnn", plain},
    {"ratio = .25", plain, "ttttt o nnn", plain},
    {"a = b ; trailing comment", plain, "t o   cccccccccccccccccc", plain},
    {"text = \"semi ; inside\"", plain, "tttt o sssssssssssssss", plain},
    {"x == 1", plain, "  oO n", plain},
    {"[1, 2]", plain, " n  n", plain},
    {"[ node ]", plain, "", plain},
    {"[node name=\"a]", plain, "kkkkk ttttosss", in_quote},
    {"b\"]", in_quote, "ss", plain},
    {"[gd_scene", plain, "kkkkkkkkk", plain},
    {"a = \"x\\", plain, "t o sss", in_quote},
    {"&\"", plain, "ss", in_quote},
    {"[SubResource(\"1\"), SubResource(\"2\")]", plain, "kkkkkkkkkkkk sss   ttttttttttt sss k",
     plain},
    {"日本 = \"値\"", plain, "   o sss", plain},
    {"[resource]", plain, "kkkkkkkkkk", plain},
    {"", plain, "", plain},
    {"int x; /* c", raw_xyz, "     cccccc", plain},
}};

// C#（#857）: 宣言・`///`・逐語的文字列の行またぎと `""`・生の文字列（引用符 3 つと 4
// つ）・補間（`{…}` の中 は分類しない）・前処理の行・文字・数の形・閉じない字句。
constexpr std::array<LexCase, 65> csharp_cases{{
    {"using System.Collections.Generic;", plain, "Kkkkk       O           O       O", plain},
    {"namespace Demo.App;", plain, "Kkkkkkkkk     O   O", plain},
    {"public sealed class Player : IDisposable {", plain,
     "Kkkkkk Kkkkkk Kkkkk        O             O", plain},
    {"public record Point(int X, int Y);", plain, "Kkkkkk Kkkkkk FffffOTtt  O Ttt  Oo", plain},
    {"/// <summary>Moves the player.</summary>", plain, "Cccccccccccccccccccccccccccccccccccccccc",
     plain},
    {"    // line comment", plain, "    Ccccccccccccccc", plain},
    {"    private static readonly string Name = \"nib\";", plain,
     "    Kkkkkkk Kkkkkk Kkkkkkkk Tttttt      O SssssO", plain},
    {"    var path = @\"C:\\temp\\\";", plain, "    Ttt      O SssssssssssO", plain},
    {"    var multi = @\"first line", plain, "    Ttt       O Ssssssssssss", cs_verbatim},
    {"second \"\"quoted\"\" line", cs_verbatim, "Ssssssssssssssssssssss", cs_verbatim},
    {"end\"; int after = 1;", cs_verbatim, "SsssO Ttt       O NO", plain},
    {"    var raw = \"\"\"", plain, "    Ttt     O Sss", triple_dq},
    {"    He said \"hi\" and \"\"twice\"\"", triple_dq, "Ssssssssssssssssssssssssssssss",
     triple_dq},
    {"    \"\"\";", triple_dq, "SssssssO", plain},
    {"var four = \"\"\"\"", plain, "Ttt      O Ssss", quad_dq},
    {"contains \"\"\" three", quad_dq, "Ssssssssssssssssss", quad_dq},
    {"\"\"\"\";", quad_dq, "SsssO", plain},
    {"var one = \"\"\"inline \"quoted\" text\"\"\";", plain,
     "Ttt     O SsssssssssssssssssssssssssO", plain},
    {"var s = $\"{x} and {y:N2}\";", plain, "Ttt   O SssssssssssssssssO", plain},
    {"var t = $\"{dict[\"key\"]} done\";", plain, "Ttt   O SssssssssssssssssssssO", plain},
    {"var e = $\"{{literal}}\" + 1;", plain, "Ttt   O Ssssssssssssss O NO", plain},
    {"var v = $@\"C:\\{dir}", plain, "Ttt   O Sssssssssss", cs_interp_verbatim},
    {"\\file\"; x = 2;", cs_interp_verbatim, "SsssssO   O NO", plain},
    {"var w = @$\"{a}\"\"b\"\"\";", plain, "Ttt   O SsssssssssssO", plain},
    {"var r = $\"\"\"{a} \"quoted\" \"\"\";", plain, "Ttt   O SsssssssssssssssssssO", plain},
    {"var rr = $$\"\"\"", plain, "Ttt    O Sssss", triple_dq},
    {"#region Helpers // keep", plain, "Kkkkkkkkkkkkkkkkkkkkkkk", plain},
    {"#if DEBUG // only debug", plain, "Kkkkkkkkk Ccccccccccccc", plain},
    {"#endregion", plain, "Kkkkkkkkkk", plain},
    {"    #nullable enable", plain, "    Kkkkkkkkkkkkkkkk", plain},
    {"#error can't build", plain, "Kkkkkkkkkkkkkkkkkk", plain},
    {"[Serializable]", plain, "O            O", plain},
    {"char c = 'a', n = '\\n', q = '\\'';", plain, "Tttt   O SssO   O SsssO   O SsssO", plain},
    {"int hex = 0x_FF + 0b1010_0101;", plain, "Ttt     O Nnnnn O NnnnnnnnnnnO", plain},
    {"decimal m = 1.5m; double d = 1e-3; float f = .5f;", plain,
     "Ttttttt   O NnnnO Tttttt   O NnnnO Ttttt   O NnnO", plain},
    {"long big = 1_000_000UL; var r = 1..2;", plain, "Tttt     O NnnnnnnnnnnO Ttt   O NONnO",
     plain},
    {"public async Task<int> RunAsync() { await Task.Delay(10); }", plain,
     "Kkkkkk Kkkkk     OTttO FfffffffOo O Kkkkk     OFffffONnOo O", plain},
    {"var list = new List<string>();", plain, "Ttt      O Kkk     OTtttttOooo", plain},
    {"if (x is null || y == true) return;", plain, "Kk O  Kk Vvvv Oo   Oo VvvvO KkkkkkO", plain},
    {"foreach (var item in items) yield return item;", plain,
     "Kkkkkkk OTtt      Kk      O Kkkkk Kkkkkk     O", plain},
    {"string bad = \"open", plain, "Tttttt     O Eeeee", plain},
    {"char bad = 'x", plain, "Tttt     O Ee", plain},
    {"/* block", plain, "Cccccccc", block},
    {"still */ int z;", block, "Cccccccc Ttt  O", plain},
    {"var u8s = \"abc\"u8;", plain, "Ttt     O SssssssO", plain},
    {"var @class = 1;", plain, "Ttt        O NO", plain},
    {"x = a ? b : c ?? d; f = x => x * 2;", plain, "  O   O   O   Oo  O   O   Oo   O NO", plain},
    {"\"", plain, "E", plain},
    {"@\"", plain, "Ss", cs_verbatim},
    {"\"\"\"", plain, "Sss", triple_dq},
    {"'", plain, "E", plain},
    {"/*", plain, "Cc", block},
    {"$", plain, "O", plain},
    {"// 日本語のコメント", plain, "Ccccccccccc", plain},
    {"var j = \"日本\";", plain, "Ttt   O SsssO", plain},
    // #858 のレビュー: 逐語的の `""` は閉じない・`\\` と `\"`・閉じない補間は行末まで error・
    // 引用符 17 個の生の文字列（16 個の列で閉じる限界）・`u8` の付いた生の文字列・補間の書式の中の
    // `'`・文字・`/*/`。
    {"var a = @\"a\"\"\";", plain, "Ttt   O SsssssO", plain},
    {"var b = \"\\\\\"; var c = \"\\\"\";", plain, "Ttt   O SsssO Ttt   O SsssO", plain},
    {"var d = $\"{x}", plain, "Ttt   O Eeeee", plain},
    {"var k = \"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"a\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\"\";", plain,
     "Ttt   O SsssssssssssssssssssssssssssssssssEe", plain},
    {"var f = \"\"\"raw\"\"\"u8;", plain, "Ttt   O SssssssssssO", plain},
    {"var g = $\"{d:yyyy'-'MM}\";", plain, "Ttt   O SsssssssssssssssO", plain},
    {"char h = 'A';", plain, "Tttt   O SssO", plain},
    {"/*/", plain, "Ccc", block},
    {"", plain, "", plain},
    {"int x; xyz", raw_xyz, "Ttt  O", plain},
}};

// Rust（#857）: 宣言・属性・入れ子のコメントの行またぎ（delimiter は深さ）・生の文字列 `r#"…"#`
// `r##"…"##` の行またぎ・文字とライフタイム・バイト文字列・数の形・マクロ・閉じない字句。
constexpr std::array<LexCase, 65> rust_cases{{
    {"use std::collections::HashMap;", plain, "Kkk    Oo           Oo       O", plain},
    {"pub fn main() {", plain, "Kkk Kk FfffOo O", plain},
    {"fn add<'a>(x: &'a str) -> &'a str { x }", plain, "Kk FffOTtOo O OTt TttO Oo OTt Ttt O   O",
     plain},
    {"impl<T: Clone> Shape for Wrapper<T> {}", plain, "KkkkO O TttttO       Kkk        O O Oo",
     plain},
    {"pub trait Shape { fn area(&self) -> f64; }", plain,
     "Kkk Kkkkk       O Kk FfffOoKkkkO Oo TttO O", plain},
    {"#[derive(Debug, Clone)]", plain, "Kkkkkkkkkkkkkkkkkkkkkkk", plain},
    {"#![allow(dead_code)]", plain, "Kkkkkkkkkkkkkkkkkkkk", plain},
    {"#[doc = \"a ] b\"] struct S;", plain, "KkkkkkkkSssssssK Kkkkkk  O", plain},
    {"#[cfg(test)] mod tests {", plain, "Kkkkkkkkkkkk Kkk       O", plain},
    {"/* outer /* inner */ still outer */ let x = 1;", plain,
     "Ccccccccccccccccccccccccccccccccccc Kkk   O NO", plain},
    {"/* open /* nested", plain, "Ccccccccccccccccc", nest2},
    {"middle */ still one", nest2, "Ccccccccccccccccccc", nest1},
    {"done */ let y = 2;", nest1, "Ccccccc Kkk   O NO", plain},
    {"still /* x", block, "Cccccccccc", nest2},
    {"let s = r#\"raw \"quoted\" text\"#;", plain, "Kkk   O SsssssssssssssssssssssO", plain},
    {"let t = r##\"opens", plain, "Kkk   O Sssssssss", raw_r2},
    {"has \"# inside", raw_r2, "Sssssssssssss", raw_r2},
    {"ends\"##; let z = 3;", raw_r2, "SssssssO Kkk   O NO", plain},
    {"let w = r#\"opens", plain, "Kkk   O Ssssssss", raw_r1},
    {"body \"quoted\" and \"## more", raw_r1, "SsssssssssssssssssssO", plain},
    {"let u = r##\"a \"# b\"##;", plain, "Kkk   O SssssssssssssO", plain},
    {"let c = 'a'; let n = '\\n'; let q = '\\'';", plain,
     "Kkk   O SssO Kkk   O SsssO Kkk   O SsssO", plain},
    {"let lt: &'static str = \"s\";", plain, "Kkk   O OTtttttt Ttt O SssO", plain},
    {"'outer: loop { break 'outer; }", plain, "TtttttO Kkkk O Kkkkk TtttttO O", plain},
    {"let b = b'a'; let bs = b\"bytes\";", plain, "Kkk   O SsssO Kkk    O SsssssssO", plain},
    {"let n = 1_000u32 + 0b1010 + 0x_FF + 0o77 + 1.5e-3f64;", plain,
     "Kkk   O Nnnnnnnn O Nnnnnn O Nnnnn O Nnnn O NnnnnnnnnO", plain},
    {"println!(\"{}\", x);", plain, "FfffffffOSsssO  Oo", plain},
    {"let v = vec![1, 2, 3];", plain, "Kkk   O FfffONO NO NOo", plain},
    {"if a != b { return Self::new(); }", plain, "Kk   Oo   O Kkkkkk TtttOoFffOoo O", plain},
    {"let me = self.value;", plain, "Kkk    O KkkkO     O", plain},
    {"let o: Option<i32> = Some(1); let e = Err(e); None", plain,
     "Kkk  O TtttttOTttO O VvvvONOo Kkk   O VvvO Oo Vvvv", plain},
    {"let multi = \"first", plain, "Kkk       O Ssssss", in_quote},
    {"second line \\\"esc\\\"", in_quote, "Sssssssssssssssssss", in_quote},
    {"end\" + 1;", in_quote, "Ssss O NO", plain},
    {"let cont = \"line \\", plain, "Kkk      O Sssssss", in_quote},
    {"match x { None => 0, _ => 1 }", plain, "Kkkkk   O Vvvv Oo NO   Oo N O", plain},
    {"let r#match = 1;", plain, "Kkk         O NO", plain},
    {"let br = br\"raw bytes\"; let cs = c\"c str\";", plain,
     "Kkk    O SssssssssssssO Kkk    O SsssssssO", plain},
    {"async fn fetch() -> u8 { fut.await }", plain, "Kkkkk Kk FffffOo Oo Tt O    OKkkkk O", plain},
    {"x.iter().map(|v| v * 2).collect::<Vec<_>>();", plain,
     " OFfffOooFffOo O   O NOoFffffffOooTttO Ooooo", plain},
    {"let range = 1..10; let t = (1, 2).0;", plain, "Kkk       O NOoNnO Kkk   O ONO NOoNO", plain},
    {"unsafe impl Send for P {}", plain, "Kkkkkk Kkkk Tttt Kkk   Oo", plain},
    {"macro_rules! square { ($x:expr) => { $x * $x }; }", plain,
     "Ffffffffffff        O Oo O    O Oo O O  O O  Oo O", plain},
    {"// line comment", plain, "Ccccccccccccccc", plain},
    {"/// doc comment", plain, "Ccccccccccccccc", plain},
    {"//! inner doc", plain, "Ccccccccccccc", plain},
    {"'", plain, "E", plain},
    {"r#\"", plain, "Sss", raw_r1},
    {"/*", plain, "Cc", nest1},
    {"\"", plain, "S", in_quote},
    {"b'", plain, " E", plain},
    // #858 のレビュー: 生の文字列は `\` を飛ばさない・文字の `\'` と `\u{…}`・ライフタイムの列・
    // 閉じの無い属性・属性の中の文字の `]`（属性はそこで閉じる限界）・`/*/`・
    // 16 進は符号を続けない・予約語は `(` の前でも予約語・`!=` はマクロでない・タプルの添字・
    // 孤立した `'` は 1 文字だけ error。
    {"let s = r\"a\\\";", plain, "Kkk   O SssssO", plain},
    {"let q = b'\\''; let e = '\\u{1F600}';", plain, "Kkk   O SssssO Kkk   O SssssssssssO", plain},
    {"struct P<'a, 'b> { x: &'a str }", plain, "Kkkkkk  OTtO TtO O  O OTt Ttt O", plain},
    {"#[cfg(feature = \"x\"", plain, "KkkkkkkkkkkkkkkkSss", plain},
    {"#[a(']')] fn f() {}", plain, "KkkkkkEOo Kk FOo Oo", plain},
    {"/*/", plain, "Ccc", nest1},
    {"let h = 0xE+1;", plain, "Kkk   O NnnONO", plain},
    {"if(a) {} while (b) {}", plain, "KkO O Oo Kkkkk O O Oo", plain},
    {"if foo!=bar {}", plain, "Kk    Oo    Oo", plain},
    {"let v = a.0.1;", plain, "Kkk   O  ONnnO", plain},
    {"let a = ' x;", plain, "Kkk   O E  O", plain},
    {"let k = '日';", plain, "Kkk   O SssO", plain},
    {"", plain, "", plain},
    {"int x; xyz", raw_xyz, "     O", plain},
}};

// Go（#857）: `package`・`import (…)`・メソッドの受け手の後ろの名前・生の文字列の行またぎ・rune・
// 組み込みの関数・`nil` `iota`・数の形（`0x1p-2`）・`:=`・閉じない字句。
constexpr std::array<LexCase, 53> go_cases{{
    {"package main", plain, "Kkkkkkk", plain},
    {"import (", plain, "Kkkkkk O", plain},
    {"    \"fmt\"", plain, "    Sssss", plain},
    {"    str \"strings\"", plain, "        Sssssssss", plain},
    {")", plain, "O", plain},
    {"import \"os\"", plain, "Kkkkkk Ssss", plain},
    {"type Point struct { X, Y int }", plain, "Kkkk       Kkkkkk O  O   Ttt O", plain},
    {"type Shape interface { Area() float64 }", plain, "Kkkk       Kkkkkkkkk O     Oo Ttttttt O",
     plain},
    {"func main() {", plain, "Kkkk FfffOo O", plain},
    {"func (p *Point) Move(dx int) error {", plain, "Kkkk O  O     O FfffO   TttO Ttttt O", plain},
    {"func (Point) Name() string { return \"p\" }", plain,
     "Kkkk O     O FfffOo Tttttt O Kkkkkk Sss O", plain},
    {"func Map[T any](xs []T) []T { return xs }", plain,
     "Kkkk FffO  TttOo   Oo O Oo  O Kkkkkk    O", plain},
    {"    x := 10", plain, "      Oo Nn", plain},
    {"    var r rune = 'a'", plain, "    Kkk   Tttt O Sss", plain},
    {"    esc := '\\n'", plain, "        Oo Ssss", plain},
    {"    s := \"hello\\n\"", plain, "      Oo Sssssssss", plain},
    {"    raw := `raw text", plain, "        Oo Sssssssss", tpl},
    {"still raw \\n \"quoted\"", tpl, "Sssssssssssssssssssss", tpl},
    {"done` + x", tpl, "Sssss O", plain},
    {"    f := 0x1p-2 + 1e3 + .5 + 0b1010 + 0o17 + 1_000 + 2i", plain,
     "      Oo Nnnnnn O Nnn O Nn O Nnnnnn O Nnnn O Nnnnn O Nn", plain},
    {"    var g float64 = 0x1.8p1", plain, "    Kkk   Ttttttt O Nnnnnnn", plain},
    {"    n := len(xs) + cap(xs)", plain, "      Oo FffO  O O FffO  O", plain},
    {"    m := make(map[string]int)", plain, "      Oo FfffOTttOTtttttOTttO", plain},
    {"    xs = append(xs, 1)", plain, "       O FfffffO  O NO", plain},
    {"    if err != nil { panic(err) }", plain, "    Kk     Oo Vvv O FffffO   O O", plain},
    {"    const ( A = iota; B )", plain, "    Kkkkk O   O VvvvO   O", plain},
    {"    for i := range 10 { defer close(ch) }", plain,
     "    Kkk   Oo Kkkkk Nn O Kkkkk FffffO  O O", plain},
    {"    go func() { ch <- true }()", plain, "    Kk KkkkOo O    Oo Vvvv Ooo", plain},
    {"    select { case v := <-ch: _ = v; default: }", plain,
     "    Kkkkkk O Kkkk   Oo Oo  O   O  O KkkkkkkO O", plain},
    {"    var c chan bool = nil", plain, "    Kkk   Tttt Tttt O Vvv", plain},
    {"/* block", plain, "Cccccccc", block},
    {"still */ x := 1", block, "Cccccccc   Oo N", plain},
    {"// comment", plain, "Cccccccccc", plain},
    {"    bad := \"open", plain, "        Oo Eeeee", plain},
    {"    r := 'x", plain, "      Oo Ee", plain},
    {"\"", plain, "E", plain},
    {"`", plain, "S", tpl},
    {"'", plain, "E", plain},
    {"/*", plain, "Cc", block},
    {"    fallthrough", plain, "    Kkkkkkkkkkk", plain},
    {"    goto end", plain, "    Kkkk", plain},
    {"    s := string(b) + fmt.Sprintf(\"%d\", 3)", plain,
     "      Oo TtttttO O O    O       OSsssO NO", plain},
    {"func init() { println(\"x\") }", plain, "Kkkk FfffOo O FffffffOSssO O", plain},
    {"    t := `one line` + `two`", plain, "      Oo Ssssssssss O Sssss", plain},
    {"var 日本 = \"語\"", plain, "Kkk    O Sss", plain},
    // #858 のレビュー: 16 進は `p` の後だけ符号を続ける・rune の `\'`・`/*/`・受け手に `)` を含む
    // メソッドの名前は塗らない（限界）・生の文字列は `\` を飛ばさない・
    // コメントの続きの行頭の `*/`。
    {"x := 0xe+1", plain, "  Oo NnnON", plain},
    {"r := '\\''", plain, "  Oo Ssss", plain},
    {"/*/", plain, "Ccc", block},
    {"func (f func(int)) N() {}", plain, "Kkkk O  KkkkOTttOo  Oo Oo", plain},
    {"x := `a\\` + `\\n`", plain, "  Oo Ssss O Ssss", plain},
    {"*/ x := 1", block, "Cc   Oo N", plain},
    {"", plain, "", plain},
    {"int x; xyz", raw_xyz, "Ttt  O", plain},
}};

// PHP（#860）: `<?php` で始まるファイル・`namespace` `use` `class` `function` `fn`・`$this->x`
// （`->` の後ろの名前は予約語にしない）・属性 `#[…]`・`'…'` `"…"` の行またぎ・ヒアドキュメントと
// ナウドキュメント（字下げした閉じ・閉じない形・識別子が行末でない形と 16 バイトを越える形）・
// `?>` の後の HTML と `<?=`・大小を問わない `TRUE` `Null` `ECHO`・行コメントは `?>` で終わる。
constexpr std::array<LexCase, 73> php_cases{{
    {"<?php", plain, "Kkkkk", php_code},
    {"", php_code, "", php_code},
    {"namespace App\\Models;", php_code, "Kkkkkkkkk           O", php_code},
    {"use App\\Contracts\\Shape;", php_code, "Kkk                    O", php_code},
    {"final class Circle extends Base implements Shape", php_code,
     "Kkkkk Kkkkk        Kkkkkkk      Kkkkkkkkkk", php_code},
    {"{", php_code, "O", php_code},
    {"    private readonly float $radius;", php_code, "    Kkkkkkk Kkkkkkkk Ttttt        O",
     php_code},
    {"    public function __construct(float $r = 1.5) { $this->radius = $r; }", php_code,
     "    Kkkkkk Kkkkkkkk FffffffffffOTtttt    O NnnO O      Oo       O   O O", php_code},
    {"    public function area(): float { return 3.14 * $this->radius ** 2; }", php_code,
     "    Kkkkkk Kkkkkkkk FfffOoo Ttttt O Kkkkkk Nnnn O      Oo       Oo NO O", php_code},
    {"    public static function make(): static { return new static(); }", php_code,
     "    Kkkkkk Kkkkkk Kkkkkkkk FfffOoo Kkkkkk O Kkkkkk Kkk KkkkkkOoo O", php_code},
    {"#[Attribute(Attribute::TARGET_CLASS)]", php_code, "Kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
     php_code},
    {"#[Route(\"/users\", methods: ['GET'])] final class C {}", php_code,
     "KkkkkkkkSsssssssKkkkkkkkkkkkSssssKkk Kkkkk Kkkkk   Oo", php_code},
    {"$x = fn($y) => $y + 1;", php_code, "   O KkO  O Oo    O NO", php_code},
    {"$a = TRUE || Null ?? false;", php_code, "   O Vvvv Oo Vvvv Oo VvvvvO", php_code},
    {"echo __LINE__ . __DIR__;", php_code, "Kkkk Vvvvvvvv O VvvvvvvO", php_code},
    {"if (isset($list[0])) { ECHO 'it\\'s'; }", php_code, "Kk OKkkkkO     ONOoo O Kkkk SssssssO O",
     php_code},
    {"$s = \"tab\\t and \\\"quote\\\" {$x}\";", php_code, "   O SsssssssssssssssssssssssssO",
     php_code},
    {"$t = 'first line", php_code, "   O Sssssssssss", in_squote},
    {"second \\' line", in_squote, "Ssssssssssssss", in_squote},
    {"ends here'; return $t;", in_squote, "SsssssssssO Kkkkkk   O", php_code},
    {"$u = \"double", php_code, "   O Sssssss", in_quote},
    {"closes\";", in_quote, "SssssssO", php_code},
    {"$h = <<<EOT", php_code, "   O Ssssss", here_eot},
    {"  Hello {$name}", here_eot, "Sssssssssssssss", here_eot},
    {"EOTX is not the end", here_eot, "Sssssssssssssssssss", here_eot},
    {"  EOT;", here_eot, "SssssO", php_code},
    {"$n = <<<'NOW'", php_code, "   O Ssssssss", here_now},
    {"raw $x {$y}", here_now, "Sssssssssss", here_now},
    {"NOW, 'next');", here_now, "SssO SsssssOo", php_code},
    {"$q = <<< \"SQL\"", php_code, "   O Sssssssss", here_sql},
    {"SQL", here_sql, "Sss", php_code},
    {"$z = <<<ABCDEFGHIJKLMNOPQ", php_code, "   O Ooo", php_code},
    {"$w = <<<EOT x;", php_code, "   O Ooo     O", php_code},
    {"/* start", php_code, "Cccccccc", block},
    {"still */ echo 1;", block, "Cccccccc Kkkk NO", php_code},
    {"// comment ?> after", php_code, "CccccccccccKk", plain},
    {"# hash comment", php_code, "Cccccccccccccc", php_code},
    {"?>", php_code, "Kk", plain},
    {"<p>Hello <?= $name ?></p>", plain, "         Kkk       Kk", plain},
    {"<div><?php if ($ok): ?><b>yes</b><?php endif; ?></div>", plain,
     "     Kkkkk Kk O   Oo Kk          Kkkkk KkkkkO Kk", plain},
    {"<html> function echo 'x' /* y", plain, "", plain},
    {"<?PHP echo 0x1F + 0b101 + 1_000 + 1.5e-3 + .5;", plain,
     "Kkkkk Kkkk Nnnn O Nnnnn O Nnnnn O Nnnnnn O NnO", php_code},
    {"<?phpx <?xml", plain, "", plain},
    {"$this->class = $obj?->list;", php_code, "     Oo      O     Ooo    O", php_code},
    {"Foo::class;", php_code, "   OoKkkkkO", php_code},
    {"function &getRef() {}", php_code, "Kkkkkkkk OFfffffOo Oo", php_code},
    {"$cmd = `ls -la`;", php_code, "     O SsssssssO", php_code},
    {"echo \"日本語\"; // コメント", php_code, "Kkkk SssssO Ccccccc", php_code},
    {"\"", php_code, "S", in_quote},
    {"/*", php_code, "Cc", block},
    {"", block, "", block},
    {"enum Suit: string { case Hearts = 'H'; }", php_code,
     "Kkkk     O Tttttt O Kkkk        O SssO O", php_code},
    {"match($x) { 1 => 'a', default => 'b' };", php_code, "KkkkkO  O O N Oo SssO Kkkkkkk Oo Sss Oo",
     php_code},
    {"\"?>\"; Function Run() {}", php_code, "SsssO Kkkkkkkk FffOo Oo", php_code},
    {"<?php echo 1;", php_code, "Kkkkk Kkkk NO", php_code},
    {"#[A('x]')] #[B]", php_code, "KkkkSsssKk Kkkk", php_code},
    {"#[Open('a',", php_code, "KkkkkkkSssK", php_code},
    // #861: 中の開きのタグは keyword（状態は中のまま）・`::` と `->` の後ろの名前は区間なし
    // （`::class` だけ keyword）・行頭の `#!` は行末までコメント・閉じの後ろの `)` `,`。
    {"<?= $x ?>", php_code, "Kkk    Kk", plain},
    {"<?php ?>", plain, "Kkkkk Kk", plain},
    {"echo 1; <?php echo 2;", php_code, "Kkkk NO Kkkkk Kkkk NO", php_code},
    {"Foo::list();", php_code, "   Oo    Ooo", php_code},
    {"static::new;", php_code, "KkkkkkOo   O", php_code},
    {"Status::default;", php_code, "      Oo       O", php_code},
    {"Foo::CLASS;", php_code, "   OoKkkkkO", php_code},
    {"#!/usr/bin/env php", plain, "Cccccccccccccccccc", plain},
    {"#!/usr/bin/php -q", php_code, "Ccccccccccccccccc", php_code},
    {"#!x ?> y", php_code, "Cccccccc", php_code},
    {"__halt_compiler();", php_code, "KkkkkkkkkkkkkkkOoo", php_code},
    {"$$var = 1;", php_code, "      O NO", php_code},
    {"#[", php_code, "Kk", php_code},
    {"EOT);", here_eot, "SssOo", php_code},
    {"EOT, 1);", here_eot, "SssO NOo", php_code},
    // I4: lex_line に来る行は `\r` を含まない（CRLF の `\r` は TextBuffer::line_end が除く）。
    {"EOT", here_eot, "Sss", php_code},
}};

// SQL（#860）: 文と句・`CREATE TABLE` と型・`--` と `/* */` の行またぎ・`'it''s'` と行をまたぐ
// `'…'`・`"col"` と `` `col` `` は区間なし・小文字と混ざった大小のキーワード・`COUNT(*)`。
constexpr std::array<LexCase, 55> sql_cases{{
    {"SELECT id, name FROM users WHERE active = TRUE;", plain,
     "Kkkkkk   O      Kkkk       Kkkkk        O VvvvO", plain},
    {"select count(*) as n from t where x is not null", plain,
     "Kkkkkk FffffOoo Kk   Kkkk   Kkkkk   Kk Kkk Vvvv", plain},
    {"CREATE TABLE users (", plain, "Kkkkkk Kkkkk       O", plain},
    {"    id INTEGER PRIMARY KEY,", plain, "       Ttttttt Kkkkkkk KkkO", plain},
    {"    name VARCHAR(255) NOT NULL DEFAULT '',", plain,
     "         TttttttONnnO Kkk Vvvv Kkkkkkk SsO", plain},
    {"    price DECIMAL(10,2),", plain, "          TttttttONnONOo", plain},
    {"    created TIMESTAMP DEFAULT CURRENT_TIMESTAMP", plain,
     "            Ttttttttt Kkkkkkk Fffffffffffffffff", plain},
    {");", plain, "Oo", plain},
    {"-- a line comment", plain, "Ccccccccccccccccc", plain},
    {"SELECT 1 -- trailing", plain, "Kkkkkk N Ccccccccccc", plain},
    {"/* block", plain, "Cccccccc", block},
    {"still block", block, "Ccccccccccc", block},
    {"end */ SELECT 2;", block, "Cccccc Kkkkkk NO", plain},
    {"INSERT INTO t VALUES ('it''s', 3.5e-2, .5);", plain,
     "Kkkkkk Kkkk   Kkkkkk OSssssssO NnnnnnO NnOo", plain},
    {"UPDATE t SET \"select\" = 1, `from` = 2", plain, "Kkkkkk   Kkk          O NO        O N",
     plain},
    {"SELECT 'first", plain, "Kkkkkk Ssssss", in_squote},
    {"middle '' still", in_squote, "Sssssssssssssss", in_squote},
    {"last', 1", in_squote, "SssssO N", plain},
    {"SELECT a.b FROM t1 LEFT JOIN t2 ON t1.id = t2.id", plain,
     "Kkkkkk  O  Kkkk    Kkkk Kkkk    Kk   O   O   O", plain},
    {"GROUP BY a HAVING SUM(b) > 10 ORDER BY a DESC LIMIT 5", plain,
     "Kkkkk Kk   Kkkkkk FffO O O Nn Kkkkk Kk   Kkkk Kkkkk N", plain},
    {"WITH RECURSIVE r AS (SELECT 1) SELECT * FROM r", plain,
     "Kkkk Kkkkkkkkk   Kk OKkkkkk NO Kkkkkk O Kkkk", plain},
    {"CASE WHEN x <> 0 THEN 'y' ELSE 'n' END", plain, "Kkkk Kkkk   Oo N Kkkk Sss Kkkk Sss Kkk",
     plain},
    {"BETWEEN 1 AND 2 OR FALSE", plain, "Kkkkkkk N Kkk N Kk Vvvvv", plain},
    {"COALESCE(a, 0) || 'x'", plain, "FfffffffO O NO Oo Sss", plain},
    {"DROP TABLE IF EXISTS old_users;", plain, "Kkkk Kkkkk Kk Kkkkkk          O", plain},
    {"ALTER TABLE t ADD COLUMN flag BOOLEAN;", plain, "Kkkkk Kkkkk   Kkk Kkkkkk      TttttttO",
     plain},
    {"GRANT SELECT ON t TO reader;", plain, "Kkkkk Kkkkkk Kk   Kk       O", plain},
    {"BEGIN; COMMIT; ROLLBACK;", plain, "KkkkkO KkkkkkO KkkkkkkkO", plain},
    {"x -1 - 2", plain, "  ON O N", plain},
    {"a/b*c%d", plain, " O O O", plain},
    {"x = --", plain, "  O Cc", plain},
    {"/*", plain, "Cc", block},
    {"/**/ x", plain, "Cccc", plain},
    {"'", plain, "S", in_squote},
    {"''''", plain, "Ssss", plain},
    {"'''", plain, "Sss", in_squote},
    {"SELECT 日本 FROM '語'", plain, "Kkkkkk    Kkkk Sss", plain},
    {"0x1F 1e10 12 1E+5", plain, "Nnnn Nnnn Nn Nnnn", plain},
    {"Select Max(price) From items", plain, "Kkkkkk FffO     O Kkkk", plain},
    {"WHERE id = :id AND @var = 1", plain, "Kkkkk    O O   Kkk      O N", plain},
    {"CREATE INDEX idx ON t (a);", plain, "Kkkkkk Kkkkk     Kk   O Oo", plain},
    {"\"unclosed select from", plain, "", plain},
    {"CAST(x AS INT)", plain, "KkkkO  Kk TttO", plain},
    {"*/", block, "Cc", plain},
    {"it''s'", in_squote, "Ssssss", plain},
    {"SELECT 'a\\' 1", plain, "Kkkkkk Ssss N", plain},
    // #861: `--` の直後に空白なし・`.` の後ろの型の名前（限界・型に塗る）・`"` は行をまたがない・
    // 足した予約語と型。
    {"--x", plain, "Ccc", plain},
    {"SELECT t.date FROM t", plain, "Kkkkkk  OTttt Kkkk", plain},
    {"SELECT \"a", plain, "Kkkkkk", plain},
    {"b\" FROM t", plain, "", plain},
    {"SELECT * FROM t FOR UPDATE", plain, "Kkkkkk O Kkkk   Kkk Kkkkkk", plain},
    {"a ILIKE 'x' COLLATE c", plain, "  Kkkkk Sss Kkkkkkk", plain},
    {"EXPLAIN SELECT 1", plain, "Kkkkkkk Kkkkkk N", plain},
    {"id BIGSERIAL, b BYTEA, t LONGTEXT, n NUMBER", plain,
     "   TttttttttO   TttttO   TtttttttO   Tttttt", plain},
    {"x INT UNSIGNED AUTO_INCREMENT", plain, "  Ttt Kkkkkkkk Kkkkkkkkkkkkkk", plain},
}};

// Lua（#860）: `local function`・`function a.b:c` の名前・`--[[ ]]` と `--[==[ ]==]` の行またぎ・
// 長い文字列 `[[ ]]` `[=[ ]=]`・`nil` `true` `false`・16 進の浮動小数 `0x1p4`・`..`・閉じない
// 引用符は行末まで error・`=` が 16 個を越える開きは長い括弧にしない。
constexpr std::array<LexCase, 49> lua_cases{{
    {"local function greet(name)", plain, "Kkkkk Kkkkkkkk FffffO    O", plain},
    {"  return \"Hello, \" .. name .. '!'", plain, "  Kkkkkk Sssssssss Oo      Oo Sss", plain},
    {"end", plain, "Kkk", plain},
    {"function M.helper(x, y) return x end", plain, "Kkkkkkkk  OFfffffO O  O Kkkkkk   Kkk", plain},
    {"function obj:method() end", plain, "Kkkkkkkk    OFfffffOo Kkk", plain},
    {"local f = function(a) return a end", plain, "Kkkkk   O KkkkkkkkO O Kkkkkk   Kkk", plain},
    {"local t = { 1, 2.5, 0x1p4, 0xA.8p-1, 3e10, .5 }", plain,
     "Kkkkk   O O NO NnnO NnnnnO NnnnnnnnO NnnnO Nn O", plain},
    {"if x == nil then x = true elseif x ~= false then end", plain,
     "Kk   Oo Vvv Kkkk   O Vvvv Kkkkkk   Oo Vvvvv Kkkk Kkk", plain},
    {"-- line comment", plain, "Ccccccccccccccc", plain},
    {"x = 1 -- trailing", plain, "  O N Ccccccccccc", plain},
    {"--[[ long comment", plain, "Ccccccccccccccccc", block},
    {"inside ]=] still", block, "Cccccccccccccccc", block},
    {"]] x = 1", block, "Cc   O N", plain},
    {"--[==[ level two", plain, "Cccccccccccccccc", lua_c2},
    {"]] not yet ]=]", lua_c2, "Cccccccccccccc", lua_c2},
    {"]==] return", lua_c2, "Cccc Kkkkkk", plain},
    {"s = [[long string", plain, "  O Sssssssssssss", raw_empty},
    {"still]=] in", raw_empty, "Sssssssssss", raw_empty},
    {"end]] .. \"x\"", raw_empty, "Sssss Oo Sss", plain},
    {"s = [==[a]]b]==]", plain, "  O Ssssssssssss", plain},
    {"s = [=[open", plain, "  O Sssssss", lua_s1},
    {"]=]", lua_s1, "Sss", plain},
    {"for i = 1, 10 do break end", plain, "Kkk   O NO Nn Kk Kkkkk Kkk", plain},
    {"while not done and ok or fail do end", plain, "Kkkkk Kkk      Kkk    Kk      Kk Kkk", plain},
    {"repeat n = n - 1 until n <= 0", plain, "Kkkkkk   O   O N Kkkkk   Oo N", plain},
    {"goto skip ::skip::", plain, "Kkkk      Oo    Oo", plain},
    {"t[1] = #t", plain, " ONO O O", plain},
    {"x = a // b % c ^ 2 ~ d << 1", plain, "  O   Oo   O   O N O   Oo N", plain},
    {"\"esc \\\" quote\"", plain, "Ssssssssssssss", plain},
    {"x = \"unclosed", plain, "  O Eeeeeeeee", plain},
    {"'", plain, "E", plain},
    {"x = 1..2", plain, "  O NOoN", plain},
    {"function f(...) end", plain, "Kkkkkkkk FOoooo Kkk", plain},
    {"local x <const> = 5", plain, "Kkkkk   O     O O N", plain},
    {"a[[x]]", plain, " Sssss", plain},
    {"--[[]] x", plain, "Cccccc", plain},
    {"--[==] not long", plain, "Ccccccccccccccc", plain},
    {"return 日本 -- 語", plain, "Kkkkkk    Cccc", plain},
    {"", block, "", block},
    {"--[=================[ seventeen", plain, "Ccccccccccccccccccccccccccccccc", plain},
    {"local a, b = nil, nil", plain, "Kkkkk  O   O VvvO Vvv", plain},
    {"[[", plain, "Ss", raw_empty},
    {"x = -1", plain, "  O ON", plain},
    {"]][[", raw_empty, "SsSs", raw_empty},
    {"]]--[[", block, "CcCccc", block},
    // #861: 行頭の `#!` は行末までコメント・`#` の長さの演算子・`\z`・3 段の名前の `function`。
    {"#!/usr/bin/env lua", plain, "Cccccccccccccccccc", plain},
    {"x = #t", plain, "  O O", plain},
    {"s = \"a\\z b\"", plain, "  O Sssssss", plain},
    {"function a.b.c() end", plain, "Kkkkkkkk  O OFOo Kkk", plain},
}};

// Dockerfile（#860）: `FROM x AS y`・`RUN a \` の続きの行（行頭の語を命令にしない・続きの中の
// コメントと空行）・`ENV A=1`・`$VAR` `${VAR}`・`# syntax=`・`<<EOF` `<<-EOF` `<<"EOT"`
// の行またぎ・ 小文字の命令・`ONBUILD` の後ろの命令。
constexpr std::array<LexCase, 63> dockerfile_cases{{
    {"# syntax=docker/dockerfile:1", plain, "Cccccccccccccccccccccccccccc", plain},
    {"FROM golang:1.22 AS build", plain, "Kkkk             Kk", plain},
    {"from alpine as runtime", plain, "Kkkk        Kk", plain},
    {"WORKDIR /src", plain, "Kkkkkkk", plain},
    {"COPY go.mod go.sum ./", plain, "Kkkk", plain},
    {"RUN go mod download \\", plain, "Kkk", run_continued},
    {"    # comment inside continuation", run_continued, "    Ccccccccccccccccccccccccccccc",
     run_continued},
    {"    && go build -o /out/app . \\", run_continued, "", run_continued},
    {"    && echo done", run_continued, "", plain},
    {"ENV A=1 B=\"two words\"", plain, "Kkk       Sssssssssss", plain},
    {"ARG VERSION=latest", plain, "Kkk", plain},
    {"RUN echo $VERSION ${HOME:-/root} \"$HOME\"", plain,
     "Kkk      Vvvvvvvv Vvvvvvvvvvvvvv Sssssss", plain},
    {"LABEL maintainer=\"nene \\\"nib\\\"\"", plain, "Kkkkk            Ssssssssssssss", plain},
    {"CMD [\"./app\", \"--port\", \"8080\"]", plain, "Kkk  Sssssss  Ssssssss  Ssssss", plain},
    {"ENTRYPOINT [\"/bin/sh\", \"-c\"]", plain, "Kkkkkkkkkk  Sssssssss  Ssss", plain},
    {"EXPOSE 8080/tcp", plain, "Kkkkkk", plain},
    {"run apk add --no-cache git", plain, "Kkk", plain},
    {"ONBUILD RUN make", plain, "Kkkkkkk Kkk", plain},
    {"HEALTHCHECK --interval=30s CMD curl -f http://localhost/", plain, "Kkkkkkkkkkk", plain},
    {"RUN <<EOF", plain, "Kkk Sssss", here_eof},
    {"apk add curl", here_eof, "Ssssssssssss", here_eof},
    {"  EOF", here_eof, "Sssss", here_eof},
    {"EOF", here_eof, "Sss", plain},
    {"COPY <<-SCRIPT /run.sh", plain, "Kkkk Sssssssss", here_dash},
    {"\techo hi", here_dash, "Ssssssss", here_dash},
    {"\t\tSCRIPT", here_dash, "Ssssssss", plain},
    {"RUN <<\"EOT\" bash", plain, "Kkk Sssssss", here_eot},
    {"EOT", here_eot, "Sss", plain},
    {"RUN cat <<'A' <<B", plain, "Kkk     Sssss", here_a},
    {"RUN echo a << b", plain, "Kkk", plain},
    {"USER nobody", plain, "Kkkk", plain},
    {"VOLUME [\"/data\"]", plain, "Kkkkkk  Sssssss", plain},
    {"STOPSIGNAL SIGTERM", plain, "Kkkkkkkkkk", plain},
    {"SHELL [\"pwsh\", \"-Command\"]", plain, "Kkkkk  Ssssss  Ssssssssss", plain},
    {"MAINTAINER someone", plain, "Kkkkkkkkkk", plain},
    {"ADD --chown=app:app https://example.com/a.tar.gz /a/", plain, "Kkk", plain},
    {"FROM ${BASE} AS base", plain, "Kkkk Vvvvvvv Kk", plain},
    {"NOTANINSTRUCTION foo as bar", plain, "", plain},
    {"ENV PATH=$PATH:/opt/bin", plain, "Kkk      Vvvvv", plain},
    {"RUN echo \"unclosed", plain, "Kkk      Sssssssss", plain},
    {"   RUN indented", plain, "   Kkk", plain},
    {"", continued, "", continued},
    {"RUN not an instruction", continued, "", plain},
    {"RUN a \\  ", plain, "Kkk", run_continued},
    {"RUN echo ${unclosed", plain, "Kkk      Vvvvvvvvvv", plain},
    {"#", plain, "C", plain},
    {"FROM x AS y # not a comment", plain, "Kkkk   Kk", plain},
    {"RUN echo \\$HOME $ 1", plain, "Kkk", plain},
    {"COPY <<EOFTOOLONGWORDXYZ /x", plain, "Kkkk", plain},
    {"RUN <<EOF \\", plain, "Kkk Sssss", here_eof},
    {"EOF ", here_eof, "Ssss", here_eof},
    {"", plain, "", plain},
    // #861: ヒアドキュメントは `RUN` `COPY` `ADD` の行（と続き）で `<<`
    // の直前が空白か行頭のときだけ。
    {"RUN echo $((1<<4))", plain, "Kkk", plain},
    {"RUN a<<b", plain, "Kkk", plain},
    {"RUN cat <<<word", plain, "Kkk", plain},
    {"ENV X <<EOF", plain, "Kkk", plain},
    {"# escape=`", plain, "Cccccccccc", plain},
    {"RUN apt-get update && \\", plain, "Kkk", run_continued},
    {"  cat <<EOF > /x", run_continued, "      Sssss", here_eof},
    {"ENV A=1 \\", plain, "Kkk", continued},
    {"  B= <<x", continued, "", plain},
    {"ONBUILD RUN <<EOF", plain, "Kkkkkkk Kkk Sssss", here_eof},
    // I4: lex_line に来る行は `\r` を含まない（CRLF の `\r` は TextBuffer::line_end が除く）。
    {"EOF", here_eof, "Sss", plain},
}};

// PowerShell（#862）: `param(...)`・`function Get-Thing`・`-Path $env:TEMP`・`<# #>` と `@"` `"@`
// の行またぎ（行頭でない `"@` は閉じない）・`'it''s'`・`` "a`"b" ``・`[string]$x`・`-eq`
// `-NotLike`・
// `$true`・`0x1F` `1kb`・大小の混ざったキーワード・閉じない形。
constexpr std::array<LexCase, 50> powershell_cases{{
    {"param(", plain, "KkkkkO", plain},
    {"    [Parameter(Mandatory = $true)]", plain, "    OTttttttttO          O VvvvvOo", plain},
    {"    [string]$Path,", plain, "    OTtttttOVvvvvO", plain},
    {"    [int]$Count = 0x1F", plain, "    OTttOVvvvvv O Nnnn", plain},
    {")", plain, "O", plain},
    {"function Get-Thing {", plain, "Kkkkkkkk Fffffffff O", plain},
    {"    Get-ChildItem -Path $env:TEMP -Filter *.log", plain,
     "    Fffffffffffff       Vvvvvvvvv         Oo", plain},
    {"    if ($x -eq 1kb -and $y -NotLike 'a*') { return $true }", plain,
     "    Kk OVv Ooo Nnn Oooo Vv Oooooooo SsssO O Kkkkkk Vvvvv O", plain},
    {"    ElseIf ($null -ne $z) { $false }", plain, "    Kkkkkk OVvvvv Ooo VvO O Vvvvvv O", plain},
    {"}", plain, "O", plain},
    {"<# block comment", plain, "Cccccccccccccccc", block},
    {"still inside #> $a = 1", block, "Ccccccccccccccc Vv O N", plain},
    {"<# one #> Write-Host \"hi\"", plain, "Ccccccccc Ffffffffff Ssss", plain},
    {"$s = @\"", plain, "Vv O Ss", ps_here_dq},
    {"  not \"@ closed", ps_here_dq, "Sssssssssssssss", ps_here_dq},
    {"\"@ + 'x'", ps_here_dq, "Ss O Sss", plain},
    {"$t = @'  ", plain, "Vv O Ssss", ps_here_sq},
    {"'@", ps_here_sq, "Ss", plain},
    {"\"@", ps_here_sq, "Ss", ps_here_sq},
    {"'it''s'", plain, "Sssssss", plain},
    {"\"a`\"b\" + \"c\"\"d\"", plain, "Ssssss O Ssssss", plain},
    {"$o = 'open", plain, "Vv O Sssss", in_squote},
    {"still'' in", in_squote, "Ssssssssss", in_squote},
    {"end' -replace 'a', 'b'", in_squote, "Ssss Oooooooo SssO Sss", plain},
    {"$d = \"open `\"", plain, "Vv O Ssssssss", in_quote},
    {"end\" | Out-Null", in_quote, "Ssss O Ffffffff", plain},
    {"# line comment", plain, "Cccccccccccccc", plain},
    {"Write-Output a#b", plain, "Ffffffffffff", plain},
    {"FOREACH ($i in 1..10) { $i++ }", plain, "Kkkkkkk OVv Kk NOoNnO O VvOo O", plain},
    {"$h = @{ Name = 'x' }; Invoke-It @args", plain, "Vv O OO      O Sss Oo Fffffffff Vvvvv",
     plain},
    {"[System.IO.File]::ReadAllText($p)", plain, "OTtttttttttttttOoo           OVvO", plain},
    {"try { throw } catch [Exception] { } finally { }", plain,
     "Kkk O Kkkkk O Kkkkk OTttttttttO O O Kkkkkkk O O", plain},
    {"class Point { [int]$X }", plain, "Kkkkk Ttttt O OTttOVv O", plain},
    {"enum Color { Red }", plain, "Kkkk Ttttt O     O", plain},
    {"$r = 1.5e3 + .5 - 10d", plain, "Vv O Nnnnn O Nn O Nnn", plain},
    {"Switch -Regex ($s) { default { break } }", plain, "Kkkkkk        OVvO O Kkkkkkk O Kkkkk O O",
     plain},
    {"$x.Count -gt 2 -or $x.end", plain, "VvO      Ooo N Ooo VvO", plain},
    {"$a = `$b", plain, "Vv O", plain},
    {"${my var} = $_", plain, "Vvvvvvvvv O Vv", plain},
    {"\"$(Get-Date)\"", plain, "Sssssssssssss", plain},
    {"$? -and $$", plain, "Vv Oooo Vv", plain},
    {"while ($true) { continue }", plain, "Kkkkk OVvvvvO O Kkkkkkkk O", plain},
    {"filter Out-Foo { $_ }", plain, "Kkkkkk Fffffff O Vv O", plain},
    {"<#", plain, "Cc", block},
    {"#>", block, "Cc", plain},
    {"\"é\" -join ','", plain, "Sss Ooooo Sss", plain},
    {"$x = 1", raw_xyz, "Vv O N", plain},
    {"-split $s", plain, "Oooooo Vv", plain},
    {"$y = [ordered]@{}", plain, "Vv O OTttttttOOOo", plain},
    {"", ps_here_dq, "", ps_here_dq},
}};

// shell（#862）: `#!`・`a#b`・`if [[ ]]; then … fi`・`case … esac`・`f() {`・`$(cmd)` と `` `cmd`
// ``・
// `${x:-y}`・`"$@"`・`'…'` `"…"` `$'…'` の行またぎ・`<<EOF` `<<-EOF` `<<'END'` `<< "X"`
// の行またぎ・
// `<<<`・算術の `<<`・1 行の 2 つ目のヒアドキュメント・16 バイトの語・閉じない形。
constexpr std::array<LexCase, 54> shell_cases{{
    {"#!/bin/bash", plain, "Ccccccccccc", plain},
    {"set -euo pipefail", plain, "Fff", plain},
    {"echo a#b # comment", plain, "Ffff     Ccccccccc", plain},
    {"if [[ -f \"$file\" ]]; then", plain, "Kk Kk    Sssssss KkO Kkkk", plain},
    {"  echo \"found $file\"", plain, "  Ffff Sssssssssssss", plain},
    {"elif [ -d x ]; then", plain, "Kkkk O      OO Kkkk", plain},
    {"else", plain, "Kkkk", plain},
    {"fi", plain, "Kk", plain},
    {"case \"$1\" in", plain, "Kkkk Ssss Kk", plain},
    {"  start|stop) run \"$1\" ;;", plain, "       O    O     Ssss Oo", plain},
    {"  *) exit 1 ;;", plain, "   O Ffff N Oo", plain},
    {"esac", plain, "Kkkk", plain},
    {"f() {", plain, "FOO O", plain},
    {"  local x=${1:-default}", plain, "  Fffff   Vvvvvvvvvvvvv", plain},
    {"  return 0", plain, "  Ffffff N", plain},
    {"}", plain, "O", plain},
    {"function greet {", plain, "Kkkkkkkk Fffff O", plain},
    {"out=$(ls -la | grep foo)", plain, "    OO       O         O", plain},
    {"old=`date`", plain, "    O    O", plain},
    {"for i in 1 2 3; do echo $i; done", plain, "Kkk   Kk N N NO Kk Ffff VvO Kkkk", plain},
    {"while read -r line; do :; done < file", plain, "Kkkkk Ffff        O Kk  O Kkkk O", plain},
    {"printf '%s\\n' \"$@\"", plain, "Ffffff Ssssss Ssss", plain},
    {"echo 'open \\", plain, "Ffff Sssssss", in_squote},
    {"still' done", in_squote, "Ssssss Kkkk", plain},
    {"echo \"open", plain, "Ffff Sssss", in_quote},
    {"end \\\" still\"", in_quote, "Sssssssssssss", plain},
    {"cat <<EOF", plain, "    Sssss", here_eof_sh},
    {"  $HOME is 'here", here_eof_sh, "Ssssssssssssssss", here_eof_sh},
    {"EOF", here_eof_sh, "Sss", plain},
    {"cat <<-EOF > out", plain, "    Ssssss O", here_dash_sh},
    {"\t\tbody", here_dash_sh, "Ssssss", here_dash_sh},
    {"\tEOF", here_dash_sh, "Ssss", plain},
    {"cat <<'END'", plain, "    Sssssss", here_end_sh},
    {"END", here_end_sh, "Sss", plain},
    {"cat << \"X\" | sort", plain, "    Ssssss O", here_x_sh},
    {"\tX", here_x_sh, "Ss", here_x_sh},
    {"grep x <<< \"$v\"", plain, "       Ooo Ssss", plain},
    {"echo $((1<<4))", plain, "Ffff OOoNOoNOo", plain},
    {"echo $'a\\'b'", plain, "Ffff Sssssss", plain},
    {"echo $'open", plain, "Ffff Ssssss", sh_ansi},
    {"x' y", sh_ansi, "Ss", plain},
    {"[ $# -gt 0 ] && echo $? $$ $!", plain, "O Vv     N O Oo Ffff Vv Vv Vv", plain},
    {"export PATH=\"$PATH:/bin\"", plain, "Ffffff      Ssssssssssss", plain},
    {"time sleep 1", plain, "Kkkk       N", plain},
    {"x=1 # trailing", plain, "    Cccccccccc", plain},
    {"cat <<EOF <<EOT", plain, "    Sssss Sssss", here_eof_sh},
    {"cat <<ABCDEFGHIJKLMNOP", plain, "    Oo", plain},
    {"cat <<EOF \"open", plain, "    Sssss Sssss", here_eof_sh},
    {"select opt in a b; do break; done", plain, "Kkkkkk     Kk    O Kk FffffO Kkkk", plain},
    {"! true", plain, "O Ffff", plain},
    {"EOF ", here_eof_sh, "Ssss", here_eof_sh},
    {"echo ${x}#not", plain, "Ffff Vvvv", plain},
    {"x=1", raw_xyz, "", plain},
    {"", in_quote, "", in_quote},
}};

// CMake（#862）: `cmake_minimum_required(VERSION 3.28)`・`if(WIN32) … endif()`・`${VAR}` `$ENV{X}`
// `$CACHE{X}` `$<TARGET_FILE:x>`・`#[[ ]]` `#[==[ ]==]` の行またぎ・括弧の引数・`"…"` の行またぎ・
// `ON` `off`・`*-NOTFOUND`・大文字の引数の語は区間なし・大小を問わないコマンド。
constexpr std::array<LexCase, 51> cmake_cases{{
    {"cmake_minimum_required(VERSION 3.28)", plain, "Ffffffffffffffffffffff         Nnnn", plain},
    {"project(nenenib LANGUAGES CXX)", plain, "Fffffff", plain},
    {"add_executable(nib src/main.cpp)", plain, "Ffffffffffffff", plain},
    {"if(WIN32)", plain, "Kk", plain},
    {"  target_compile_options(nib PRIVATE /W4)", plain, "  Ffffffffffffffffffffff", plain},
    {"elseif(UNIX AND NOT APPLE)", plain, "Kkkkkk", plain},
    {"else()", plain, "Kkkk", plain},
    {"endif()", plain, "Kkkkk", plain},
    {"set(FLAG ON)", plain, "Fff      Vv", plain},
    {"option(USE_X \"Use x\" off)", plain, "Ffffff       Sssssss Vvv", plain},
    {"message(STATUS \"dir: ${CMAKE_SOURCE_DIR}\")", plain,
     "Fffffff        Ssssssssssssssssssssssssss", plain},
    {"set(P $ENV{PATH} $CACHE{X})", plain, "Fff   Vvvvvvvvvv Vvvvvvvvv", plain},
    {"target_link_libraries(nib $<TARGET_FILE:x>)", plain,
     "Fffffffffffffffffffff     Vvvvvvvvvvvvvvvv", plain},
    {"  $<$<CONFIG:Debug>:-g>", plain, "  Vvvvvvvvvvvvvvvvvvvvv", plain},
    {"#[[ bracket comment", plain, "Ccccccccccccccccccc", block},
    {"still ]=] inside", block, "Cccccccccccccccc", block},
    {"]] set(a b)", block, "Cc", plain},
    {"#[==[ level two", plain, "Ccccccccccccccc", lua_c2},
    {"]==]", lua_c2, "Cccc", plain},
    {"set(S [[raw", plain, "Fff   Sssss", raw_empty},
    {"text]])", raw_empty, "Ssssss", plain},
    {"set(T [=[a]]b]=])", plain, "Fff   Ssssssssss", plain},
    {"set(Q \"open", plain, "Fff   Sssss", in_quote},
    {"still \\\" here\")", in_quote, "Ssssssssssssss", plain},
    {"# line comment", plain, "Cccccccccccccc", plain},
    {"foreach(item IN LISTS items)", plain, "Kkkkkkk", plain},
    {"endforeach()", plain, "Kkkkkkkkkk", plain},
    {"function(my_func arg)", plain, "Kkkkkkkk", plain},
    {"endfunction()", plain, "Kkkkkkkkkkk", plain},
    {"macro(m)", plain, "Kkkkk", plain},
    {"endmacro()", plain, "Kkkkkkkk", plain},
    {"while(x)", plain, "Kkkkk", plain},
    {"endwhile()", plain, "Kkkkkkkk", plain},
    {"IF(A)", plain, "Kk", plain},
    {"Add_Library(lib STATIC a.c)", plain, "Fffffffffff", plain},
    {"if(X-NOTFOUND OR FOO_NOTFOUND)", plain, "Kk Vvvvvvvvvv", plain},
    {"set(V 1.2.3 3rdparty)", plain, "Fff   Nnnnn", plain},
    {"set(X ${a_${b}})", plain, "Fff   Vvvvvvvvv", plain},
    {"return()", plain, "Kkkkkk", plain},
    {"block()", plain, "Kkkkk", plain},
    {"endblock()", plain, "Kkkkkkkk", plain},
    {"  VERSION 1.0", plain, "          Nnn", plain},
    {"set(a b) # trailing", plain, "Fff      Cccccccccc", plain},
    {"set(E \"a\\\\\" b)", plain, "Fff   Sssss", plain},
    {"#[=================[ seventeen", plain, "Cccccccccccccccccccccccccccccc", plain},
    {"include (GNUInstallDirs)", plain, "Fffffff", plain},
    {"set(Y y yes TRUE)", plain, "Fff V V Vvv Vvvv", plain},
    {"\"", plain, "S", in_quote},
    {"add_test(NAME t COMMAND nib --lexer)", plain, "Ffffffff", plain},
    {"set(x 1)", raw_xyz, "Fff   N", plain},
    {"", block, "", block},
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
    for (const Language language :
         {Language::c,        Language::cpp,        Language::json,       Language::python,
          Language::markdown, Language::javascript, Language::typescript, Language::html,
          Language::xml,      Language::css,        Language::yaml,       Language::toml,
          Language::gdscript, Language::dosini,     Language::gdresource, Language::cs,
          Language::rust,     Language::go,         Language::php,        Language::sql,
          Language::lua,      Language::dockerfile, Language::powershell, Language::shell,
          Language::cmake})
    {
        expect(core::has_lexer(language), "the twenty-five languages with a lexer report one");
    }
    // #862 で plain の外の言語はすべて字句器を持つ。plain は区間なしで状態をそのまま返す。
    expect(!core::has_lexer(Language::plain), "plain has no lexer");
    for (const LexState &in : {plain, block, raw_xyz})
    {
        const LexLine lexed = core::lex_line(Language::plain, in, "int x = \"s\"; /* c");
        expect(lexed.spans.empty(), "a language without a lexer paints nothing");
        expect(lexed.out == in, "a language without a lexer keeps the state");
        expect(core::lex_state(Language::plain, in, "/* c") == in,
               "lex_state keeps the state for a language without a lexer");
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

// 閉じない文字クラスの後ろに `[/` を 10,000 回並べた 1 行（#836 の追いの指摘）。どの `/` も
// 閉じず 1 字の除算に読み直され、`[` と `/` が 1 字ずつの operators になる。閉じの探しは前に
// 閉じなかった `/` の後ろでは最初の `[` `]` で打ち切るので O(n)。
void verify_script_worst_line()
{
    std::string line;
    for (std::size_t index = 0; index < 10'000U; ++index)
    {
        line += "[/";
    }
    for (const Language language : {Language::javascript, Language::typescript})
    {
        const LexLine lexed = core::lex_line(language, plain, line);
        const bool operators =
            std::ranges::all_of(lexed.spans,
                                [](const ColorSpan &span)
                                {
                                    return span.role == SyntaxRole::operators &&
                                           span.end.value == span.begin.value + 1U;
                                });
        expect(lexed.spans.size() == line.size() && operators && lexed.out == plain,
               "a long line of unclosed regex openers reads every slash as division");
    }
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

// #838: 行をまたぐ状態（コメント・script の中・ブロックスカラー・3 連引用符）も値で次の行へ渡し、
// lex_state は lex_line と同じ状態を返す。
void verify_batch2_threading()
{
    constexpr std::array<std::string_view, 5> html{"<p>a <!-- c", "d --> <script>", "if (a < b)",
                                                   "</script> <b>", "x"};
    constexpr std::array<LexKind, 5> html_kinds{LexKind::block_comment, LexKind::raw_string,
                                                LexKind::raw_string, LexKind::plain,
                                                LexKind::plain};
    verify_threading_of(Language::html, html, html_kinds);
    constexpr std::array<std::string_view, 6> yaml{"a:", "  b: >",   "    text",
                                                   "",   "    more", "  c: 1"};
    constexpr std::array<LexKind, 6> yaml_kinds{LexKind::plain,      LexKind::raw_string,
                                                LexKind::raw_string, LexKind::raw_string,
                                                LexKind::raw_string, LexKind::plain};
    verify_threading_of(Language::yaml, yaml, yaml_kinds);
    constexpr std::array<std::string_view, 4> toml{"a = '''", "b", "c''' # d", "[t]"};
    constexpr std::array<LexKind, 4> toml_kinds{LexKind::raw_string, LexKind::raw_string,
                                                LexKind::plain, LexKind::plain};
    verify_threading_of(Language::toml, toml, toml_kinds);
}

[[nodiscard]] std::string repeated_text(std::string_view piece, std::size_t count)
{
    std::string text;
    text.reserve(piece.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        text += piece;
    }
    return text;
}

// #838: 閉じの無い字句を並べた長い 1 行（決定 3）。どれも 1 回の走査で読み（二乗なら Debug で目に
// 見えて遅くなる）、区間は昇順のまま。
void verify_batch2_worst_lines()
{
    const LexLine tags = core::lex_line(Language::html, plain, repeated_text("<a", 10'000U));
    expect(tags.spans.size() == 10'000U && tags.out == plain,
           "a long line of unclosed tags paints each tag head");
    const LexLine raw =
        core::lex_line(Language::html, plain, "<script>" + repeated_text("</scrip", 3'000U));
    expect(raw.spans.size() == 2U && raw.out == in_script,
           "a long script line without its closing tag stays inside the script");
    const std::string css = repeated_text("{a:", 10'000U);
    expect(well_formed(core::lex_line(Language::css, plain, css).spans, css),
           "a long css line of selectors reads in one pass");
    const LexLine yaml = core::lex_line(Language::yaml, plain, repeated_text("- ", 10'000U));
    expect(yaml.spans.size() == 10'000U, "a long yaml line of list markers paints each marker");
    const std::string toml = repeated_text("{a.a,", 5'000U);
    expect(core::lex_line(Language::toml, plain, toml).out == plain,
           "a long toml line of unfinished keys reads in one pass");
}

// #855: GDScript の 3 連引用符と gdresource の `"…"` も値で次の行へ渡し、lex_state は lex_line と
// 同じ状態を返す。dosini は行をまたがない。
void verify_godot_threading()
{
    constexpr std::array<std::string_view, 4> gdscript{"var s := \"\"\"a", "b # not comment",
                                                       "c\"\"\" # c", "func f():"};
    constexpr std::array<LexKind, 4> gdscript_kinds{LexKind::raw_string, LexKind::raw_string,
                                                    LexKind::plain, LexKind::plain};
    verify_threading_of(Language::gdscript, gdscript, gdscript_kinds);
    constexpr std::array<std::string_view, 5> resource{"[sub_resource type=\"GDScript\"]",
                                                       "script/source = \"extends Node", "",
                                                       "\tpass\"", "[node name=\"a\"]"};
    constexpr std::array<LexKind, 5> resource_kinds{
        LexKind::plain, LexKind::raw_string, LexKind::raw_string, LexKind::plain, LexKind::plain};
    verify_threading_of(Language::gdresource, resource, resource_kinds);
    constexpr std::array<std::string_view, 3> ini{"a = \"x", "[s]", "; c"};
    constexpr std::array<LexKind, 3> ini_kinds{LexKind::plain, LexKind::plain, LexKind::plain};
    verify_threading_of(Language::dosini, ini, ini_kinds);
}

// #855: 型の注釈・`%`・見出しの入れ子を何度も読み直しうる長い 1 行（決定 3）。どれも 1 回の走査で
// 読み（二乗なら Debug で目に見えて遅くなる）、区間は昇順のまま。
void verify_godot_worst_lines()
{
    const std::string spaces(20'000U, ' ');
    const std::string params = "func f(" + repeated_text("a: b, ", 5'000U);
    for (const std::string &line : {params, spaces + params, repeated_text("x %y (%z ", 5'000U)})
    {
        expect(well_formed(core::lex_line(Language::gdscript, plain, line).spans, line),
               "a long gdscript line of annotations and percent signs reads in one pass");
    }
    const std::string header = "[node " + repeated_text("a=[b ", 5'000U);
    const LexLine resource = core::lex_line(Language::gdresource, plain, header);
    expect(well_formed(resource.spans, header) && resource.out == plain,
           "a long gdresource header of nested brackets reads in one pass");
    const std::string ini = repeated_text("a = \"b\" ", 5'000U);
    expect(well_formed(core::lex_line(Language::dosini, plain, ini).spans, ini),
           "a long ini line of quoted values reads in one pass");
}

// #857: C# の逐語的文字列と生の文字列、Rust の入れ子のコメントと生の文字列、Go の生の文字列も値で
// 次の行へ渡し、lex_state は lex_line と同じ状態を返す。
void verify_c_family_threading()
{
    constexpr std::array<std::string_view, 6> csharp{"var s = @\"a",        "b \"\" c", "d\"; /* e",
                                                     "f */ var r = \"\"\"", "g",        "\"\"\";"};
    constexpr std::array<LexKind, 6> csharp_kinds{LexKind::raw_string,    LexKind::raw_string,
                                                  LexKind::block_comment, LexKind::raw_string,
                                                  LexKind::raw_string,    LexKind::plain};
    verify_threading_of(Language::cs, csharp, csharp_kinds);
    constexpr std::array<std::string_view, 5> rust{"/* a /* b", "c */", "d */ let s = r#\"x",
                                                   "y\"#; let t = \"z", "w\";"};
    constexpr std::array<LexKind, 5> rust_kinds{LexKind::block_comment, LexKind::block_comment,
                                                LexKind::raw_string, LexKind::raw_string,
                                                LexKind::plain};
    verify_threading_of(Language::rust, rust, rust_kinds);
    constexpr std::array<std::string_view, 4> go{"x := `a", "b", "c` /* d", "e */ y"};
    constexpr std::array<LexKind, 4> go_kinds{LexKind::raw_string, LexKind::raw_string,
                                              LexKind::block_comment, LexKind::plain};
    verify_threading_of(Language::go, go, go_kinds);
}

// #857: 入れ子のコメントの開き・ライフタイム・閉じない属性・閉じない補間・受け手の `(` を並べた長い
// 1 行（決定 4）。どれも 1 回の走査で読み（二乗なら Debug
// で目に見えて遅くなる）、区間は昇順のまま。
void verify_c_family_worst_lines()
{
    const LexLine nests = core::lex_line(Language::rust, plain, repeated_text("/* ", 10'000U));
    expect(nests.spans.size() == 1U && nests.out == nested("10000"),
           "a long rust line of comment openers counts the depth in one pass");
    const LexLine lifetimes = core::lex_line(Language::rust, plain, repeated_text("'a ", 10'000U));
    expect(lifetimes.spans.size() == 10'000U && lifetimes.out == plain,
           "a long rust line of lifetimes paints each lifetime");
    const std::string attributes = repeated_text("#[a", 5'000U);
    const LexLine attribute = core::lex_line(Language::rust, plain, attributes);
    expect(attribute.spans.size() == 1U && attribute.out == plain,
           "a long rust line of unclosed attributes is one attribute");
    const std::string holes = repeated_text("$\"{\"", 5'000U);
    expect(well_formed(core::lex_line(Language::cs, plain, holes).spans, holes),
           "a long csharp line of unclosed interpolation holes reads in one pass");
    const std::string receivers = repeated_text("func (", 5'000U);
    expect(well_formed(core::lex_line(Language::go, plain, receivers).spans, receivers),
           "a long go line of unclosed receivers reads in one pass");
}
// #860: PHP の文字列・コメント・ヒアドキュメントと `?>`、SQL のコメントと文字列、Lua の長い括弧、
// Dockerfile の行の続きとヒアドキュメントも値で次の行へ渡し、lex_state は lex_line と同じ状態を
// 返す。PHP の中と行の続きは plain の種類のまま。
void verify_batch4_threading()
{
    constexpr std::array<std::string_view, 8> php{
        "<?php", "$s = 'a", "b';", "/* c", "d */ $h = <<<EOT", "x", "EOT; ?>", "<p>"};
    constexpr std::array<LexKind, 8> php_kinds{
        LexKind::plain,      LexKind::raw_string, LexKind::plain, LexKind::block_comment,
        LexKind::raw_string, LexKind::raw_string, LexKind::plain, LexKind::plain};
    verify_threading_of(Language::php, php, php_kinds);
    constexpr std::array<std::string_view, 3> sql{"/* a", "b */ x = 'c", "d'"};
    constexpr std::array<LexKind, 3> sql_kinds{LexKind::block_comment, LexKind::raw_string,
                                               LexKind::plain};
    verify_threading_of(Language::sql, sql, sql_kinds);
    constexpr std::array<std::string_view, 5> lua{"--[[ a", "b ]]", "s = [==[", "x", "]==]"};
    constexpr std::array<LexKind, 5> lua_kinds{LexKind::block_comment, LexKind::plain,
                                               LexKind::raw_string, LexKind::raw_string,
                                               LexKind::plain};
    verify_threading_of(Language::lua, lua, lua_kinds);
    constexpr std::array<std::string_view, 5> docker{"RUN a \\", "b", "RUN <<EOF", "x", "EOF"};
    constexpr std::array<LexKind, 5> docker_kinds{
        LexKind::plain, LexKind::plain, LexKind::raw_string, LexKind::raw_string, LexKind::plain};
    verify_threading_of(Language::dockerfile, docker, docker_kinds);
    LexState state{};
    for (const std::string_view line : {"<?php", "$s = 'a", "b';"})
    {
        state = core::lex_line(Language::php, state, line).out;
    }
    expect(state == php_code, "a closed string goes back inside php");
}

// #860: 開きのタグの頭・閉じない属性・ヒアドキュメントの開きに見える `<<<`・長い括弧の中の `]`・
// `''` の続く文字列・閉じの語の無い `<<` と閉じない `${` を並べた長い 1 行（決定 4）。どれも
// 1 回の走査で読み（二乗なら Debug で目に見えて遅くなる）、区間は昇順のまま。
void verify_batch4_worst_lines()
{
    const LexLine tags = core::lex_line(Language::php, plain, repeated_text("<?", 10'000U));
    expect(tags.spans.empty() && tags.out == plain,
           "a long html line of tag heads without php stays outside");
    const LexLine attribute =
        core::lex_line(Language::php, php_code, repeated_text("#['a", 5'000U));
    expect(well_formed(attribute.spans, repeated_text("#['a", 5'000U)) && attribute.out == php_code,
           "a long php line of unclosed attributes reads in one pass");
    const std::string heredocs = repeated_text("<<<a ", 5'000U);
    expect(well_formed(core::lex_line(Language::php, php_code, heredocs).spans, heredocs),
           "a long php line of heredoc look-alikes reads in one pass");
    const LexLine brackets = core::lex_line(
        Language::lua, plain, "[================[" + repeated_text("]===============]", 2'000U));
    expect(brackets.spans.size() == 1U && brackets.out.kind == LexKind::raw_string,
           "a long lua string of near closings stays open");
    const LexLine quotes = core::lex_line(Language::sql, plain, "'" + repeated_text("''", 10'000U));
    expect(quotes.spans.size() == 1U && quotes.out == in_squote,
           "a long sql string of doubled quotes stays open");
    const LexLine markers =
        core::lex_line(Language::dockerfile, plain, "RUN " + repeated_text("<<${", 5'000U));
    expect(markers.spans.size() == 2U && markers.out == plain,
           "a long dockerfile line of heredoc heads and open variables reads in one pass");
}

// #862: PowerShell のブロックコメント・ヒア文字列・引用符、shell のヒアドキュメントと引用符、CMake
// の 括弧のコメント・括弧の引数・引用符も値で次の行へ渡し、lex_state は lex_line と同じ状態を返す。
void verify_batch5_threading()
{
    constexpr std::array<std::string_view, 7> powershell{"<# a", "b #>",     "$s = @'", "x",
                                                         "'@",   "$t = \"a", "b\""};
    constexpr std::array<LexKind, 7> powershell_kinds{
        LexKind::block_comment, LexKind::plain,      LexKind::raw_string, LexKind::raw_string,
        LexKind::plain,         LexKind::raw_string, LexKind::plain};
    verify_threading_of(Language::powershell, powershell, powershell_kinds);
    constexpr std::array<std::string_view, 6> shell{"cat <<-EOF", "\tx", "\tEOF",
                                                    "echo 'a",    "b'",  "x"};
    constexpr std::array<LexKind, 6> shell_kinds{LexKind::raw_string, LexKind::raw_string,
                                                 LexKind::plain,      LexKind::raw_string,
                                                 LexKind::plain,      LexKind::plain};
    verify_threading_of(Language::shell, shell, shell_kinds);
    constexpr std::array<std::string_view, 7> cmake{"#[[ a", "]]",        "set(x [==[", "y",
                                                    "]==])", "set(q \"a", "b\")"};
    constexpr std::array<LexKind, 7> cmake_kinds{
        LexKind::block_comment, LexKind::plain,      LexKind::raw_string, LexKind::raw_string,
        LexKind::plain,         LexKind::raw_string, LexKind::plain};
    verify_threading_of(Language::cmake, cmake, cmake_kinds);
}

// #862: 閉じない引用符の続き・閉じの無い算術の頭・長すぎる閉じの語・閉じの無い括弧とジェネレータ
// 式を並べた長い 1 行（決定 5）。どれも 1 回の走査で読み、区間は昇順のまま。
void verify_batch5_worst_lines()
{
    const LexLine quotes =
        core::lex_line(Language::powershell, plain, "'" + repeated_text("''", 10'000U));
    expect(quotes.spans.size() == 1U && quotes.out == in_squote,
           "a long powershell string of doubled quotes stays open");
    const std::string types = repeated_text("[a.b -x ", 5'000U);
    expect(well_formed(core::lex_line(Language::powershell, plain, types).spans, types),
           "a long powershell line of types and parameters reads in one pass");
    const std::string arithmetic = "echo " + repeated_text("$((1<<", 5'000U);
    const LexLine counted = core::lex_line(Language::shell, plain, arithmetic);
    expect(well_formed(counted.spans, arithmetic) && counted.out == plain,
           "a long shell line of unclosed arithmetic opens no heredoc");
    const std::string markers = "cat " + repeated_text("<<ABCDEFGHIJKLMNOPQ ", 2'000U);
    expect(core::lex_line(Language::shell, plain, markers).out == plain,
           "a long shell line of too long heredoc words opens none");
    const LexLine brackets =
        core::lex_line(Language::cmake, plain, "#[==[" + repeated_text("]=]", 3'000U));
    expect(brackets.spans.size() == 1U && brackets.out == lua_c2,
           "a long cmake bracket comment of near closings stays open");
    const LexLine genexes =
        core::lex_line(Language::cmake, plain, "set(x " + repeated_text("$<${", 5'000U));
    expect(genexes.spans.size() == 2U && genexes.out == plain,
           "a long cmake line of unclosed references reads in one pass");
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
    verify_language_table(Language::html, html_common_cases);
    verify_language_table(Language::html, html_only_cases);
    verify_language_table(Language::xml, html_common_cases);
    verify_language_table(Language::xml, xml_only_cases);
    verify_language_table(Language::css, css_cases);
    verify_language_table(Language::yaml, yaml_cases);
    verify_language_table(Language::toml, toml_cases);
    verify_language_table(Language::gdscript, gdscript_cases);
    verify_language_table(Language::dosini, dosini_cases);
    verify_language_table(Language::gdresource, gdresource_cases);
    verify_language_table(Language::cs, csharp_cases);
    verify_language_table(Language::rust, rust_cases);
    verify_language_table(Language::go, go_cases);
    verify_language_table(Language::php, php_cases);
    verify_language_table(Language::sql, sql_cases);
    verify_language_table(Language::lua, lua_cases);
    verify_language_table(Language::dockerfile, dockerfile_cases);
    verify_language_table(Language::powershell, powershell_cases);
    verify_language_table(Language::shell, shell_cases);
    verify_language_table(Language::cmake, cmake_cases);
    verify_other_languages();
    verify_columns();
    verify_state_threading();
    verify_markdown_and_script_threading();
    verify_batch2_threading();
    verify_batch2_worst_lines();
    verify_godot_threading();
    verify_godot_worst_lines();
    verify_c_family_threading();
    verify_c_family_worst_lines();
    verify_batch4_threading();
    verify_batch4_worst_lines();
    verify_batch5_threading();
    verify_batch5_worst_lines();
    verify_markdown_worst_lines();
    verify_script_worst_line();
}
} // namespace nenenib::tests
