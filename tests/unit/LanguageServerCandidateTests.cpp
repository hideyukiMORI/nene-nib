// scope `--language-server-candidates` の単体テスト（LSP 段 S4・#1945・ADR 0170 と ADR 0160 の決定
// 7 の追補）。よく知られたサーバーの表・npm の `bin` の読み・命令の行の書き方（core）と、設定の
// 「言語サーバー」の節（application）: 面を開いたときだけ探しを頼む・合図の道で採る・状態の語・
// 「使う」は絶対パスの 1 行を状態の設定に置くだけで保存は閉じるときの 1 回・「外す」は行を消して
// 動いている席を止める・「元に戻す」・畳みの 1 行・表に無い言語の行。替え玉の探しの口
// （ScriptedLanguageServerSearch）と言語サーバー（ScriptedLanguageServer）で確かめ、ファイルも
// 環境変数も読まない。
#include "ChangeSetting.hpp"
#include "CloseSettings.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EditorSettings.hpp"
#include "FilePath.hpp"
#include "FocusSettingsRow.hpp"
#include "FoundLanguageServer.hpp"
#include "JumpSettingsSection.hpp"
#include "KnownLanguageServers.hpp"
#include "Language.hpp"
#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "LanguageServerSlot.hpp"
#include "OpenDocument.hpp"
#include "OpenSettings.hpp"
#include "RevertSettings.hpp"
#include "Scopes.hpp"
#include "ServerCommand.hpp"
#include "ServerMessage.hpp"
#include "ServerStarted.hpp"
#include "SettingsRows.hpp"
#include "SettingsSection.hpp"
#include "SettingsView.hpp"
#include "StepSettings.hpp"
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
using State = app::LanguageServerRowState;
using Action = app::LanguageServerRowAction;

constexpr std::string_view clangd_path = R"(C:\LLVM\bin\clangd.exe)";
constexpr std::string_view node_path = R"(C:\Program Files\nodejs\node.exe)";
constexpr std::string_view pyright_script = R"(C:\npm\node_modules\pyright\langserver.index.js)";
constexpr std::string_view cpp_path = R"(C:\work\main.cpp)";
constexpr std::string_view running_answer = R"({"capabilities":{"textDocumentSync":2}})";

[[nodiscard]] core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

[[nodiscard]] const app::SettingsView &panel_of(const app::EditorFrame &frame)
{
    static const app::SettingsView empty{};
    return frame.settings_panel.has_value() ? frame.settings_panel.value() : empty;
}

// 表の候補の数（探しに渡す候補の列の長さ）。
[[nodiscard]] std::size_t candidate_count()
{
    std::size_t count = 0;
    for (const auto &row : core::known_language_servers())
    {
        count += row.candidates.size();
    }
    return count;
}

// 表の行 row の候補 candidate の、平らな列の中の位置。
[[nodiscard]] std::size_t probe_at(std::size_t row, std::size_t candidate)
{
    std::size_t at = 0;
    for (std::size_t index = 0; index < row; ++index)
    {
        at += core::known_language_servers()[index].candidates.size();
    }
    return at + candidate;
}

// clangd（行 0）と pyright（行 1 の候補 0・npm）だけが見つかった結果。
[[nodiscard]] ScriptedLanguageServerSearch::Found clangd_and_pyright()
{
    ScriptedLanguageServerSearch::Found found(candidate_count());
    found.at(probe_at(0, 0)) = app::FoundLanguageServer{path_of(clangd_path), std::nullopt};
    found.at(probe_at(1, 0)) =
        app::FoundLanguageServer{path_of(node_path), path_of(pyright_script)};
    return found;
}

[[nodiscard]] const std::string *command_of(const core::EditorSettings &settings,
                                            std::string_view name)
{
    const auto found = std::ranges::find_if(settings.lsp_commands, [name](const auto &entry)
                                            { return entry.first == name; });
    return found != settings.lsp_commands.end() ? &found->second : nullptr;
}

// 面を開いて探しの結果を届け、焦点を「言語サーバー」の節の最初の行（C / C++）へ置く。
void opened_with_results(Editing &editing)
{
    auto &controller = editing.controller();
    static_cast<void>(controller.apply(app::OpenSettings{}));
    editing.language_server_search().serve(clangd_and_pyright());
    static_cast<void>(controller.apply(app::WorkCompleted{}));
    static_cast<void>(
        controller.apply(app::JumpSettingsSection{core::SettingsSection::language_server}));
}

// 表（core）: 先頭のよく使う言語・候補・入れ方。
void verify_known_table()
{
    const auto known = core::known_language_servers();
    expect(known.size() >= core::known_language_servers_leading &&
               known.front().title == "C / C++" &&
               known.front().candidates.front().program == "clangd" &&
               std::ranges::contains(known.front().languages, core::Language::cpp) &&
               std::ranges::contains(known.front().languages, core::Language::c),
           "the table starts with clangd for C and C++");
    expect(std::ranges::all_of(
               known, [](const core::KnownLanguageServer &row)
               { return !row.title.empty() && !row.candidates.empty() && !row.languages.empty(); }),
           "every row names its languages and at least one candidate");
    expect(known[1].candidates.front().package == "pyright" &&
               known[1].candidates.front().program == "pyright-langserver" &&
               known[1].candidates.front().arguments == "--stdio",
           "pyright is an npm package whose bin is written with node.exe and --stdio");
}

// npm の `bin`（core）。包みの外を指す・形の違う値は受けない。
void verify_npm_bin()
{
    constexpr std::string_view manifest =
        R"({"name":"pyright","bin":{"pyright":"index.js","pyright-langserver":"./dist/langserver.index.js"}})";
    expect(core::npm_bin_script(manifest, "pyright", "pyright-langserver") ==
                   std::optional<std::string>{R"(dist\langserver.index.js)"} &&
               core::npm_bin_script(manifest, "pyright", "pyright") ==
                   std::optional<std::string>{"index.js"} &&
               !core::npm_bin_script(manifest, "pyright", "other").has_value(),
           "an object bin is read by its name and ./ is dropped");
    expect(core::npm_bin_script(R"({"bin":"cli.js"})", "@scope/tool", "tool") ==
                   std::optional<std::string>{"cli.js"} &&
               !core::npm_bin_script(R"({"bin":"cli.js"})", "@scope/tool", "other").has_value(),
           "a string bin belongs to the package name without its scope");
    for (const std::string_view bad :
         {R"({"bin":{"x":"../evil.js"}})", R"({"bin":{"x":"C:\\evil.js"}})",
          R"({"bin":{"x":"/evil.js"}})", R"({"bin":{"x":"a/../../evil.js"}})",
          R"({"bin":{"x":"a\"b.js"}})", R"({"bin":{"x":""}})", R"({"bin":{"x":7}})",
          R"({"name":"x"})", R"([1,2])", "{"})
    {
        expect(!core::npm_bin_script(bad, "x", "x").has_value(),
               "a bin outside the package or of a wrong shape is refused");
    }
}

// 命令の行（core）。空白を含むパスは `"` で囲み、application の読みが同じ語に読み戻す。
void verify_command_text()
{
    const auto line = core::language_server_command_text(node_path, pyright_script, "--stdio");
    expect(
        line ==
            std::optional<std::string>{
                R"("C:\Program Files\nodejs\node.exe" C:\npm\node_modules\pyright\langserver.index.js --stdio)"},
        "a path with a blank is quoted and the arguments follow");
    const auto launch = line.has_value()
                            ? app::server_launch_of(line.value(), path_of(R"(C:\work)"))
                            : std::nullopt;
    expect(launch.has_value() && launch.value().executable == path_of(node_path) &&
               launch.value().arguments ==
                   std::vector<std::string>{std::string{pyright_script}, "--stdio"},
           "the written line reads back to the same executable and arguments");
    expect(core::language_server_command_text(clangd_path, "", "") ==
                   std::optional<std::string>{std::string{clangd_path}} &&
               !core::language_server_command_text("", "", "").has_value() &&
               !core::language_server_command_text(R"(C:\a"b.exe)", "", "").has_value() &&
               !core::language_server_command_text(clangd_path, "", "a\nb").has_value(),
           "an empty executable, a quote and a line break are not written");
}

// 面を開いたときだけ探す。走っている間は重ねて頼まない。合図の道は頼んだときだけ採る。
void verify_search_on_open_only()
{
    Editing editing;
    auto &controller = editing.controller();
    auto &search = editing.language_server_search();
    static_cast<void>(controller.apply(app::WorkCompleted{}));
    expect(search.touches() == 0, "the work signal does not collect a search nobody asked for");
    const auto opened = controller.apply_frame(app::OpenSettings{});
    expect(search.searches() == 1 && search.probes().size() == candidate_count() &&
               search.probes().front().program == "clangd" &&
               search.probes().at(probe_at(1, 0)).package == "pyright",
           "opening the panel asks one search with every candidate in the table order");
    const auto &panel = panel_of(opened);
    expect(panel.servers.size() == core::known_language_servers_leading &&
               panel.servers_hidden ==
                   core::known_language_servers().size() - core::known_language_servers_leading &&
               std::ranges::all_of(
                   panel.servers, [](const app::LanguageServerRowView &row)
                   { return row.state == State::searching && row.action == Action::none; }),
           "before the result the leading rows say they are searching and the rest fold");
    static_cast<void>(controller.apply(app::WorkCompleted{}));
    static_cast<void>(controller.apply(app::CloseSettings{}));
    static_cast<void>(controller.apply(app::OpenSettings{}));
    expect(search.searches() == 1, "reopening while the search runs does not ask again");
    search.serve(clangd_and_pyright());
    const auto served = controller.apply_frame(app::WorkCompleted{});
    const auto &rows = panel_of(served).servers;
    expect(rows.size() == core::known_language_servers_leading &&
               rows.at(0).state == State::found && rows.at(0).action == Action::use &&
               rows.at(0).detail == clangd_path && rows.at(0).code &&
               rows.at(1).state == State::found && rows.at(1).server == "pyright-langserver" &&
               rows.at(1).detail == pyright_script && rows.at(2).state == State::missing &&
               rows.at(2).action == Action::none &&
               rows.at(2).detail == core::known_language_servers()[2].install,
           "found rows offer Use with their path and missing rows show how to install");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    static_cast<void>(controller.apply(app::OpenSettings{}));
    expect(search.searches() == 2 && editing.settings().writes() == 0,
           "each opening after the result searches again and nothing is written");
}

// 「使う」は状態の設定に置くだけで、保存は閉じるときの 1 回（ADR 0170 の決定 6）。C / C++ の行は
// c と cpp の 2 つの命令の行を書く。npm の物は node.exe とスクリプトの形。
void verify_use_writes_on_close()
{
    Editing editing;
    auto &controller = editing.controller();
    opened_with_results(editing);
    const auto focused = controller.frame();
    expect(panel_of(focused).focused == std::optional<std::size_t>{panel_of(focused).rows.size()} &&
               panel_of(focused).section == std::optional{core::SettingsSection::language_server},
           "the language server tab focuses its first row");
    const auto used = controller.apply_frame(app::ChangeSetting{true});
    const auto *cpp = command_of(used.settings, "cpp");
    const auto *c = command_of(used.settings, "c");
    expect(cpp != nullptr && c != nullptr && *cpp == clangd_path && *c == clangd_path &&
               panel_of(used).servers.at(0).state == State::configured &&
               panel_of(used).servers.at(0).action == Action::remove &&
               editing.settings().writes() == 0 && editing.language_server().touches() == 0,
           "Use puts the absolute path for C and C++ without saving or starting a server");
    static_cast<void>(controller.apply(app::StepSettings{true}));
    const auto python = controller.apply_frame(app::ChangeSetting{true});
    const auto *line = command_of(python.settings, "python");
    expect(
        line != nullptr &&
            *line ==
                R"("C:\Program Files\nodejs\node.exe" C:\npm\node_modules\pyright\langserver.index.js --stdio)",
        "an npm server is written as node.exe and its script, never the .cmd");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    const auto &written = editing.settings().written();
    expect(editing.settings().writes() == 1 && written.has_value() &&
               written.value().lsp_commands.size() == 3 &&
               written.value().lsp_commands.at(0).first == "c" &&
               written.value().lsp_commands.at(1).first == "cpp" &&
               written.value().lsp_commands.at(2).first == "python",
           "closing writes once with the lines in name order");
}

// 「元に戻す」は置いた行を外し、閉じても書かない。
void verify_revert_drops_use()
{
    Editing editing;
    auto &controller = editing.controller();
    opened_with_results(editing);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto reverted = controller.apply_frame(app::RevertSettings{});
    expect(reverted.settings.lsp_commands.empty() &&
               panel_of(reverted).servers.at(0).state == State::found,
           "revert takes the written line back and the row is found again");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 0, "a reverted Use is not saved");
}

// 「外す」は行を消し、動いている席を止める（`shutdown`）。
void verify_remove_stops_running()
{
    auto settings = core::default_editor_settings();
    settings.lsp = true;
    settings.lsp_commands = {{"cpp", std::string{clangd_path}}};
    Editing editing(SettingsReading{std::optional<core::EditorSettings>{settings}});
    editing.files().hold_at(std::string{cpp_path}, Bytes{std::string("int x;\n")});
    auto &controller = editing.controller();
    auto &server = editing.language_server();
    applied(controller, app::OpenDocument{path_of(cpp_path)});
    server.serve(app::LanguageServerSlot::first, app::ServerStarted{});
    static_cast<void>(controller.apply_frame(app::WorkCompleted{}));
    const auto started = controller.apply_frame(app::OpenSettings{});
    expect(panel_of(started).servers.at(0).state == State::starting &&
               panel_of(started).servers.at(0).detail == clangd_path,
           "a configured server waiting for initialize reads as starting");
    const auto initialize = server.requests("initialize");
    if (!initialize.empty())
    {
        server.answer(initialize.back(), running_answer);
    }
    static_cast<void>(controller.apply(app::WorkCompleted{}));
    static_cast<void>(
        controller.apply(app::JumpSettingsSection{core::SettingsSection::language_server}));
    const auto running = controller.frame();
    expect(panel_of(running).servers.at(0).state == State::running &&
               panel_of(running).servers.at(0).action == Action::remove,
           "a running server reads as running with Remove");
    const auto removed = controller.apply_frame(app::ChangeSetting{true});
    expect(removed.settings.lsp_commands.empty() && server.requests("shutdown").size() == 1 &&
               panel_of(removed).servers.at(0).state == State::searching,
           "Remove drops the line and shuts the running server down");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    const auto &written = editing.settings().written();
    expect(editing.settings().writes() == 1 && written.has_value() &&
               written.value().lsp_commands.empty(),
           "the removal is saved once on close");
}

// 畳みの 1 行と、表に無い言語の命令の行。
void verify_fold_and_unlisted()
{
    auto settings = core::default_editor_settings();
    settings.lsp_commands = {{"java", R"(C:\jdtls\jdtls.exe)"}};
    Editing editing(SettingsReading{std::optional<core::EditorSettings>{settings}});
    auto &controller = editing.controller();
    opened_with_results(editing);
    const auto folded = controller.frame();
    const auto &panel = panel_of(folded);
    const std::size_t hidden =
        core::known_language_servers().size() - core::known_language_servers_leading;
    expect(panel.servers.size() == core::known_language_servers_leading + 1 &&
               panel.servers.back().title == "java" && panel.servers.back().server == "jdtls" &&
               panel.servers.back().state == State::configured &&
               panel.servers.back().action == Action::remove && panel.servers_hidden == hidden,
           "a line of a language outside the table stays shown with Remove");
    const std::size_t fold = panel.rows.size() + panel.servers.size();
    static_cast<void>(controller.apply(app::FocusSettingsRow{fold}));
    const auto unfolded = controller.apply_frame(app::ChangeSetting{true});
    const auto &open = panel_of(unfolded);
    expect(open.servers.size() == core::known_language_servers().size() + 1 &&
               open.servers_hidden == 0 &&
               open.focused == std::optional<std::size_t>{open.rows.size() +
                                                          core::known_language_servers_leading} &&
               open.servers.at(core::known_language_servers_leading).title ==
                   core::known_language_servers()[core::known_language_servers_leading].title,
           "the fold row shows every row and focuses the first one it revealed");
    static_cast<void>(
        controller.apply(app::FocusSettingsRow{open.rows.size() + open.servers.size() - 1}));
    const auto removed = controller.apply_frame(app::ChangeSetting{true});
    expect(removed.settings.lsp_commands.empty() &&
               panel_of(removed).servers.size() == core::known_language_servers().size(),
           "Remove on the unlisted row drops its line");
    static_cast<void>(controller.apply(app::CloseSettings{}));
    expect(editing.settings().writes() == 1, "the removal is saved once");
}
// 手で書いた引数つきの命令の行は、別の行の「使う」・「外す」・畳みを開くの後に閉じても、書いた
// bytes のまま残る（触るのは押した行の言語の行だけ・#1945 のレビュー）。
void verify_hand_written_lines_stay()
{
    constexpr std::string_view rust = R"("C:\my tools\rust-analyzer.exe" --log-file C:\w\ra.log)";
    constexpr std::string_view java = R"(C:\jdtls\jdtls.exe -data "C:\w s\ws" --stdio)";
    auto settings = core::default_editor_settings();
    settings.lsp_commands = {{"java", std::string{java}}, {"rust", std::string{rust}}};
    Editing editing(SettingsReading{std::optional<core::EditorSettings>{settings}});
    auto &controller = editing.controller();
    opened_with_results(editing);
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    const auto panel = panel_of(controller.frame());
    const std::size_t fold = panel.rows.size() + panel.servers.size();
    static_cast<void>(controller.apply(app::FocusSettingsRow{fold}));
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::FocusSettingsRow{panel.rows.size()}));
    static_cast<void>(controller.apply(app::StepSettings{true}));
    static_cast<void>(controller.apply(app::ChangeSetting{true}));
    static_cast<void>(controller.apply(app::CloseSettings{}));
    const auto written = editing.settings().written().value_or(core::default_editor_settings());
    const auto *kept_rust = command_of(written, "rust");
    const auto *kept_java = command_of(written, "java");
    expect(editing.settings().writes() == 1 && kept_rust != nullptr && *kept_rust == rust &&
               kept_java != nullptr && *kept_java == java &&
               command_of(written, "cpp") != nullptr && command_of(written, "python") != nullptr,
           "hand-written lines with arguments survive other rows' Use and the fold");
}
} // namespace

void verify_language_server_candidates_scope()
{
    verify_known_table();
    verify_npm_bin();
    verify_command_text();
    verify_search_on_open_only();
    verify_use_writes_on_close();
    verify_revert_drops_use();
    verify_remove_stops_running();
    verify_fold_and_unlisted();
    verify_hand_written_lines_stay();
}
} // namespace nenenib::tests
