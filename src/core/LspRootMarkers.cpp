#include "LspRootMarkers.hpp"

#include "FileFolder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 言語ごとの印（出どころは nvim-lspconfig の `lsp/<server>.lua` の `root_markers`・ADR 0161 の
// 2026-11-22 の追補）。どの列も最後は `.git`。
constexpr std::array<std::string_view, 1> git_only{".git"};
// clangd。`.clang-tidy` `.clang-format` `configure.ac` は入れない（入れ子のフォルダに置かれやすく、
// 席は 4 つなので root が割れると譲り合いになる）。
constexpr std::array<std::string_view, 4> clangd_markers{"compile_commands.json",
                                                         "compile_flags.txt", ".clangd", ".git"};
constexpr std::array<std::string_view, 3> rust_markers{"Cargo.toml", "rust-project.json", ".git"};
constexpr std::array<std::string_view, 3> go_markers{"go.work", "go.mod", ".git"};
constexpr std::array<std::string_view, 4> script_markers{"tsconfig.json", "jsconfig.json",
                                                         "package.json", ".git"};
constexpr std::array<std::string_view, 7> python_markers{
    "pyrightconfig.json", "pyproject.toml", "setup.py", "setup.cfg",
    "requirements.txt",   "Pipfile",        ".git"};
constexpr std::array<std::string_view, 4> lua_markers{".emmyrc.json", ".luarc.json", ".luarc.jsonc",
                                                      ".git"};
constexpr std::array<std::string_view, 2> php_markers{"composer.json", ".git"};

[[nodiscard]] bool separator(char value) noexcept
{
    return value == '\\' || value == '/';
}

// folder の下の name（区切りは folder の末尾に無ければ `\` を 1 つ）。
[[nodiscard]] FilePath child_of(const FilePath &folder, std::string_view name)
{
    std::string text(folder.text());
    if (!text.empty() && !separator(text.back()))
    {
        text.push_back('\\');
    }
    text.append(name);
    // folder も name も検証済みの文字だけなので、つないだ文字列も読める（不変条件）。
    return FilePath::parse(text).value();
}

// folder の下に印のどれかがあるか（表の順に見て、最初にあった所で止める）。
[[nodiscard]] bool marked(const FilePath &folder, std::span<const std::string_view> markers,
                          const std::function<bool(const FilePath &)> &present)
{
    return std::ranges::any_of(markers, [&](std::string_view marker)
                               { return present(child_of(folder, marker)); });
}
} // namespace

// 言語を足すとこの switch が落ちる（CPP-002）。足した言語の印を nvim-lspconfig で決める。
std::span<const std::string_view> lsp_root_markers(Language language) noexcept
{
    switch (language)
    {
    case Language::c:
    case Language::cpp:
        return clangd_markers;
    case Language::rust:
        return rust_markers;
    case Language::go:
        return go_markers;
    case Language::javascript:
    case Language::typescript:
        return script_markers;
    case Language::python:
        return python_markers;
    case Language::lua:
        return lua_markers;
    case Language::php:
        return php_markers;
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::json:
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
    case Language::sql:
    case Language::dockerfile:
        return git_only;
    }
    return git_only;
}

std::optional<FilePath> lsp_root_found(const FilePath &folder, Language language,
                                       const std::function<bool(const FilePath &)> &present)
{
    const auto markers = lsp_root_markers(language);
    std::optional<FilePath> current = folder;
    for (std::size_t depth = 0; depth < lsp_root_search_depth && current.has_value(); ++depth)
    {
        const FilePath &here = current.value();
        if (marked(here, markers, present))
        {
            return here;
        }
        auto parent = folder_of(here);
        if (!parent.has_value() || parent.value().text() == here.text())
        {
            return std::nullopt;
        }
        current = std::move(parent);
    }
    return std::nullopt;
}
} // namespace nenenib::core
