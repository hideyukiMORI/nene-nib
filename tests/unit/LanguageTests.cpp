// scope `--language` の単体テスト（ADR 0042 決定 2・ADR 0154・Issue #776）。文書の言語の表
// （拡張子・名前・逆引き）が core の 1 つであること、Ex の `:set filetype=` `:set ft=` と問いの
// `?`・閉じた名前の外の E474・補完の候補、言語が文書の値として開く・名前が変わる保存・新しい
// タブ・タブの切り替え・まだ読んでいない文書を読むときに付いていき、設定にもセッションにも
// 書かれないこと。
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "CloseTab.hpp"
#include "CommandLayout.hpp"
#include "DevicePixels.hpp"
#include "Editing.hpp"
#include "EndSession.hpp"
#include "ExFailure.hpp"
#include "ExLanguageRequest.hpp"
#include "ExResult.hpp"
#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageTable.hpp"
#include "NewTab.hpp"
#include "OpenDocument.hpp"
#include "RestoreAutosave.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SelectEditMode.hpp"
#include "SelectLanguage.hpp"
#include "SessionEnd.hpp"
#include "StatusBarLayout.hpp"
#include "StatusItems.hpp"
#include "SwitchTab.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextPosition.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using core::Language;

constexpr std::string_view markdown_path = "C:\\work\\README.md";
constexpr std::string_view python_path = "C:\\work\\tool.py";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] std::string message(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : "";
}

// 今の文書の言語を Ex の問いで読む（frame の外から観る口は `:set ft?` の 1 行）。
[[nodiscard]] std::string asked_language(app::EditorController &controller)
{
    return message(run_ex(controller, "set ft?"));
}

void hold_documents(ScriptedFiles &files)
{
    files.hold_at(std::string(markdown_path), Bytes{std::string("# title\n")});
    files.hold_at(std::string(python_path), Bytes{std::string("print(1)\n")});
}

[[nodiscard]] bool fails_with(std::string_view command, core::ExFailure expected)
{
    const auto result =
        core::evaluate_ex(command, core::default_editor_settings(), core::Appearance::dark);
    if (result.has_value())
    {
        return false;
    }
    const auto *const failure = std::get_if<core::ExFailure>(&result.error());
    return failure != nullptr && *failure == expected;
}

[[nodiscard]] std::optional<core::ExLanguageRequest> language_request(std::string_view command)
{
    const auto result =
        core::evaluate_ex(command, core::default_editor_settings(), core::Appearance::dark);
    if (!result.has_value())
    {
        return std::nullopt;
    }
    return result.value().language;
}

// 拡張子の表（決定 2）。大文字小文字は区別せず、区切りの後ろの名前だけを見る。
void verify_extension_table()
{
    using Row = std::pair<std::string_view, Language>;
    constexpr std::array<Row, 44> rows{{
        {"a.md", Language::markdown},
        {"a.markdown", Language::markdown},
        {"a.html", Language::html},
        {"a.htm", Language::html},
        {"a.c", Language::c},
        {"a.h", Language::c},
        {"a.cpp", Language::cpp},
        {"a.cc", Language::cpp},
        {"a.cxx", Language::cpp},
        {"a.hpp", Language::cpp},
        {"a.hh", Language::cpp},
        {"a.hxx", Language::cpp},
        {"a.ipp", Language::cpp},
        {"a.py", Language::python},
        {"a.pyi", Language::python},
        {"a.json", Language::json},
        {"a.js", Language::javascript},
        {"a.mjs", Language::javascript},
        {"a.cjs", Language::javascript},
        {"a.ts", Language::typescript},
        {"a.mts", Language::typescript},
        {"a.cts", Language::typescript},
        {"a.yml", Language::yaml},
        {"a.yaml", Language::yaml},
        {"a.toml", Language::toml},
        {"a.xml", Language::xml},
        {"a.css", Language::css},
        {"a.ps1", Language::powershell},
        {"a.psm1", Language::powershell},
        {"a.psd1", Language::powershell},
        {"a.sh", Language::shell},
        {"a.bash", Language::shell},
        {"a.zsh", Language::shell},
        {"a.cmake", Language::cmake},
        {"a.txt", Language::plain},
        {"C:\\src\\CMakeLists.txt", Language::cmake},
        {"README.MD", Language::markdown},
        {"C:\\x\\Main.CPP", Language::cpp},
        {"tool.Py", Language::python},
        {"Makefile", Language::plain},
        {"", Language::plain},
        {"C:\\dir.md\\notes", Language::plain},
        {"src/a.b/c.json", Language::json},
        {"archive.tar.gz", Language::plain},
    }};
    for (const auto &[path, language] : rows)
    {
        expect(core::language_of_path(path) == language,
               "language_of_path reads the extension after the last separator");
    }
    expect(core::language_of_path("cmakelists.TXT") == Language::cmake,
           "the CMakeLists.txt name ignores ASCII case like the extensions");
    expect(core::language_of_path("trailing.") == Language::plain &&
               core::language_of_path(".md") == Language::markdown,
           "an empty extension is plain and a dot file uses the text after the dot");
}

// #855: GDScript と Ini と Godot の設定の拡張子（Vim の filetype.vim に寄せる）。`.cfg` は Vim では
// 別の filetype（cfg）なので plain のまま。`.godot`（project.godot）は Vim に行が無く gdresource。
void verify_godot_extensions()
{
    using Row = std::pair<std::string_view, Language>;
    constexpr std::array<Row, 7> rows{{
        {"player.gd", Language::gdscript},
        {"php.ini", Language::dosini},
        {"SETTINGS.INI", Language::dosini},
        {"main.tscn", Language::gdresource},
        {"theme.tres", Language::gdresource},
        {"C:\\game\\project.godot", Language::gdresource},
        {"export_presets.cfg", Language::plain},
    }};
    for (const auto &[path, language] : rows)
    {
        expect(core::language_of_path(path) == language,
               "language_of_path maps the Godot and Ini extensions");
    }
}

// #857: C#・Rust・Go の拡張子（Vim の filetype.vim に寄せる・`.csx` `.cake` も cs）。
void verify_c_family_extensions()
{
    using Row = std::pair<std::string_view, Language>;
    constexpr std::array<Row, 6> rows{{
        {"Program.cs", Language::cs},
        {"script.CSX", Language::cs},
        {"build.cake", Language::cs},
        {"src\\main.rs", Language::rust},
        {"main.go", Language::go},
        {"go.mod", Language::plain},
    }};
    for (const auto &[path, language] : rows)
    {
        expect(core::language_of_path(path) == language,
               "language_of_path maps the C#, Rust and Go extensions");
    }
}

// #860: PHP・SQL・Lua の拡張子と Dockerfile の名前（Vim の filetype.vim に寄せる）。名前の行は
// CMakeLists.txt と同じく大小を問わず、`Dockerfile.*` `Containerfile.*` は拡張子の表に当たらない
// ときだけ dockerfile（`Dockerfile.json` は json・`Dockerfile.txt` は plain）。
void verify_batch4_extensions()
{
    using Row = std::pair<std::string_view, Language>;
    constexpr std::array<Row, 21> rows{{
        {"index.php", Language::php},
        {"legacy.php5", Language::php},
        {"view.PHTML", Language::php},
        {"test.phpt", Language::php},
        {"schema.sql", Language::sql},
        {"init.lua", Language::lua},
        {"pkg-1.0.rockspec", Language::lua},
        {".luacheckrc", Language::lua},
        {"Dockerfile", Language::dockerfile},
        {"src\\dockerfile", Language::dockerfile},
        {"Containerfile", Language::dockerfile},
        {"app.Dockerfile", Language::dockerfile},
        {"build.dockerfile", Language::dockerfile},
        {"Dockerfile.dev", Language::dockerfile},
        {"Containerfile.prod", Language::dockerfile},
        {"Dockerfile.json", Language::json},
        {"Dockerfile.txt", Language::plain},
        {"MyDockerfile", Language::plain},
        {"php", Language::plain},
        {"x.luau", Language::plain},
        {"Dockerfile.", Language::dockerfile},
    }};
    for (const auto &[path, language] : rows)
    {
        expect(core::language_of_path(path) == language,
               "language_of_path maps the PHP, SQL and Lua extensions and the Dockerfile names");
    }
}

// #864: 字句器のある言語の拡張子と名前の行（Vim の filetype.vim に寄せ、Vim の typescriptreact
// javascriptreact bash zsh は親の言語に畳む）。`*.cmake.in` は名前の末尾の照合で、拡張子の
// `in` は表に無いので plain。
void verify_extension_additions()
{
    using Row = std::pair<std::string_view, Language>;
    constexpr std::array<Row, 19> rows{{
        {"App.tsx", Language::typescript},
        {"VIEW.TSX", Language::typescript},
        {"button.jsx", Language::javascript},
        {"session.pssc", Language::powershell},
        {"icon.png.import", Language::gdresource},
        {".bashrc", Language::shell},
        {"C:\\Users\\a\\.bash_profile", Language::shell},
        {".bash_logout", Language::shell},
        {".profile", Language::shell},
        {"/home/a/.zshrc", Language::shell},
        {".ZPROFILE", Language::shell},
        {"config.cmake.in", Language::cmake},
        {"src\\FooConfig.CMAKE.IN", Language::cmake},
        {".cmake.in", Language::cmake},
        {"config.in", Language::plain},
        {"config.cmake.in.bak", Language::plain},
        {".bashrc.bak", Language::plain},
        {"x.bashrc", Language::plain},
        {"cmake.in", Language::plain},
    }};
    for (const auto &[path, language] : rows)
    {
        expect(core::language_of_path(path) == language,
               "language_of_path maps the #864 extensions, the shell names and *.cmake.in");
    }
}

// 名前の表と逆引き（決定 3）。26 本の往復と、小文字だけを受けること。
void verify_names()
{
    constexpr std::array<std::pair<Language, std::string_view>, 26> names{{
        {Language::plain, "text"},
        {Language::markdown, "markdown"},
        {Language::html, "html"},
        {Language::c, "c"},
        {Language::cpp, "cpp"},
        {Language::python, "python"},
        {Language::json, "json"},
        {Language::javascript, "javascript"},
        {Language::typescript, "typescript"},
        {Language::yaml, "yaml"},
        {Language::toml, "toml"},
        {Language::xml, "xml"},
        {Language::css, "css"},
        {Language::powershell, "ps1"},
        {Language::shell, "sh"},
        {Language::cmake, "cmake"},
        {Language::gdscript, "gdscript"},
        {Language::dosini, "dosini"},
        {Language::gdresource, "gdresource"},
        {Language::cs, "cs"},
        {Language::rust, "rust"},
        {Language::go, "go"},
        {Language::php, "php"},
        {Language::sql, "sql"},
        {Language::lua, "lua"},
        {Language::dockerfile, "dockerfile"},
    }};
    for (const auto &[language, name] : names)
    {
        const auto back = core::language_of_name(name);
        expect(core::language_name(language) == name && back.has_value() &&
                   back.value() == language,
               "language_name and language_of_name round-trip every language");
    }
    expect(core::all_language_names().size() == names.size(),
           "the name table has one row for every language");
    expect(!core::language_of_name("Markdown").has_value() &&
               !core::language_of_name("plain").has_value() &&
               !core::language_of_name("").has_value() &&
               !core::language_of_name("zig").has_value(),
           "language_of_name accepts only the lower-case names of the closed list");
}

// Ex の評価（決定 4）。言語は設定に載らず、名前の外は E474。
void verify_ex_evaluation()
{
    for (const std::string_view command : {"set filetype=markdown", "set ft=markdown"})
    {
        const auto result =
            core::evaluate_ex(command, core::default_editor_settings(), core::Appearance::dark);
        expect(result.has_value() && !result.value().settings.has_value(),
               ":set filetype= does not touch the settings");
        const auto request = language_request(command);
        expect(request.has_value() && request.value_or(core::ExLanguageRequest{}).language ==
                                          std::optional{Language::markdown},
               ":set filetype= and :set ft= carry the language");
    }
    for (const std::string_view command : {"set filetype?", "set ft?"})
    {
        const auto request = language_request(command);
        expect(request.has_value() &&
                   !request.value_or(core::ExLanguageRequest{Language::c}).language.has_value(),
               ":set filetype? asks for the current language");
    }
    for (const std::string_view command : {"set filetype=zig", "set ft=Markdown", "set ft="})
    {
        expect(fails_with(command, core::ExFailure::invalid_argument),
               "a name outside the closed list is E474");
    }
    expect(core::ex_failure_message(core::ExFailure::invalid_argument).text() ==
               "E474: Invalid argument",
           "the failure reads like Vim");
    expect(fails_with("set filetypes=c", core::ExFailure::unknown_option),
           "a longer option name is not the language option");
}

// 補完（決定 4）。`=` まで打ったときだけ名前を並べ、全件の一覧には命令の頭だけが出る。
void verify_completion()
{
    const auto all = core::ex_command_candidates();
    expect(std::ranges::find(all, std::string("set filetype=")) != all.end() &&
               std::ranges::find(all, std::string("set ft=")) != all.end() &&
               std::ranges::find(all, std::string("set ft=markdown")) == all.end(),
           "the candidate table lists the two command heads only");
    expect(core::command_completions("set filetype=py") ==
               std::vector<std::string>{"set filetype=python"},
           "set filetype= completes language names");
    expect(core::command_completions("set ft=c") ==
               std::vector<std::string>{"set ft=c", "set ft=cpp", "set ft=css", "set ft=cmake",
                                        "set ft=cs"},
           "set ft= completes in the order of the name table");
    const auto every = core::command_completions("set ft=");
    expect(every.size() == 1 + core::all_language_names().size() && every.front() == "set ft=",
           "set ft= lists the head and every language name");
}

// 文書の値（決定 1・5）。開く・`:set ft=`・新しいタブ・切り替え・名前が変わる保存。
void verify_document_value()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{vim_visible_lines});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    expect(asked_language(controller) == "filetype=text", "the untitled document is plain");
    applied(controller, app::OpenDocument{path_of(markdown_path)});
    expect(asked_language(controller) == "filetype=markdown", "opening reads the extension");
    expect(message(run_ex(controller, "set ft=python")) == "filetype=python",
           ":set ft= answers the chosen language");
    expect(asked_language(controller) == "filetype=python", ":set ft= changes the document value");
    expect(message(run_ex(controller, "set ft=zig")) == "E474: Invalid argument" &&
               asked_language(controller) == "filetype=python",
           "E474 leaves the language as it was");
    applied(controller, app::NewTab{});
    expect(asked_language(controller) == "filetype=text", "a new tab is plain");
    applied(controller, app::SwitchTab{0});
    expect(asked_language(controller) == "filetype=python",
           "the override follows its tab through a switch");
    applied(controller, app::SelectLanguage{Language::json});
    expect(asked_language(controller) == "filetype=json", "SelectLanguage sets the value");
    expect(editing.settings().writes() == 0, "the language is never written to the settings");
    editing.files().resolve_to(path_of("C:\\work\\copy.toml"));
    static_cast<void>(run_ex(controller, "w copy.toml"));
    expect(asked_language(controller) == "filetype=json",
           "a copy that keeps the identity keeps the language");
    editing.files().resolve_to(path_of("C:\\work\\renamed.yaml"));
    static_cast<void>(run_ex(controller, "saveas renamed.yaml"));
    expect(asked_language(controller) == "filetype=yaml",
           "a save that renames reads the new extension and drops the override");
    static_cast<void>(run_ex(controller, "set ft=c"));
    static_cast<void>(run_ex(controller, "w"));
    expect(asked_language(controller) == "filetype=c", "saving under the same name keeps it");
}

// ステータスバーの右端の 4 項目目（決定 6）。並びは左から位置・文字コード・改行・言語で、言語の
// 名前は language_name の表。
void verify_status_item()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    expect(core::status_item_count == 4 && controller.frame().status_items.at(3).text() == "text",
           "the untitled document shows text at the right end");
    applied(controller, app::OpenDocument{path_of(markdown_path)});
    const auto opened = controller.frame();
    expect(opened.status_items.at(3).text() == "markdown" &&
               opened.status_items.at(0).text() == "行 1, 桁 1" &&
               opened.status_items.at(1).text() == "UTF-8",
           "the language follows the three existing items");
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(run_ex(controller, "set ft=ps1"));
    expect(controller.frame().status_items.at(3).text() == "ps1",
           ":set ft= changes the status item");
    expect(core::status_items_for(core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                  core::TextEncoding::utf8, core::LineEnding::lf,
                                  Language::typescript)
                   .at(3)
                   .text() == "typescript",
           "status_items_for writes the language name last");
    for (const std::uint32_t dpi : {96U, 120U, 144U})
    {
        const auto status =
            core::status_bar_layout(core::to_pixels(800, dpi), core::to_pixels(450, dpi), dpi);
        expect(status.items.at(0).right <= status.items.at(1).left &&
                   status.items.at(1).right <= status.items.at(2).left &&
                   status.items.at(2).right <= status.items.at(3).left &&
                   status.items.at(3).right ==
                       core::to_pixels(800, dpi) - core::to_pixels(12, dpi) &&
                   core::width_of(status.items.at(3)) == core::to_pixels(72, dpi),
               "the language item is the right end of the four items");
    }
}

// 狭い窓では言語の欄を畳む（決定 6）。`:` の入力欄の右端が 96 DIP に届かない 4 項目の配置なら
// 言語の欄は幅 0 で、ほかの 3 項目は言語が無いときの位置。96 DIP では 448 DIP が境目。
void verify_status_fold()
{
    for (const std::uint32_t dpi : {96U, 120U, 144U})
    {
        const auto narrow =
            core::status_bar_layout(core::to_pixels(360, dpi), core::to_pixels(200, dpi), dpi);
        const auto input = core::command_layout(narrow, dpi, 0).input;
        expect(core::width_of(narrow.items.at(3)) == 0 &&
                   narrow.items.at(3).left == narrow.items.at(2).right &&
                   narrow.items.at(2).right == core::to_pixels(360, dpi) - core::to_pixels(12, dpi),
               "the minimum window folds the language item at the right end");
        expect(input.right >= core::to_pixels(core::command_input_minimum_dip, dpi),
               "the minimum window keeps the : input up to 96 DIP");
        const auto wide =
            core::status_bar_layout(core::to_pixels(800, dpi), core::to_pixels(450, dpi), dpi);
        expect(core::width_of(wide.items.at(3)) == core::to_pixels(72, dpi) &&
                   core::command_layout(wide, dpi, 0).input.right >=
                       core::to_pixels(core::command_input_minimum_dip, dpi),
               "an 800 DIP window shows the four items");
    }
    expect(core::width_of(core::status_bar_layout(448, 200, 96).items.at(3)) == 72 &&
               core::width_of(core::status_bar_layout(447, 200, 96).items.at(3)) == 0,
           "the fold starts where the input would end before 96 DIP");
}

// 閉じて隣のタブへ移る（決定 1）。隣の束の言語（`:set ft=` の上書きも）が付いてくる。
void verify_close_tab()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of(markdown_path)});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(run_ex(controller, "set ft=json"));
    applied(controller, app::OpenDocument{path_of(python_path)});
    expect(asked_language(controller) == "filetype=python", "the second tab reads its path");
    applied(controller, app::CloseTab{1});
    expect(asked_language(controller) == "filetype=json" &&
               controller.frame().status_items.at(3).text() == "json",
           "closing moves to the neighbour with its own language");
}

// 自動退避から戻した束（決定 5）。言語はパスから引き、名前の無い束は plain。
void verify_restored_autosave()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{10});
    const std::optional<core::FilePath> named{path_of("C:\\work\\data.json")};
    editing.autosave().remain(
        app::AutosaveRemnant{"1f/0", app::AutosaveId{0}, named,
                             core::tab_title_for(named, core::SaveState::modified),
                             core::TextPosition{core::LineNumber{1}, core::Column{1}}},
        "{}");
    editing.autosave().remain(
        app::AutosaveRemnant{"1f/1", app::AutosaveId{0}, std::nullopt,
                             core::tab_title_for(std::nullopt, core::SaveState::modified),
                             core::TextPosition{core::LineNumber{1}, core::Column{1}}},
        "gamma");
    static_cast<void>(controller.autosave_remnants());
    const auto restored = controller.apply_frame(app::RestoreAutosave{});
    expect(restored.status_items.at(3).text() == "json",
           "the first restored tab takes its language from the path");
    expect(controller.apply_frame(app::SwitchTab{2}).status_items.at(3).text() == "text",
           "the untitled restored tab is plain");
}

// ダイアログの絞り込みは拡張子の表から作る（ADR 0154 の追補 2026-10-28・#849）。列挙は表の
// 拡張子だけで重複が無く、pattern は列挙と同じ数の `*.` の項を `;` でつないだもの。
void verify_dialog_pattern()
{
    const auto listed = core::listed_language_extensions();
    std::vector<std::string_view> sorted(listed.begin(), listed.end());
    std::ranges::sort(sorted);
    expect(std::ranges::adjacent_find(sorted) == sorted.end(),
           "the listed extensions are distinct");
    expect(std::ranges::all_of(listed,
                               [](std::string_view extension)
                               {
                                   return !extension.empty() && extension.front() != '.' &&
                                          (extension == "txt" ||
                                           core::language_of_path("a." + std::string(extension)) !=
                                               Language::plain);
                               }),
           "every listed extension is a row of the extension table");
    for (const std::string_view wanted : {"cpp", "yaml", "md", "txt"})
    {
        expect(std::ranges::find(listed, wanted) != listed.end(),
               "the listed extensions include cpp, yaml, md and txt");
    }
    const auto pattern = core::file_dialog_pattern();
    std::vector<std::string_view> items;
    for (const auto item : std::views::split(std::string_view(pattern), ';'))
    {
        items.emplace_back(item.begin(), item.end());
    }
    expect(items.size() == listed.size() &&
               std::ranges::all_of(items, [](std::string_view item)
                                   { return item.starts_with("*.") && item.size() > 2; }),
           "the pattern joins one *. item per listed extension with ;");
    expect(std::ranges::equal(items, listed, [](std::string_view item, std::string_view extension)
                              { return item.substr(2) == extension; }),
           "the pattern keeps the table order");
    expect(pattern.find("CMakeLists") == std::string::npos && items.front() == "*.md",
           "the pattern lists extensions only, not the named rows");
}

// まだ読んでいない文書（決定 5）。一覧に言語は無く、読んだときにパスから決める。
void verify_unloaded_document()
{
    Editing editing;
    hold_documents(editing.files());
    auto &controller = editing.controller();
    applied(controller, app::OpenDocument{path_of(markdown_path)});
    applied(controller, app::OpenDocument{path_of(python_path)});
    applied(controller, app::SelectEditMode{core::EditMode::vim});
    static_cast<void>(run_ex(controller, "set ft=json"));
    applied(controller, app::SwitchTab{0});
    static_cast<void>(run_ex(controller, "set ft=c"));
    applied(controller, app::EndSession{app::SessionEnd::window_closed});
    Editing restored(editing.session().written(), hold_documents);
    auto &again = restored.controller();
    applied(again, app::SelectEditMode{core::EditMode::vim});
    expect(asked_language(again) == "filetype=markdown",
           "the restored active tab reads its language from the path");
    applied(again, app::SwitchTab{1});
    expect(asked_language(again) == "filetype=python",
           "a tab read on demand takes its language from the path");
}
} // namespace

void verify_language_scope()
{
    verify_extension_table();
    verify_godot_extensions();
    verify_c_family_extensions();
    verify_batch4_extensions();
    verify_extension_additions();
    verify_names();
    verify_dialog_pattern();
    verify_ex_evaluation();
    verify_completion();
    verify_document_value();
    verify_status_item();
    verify_status_fold();
    verify_close_tab();
    verify_restored_autosave();
    verify_unloaded_document();
}
} // namespace nenenib::tests
