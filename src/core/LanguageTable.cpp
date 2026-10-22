#include "LanguageTable.hpp"

#include "AsciiCase.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <utility>

namespace nenenib::core
{
namespace
{
// Language の値か。値を足すとこの switch がコンパイルで落ち（CPP-002）、足した後は下の数え上げが
// 1 つ増えて、名前の表に行が無ければ static_assert が落ちる（OperationTexts.cpp と同じ形）。
[[nodiscard]] consteval bool is_language(Language language) noexcept
{
    switch (language)
    {
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::c:
    case Language::cpp:
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
        return true;
    }
    return false;
}

// 値は 0 から連続して並ぶので、最初に値でなくなる所までが値の数。
[[nodiscard]] consteval std::size_t language_count() noexcept
{
    std::size_t count = 0;
    while (is_language(static_cast<Language>(count)))
    {
        ++count;
    }
    return count;
}

// 名前の表（決定 3）。i 行目が i 番目の値の Vim の filetype 名で、language_name と
// language_of_name の 2 本が同じこの配列を読む。
constexpr std::array<std::string_view, 26> language_names{
    "text",       "markdown", "html", "c",   "cpp", "python", "json",  "javascript", "typescript",
    "yaml",       "toml",     "xml",  "css", "ps1", "sh",     "cmake", "gdscript",   "dosini",
    "gdresource", "cs",       "rust", "go",  "php", "sql",    "lua",   "dockerfile"};

static_assert(language_names.size() == language_count(),
              "language_names has one row for every Language");

// 拡張子の表（決定 2）。拡張子は小文字で書き、照合は ASCII の大文字小文字を畳む。txt の行は
// 表に無い拡張子と同じ plain だが、初版の表の 1 行として書いておく。
using LanguageExtension = std::pair<std::string_view, Language>;

constexpr std::array<LanguageExtension, 64> language_extensions{{
    {"md", Language::markdown},
    {"markdown", Language::markdown},
    {"html", Language::html},
    {"htm", Language::html},
    {"c", Language::c},
    {"h", Language::c},
    {"cpp", Language::cpp},
    {"cc", Language::cpp},
    {"cxx", Language::cpp},
    {"hpp", Language::cpp},
    {"hh", Language::cpp},
    {"hxx", Language::cpp},
    {"ipp", Language::cpp},
    {"py", Language::python},
    {"pyi", Language::python},
    {"json", Language::json},
    {"js", Language::javascript},
    {"mjs", Language::javascript},
    {"cjs", Language::javascript},
    {"ts", Language::typescript},
    {"mts", Language::typescript},
    {"cts", Language::typescript},
    {"yml", Language::yaml},
    {"yaml", Language::yaml},
    {"toml", Language::toml},
    {"xml", Language::xml},
    {"css", Language::css},
    {"ps1", Language::powershell},
    {"psm1", Language::powershell},
    {"psd1", Language::powershell},
    {"sh", Language::shell},
    {"bash", Language::shell},
    {"zsh", Language::shell},
    {"cmake", Language::cmake},
    {"gd", Language::gdscript},
    {"ini", Language::dosini},
    {"tscn", Language::gdresource},
    {"tres", Language::gdresource},
    {"godot", Language::gdresource},
    {"cs", Language::cs},
    {"csx", Language::cs},
    {"cake", Language::cs},
    {"rs", Language::rust},
    {"go", Language::go},
    {"php", Language::php},
    {"php0", Language::php},
    {"php1", Language::php},
    {"php2", Language::php},
    {"php3", Language::php},
    {"php4", Language::php},
    {"php5", Language::php},
    {"php6", Language::php},
    {"php7", Language::php},
    {"php8", Language::php},
    {"php9", Language::php},
    {"phtml", Language::php},
    {"ctp", Language::php},
    {"phpt", Language::php},
    {"theme", Language::php},
    {"sql", Language::sql},
    {"lua", Language::lua},
    {"rockspec", Language::lua},
    {"dockerfile", Language::dockerfile},
    {"txt", Language::plain},
}};

// 拡張子ではなく名前で決まる行（決定 2）。照合は拡張子と同じく ASCII の大文字小文字を畳む。
// Dockerfile と Containerfile（#860）と `.luacheckrc` は Vim の filetype.vim の名前の行。
using LanguageName = std::pair<std::string_view, Language>;

constexpr std::array<LanguageName, 4> language_file_names{{
    {"CMakeLists.txt", Language::cmake},
    {"Dockerfile", Language::dockerfile},
    {"Containerfile", Language::dockerfile},
    {".luacheckrc", Language::lua},
}};

// 拡張子の表に当たらなかった名前の頭（#860）。Vim の filetype.vim の最後の節
// `Dockerfile.*` `Containerfile.*`（StarSetf）と同じく、拡張子の表が先に勝つ。
constexpr std::array<std::string_view, 2> dockerfile_prefixes{"Dockerfile.", "Containerfile."};

[[nodiscard]] constexpr bool same_ignoring_case(std::string_view left,
                                                std::string_view right) noexcept
{
    return left.size() == right.size() && ascii_prefix_ignoring_case(left, right);
}

[[nodiscard]] constexpr bool is_separator(char byte) noexcept
{
    return byte == '\\' || byte == '/';
}

[[nodiscard]] constexpr bool is_dot(char byte) noexcept
{
    return byte == '.';
}

using BytePredicate = bool (*)(char) noexcept;

// text の中で最後に is_mark が真になるバイトの位置。標準の後ろからの検索（find_last_of・rfind）は
// MSVC STL の __std_find_last_* を core の外へ出すので、素の逆走査で書く（ARC-003）。
[[nodiscard]] constexpr std::optional<std::size_t> last_where(std::string_view text,
                                                              BytePredicate is_mark) noexcept
{
    for (std::size_t index = text.size(); index > 0; --index)
    {
        if (is_mark(text[index - 1]))
        {
            return index - 1;
        }
    }
    return std::nullopt;
}

[[nodiscard]] constexpr std::string_view file_name_of(std::string_view path) noexcept
{
    const auto separator = last_where(path, is_separator);
    return separator.has_value() ? path.substr(separator.value() + 1) : path;
}
} // namespace

Language language_of_path(std::string_view path) noexcept
{
    const auto name = file_name_of(path);
    const auto named = std::ranges::find_if(language_file_names, [name](const LanguageName &row)
                                            { return same_ignoring_case(name, row.first); });
    if (named != language_file_names.end())
    {
        return named->second;
    }
    const auto dot = last_where(name, is_dot);
    const auto extension = dot.has_value() ? name.substr(dot.value() + 1) : std::string_view{};
    const auto found =
        std::ranges::find_if(language_extensions, [extension](const LanguageExtension &row)
                             { return same_ignoring_case(extension, row.first); });
    if (dot.has_value() && found != language_extensions.end())
    {
        return found->second;
    }
    const bool dockerfile =
        std::ranges::any_of(dockerfile_prefixes, [name](std::string_view prefix)
                            { return ascii_prefix_ignoring_case(name, prefix); });
    return dockerfile ? Language::dockerfile : Language::plain;
}

std::string_view language_name(Language language) noexcept
{
    return language_names[std::to_underlying(language)];
}

std::optional<Language> language_of_name(std::string_view name) noexcept
{
    const auto found = std::ranges::find(language_names, name);
    if (found == language_names.end())
    {
        return std::nullopt;
    }
    return static_cast<Language>(std::distance(language_names.begin(), found));
}

std::span<const std::string_view> all_language_names() noexcept
{
    return language_names;
}
} // namespace nenenib::core
