#include "ExplorerMark.hpp"

#include "Language.hpp"
#include "LanguageTable.hpp"

#include <utility>

namespace nenenib::core
{
std::optional<SyntaxRole> explorer_mark_role(std::string_view name) noexcept
{
    // 言語を足すとこの switch がコンパイルで落ちる（CPP-002）。色の役割は採用案の表のとおり。
    switch (language_of_path(name))
    {
    case Language::c:
    case Language::cpp:
        return header_of_path(name) ? SyntaxRole::type : SyntaxRole::function;
    case Language::python:
        return SyntaxRole::function;
    case Language::javascript:
    case Language::typescript:
        return SyntaxRole::type;
    case Language::markdown:
        return SyntaxRole::string;
    case Language::json:
    case Language::yaml:
    case Language::toml:
        return SyntaxRole::keyword;
    case Language::html:
    case Language::xml:
    case Language::css:
        return SyntaxRole::operators;
    case Language::plain:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
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
        return std::nullopt;
    }
    std::unreachable();
}
} // namespace nenenib::core
