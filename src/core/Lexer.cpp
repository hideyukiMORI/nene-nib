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

#include <utility>

namespace nenenib::core
{
namespace
{
// 言語を足すと switch が落ちる（CPP-002）。字句器を持たない言語は区間なしで状態をそのまま残す。
// lex_line と lex_state はここを共有し、違うのは cursor が区間を残すかだけ。字句器は void を返し、
// 分岐を 1 行ずつにするため `return lex_…(…);` で書く（関数長 60 行・#860）。
void lex_language(Language language, LexCursor &cursor, const LexState &in)
{
    switch (language)
    {
    case Language::c:
    case Language::cpp:
        return lex_cpp(cursor, in);
    case Language::json:
        return lex_json(cursor, in);
    case Language::python:
        return lex_python(cursor, in);
    case Language::markdown:
        return lex_markdown(cursor, in);
    case Language::javascript:
    case Language::typescript:
        return lex_javascript(cursor, in, language);
    case Language::html:
    case Language::xml:
        return lex_html(cursor, in, language);
    case Language::css:
        return lex_css(cursor, in);
    case Language::yaml:
        return lex_yaml(cursor, in);
    case Language::toml:
        return lex_toml(cursor, in);
    case Language::gdscript:
        return lex_gdscript(cursor, in);
    case Language::dosini:
    case Language::gdresource:
        return lex_ini(cursor, in, language);
    case Language::cs:
        return lex_csharp(cursor, in);
    case Language::rust:
        return lex_rust(cursor, in);
    case Language::go:
        return lex_go(cursor, in);
    case Language::php:
        return lex_php(cursor, in);
    case Language::sql:
        return lex_sql(cursor, in);
    case Language::lua:
        return lex_lua(cursor, in);
    case Language::dockerfile:
        return lex_dockerfile(cursor, in);
    case Language::powershell:
        return lex_powershell(cursor, in);
    case Language::shell:
        return lex_shell(cursor, in);
    case Language::cmake:
        return lex_cmake(cursor, in);
    case Language::plain:
        cursor.leave(in);
        return;
    }
    std::unreachable();
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
    switch (language)
    {
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
    case Language::php:
    case Language::sql:
    case Language::lua:
    case Language::dockerfile:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return true;
    case Language::plain:
        return false;
    }
    std::unreachable();
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
        return 0;
    }
    std::unreachable();
}
} // namespace nenenib::core
