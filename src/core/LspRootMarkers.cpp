#include "LspRootMarkers.hpp"

#include "FileFolder.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

std::vector<FilePath> lsp_root_folders(const FilePath &folder)
{
    std::vector<FilePath> folders{folder};
    while (folders.size() < lsp_root_search_depth)
    {
        auto parent = folder_of(folders.back());
        if (!parent.has_value() || parent.value().text() == folders.back().text())
        {
            break;
        }
        folders.push_back(std::move(parent).value());
    }
    return folders;
}

FilePath lsp_root_marker_path(const FilePath &folder, std::string_view marker)
{
    std::string text(folder.text());
    if (!text.empty() && !separator(text.back()))
    {
        text.push_back('\\');
    }
    text.append(marker);
    // folder も marker も検証済みの文字だけなので、つないだ文字列も読める（不変条件）。
    return FilePath::parse(text).value();
}
} // namespace nenenib::core
