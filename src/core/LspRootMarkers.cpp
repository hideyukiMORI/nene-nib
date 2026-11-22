#include "LspRootMarkers.hpp"

#include "AsciiCase.hpp"
#include "FileFolder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
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
// jdtls の 2 つの組（nvim 0.11.3 からの入れ子の形・1 つ目の組が `.git` を持つ）。
constexpr std::array<std::string_view, 5> java_first{"mvnw", "gradlew", "settings.gradle",
                                                     "settings.gradle.kts", ".git"};
constexpr std::array<std::string_view, 4> java_second{"build.xml", "pom.xml", "build.gradle",
                                                      "build.gradle.kts"};
constexpr std::array<LspRootTier, 2> java_tiers{LspRootTier{java_first}, LspRootTier{java_second}};
// kotlin_language_server（現物は `.git` を持たないので最後に足す。ほかの `.git`
// を足した行も同じ）。
constexpr std::array<std::string_view, 7> kotlin_markers{
    "settings.gradle", "settings.gradle.kts", "build.xml", "pom.xml",
    "build.gradle",    "build.gradle.kts",    ".git"};
constexpr auto kotlin_tiers = each_alone(kotlin_markers);
// dartls。
constexpr std::array<std::string_view, 2> dart_markers{"pubspec.yaml", ".git"};
constexpr auto dart_tiers = each_alone(dart_markers);
// zls。
constexpr std::array<std::string_view, 3> zig_markers{"zls.json", "build.zig", ".git"};
constexpr auto zig_tiers = each_alone(zig_markers);
// terraformls。
constexpr std::array<std::string_view, 2> terraform_markers{".terraform", ".git"};
constexpr auto terraform_tiers = each_alone(terraform_markers);
// ruby_lsp。
constexpr std::array<std::string_view, 2> ruby_markers{"Gemfile", ".git"};
constexpr auto ruby_tiers = each_alone(ruby_markers);
// sourcekit の root_dir の組の順（glob の `*.xcodeproj` `*.xcworkspace` の組は入れない）。
constexpr std::array<std::string_view, 2> swift_build{"buildServer.json", ".bsp"};
constexpr std::array<std::string_view, 2> swift_package{"compile_commands.json", "Package.swift"};
constexpr std::array<LspRootTier, 3> swift_tiers{
    LspRootTier{swift_build}, LspRootTier{swift_package}, LspRootTier{git_marker}};
// metals（`build.gradle` と `build.gradle.kts` は 1 つの組）。
constexpr std::array<std::string_view, 1> scala_sbt{"build.sbt"};
constexpr std::array<std::string_view, 1> scala_mill{"build.sc"};
constexpr std::array<std::string_view, 2> scala_gradle{"build.gradle", "build.gradle.kts"};
constexpr std::array<std::string_view, 1> scala_maven{"pom.xml"};
constexpr std::array<LspRootTier, 5> scala_tiers{LspRootTier{scala_sbt}, LspRootTier{scala_mill},
                                                 LspRootTier{scala_gradle},
                                                 LspRootTier{scala_maven}, LspRootTier{git_marker}};
// hls の root_pattern の 1 つの組（glob の `*.cabal` は入れない）。
constexpr std::array<std::string_view, 4> haskell_markers{"hie.yaml", "stack.yaml", "cabal.project",
                                                          "package.yaml"};
constexpr std::array<LspRootTier, 2> haskell_tiers{LspRootTier{haskell_markers},
                                                   LspRootTier{git_marker}};
// elixirls の `mix.exs`（umbrella の上の `mix.exs` へ上る処理は持たない）。
constexpr std::array<std::string_view, 2> elixir_markers{"mix.exs", ".git"};
constexpr auto elixir_tiers = each_alone(elixir_markers);
// vue_ls。
constexpr std::array<std::string_view, 2> vue_markers{"package.json", ".git"};
constexpr auto vue_tiers = each_alone(vue_markers);
// svelte の lockfile の組 → `.git`。
constexpr std::array<std::string_view, 6> svelte_locks{
    "package-lock.json", "yarn.lock", "pnpm-lock.yaml", "bun.lockb", "bun.lock", "deno.lock"};
constexpr std::array<LspRootTier, 2> svelte_tiers{LspRootTier{svelte_locks},
                                                  LspRootTier{git_marker}};

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

namespace
{
using LanguageRootRow = std::pair<Language, std::span<const LspRootTier>>;

// 言語ごとの印の組の列（CPP-012 の表・#1948 で 16 言語を足して switch から表にした）。i 行目が i
// 番目の Language で、下の static_assert が行の言語を値の順と数で縛る（値を足すと行が足りず、既定の
// 行の言語が順に合わずに落ちる）。足した言語の印を nvim-lspconfig で決める。make は autotools_ls の
// `Makefile` などが入れ子のフォルダに置かれやすく（clangd の `configure.ac` と同じ理由）`.git`
// だけ。 batch は対応する server が無く、r（r_language_server）と perl（perlnavigator）は現物も
// `.git`。
constexpr std::array<LanguageRootRow, language_count()> language_roots{{
    {Language::plain, git_only},
    {Language::markdown, git_only},
    {Language::html, git_only},
    {Language::c, clangd_tiers},
    {Language::cpp, clangd_tiers},
    {Language::python, python_tiers},
    {Language::json, git_only},
    {Language::javascript, script_tiers},
    {Language::typescript, script_tiers},
    {Language::yaml, git_only},
    {Language::toml, git_only},
    {Language::xml, git_only},
    {Language::css, git_only},
    {Language::powershell, git_only},
    {Language::shell, git_only},
    {Language::cmake, git_only},
    {Language::gdscript, git_only},
    {Language::dosini, git_only},
    {Language::gdresource, git_only},
    {Language::cs, git_only},
    {Language::rust, rust_tiers},
    {Language::go, go_tiers},
    {Language::php, php_tiers},
    {Language::sql, git_only},
    {Language::lua, lua_tiers},
    {Language::dockerfile, git_only},
    {Language::java, java_tiers},
    {Language::kotlin, kotlin_tiers},
    {Language::dart, dart_tiers},
    {Language::zig, zig_tiers},
    {Language::terraform, terraform_tiers},
    {Language::make, git_only},
    {Language::batch, git_only},
    {Language::ruby, ruby_tiers},
    {Language::swift, swift_tiers},
    {Language::scala, scala_tiers},
    {Language::haskell, haskell_tiers},
    {Language::elixir, elixir_tiers},
    {Language::r, git_only},
    {Language::perl, git_only},
    {Language::vue, vue_tiers},
    {Language::svelte, svelte_tiers},
}};

[[nodiscard]] consteval bool roots_in_order() noexcept
{
    for (std::size_t index = 0; index < language_roots.size(); ++index)
    {
        if (std::to_underlying(language_roots.at(index).first) != index)
        {
            return false;
        }
    }
    return true;
}

static_assert(roots_in_order(), "language_roots has one row for every Language in order");
} // namespace

std::span<const LspRootTier> lsp_root_markers(Language language) noexcept
{
    return language_roots.at(std::to_underlying(language)).second;
}

namespace
{
// 上れる下限の長さに、folder を含む ceiling の長さ（folder_spelling: 末尾の区切り 1 つを除く・根は
// 根のまま）を重ねる（#1960・#1963 のレビュー M1）。ceiling は folder の前の部分で区切りの境目に
// ある（folder_contains と同じ畳み方）ので、上る列は ceiling のフォルダで止まる。根の下限より短く
// ならない。
[[nodiscard]] std::size_t ceiling_floor(const FilePath &folder,
                                        const std::optional<FilePath> &ceiling) noexcept
{
    const std::size_t floor = climb_floor(folder.text());
    if (!ceiling.has_value() || !folder_contains(ceiling.value(), folder))
    {
        return floor;
    }
    return std::max(floor, folder_spelling(ceiling.value()).size());
}
} // namespace

std::vector<FilePath> lsp_root_folders(const FilePath &folder,
                                       const std::optional<FilePath> &ceiling)
{
    std::vector<FilePath> folders{folder};
    const std::size_t floor = ceiling_floor(folder, ceiling);
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
