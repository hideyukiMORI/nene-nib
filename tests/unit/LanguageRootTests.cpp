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
#include "LanguageTable.hpp"
#include "LspRootMarkers.hpp"
#include "OpenDocument.hpp"
#include "Scopes.hpp"
#include "SwitchTab.hpp"
#include "TestSupport.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

// 印の表: どの言語も最後は `.git`、言語ごとの印は nvim-lspconfig の名前。
void verify_marker_table()
{
    bool every_git = true;
    for (const std::string_view name : core::all_language_names())
    {
        const auto markers =
            core::lsp_root_markers(core::language_of_name(name).value_or(core::Language::plain));
        every_git = every_git && !markers.empty() && markers.back() == ".git";
    }
    expect(every_git, "every language's markers end with .git");
    const auto has = [](core::Language language, std::string_view marker)
    {
        return std::ranges::find(core::lsp_root_markers(language), marker) !=
               core::lsp_root_markers(language).end();
    };
    expect(has(core::Language::rust, "Cargo.toml") &&
               has(core::Language::cpp, "compile_commands.json") &&
               has(core::Language::c, "compile_flags.txt") &&
               has(core::Language::python, "pyproject.toml") && has(core::Language::go, "go.mod") &&
               has(core::Language::typescript, "tsconfig.json") &&
               has(core::Language::lua, ".luarc.json") && has(core::Language::php, "composer.json"),
           "the language markers come from nvim-lspconfig");
    expect(core::lsp_root_markers(core::Language::cs).size() == 1,
           "C# has no glob marker and falls back to .git");
}

// 上り方: 最初に印のあるフォルダ・根で止まる・32 段まで。
void verify_walk()
{
    const auto present_at = [](std::string_view wanted)
    { return [wanted](const core::FilePath &path) { return path.text() == wanted; }; };
    const auto found = core::lsp_root_found(path_of("C:\\repo\\src\\a"), core::Language::rust,
                                            present_at("C:\\repo\\Cargo.toml"));
    expect(found.has_value() && found.value().text() == "C:\\repo",
           "the first folder upward with a marker is the root");
    const auto drive =
        core::lsp_root_found(path_of("C:\\repo"), core::Language::rust, present_at("C:\\.git"));
    expect(drive.has_value() && drive.value().text() == "C:\\",
           "a marker at the drive root joins without a doubled separator");
    std::size_t asked = 0;
    const auto none = core::lsp_root_found(path_of("C:\\a\\b"), core::Language::cs,
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
    static_cast<void>(core::lsp_root_found(path_of(deep), core::Language::cs,
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

// 設定の root は文書がその下にあるときだけ使い、探さない。外の文書は印で探す。
void verify_configured_root()
{
    Editing editing(root_settings(true, "C:\\work"));
    hold_all(editing);
    auto &roots = editing.language_roots();
    roots.place("C:\\repo\\Cargo.toml");
    applied(editing.controller(), app::OpenDocument{path_of(cpp_file)});
    expect(roots.touches() == 0 && editing.language_server().launches().size() == 1 &&
               last_root(editing) == "C:\\work",
           "a document under the configured root starts there without searching");
    applied(editing.controller(), app::OpenDocument{path_of(main_rs)});
    applied(editing.controller(), app::WorkCompleted{});
    expect(roots.requests().size() == 1 && last_root(editing) == "C:\\repo",
           "a document outside the configured root searches for its markers");
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
} // namespace

void verify_language_root_scope()
{
    verify_marker_table();
    verify_walk();
    verify_folder_contains();
    verify_marker_root();
    verify_configured_root();
    verify_waiting();
    verify_quiet();
}
} // namespace nenenib::tests
