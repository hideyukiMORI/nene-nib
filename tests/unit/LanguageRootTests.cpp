// 契約 --language-root（ADR 0161 の 2026-11-22 の追補・LSP 段 S3・#1940）。言語サーバーの root を
// 文書のフォルダから上へ印で探す。core の印の表と上り方、設定の root の内外、答えを待つ間は子を
// 起こさない・受けない、同じ（フォルダ・言語）は 2 回探さない、別の木の文書は別の root の席、
// オフと命令の無い言語では port を呼ばない、を替え玉 ScriptedLanguageRoots と
// ScriptedLanguageServer で確かめる。本物のファイルもサーバーも窓も使わない。
#include "Editing.hpp"
#include "EditorSettings.hpp"
#include "FileFolder.hpp"
#include "FilePath.hpp"
#include "InsertText.hpp"
#include "Language.hpp"
#include "LanguageRootRequest.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageTable.hpp"
#include "LspRootMarkers.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "ServerFailure.hpp"
#include "ServerStarted.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;

constexpr std::string_view main_rs = "C:\\repo\\src\\main.rs";
constexpr std::string_view lib_rs = "C:\\repo\\src\\lib.rs";
constexpr std::string_view other_rs = "C:\\other\\deep\\x.rs";
constexpr std::string_view loose_rs = "C:\\loose\\y.rs";
constexpr std::string_view note_md = "C:\\repo\\README.md";
constexpr std::string_view cpp_file = "C:\\work\\a\\b.cpp";
constexpr std::string_view tests_rs = "C:\\repo\\tests\\t.rs";
constexpr std::string_view upper_lib_rs = "c:\\REPO\\src\\lib.rs";
constexpr std::string_view upper_tests_rs = "C:\\Repo\\tests\\u.rs";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

// lsp を on / off にし、rust と cpp に命令を書いた設定（root は root の値）。
[[nodiscard]] SettingsReading root_settings(bool on, std::optional<std::string_view> root = {})
{
    auto settings = core::default_editor_settings();
    settings.lsp = on;
    settings.lsp_commands = {{"rust", R"(C:\Rust\rust-analyzer.exe)"},
                             {"cpp", R"(C:\LLVM\bin\clangd.exe)"}};
    if (root.has_value())
    {
        settings.root = path_of(root.value());
    }
    return SettingsReading{std::optional<core::EditorSettings>{std::move(settings)}};
}

void hold_all(Editing &editing)
{
    for (const std::string_view path : {main_rs, lib_rs, other_rs, loose_rs, note_md, cpp_file})
    {
        editing.files().hold_at(std::string(path), Bytes{std::string("x\n")});
    }
}

[[nodiscard]] std::string last_root(Editing &editing)
{
    const auto &launches = editing.language_server().launches();
    return launches.empty() ? std::string{} : std::string(launches.back().root.text());
}

// 言語の印の組を優先の順に、組ごとに印を `|` でつないで `,` で並べた文字列。
[[nodiscard]] std::string tiers_of(core::Language language)
{
    std::string text;
    for (const core::LspRootTier tier : core::lsp_root_markers(language))
    {
        text += text.empty() ? "" : ",";
        std::string joined;
        for (const std::string_view marker : tier)
        {
            joined += joined.empty() ? "" : "|";
            joined += marker;
        }
        text += joined;
    }
    return text;
}

// #1917 で足した 16 言語の行（#1948 の噛み合わせ）。現物が `.git` を持たない行は最後に足し、glob
// の印（sourcekit の `*.xcodeproj`・hls の `*.cabal`）は入れない。
void verify_added_marker_rows()
{
    expect(tiers_of(core::Language::java) ==
                   "mvnw|gradlew|settings.gradle|settings.gradle.kts|.git,"
                   "build.xml|pom.xml|build.gradle|build.gradle.kts" &&
               tiers_of(core::Language::kotlin) ==
                   "settings.gradle,settings.gradle.kts,build.xml,pom.xml,build.gradle,"
                   "build.gradle.kts,.git" &&
               tiers_of(core::Language::dart) == "pubspec.yaml,.git" &&
               tiers_of(core::Language::zig) == "zls.json,build.zig,.git" &&
               tiers_of(core::Language::terraform) == ".terraform,.git" &&
               tiers_of(core::Language::ruby) == "Gemfile,.git" &&
               tiers_of(core::Language::swift) ==
                   "buildServer.json|.bsp,compile_commands.json|Package.swift,.git" &&
               tiers_of(core::Language::scala) ==
                   "build.sbt,build.sc,build.gradle|build.gradle.kts,pom.xml,.git" &&
               tiers_of(core::Language::haskell) ==
                   "hie.yaml|stack.yaml|cabal.project|package.yaml,.git" &&
               tiers_of(core::Language::elixir) == "mix.exs,.git" &&
               tiers_of(core::Language::vue) == "package.json,.git" &&
               tiers_of(core::Language::svelte) ==
                   "package-lock.json|yarn.lock|pnpm-lock.yaml|bun.lockb|bun.lock|deno.lock,.git",
           "the added languages follow nvim-lspconfig in priority order");
    expect(tiers_of(core::Language::make) == ".git" && tiers_of(core::Language::batch) == ".git" &&
               tiers_of(core::Language::r) == ".git" && tiers_of(core::Language::perl) == ".git",
           "make, batch, r and perl fall back to .git");
}

// 印の表: 組の列は優先の順で空の組が無く、`.git` を持つ。中身と順は nvim-lspconfig の現物。
void verify_marker_table()
{
    bool every_git = true;
    for (const std::string_view name : core::all_language_names())
    {
        const auto tiers =
            core::lsp_root_markers(core::language_of_name(name).value_or(core::Language::plain));
        every_git =
            every_git && !tiers.empty() &&
            std::ranges::all_of(tiers, [](core::LspRootTier tier) { return !tier.empty(); }) &&
            std::ranges::any_of(tiers, [](core::LspRootTier tier)
                                { return std::ranges::find(tier, ".git") != tier.end(); });
    }
    expect(every_git, "every language has non-empty tiers and a .git marker");
    expect(tiers_of(core::Language::cpp) ==
                   ".clangd,compile_commands.json,compile_flags.txt,.git" &&
               tiers_of(core::Language::c) == tiers_of(core::Language::cpp) &&
               tiers_of(core::Language::rust) == "Cargo.toml,rust-project.json,.git" &&
               tiers_of(core::Language::go) == "go.work,go.mod,.git" &&
               tiers_of(core::Language::typescript) ==
                   "tsconfig.json,jsconfig.json,package.json,.git" &&
               tiers_of(core::Language::python) ==
                   "pyrightconfig.json,pyproject.toml,setup.py,setup.cfg,requirements.txt,"
                   "Pipfile,.git" &&
               tiers_of(core::Language::lua) == ".emmyrc.json|.luarc.json|.luarc.jsonc,.git" &&
               tiers_of(core::Language::php) == ".git,composer.json",
           "the tiers follow nvim-lspconfig in priority order");
    expect(tiers_of(core::Language::cs) == ".git", "C# has no glob marker and falls back to .git");
    verify_added_marker_rows();
}

// 優先の順: 1 つ目の組を根まで当ててから 2 つ目へ（近い下位の印より遠い上位の印が勝つ）。同じ
// 組の印は近い方。確かめは多くて（印の数 × 32 段）回。
void verify_priority()
{
    const auto present_in = [](std::vector<std::string_view> placed)
    {
        return [placed = std::move(placed)](const core::FilePath &path)
        { return std::ranges::find(placed, path.text()) != placed.end(); };
    };
    const auto php =
        core::lsp_root_found(path_of("C:\\repo\\pkg"), std::nullopt, core::Language::php,
                             present_in({"C:\\repo\\.git", "C:\\repo\\pkg\\composer.json"}));
    expect(php.has_value() && php.value().text() == "C:\\repo",
           "intelephense looks for .git before composer.json");
    const auto submodule = core::lsp_root_found(
        path_of("C:\\repo\\third\\sub"), std::nullopt, core::Language::cpp,
        present_in({"C:\\repo\\compile_commands.json", "C:\\repo\\third\\sub\\.git"}));
    expect(submodule.has_value() && submodule.value().text() == "C:\\repo",
           "compile_commands.json above wins over a nearer submodule .git");
    const auto work =
        core::lsp_root_found(path_of("C:\\ws\\mod\\cmd"), std::nullopt, core::Language::go,
                             present_in({"C:\\ws\\go.work", "C:\\ws\\mod\\go.mod"}));
    expect(work.has_value() && work.value().text() == "C:\\ws",
           "go.work above wins over a nearer go.mod");
    const auto lua =
        core::lsp_root_found(path_of("C:\\a\\b"), std::nullopt, core::Language::lua,
                             present_in({"C:\\a\\.emmyrc.json", "C:\\a\\b\\.luarc.json"}));
    expect(lua.has_value() && lua.value().text() == "C:\\a\\b",
           "markers of one tier are equal and the nearest folder wins");
    std::string deep = "C:";
    for (std::size_t level = 0; level < core::lsp_root_search_depth + 8; ++level)
    {
        deep += "\\d";
    }
    std::size_t looked = 0;
    static_cast<void>(core::lsp_root_found(path_of(deep), std::nullopt, core::Language::python,
                                           [&looked](const core::FilePath &)
                                           {
                                               ++looked;
                                               return false;
                                           }));
    expect(looked == 7 * core::lsp_root_search_depth,
           "the checks stay within markers times 32 folders");
}

// 上る列の文字（近い順に `;` でつないだもの）。
[[nodiscard]] std::string climbed(std::string_view folder)
{
    std::string text;
    for (const core::FilePath &here : core::lsp_root_folders(path_of(folder), std::nullopt))
    {
        text += text.empty() ? "" : ";";
        text += here.text();
    }
    return text;
}

// 上り方はドライブの根と共有の根で止まる（#1944 のレビュー R1）。`\\server` と `\` を見に行かない。
void verify_climb_floor()
{
    expect(climbed("C:\\a") == "C:\\a;C:\\", "a drive path stops at the drive root");
    expect(climbed("\\\\srv\\share\\a\\b") ==
               "\\\\srv\\share\\a\\b;\\\\srv\\share\\a;\\\\srv\\share",
           "a UNC path stops at the share root");
    expect(climbed("\\\\srv\\share") == "\\\\srv\\share", "the share root itself does not climb");
    expect(climbed("\\\\?\\C:\\a\\b") == "\\\\?\\C:\\a\\b;\\\\?\\C:\\a;\\\\?\\C:\\",
           "a \\\\?\\ drive path stops at its drive root");
    expect(climbed("\\\\?\\UNC\\srv\\share\\a") ==
               "\\\\?\\UNC\\srv\\share\\a;\\\\?\\UNC\\srv\\share",
           "a \\\\?\\UNC path stops at the share root");
    std::vector<std::string> asked;
    const auto found =
        core::lsp_root_found(path_of("\\\\srv\\share\\a"), std::nullopt, core::Language::rust,
                             [&asked](const core::FilePath &path)
                             {
                                 asked.emplace_back(path.text());
                                 return path.text() == "\\\\srv\\share\\.git";
                             });
    expect(
        found.has_value() && found.value().text() == "\\\\srv\\share" &&
            std::ranges::none_of(asked,
                                 [](const std::string &path)
                                 {
                                     return path.starts_with("\\\\srv\\") &&
                                            !path.starts_with("\\\\srv\\share\\");
                                 }) &&
            std::ranges::none_of(asked, [](const std::string &path)
                                 { return path.size() > 1 && path[0] == '\\' && path[1] != '\\'; }),
        "a UNC document never looks above the share or at the current drive's root");
}

// 根の形ごとの下限（LspRootMarkers.cpp の climb_floor・#2013）: `/` の区切り、`\` から始まる経路は
// `\`、`\\.\X:\` は 7、共有の無い `\\server` と区切りの無い `X:` と `\\?\UNC` は上らない。区切りの
// 無い名前は親が無くて止まる。
void verify_climb_floor_shapes()
{
    expect(climbed("C:/a/b") == "C:/a/b;C:/a;C:/", "a forward-slash drive path stops at C:/");
    expect(climbed("\\a\\b") == "\\a\\b;\\a;\\" && climbed("\\") == "\\",
           "a path rooted at \\ climbs to \\ and no further");
    expect(climbed("\\\\.\\C:\\a") == "\\\\.\\C:\\a;\\\\.\\C:\\",
           "a \\\\.\\ drive path stops at its drive root");
    expect(climbed("\\\\srv") == "\\\\srv" && climbed("C:") == "C:" &&
               climbed("\\\\?\\C:") == "\\\\?\\C:" && climbed("\\\\?\\UNC") == "\\\\?\\UNC" &&
               climbed("\\\\?x\\a") == "\\\\?x\\a",
           "a server alone, a bare drive and an unfinished device path do not climb");
    expect(climbed("a\\b") == "a\\b;a", "a relative name without a separator has no parent");
    expect(core::lsp_root_marker_path(path_of("C:/"), ".git").text() == "C:/.git" &&
               core::lsp_root_marker_path(path_of("C:/a"), ".git").text() == "C:/a\\.git",
           "a marker joins after an existing / and adds \\ otherwise");
}

// 上り方: 最初に印のあるフォルダ・根で止まる・32 段まで。
void verify_walk()
{
    const auto present_at = [](std::string_view wanted)
    { return [wanted](const core::FilePath &path) { return path.text() == wanted; }; };
    const auto found =
        core::lsp_root_found(path_of("C:\\repo\\src\\a"), std::nullopt, core::Language::rust,
                             present_at("C:\\repo\\Cargo.toml"));
    expect(found.has_value() && found.value().text() == "C:\\repo",
           "the first folder upward with a marker is the root");
    const auto drive = core::lsp_root_found(path_of("C:\\repo"), std::nullopt, core::Language::rust,
                                            present_at("C:\\.git"));
    expect(drive.has_value() && drive.value().text() == "C:\\",
           "a marker at the drive root joins without a doubled separator");
    std::size_t asked = 0;
    const auto none = core::lsp_root_found(path_of("C:\\a\\b"), std::nullopt, core::Language::cs,
                                           [&asked](const core::FilePath &)
                                           {
                                               ++asked;
                                               return false;
                                           });
    expect(!none.has_value() && asked == 3, "without markers the walk stops at the drive root");
    std::string deep = "C:";
    for (std::size_t level = 0; level < core::lsp_root_search_depth + 8; ++level)
    {
        deep += "\\d";
    }
    std::size_t looked = 0;
    static_cast<void>(core::lsp_root_found(path_of(deep), std::nullopt, core::Language::cs,
                                           [&looked](const core::FilePath &)
                                           {
                                               ++looked;
                                               return false;
                                           }));
    expect(looked == core::lsp_root_search_depth, "the walk climbs at most 32 folders");
}

void verify_folder_contains()
{
    expect(core::folder_contains(path_of("C:\\Repo"), path_of("c:\\repo\\src\\a.rs")) &&
               core::folder_contains(path_of("C:\\repo\\"), path_of("C:\\repo\\a.rs")) &&
               core::folder_contains(path_of("C:\\repo"), path_of("C:/repo/a.rs")) &&
               core::folder_contains(path_of("C:\\repo"), path_of("C:\\repo")),
           "a folder contains itself and what is below it, in any ASCII case and separator");
    expect(!core::folder_contains(path_of("C:\\repo"), path_of("C:\\repository\\a.rs")) &&
               !core::folder_contains(path_of("C:\\repo\\src"), path_of("C:\\repo\\a.rs")),
           "a sibling with a longer name or a parent is not inside");
    expect(core::folder_contains(path_of("C:\\repo\\"), path_of("C:\\repo")) &&
               core::folder_contains(path_of("C:\\"), path_of("C:\\repo")) &&
               core::same_folder(path_of("C:\\Repo\\"), path_of("c:/repo")) &&
               core::same_folder(path_of("C:\\"), path_of("c:/")) &&
               !core::same_folder(path_of("C:\\"), path_of("C:")) &&
               !core::same_folder(path_of("C:\\repo"), path_of("C:\\repo\\src")) &&
               core::folder_spelling(path_of("C:\\")) == "C:\\" &&
               core::folder_spelling(path_of("\\\\srv\\share\\")) == "\\\\srv\\share",
           "a trailing separator is folded away except on a root (#1963 review M1)");
}

// `repo\src\main.rs` の root は `Cargo.toml` のある `repo`。答えの前は子を起こさない。同じ木の
// 文書は同じ席が受け、探し直さない。別の木は別の root の席。
void verify_marker_root()
{
    Editing editing(root_settings(true));
    hold_all(editing);
    auto &roots = editing.language_roots();
    roots.place("C:\\repo\\Cargo.toml");
    roots.place("C:\\other\\Cargo.toml");
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    expect(server.launches().empty() && roots.requests().size() == 1 && roots.collects() == 0 &&
               roots.requests().front().folder.text() == "C:\\repo\\src" &&
               !editing.controller().request_hover(),
           "opening asks for the root once and starts nothing until the answer");
    applied(editing.controller(), app::InsertText{"a"});
    expect(roots.requests().size() == 1 && server.launches().empty(),
           "typing while waiting neither asks again nor starts the server");
    applied(editing.controller(), app::WorkCompleted{});
    expect(server.launches().size() == 1 && last_root(editing) == "C:\\repo",
           "the answer starts rust-analyzer in the folder of Cargo.toml");
    applied(editing.controller(), app::OpenDocument{path_of(lib_rs)});
    expect(roots.requests().size() == 1 && server.launches().size() == 1,
           "a document in the same folder uses the remembered root and the same server");
    applied(editing.controller(), app::OpenDocument{path_of(other_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 2 && server.launches().size() == 2 &&
               last_root(editing) == "C:\\other",
           "a document of another tree starts a server at that tree's root");
    applied(editing.controller(), app::OpenDocument{path_of(loose_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(server.launches().size() == 3 && last_root(editing) == "C:\\loose",
           "without a marker the document's folder is the root");
    const std::size_t asked = roots.requests().size();
    applied(editing.controller(), app::SwitchTab{0});
    applied(editing.controller(), app::InsertText{"b"});
    applied(editing.controller(), app::SwitchTab{2});
    expect(roots.requests().size() == asked && server.launches().size() == 3,
           "switching between known trees neither searches nor starts again");
}

// 設定の root の下の文書も印を探し、設定の root より上へは出ない（#1960）。印が無ければ設定の
// root。外の文書は上限なしで印で探す。
void verify_configured_root()
{
    Editing editing(root_settings(true, "C:\\work"));
    hold_all(editing);
    auto &roots = editing.language_roots();
    roots.place("C:\\repo\\Cargo.toml");
    applied(editing.controller(), app::OpenDocument{path_of(cpp_file)});
    expect(roots.requests().size() == 1 && editing.language_server().launches().empty() &&
               roots.requests().back().folder.text() == "C:\\work\\a" &&
               roots.requests().back().ceiling.value_or(path_of("C:\\none")).text() == "C:\\work",
           "a document under the configured root asks for markers up to the configured root");
    applied(editing.controller(), app::WorkCompleted{});
    expect(editing.language_server().launches().size() == 1 && last_root(editing) == "C:\\work",
           "without a marker inside, the configured root is the root");
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 2 && !roots.requests().back().ceiling.has_value() &&
               last_root(editing) == "C:\\repo",
           "a document outside the configured root searches for its markers without a ceiling");
}

constexpr std::string_view nested_rs = "C:\\work\\evidence\\hello\\src\\main.rs";
constexpr std::string_view flat_rs = "C:\\work\\src\\lib.rs";

// 開いたフォルダの中のプロジェクト（#1960 の再現）: 設定の root が `C:\work` で、文書が
// `C:\work\evidence\hello\Cargo.toml` の下なら root は `hello`。設定の root の直下に印がある普通の
// 形は設定の root のまま。印が設定の root より上にだけあれば設定の root（上へ出ない）。
void verify_project_inside_configured_root()
{
    Editing editing(root_settings(true, "C:\\work"));
    editing.files().hold_at(std::string(nested_rs), Bytes{std::string("x\n")});
    auto &roots = editing.language_roots();
    roots.place("C:\\work\\evidence\\hello\\Cargo.toml");
    roots.place("C:\\.git");
    applied(editing.controller(), app::OpenDocument{path_of(nested_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(last_root(editing) == "C:\\work\\evidence\\hello",
           "a project inside the configured root starts at the folder of its Cargo.toml");
    Editing flat(root_settings(true, "C:\\work"));
    flat.files().hold_at(std::string(flat_rs), Bytes{std::string("x\n")});
    flat.language_roots().place("C:\\work\\Cargo.toml");
    applied(flat.controller(), app::OpenDocument{path_of(flat_rs)});
    applied(flat.controller(), app::WorkCompleted{});
    expect(last_root(flat) == "C:\\work",
           "a marker right in the configured root keeps the configured root");
    Editing above(root_settings(true, "C:\\work"));
    above.files().hold_at(std::string(flat_rs), Bytes{std::string("x\n")});
    above.language_roots().place("C:\\Cargo.toml");
    above.language_roots().place("C:\\.git");
    applied(above.controller(), app::OpenDocument{path_of(flat_rs)});
    applied(above.controller(), app::WorkCompleted{});
    expect(last_root(above) == "C:\\work",
           "markers only above the configured root leave the configured root");
    Editing direct(root_settings(true, "C:\\work"));
    direct.files().hold_at("C:\\work\\top.rs", Bytes{std::string("x\n")});
    applied(direct.controller(), app::OpenDocument{path_of("C:\\work\\top.rs")});
    expect(direct.language_roots().touches() == 0 && last_root(direct) == "C:\\work",
           "a document right in the configured root starts there without searching");
}

// 上る列は上限のフォルダ自身で止まる（大小と末尾の区切りを畳む）。上限が文書を含まなければ無いのと
// 同じ。覚えの鍵は上限が違えば別の頼み（設定の root を変えたら古い答えを使わない・#1960）。
void verify_climb_ceiling()
{
    const auto climbed_under = [](std::string_view folder, std::string_view ceiling)
    {
        std::string text;
        for (const core::FilePath &here :
             core::lsp_root_folders(path_of(folder), std::optional{path_of(ceiling)}))
        {
            text += text.empty() ? "" : ";";
            text += here.text();
        }
        return text;
    };
    expect(climbed_under("C:\\work\\a\\b", "C:\\work") == "C:\\work\\a\\b;C:\\work\\a;C:\\work" &&
               climbed_under("C:\\work\\a", "c:\\WORK\\") == "C:\\work\\a;C:\\work" &&
               climbed_under("C:\\work", "C:\\work") == "C:\\work" &&
               climbed_under("C:\\work", "C:\\work\\") == "C:\\work" &&
               climbed_under("C:\\work", "c:/WORK/") == "C:\\work" &&
               climbed_under("C:\\work\\a", "C:\\") == "C:\\work\\a;C:\\work;C:\\",
           "the walk stops at the ceiling folder and still looks at it");
    expect(climbed_under("C:\\other\\a", "C:\\work") == "C:\\other\\a;C:\\other;C:\\",
           "a ceiling that does not contain the folder is ignored");
    const app::LanguageRootRequest open{path_of("C:\\work\\a"), core::Language::rust, std::nullopt};
    const app::LanguageRootRequest capped{path_of("C:\\work\\a"), core::Language::rust,
                                          path_of("C:\\work")};
    const app::LanguageRootRequest folded{path_of("c:\\WORK\\a"), core::Language::rust,
                                          path_of("c:\\Work")};
    const app::LanguageRootRequest lower{path_of("C:\\work\\a"), core::Language::rust,
                                         path_of("C:\\work\\a")};
    expect(!app::same_language_root_request(open, capped) &&
               app::same_language_root_request(capped, folded) &&
               !app::same_language_root_request(capped, lower),
           "the remembered key holds the ceiling, folded like the folder");
}

// 設定の root の書き方（末尾の区切り・`/`）が違っても、直下の文書は探さずに設定の root（#1963 の
// レビュー M1 / R1）。上に印があっても設定の root より上へ出ない。
void verify_configured_root_spelling()
{
    for (const std::string_view root : {"C:\\work\\", "C:/work"})
    {
        Editing editing(root_settings(true));
        editing.files().hold_at("C:\\work\\top.rs", Bytes{std::string("x\n")});
        editing.language_roots().place("C:\\.git");
        static_cast<void>(run_ex(editing.controller(), "set root=" + std::string(root)));
        applied(editing.controller(), app::OpenDocument{path_of("C:\\work\\top.rs")});
        applied(editing.controller(), app::WorkCompleted{});
        expect(editing.language_roots().requests().empty() &&
                   editing.language_server().launches().size() == 1 && last_root(editing) == root,
               "a document right in the configured root, spelled with a trailing or a forward "
               "separator, starts there without searching");
    }
}

// 答えを待つ間に設定の root を変えたら、新しい上限で頼み直し、古い上限の答えを使わない（#1963 の
// レビュー R3・覚えの鍵の上限の比べ）。
void verify_ceiling_changed_while_waiting()
{
    Editing editing(root_settings(true, "C:\\work"));
    editing.files().hold_at(std::string(nested_rs), Bytes{std::string("x\n")});
    auto &roots = editing.language_roots();
    roots.place("C:\\work\\Cargo.toml");
    roots.hold();
    applied(editing.controller(), app::OpenDocument{path_of(nested_rs)});
    static_cast<void>(run_ex(editing.controller(), "set root=C:\\work\\evidence"));
    expect(roots.requests().size() == 2 &&
               roots.requests().back().ceiling.value_or(path_of("C:\\none")).text() ==
                   "C:\\work\\evidence" &&
               editing.language_server().launches().empty(),
           "changing the configured root while waiting asks again with the new ceiling");
    roots.release();
    applied(editing.controller(), app::WorkCompleted{});
    expect(editing.language_server().launches().size() == 1 &&
               last_root(editing) == "C:\\work\\evidence",
           "the answer for the old ceiling is not used for the new one");
}

// 答えを待つ間に同じ（フォルダ・言語）を開き直しても 2 回頼まない。答えが来れば起きる。
void verify_waiting()
{
    Editing editing(root_settings(true));
    hold_all(editing);
    auto &roots = editing.language_roots();
    roots.hold();
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    applied(editing.controller(), app::OpenDocument{path_of(lib_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 1 && editing.language_server().launches().empty(),
           "while the answer is pending the same folder is not asked again and nothing starts");
    roots.release();
    applied(editing.controller(), app::WorkCompleted{});
    expect(editing.language_server().launches().size() == 1 &&
               last_root(editing) == "C:\\repo\\src",
           "the late answer starts the server in the document's folder");
}

// オフ・命令の無い言語では port を呼ばない。
void verify_quiet()
{
    Editing off(root_settings(false));
    hold_all(off);
    applied(off.controller(), app::OpenDocument{path_of(main_rs)});
    applied(off.controller(), app::InsertText{"a"});
    applied(off.controller(), app::OpenDocument{path_of(lib_rs)});
    applied(off.controller(), app::SwitchTab{0});
    applied(off.controller(), app::WorkCompleted{});
    expect(off.language_roots().touches() == 0 && off.language_server().touches() == 0,
           "with lsp off no path touches the root port");
    Editing plain(root_settings(true));
    hold_all(plain);
    applied(plain.controller(), app::OpenDocument{path_of(note_md)});
    applied(plain.controller(), app::InsertText{"a"});
    applied(plain.controller(), app::WorkCompleted{});
    expect(plain.language_roots().touches() == 0, "a language without a command is never searched");
}
// 席 first を ServerStarted → `initialize` の答え → `initialized` まで進める。
void run_first_slot(Editing &editing)
{
    auto &server = editing.language_server();
    server.serve(app::LanguageServerSlot::first, app::ServerStarted{});
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
    std::int64_t id = 0;
    for (const std::string &body : server.sent_to(app::LanguageServerSlot::first))
    {
        constexpr std::string_view key = R"("id":)";
        if (body.find(R"("method":"initialize")") != std::string::npos)
        {
            id = std::stoll(body.substr(body.find(key) + key.size()));
        }
    }
    server.answer(app::LanguageServerSlot::first, id,
                  R"({"capabilities":{"textDocumentSync":2,"hoverProvider":true}})");
    static_cast<void>(editing.controller().apply_frame(app::WorkCompleted{}));
}

// 席 first に送った didOpen のうち、part を含むものの数。
[[nodiscard]] std::size_t opened_on_first(Editing &editing, std::string_view part)
{
    const auto bodies = editing.language_server().sent_to(app::LanguageServerSlot::first);
    return static_cast<std::size_t>(std::ranges::count_if(
        bodies,
        [part](const std::string &body)
        {
            return body.find(R"("method":"textDocument/didOpen")") != std::string::npos &&
                   body.find(part) != std::string::npos;
        }));
}

// 諦めた言語でも、同じ言語の生きている別の root の席が、まだ探していないフォルダの文書を受ける
// （#1944 のレビュー R3）。新しい子は起こさない。
void verify_given_up_keeps_live_slot()
{
    Editing editing(root_settings(true));
    hold_all(editing);
    editing.files().hold_at(std::string(tests_rs), Bytes{std::string("x\n")});
    auto &roots = editing.language_roots();
    roots.place("C:\\repo\\Cargo.toml");
    roots.place("C:\\other\\Cargo.toml");
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    run_first_slot(editing);
    server.refuse(app::ServerFailure::cannot_start);
    applied(editing.controller(), app::OpenDocument{path_of(other_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(server.launches().size() == 2 && last_root(editing) == "C:\\other",
           "the refused start in another tree gives rust up");
    applied(editing.controller(), app::OpenDocument{path_of(tests_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 3 && server.launches().size() == 2 &&
               opened_on_first(editing, "tests/t.rs") == 1,
           "a given-up language still searches and the live server of that root takes the "
           "document");
}

// 席の鍵と覚えの鍵は Windows のパスとして ASCII の大小を区別しない（#1944 のレビュー L4）。
void verify_case_folded_roots()
{
    Editing editing(root_settings(true));
    hold_all(editing);
    editing.files().hold_at(std::string(upper_lib_rs), Bytes{std::string("x\n")});
    editing.files().hold_at(std::string(upper_tests_rs), Bytes{std::string("x\n")});
    auto &roots = editing.language_roots();
    roots.place("C:\\repo\\Cargo.toml");
    roots.place("C:\\Repo\\Cargo.toml");
    auto &server = editing.language_server();
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    applied(editing.controller(), app::OpenDocument{path_of(upper_lib_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 1 && server.launches().size() == 1,
           "a folder that differs only in ASCII case is not searched again");
    applied(editing.controller(), app::OpenDocument{path_of(upper_tests_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 2 && server.launches().size() == 1,
           "a root that differs only in ASCII case is served by the same server");
}
} // namespace

void verify_language_root_scope()
{
    verify_marker_table();
    verify_priority();
    verify_climb_floor();
    verify_climb_floor_shapes();
    verify_given_up_keeps_live_slot();
    verify_case_folded_roots();
    verify_walk();
    verify_folder_contains();
    verify_marker_root();
    verify_configured_root();
    verify_project_inside_configured_root();
    verify_climb_ceiling();
    verify_configured_root_spelling();
    verify_ceiling_changed_while_waiting();
    verify_waiting();
    verify_quiet();
}
} // namespace nenenib::tests
