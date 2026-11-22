#include "Lexer.hpp"

#include "LexCmake.hpp"
#include "LexCpp.hpp"
#include "LexCsharp.hpp"
#include "LexCss.hpp"
#include "LexCursor.hpp"
#include "LexDockerfile.hpp"
#include "LexGdscript.hpp"
#include "LexGo.hpp"
#include "LexHtml.hpp"
#include "LexIni.hpp"
#include "LexJavaScript.hpp"
#include "LexJson.hpp"
#include "LexLua.hpp"
#include "LexMarkdown.hpp"
#include "LexOutput.hpp"
#include "LexPhp.hpp"
#include "LexPowershell.hpp"
#include "LexPython.hpp"
#include "LexRust.hpp"
#include "LexShell.hpp"
#include "LexSql.hpp"
#include "LexToml.hpp"
#include "LexYaml.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace nenenib::core
{
namespace
{
// 言語ごとの字句の関数（#1917・CPP-012 の表）。i 行目が i 番目の Language で、下の static_assert が
// 行の言語を値の順と数で縛る（値を足すと行が足りず、既定の {plain, nullptr} の行の言語が順に合わず
// に落ちる）。字句器を持たない言語は nullptr（区間なしで状態をそのまま残す）。字句器の有無は欄が
// nullptr かで決め、関数の番地を比べない（`/OPT:ICF` が同じ機械語の関数を畳んでも狂わない）。
// lex_line と lex_state はここを共有し、違うのは cursor が区間を残すかだけ。
using LanguageLexer = void (*)(LexCursor &, const LexState &, Language);

template <void (*lex)(LexCursor &, const LexState &)>
void lex_alone(LexCursor &cursor, const LexState &in, Language /*language*/)
{
    lex(cursor, in);
}

using LanguageLexerRow = std::pair<Language, LanguageLexer>;

constexpr std::array<LanguageLexerRow, language_count()> language_lexers{{
    {Language::plain, nullptr},
    {Language::markdown, lex_alone<lex_markdown>},
    {Language::html, lex_html},
    {Language::c, lex_alone<lex_cpp>},
    {Language::cpp, lex_alone<lex_cpp>},
    {Language::python, lex_alone<lex_python>},
    {Language::json, lex_alone<lex_json>},
    {Language::javascript, lex_javascript},
    {Language::typescript, lex_javascript},
    {Language::yaml, lex_alone<lex_yaml>},
    {Language::toml, lex_alone<lex_toml>},
    {Language::xml, lex_html},
    {Language::css, lex_alone<lex_css>},
    {Language::powershell, lex_alone<lex_powershell>},
    {Language::shell, lex_alone<lex_shell>},
    {Language::cmake, lex_alone<lex_cmake>},
    {Language::gdscript, lex_alone<lex_gdscript>},
    {Language::dosini, lex_ini},
    {Language::gdresource, lex_ini},
    {Language::cs, lex_alone<lex_csharp>},
    {Language::rust, lex_alone<lex_rust>},
    {Language::go, lex_alone<lex_go>},
    {Language::php, lex_alone<lex_php>},
    {Language::sql, lex_alone<lex_sql>},
    {Language::lua, lex_alone<lex_lua>},
    {Language::dockerfile, lex_alone<lex_dockerfile>},
    {Language::java, nullptr},
    {Language::kotlin, nullptr},
    {Language::dart, nullptr},
    {Language::zig, nullptr},
    {Language::terraform, nullptr},
    {Language::make, nullptr},
    {Language::batch, nullptr},
    {Language::ruby, nullptr},
    {Language::swift, nullptr},
    {Language::scala, nullptr},
    {Language::haskell, nullptr},
    {Language::elixir, nullptr},
    {Language::r, nullptr},
    {Language::perl, nullptr},
    {Language::vue, nullptr},
    {Language::svelte, nullptr},
}};

[[nodiscard]] consteval bool lexers_in_order() noexcept
{
    for (std::size_t index = 0; index < language_lexers.size(); ++index)
    {
        if (std::to_underlying(language_lexers.at(index).first) != index)
        {
            return false;
        }
    }
    return true;
}

static_assert(lexers_in_order(), "language_lexers has one row for every Language in order");

[[nodiscard]] LanguageLexer lexer_of(Language language) noexcept
{
    return language_lexers.at(std::to_underlying(language)).second;
}

void lex_language(Language language, LexCursor &cursor, const LexState &in)
{
    const LanguageLexer lexer = lexer_of(language);
    if (lexer == nullptr)
    {
        cursor.leave(in);
        return;
    }
    lexer(cursor, in, language);
}
} // namespace

LexLine lex_line(Language language, const LexState &in, std::string_view line) noexcept
{
    LexCursor cursor(line, LexOutput::spans);
    lex_language(language, cursor, in);
    return std::move(cursor).finish();
}

LexState lex_state(Language language, const LexState &in, std::string_view line) noexcept
{
    LexCursor cursor(line, LexOutput::state_only);
    lex_language(language, cursor, in);
    return cursor.state();
}

bool has_lexer(Language language) noexcept
{
    return lexer_of(language) != nullptr;
}

LexState lex_start_state(Language language, bool from_first_line) noexcept
{
    switch (language)
    {
    case Language::php:
        return php_start_state(from_first_line);
    case Language::c:
    case Language::cpp:
    case Language::python:
    case Language::json:
    case Language::markdown:
    case Language::javascript:
    case Language::typescript:
    case Language::html:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::gdscript:
    case Language::dosini:
    case Language::gdresource:
    case Language::cs:
    case Language::rust:
    case Language::go:
    case Language::sql:
    case Language::lua:
    case Language::dockerfile:
    case Language::plain:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
    case Language::java:
    case Language::kotlin:
    case Language::dart:
    case Language::zig:
    case Language::terraform:
    case Language::make:
    case Language::batch:
    case Language::ruby:
    case Language::swift:
    case Language::scala:
    case Language::haskell:
    case Language::elixir:
    case Language::r:
    case Language::perl:
    case Language::vue:
    case Language::svelte:
        return LexState{};
    }
    std::unreachable();
}

std::size_t syntax_backfill_lines(Language language) noexcept
{
    switch (language)
    {
    case Language::json:
    case Language::dosini:
        return 1;
    case Language::c:
    case Language::cpp:
    case Language::python:
    case Language::javascript:
    case Language::typescript:
    case Language::html:
    case Language::xml:
    case Language::css:
    case Language::toml:
    case Language::gdscript:
    case Language::gdresource:
    case Language::cs:
    case Language::rust:
    case Language::go:
    case Language::php:
    case Language::sql:
    case Language::lua:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return 200;
    case Language::dockerfile:
        return 50;
    case Language::yaml:
        return 100;
    case Language::markdown:
        return 500;
    case Language::plain:
    case Language::java:
    case Language::kotlin:
    case Language::dart:
    case Language::zig:
    case Language::terraform:
    case Language::make:
    case Language::batch:
    case Language::ruby:
    case Language::swift:
    case Language::scala:
    case Language::haskell:
    case Language::elixir:
    case Language::r:
    case Language::perl:
    case Language::vue:
    case Language::svelte:
        return 0;
    }
    std::unreachable();
}
} // namespace nenenib::core
