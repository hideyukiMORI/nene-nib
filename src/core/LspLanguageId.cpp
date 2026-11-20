#include "LspLanguageId.hpp"

#include "AsciiCase.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"

#include <string_view>

namespace nenenib::core
{
namespace
{
// path が拡張子 extension（`.` を含む）で終わるか（大小を問わない）。
[[nodiscard]] bool ends_with_extension(std::string_view path, std::string_view extension) noexcept
{
    return path.size() > extension.size() &&
           ascii_equal_ignoring_case(path.substr(path.size() - extension.size()), extension);
}
} // namespace

// 言語を足すとこの switch が落ちる（CPP-002）。足した言語の行を仕様の表で決める。
std::string_view lsp_language_id(Language language, std::string_view path) noexcept
{
    switch (language)
    {
    case Language::plain:
        return "plaintext";
    case Language::powershell:
        return "powershell";
    case Language::shell:
        return "shellscript";
    case Language::dosini:
        return "ini";
    case Language::cs:
        return "csharp";
    case Language::javascript:
        return ends_with_extension(path, ".jsx") ? "javascriptreact" : "javascript";
    case Language::typescript:
        return ends_with_extension(path, ".tsx") ? "typescriptreact" : "typescript";
    case Language::markdown:
    case Language::html:
    case Language::c:
    case Language::cpp:
    case Language::python:
    case Language::json:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::cmake:
    case Language::gdscript:
    case Language::gdresource:
    case Language::rust:
    case Language::go:
    case Language::php:
    case Language::sql:
    case Language::lua:
    case Language::dockerfile:
        return language_name(language);
    }
    return language_name(language);
}
} // namespace nenenib::core
