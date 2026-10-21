#include "Lexer.hpp"

#include "LexCpp.hpp"
#include "LexCursor.hpp"
#include "LexJson.hpp"
#include "LexOutput.hpp"
#include "LexPython.hpp"

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
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::javascript:
    case Language::typescript:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
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
        return true;
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::javascript:
    case Language::typescript:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return false;
    }
    std::unreachable();
}
} // namespace nenenib::core
