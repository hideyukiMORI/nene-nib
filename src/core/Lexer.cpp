#include "Lexer.hpp"

#include "LexCpp.hpp"
#include "LexCss.hpp"
#include "LexCursor.hpp"
#include "LexGdscript.hpp"
#include "LexHtml.hpp"
#include "LexIni.hpp"
#include "LexJavaScript.hpp"
#include "LexJson.hpp"
#include "LexMarkdown.hpp"
#include "LexOutput.hpp"
#include "LexPython.hpp"
#include "LexToml.hpp"
#include "LexYaml.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// 言語を足すと switch が落ちる（CPP-002）。字句器を持たない言語は区間なしで状態をそのまま残す。
// lex_line と lex_state はここを共有し、違うのは cursor が区間を残すかだけ。
void lex_language(Language language, LexCursor &cursor, const LexState &in)
{
    switch (language)
    {
    case Language::c:
    case Language::cpp:
        lex_cpp(cursor, in);
        return;
    case Language::json:
        lex_json(cursor, in);
        return;
    case Language::python:
        lex_python(cursor, in);
        return;
    case Language::markdown:
        lex_markdown(cursor, in);
        return;
    case Language::javascript:
    case Language::typescript:
        lex_javascript(cursor, in, language);
        return;
    case Language::html:
    case Language::xml:
        lex_html(cursor, in, language);
        return;
    case Language::css:
        lex_css(cursor, in);
        return;
    case Language::yaml:
        lex_yaml(cursor, in);
        return;
    case Language::toml:
        lex_toml(cursor, in);
        return;
    case Language::gdscript:
        lex_gdscript(cursor, in);
        return;
    case Language::dosini:
    case Language::gdresource:
        lex_ini(cursor, in, language);
        return;
    case Language::plain:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
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
        return true;
    case Language::plain:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return false;
    }
    std::unreachable();
}

std::size_t syntax_backfill_lines(Language language) noexcept
{
    switch (language)
    {
    case Language::json:
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
        return 200;
    case Language::dosini:
        return 50;
    case Language::yaml:
        return 100;
    case Language::markdown:
        return 500;
    case Language::plain:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return 0;
    }
    std::unreachable();
}
} // namespace nenenib::core
