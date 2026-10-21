#include "Lexer.hpp"

#include "LexCpp.hpp"

#include <utility>

namespace nenenib::core
{
// 言語を足すと switch が落ちる（CPP-002）。字句器を持たない言語は区間なしで状態をそのまま返す。
LexLine lex_line(Language language, const LexState &in, std::string_view line) noexcept
{
    switch (language)
    {
    case Language::c:
    case Language::cpp:
        return lex_cpp_line(in, line);
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::python:
    case Language::json:
    case Language::javascript:
    case Language::typescript:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
        return LexLine{{}, in};
    }
    std::unreachable();
}
} // namespace nenenib::core
