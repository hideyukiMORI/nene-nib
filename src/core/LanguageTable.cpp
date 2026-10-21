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
constexpr std::array<std::string_view, 16> language_names{
    "text",       "markdown", "html", "c",   "cpp", "python", "json", "javascript",
    "typescript", "yaml",     "toml", "xml", "css", "ps1",    "sh",   "cmake"};

static_assert(language_names.size() == language_count(),
              "language_names has one row for every Language");

// 拡張子の表（決定 2）。拡張子は小文字で書き、照合は ASCII の大文字小文字を畳む。txt の行は
// 表に無い拡張子と同じ plain だが、初版の表の 1 行として書いておく。
using LanguageExtension = std::pair<std::string_view, Language>;

constexpr std::array<LanguageExtension, 35> language_extensions{{
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
    {"txt", Language::plain},
}};

// 拡張子ではなく名前で決まる行（決定 2）。照合は拡張子と同じく ASCII の大文字小文字を畳む。
constexpr std::string_view cmake_lists_name = "CMakeLists.txt";

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
    if (same_ignoring_case(name, cmake_lists_name))
    {
        return Language::cmake;
    }
    const auto dot = last_where(name, is_dot);
    if (!dot.has_value())
    {
        return Language::plain;
    }
    const auto extension = name.substr(dot.value() + 1);
    const auto found =
        std::ranges::find_if(language_extensions, [extension](const LanguageExtension &row)
                             { return same_ignoring_case(extension, row.first); });
    return found == language_extensions.end() ? Language::plain : found->second;
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
