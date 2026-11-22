#include "LspRootMarkers.hpp"

#include "AsciiCase.hpp"
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
// 印の列の 1 つずつを別の優先の組にする（nvim-lspconfig の平らな `root_markers` と同じ意味）。
template <std::size_t Count>
[[nodiscard]] consteval std::array<LspRootTier, Count>
each_alone(const std::array<std::string_view, Count> &markers)
{
    std::array<LspRootTier, Count> tiers{};
    for (std::size_t index = 0; index < Count; ++index)
    {
        tiers.at(index) = LspRootTier{markers}.subspan(index, 1);
    }
    return tiers;
}

// 言語ごとの印（出どころは nvim-lspconfig の `lsp/<server>.lua`・2026-11-22 に読んだ master・ADR
// 0161 の 2026-11-22 の追補）。列は優先の順。
constexpr std::array<std::string_view, 1> git_marker{".git"};
constexpr std::array<LspRootTier, 1> git_only{LspRootTier{git_marker}};
// clangd（現物の順）。`.clang-tidy` `.clang-format` `configure.ac` は入れない（入れ子のフォルダに
// 置かれやすく、席は 4 つなので root が割れると譲り合いになる）。
constexpr std::array<std::string_view, 4> clangd_markers{".clangd", "compile_commands.json",
                                                         "compile_flags.txt", ".git"};
constexpr auto clangd_tiers = each_alone(clangd_markers);
// rust_analyzer の root_dir（`Cargo.toml` → `rust-project.json` → `.git`）。
constexpr std::array<std::string_view, 3> rust_markers{"Cargo.toml", "rust-project.json", ".git"};
constexpr auto rust_tiers = each_alone(rust_markers);
// gopls の root_dir（`go.work` → `go.mod` → `.git`）。
constexpr std::array<std::string_view, 3> go_markers{"go.work", "go.mod", ".git"};
constexpr auto go_tiers = each_alone(go_markers);
// 以前の lspconfig の ts_ls の形（今の lockfile の組は使わない）。
constexpr std::array<std::string_view, 4> script_markers{"tsconfig.json", "jsconfig.json",
                                                         "package.json", ".git"};
constexpr auto script_tiers = each_alone(script_markers);
constexpr std::array<std::string_view, 7> python_markers{
    "pyrightconfig.json", "pyproject.toml", "setup.py", "setup.cfg",
    "requirements.txt",   "Pipfile",        ".git"};
constexpr auto python_tiers = each_alone(python_markers);
// lua_ls の 1 つ目の組（同じ優先）と `.git`。
constexpr std::array<std::string_view, 3> lua_markers{".emmyrc.json", ".luarc.json",
                                                      ".luarc.jsonc"};
constexpr std::array<LspRootTier, 2> lua_tiers{LspRootTier{lua_markers}, LspRootTier{git_marker}};
// intelephense（現物は `.git` が先）。
constexpr std::array<std::string_view, 2> php_markers{".git", "composer.json"};
constexpr auto php_tiers = each_alone(php_markers);

[[nodiscard]] constexpr bool separator(char value) noexcept
{
    return value == '\\' || value == '/';
}

// from から次の区切りの手前まで（区切りが無ければ終わりまで）の終わりの位置。
[[nodiscard]] constexpr std::size_t component_end(std::string_view text, std::size_t from) noexcept
{
    while (from < text.size() && !separator(text[from]))
    {
        ++from;
    }
    return from;
}

// at にドライブの `X:` があるか。
[[nodiscard]] constexpr bool drive_at(std::string_view text, std::size_t at) noexcept
{
    return text.size() >= at + 2 && text[at + 1] == ':' && ascii_folded(text[at]) >= 'a' &&
           ascii_folded(text[at]) <= 'z';
}

// ドライブの `X:` の根の長さ（後ろに区切りがあれば区切りまで）。
[[nodiscard]] constexpr std::size_t drive_floor(std::string_view text, std::size_t at) noexcept
{
    return text.size() > at + 2 && separator(text[at + 2]) ? at + 3 : at + 2;
}

// server から始まる `server\share` の終わり（share が無ければ全体）。
[[nodiscard]] constexpr std::size_t share_end(std::string_view text, std::size_t server) noexcept
{
    const std::size_t server_end = component_end(text, server);
    return server_end < text.size() ? component_end(text, server_end + 1) : text.size();
}

// `\\?\` `\\.\` の経路の根の長さ（後ろの 4 文字目から読む）。
[[nodiscard]] constexpr std::size_t device_floor(std::string_view text) noexcept
{
    if (drive_at(text, 4))
    {
        return drive_floor(text, 4);
    }
    const std::size_t first = component_end(text, 4);
    if (first < text.size() && ascii_equal_ignoring_case(text.substr(4, first - 4), "UNC"))
    {
        return share_end(text, first + 1);
    }
    return first;
}

// 上れる下限の長さ（#1944 のレビュー R1）。`X:\` は 3、`\\server\share` と
// `\\?\UNC\server\share` は共有の根まで、`\\?\X:\` は 7、`\\?\Volume{…}` は最初の名前まで、
// `\` から始まる経路は 1。これより短いフォルダへは行かない。
[[nodiscard]] constexpr std::size_t climb_floor(std::string_view text) noexcept
{
    if (text.size() < 2 || !separator(text[0]) || !separator(text[1]))
    {
        if (drive_at(text, 0))
        {
            return drive_floor(text, 0);
        }
        return !text.empty() && separator(text[0]) ? 1 : 0;
    }
    const bool device =
        text.size() >= 4 && (text[2] == '?' || text[2] == '.') && separator(text[3]);
    return device ? device_floor(text) : share_end(text, 2);
}

} // namespace

// 言語を足すとこの switch が落ちる（CPP-002）。足した言語の印を nvim-lspconfig で決める。
std::span<const LspRootTier> lsp_root_markers(Language language) noexcept
{
    switch (language)
    {
    case Language::c:
    case Language::cpp:
        return clangd_tiers;
    case Language::rust:
        return rust_tiers;
    case Language::go:
        return go_tiers;
    case Language::javascript:
    case Language::typescript:
        return script_tiers;
    case Language::python:
        return python_tiers;
    case Language::lua:
        return lua_tiers;
    case Language::php:
        return php_tiers;
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
    const std::size_t floor = climb_floor(folder.text());
    while (folders.size() < lsp_root_search_depth)
    {
        const std::string_view here = folders.back().text();
        if (here.size() <= floor)
        {
            break;
        }
        auto parent = folder_of(folders.back());
        if (!parent.has_value())
        {
            break;
        }
        // `\\?\C:\a` の親は folder_of では `\\?\C:` になるので、根の `\\?\C:\`
        // に揃える。区切りまでの 前の部分なので検証済みの文字だけ（不変条件）。
        auto next = parent.value().text().size() < floor
                        ? FilePath::parse(here.substr(0, floor)).value()
                        : std::move(parent).value();
        if (next.text() == here)
        {
            break;
        }
        folders.push_back(std::move(next));
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
