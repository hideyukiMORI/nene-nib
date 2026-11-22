// 設定の「言語サーバー」の節（LSP 段 S4・#1945・ADR 0170 と ADR 0160 の決定 7 の追補・案 A）。
// よく知られたサーバーの表（core::known_language_servers）の順に 1
// 行ずつ、状態と「使う」/「外す」を 並べる。PC の中を探すのは面を開いたときの 1
// 回だけで（LanguageServerSearchPort・裏の仕事）、 「使う」は見つかった候補の絶対パスの
// `lsp.<name>.command` を状態の設定に置くだけ（保存は面を 閉じるときの 1
// 回・勝手に書かない・勝手に起こさない）。「外す」は行を消し、動いていれば止める。
#include "EditorController.hpp"

#include "EditorSettings.hpp"
#include "FoundLanguageServer.hpp"
#include "KnownLanguageServer.hpp"
#include "KnownLanguageServers.hpp"
#include "Language.hpp"
#include "LanguageServerCandidate.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageServerProbe.hpp"
#include "LanguageServerRowAction.hpp"
#include "LanguageServerRowState.hpp"
#include "LanguageServerRowView.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageTable.hpp"
#include "ServerCommand.hpp"
#include "SettingsRows.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
using Commands = std::vector<std::pair<std::string, std::string>>;

// 設定の行の名前 name の命令（無ければ値なし）。
[[nodiscard]] const std::string *command_named(const Commands &commands, std::string_view name)
{
    const auto found =
        std::ranges::find_if(commands, [name](const auto &entry) { return entry.first == name; });
    return found != commands.end() ? &found->second : nullptr;
}

// 行 row の言語の最初の命令（無ければ値なし）。
[[nodiscard]] const std::string *row_command(const Commands &commands,
                                             const core::KnownLanguageServer &row)
{
    for (const core::Language language : row.languages)
    {
        if (const auto *command = command_named(commands, core::language_name(language)))
        {
            return command;
        }
    }
    return nullptr;
}

// 表のどの行にも無い名前の命令の行（設定の順・表に無い言語や表の外の Nib の言語）。
[[nodiscard]] std::vector<std::string> unlisted_names(const Commands &commands)
{
    std::vector<std::string> names;
    for (const auto &[name, command] : commands)
    {
        const auto language = core::language_of_name(name);
        const bool listed =
            language.has_value() &&
            std::ranges::any_of(core::known_language_servers(),
                                [&language](const core::KnownLanguageServer &row)
                                { return std::ranges::contains(row.languages, language.value()); });
        if (!listed)
        {
            names.push_back(name);
        }
    }
    return names;
}

[[nodiscard]] char folded(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

// text が needle を含むか（ASCII の大小を区別しない・1 バイトずつ比べる）。
[[nodiscard]] bool contains_folded(std::string_view text, std::string_view needle) noexcept
{
    if (needle.empty() || needle.size() > text.size())
    {
        return false;
    }
    for (std::size_t start = 0; start + needle.size() <= text.size(); ++start)
    {
        std::size_t matched = 0;
        while (matched < needle.size() &&
               folded(text.at(start + matched)) == folded(needle.at(matched)))
        {
            ++matched;
        }
        if (matched == needle.size())
        {
            return true;
        }
    }
    return false;
}

// 命令の最初の語のファイルの名前（`"` を外す・起こす道と同じ server_launch_of
// で読む）。読めなければ 空。
[[nodiscard]] std::string executable_name(std::string_view command)
{
    const auto root = core::FilePath::parse("C:\\");
    const auto launch = root.has_value() ? server_launch_of(command, root.value()) : std::nullopt;
    return launch.has_value() ? std::string{launch.value().executable.file_name()} : std::string{};
}

// 命令の最初の語のファイルの名前（拡張子を除く）。
[[nodiscard]] std::string executable_stem(std::string_view command)
{
    const std::string name = executable_name(command);
    std::size_t dot = name.size();
    while (dot > 0 && name.at(dot - 1) != '.')
    {
        --dot;
    }
    return dot > 1 ? name.substr(0, dot - 1) : name;
}

// 設定の命令がどの候補か（npm の包みのフォルダか実行ファイルの名前を含む）。無ければ命令の
// 実行ファイルの名前。
[[nodiscard]] std::string configured_server(const core::KnownLanguageServer &row,
                                            std::string_view command)
{
    for (const auto &candidate : row.candidates)
    {
        const std::string folder = "node_modules\\" + std::string{candidate.package} + "\\";
        if ((!candidate.package.empty() && contains_folded(command, folder)) ||
            contains_folded(command, candidate.program))
        {
            return std::string{candidate.program};
        }
    }
    return executable_stem(command);
}

// 見つかった候補の命令の行（書く値）。書けない文字を含めば値なし。
[[nodiscard]] std::optional<std::string>
found_command(const core::LanguageServerCandidate &candidate, const FoundLanguageServer &found)
{
    return core::language_server_command_text(
        found.executable.text(), found.script.has_value() ? found.script.value().text() : "",
        candidate.arguments);
}

// 見つかった候補のパス（2 行目・npm の物はスクリプト）。
[[nodiscard]] std::string found_path(const FoundLanguageServer &found)
{
    return std::string{found.script.has_value() ? found.script.value().text()
                                                : found.executable.text()};
}

// name の命令の行を value にする（無ければ名前の順の位置に足す）。
void put_command(Commands &commands, std::string_view name, const std::string &value)
{
    const auto at = std::ranges::find_if(commands, [name](const auto &entry)
                                         { return std::string_view{entry.first} >= name; });
    if (at != commands.end() && at->first == name)
    {
        at->second = value;
        return;
    }
    commands.emplace(at, std::string{name}, value);
}

// 結果の列の start から count 個（表の 1 行の候補）の中で最初に見つかった候補（候補の添字と物）。
[[nodiscard]] std::optional<std::pair<std::size_t, FoundLanguageServer>>
first_found(const std::vector<std::optional<FoundLanguageServer>> &results, std::size_t start,
            std::size_t count)
{
    for (std::size_t candidate = 0; candidate < count && start + candidate < results.size();
         ++candidate)
    {
        const auto &result = results.at(start + candidate);
        if (result.has_value())
        {
            return std::pair{candidate, result.value()};
        }
    }
    return std::nullopt;
}

// 表の行 index（表に無い言語の行なら name）の言語の命令の行を消す。
void erase_row_commands(Commands &commands, std::size_t index, std::string_view name)
{
    const auto known = core::known_language_servers();
    std::erase_if(commands,
                  [&](const auto &entry)
                  {
                      const auto language = core::language_of_name(entry.first);
                      return index >= known.size()
                                 ? entry.first == name
                                 : language.has_value() &&
                                       std::ranges::contains(known[index].languages,
                                                             language.value());
                  });
}

// before と after で命令の行が違う名前（消えた・変わった・足した）。
[[nodiscard]] std::vector<std::string> changed_names(const Commands &before, const Commands &after)
{
    std::vector<std::string> names;
    for (const auto &[name, command] : before)
    {
        const auto *now = command_named(after, name);
        if (now == nullptr || *now != command)
        {
            names.push_back(name);
        }
    }
    for (const auto &entry : after)
    {
        if (command_named(before, entry.first) == nullptr)
        {
            names.push_back(entry.first);
        }
    }
    return names;
}

// 畳んでいても出す行か（先頭のよく使う言語・設定に行がある・見つかった）。
[[nodiscard]] bool kept_when_folded(std::size_t index, const LanguageServerRowView &row) noexcept
{
    return index < core::known_language_servers_leading ||
           (row.state != LanguageServerRowState::missing &&
            row.state != LanguageServerRowState::searching);
}

// 動いている・起こしている途中の段か（止めに向かっている子は数えない）。
[[nodiscard]] std::optional<LanguageServerRowState> phase_state(LanguageServerPhase phase) noexcept
{
    switch (phase)
    {
    case LanguageServerPhase::running:
        return LanguageServerRowState::running;
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
        return LanguageServerRowState::starting;
    case LanguageServerPhase::idle:
    case LanguageServerPhase::shutting_down:
        return std::nullopt;
    }
    return std::nullopt;
}
} // namespace

// 設定に行がある言語の状態（席の段・止めたか）。languages のどれかの席の最も進んだ段。
LanguageServerRowState
EditorController::configured_state(std::span<const core::Language> languages) const
{
    bool starting = false;
    for (const auto &link : language_servers_.links)
    {
        if (!link.language.has_value() || !std::ranges::contains(languages, link.language.value()))
        {
            continue;
        }
        const auto state = phase_state(link.phase);
        if (state == std::optional{LanguageServerRowState::running})
        {
            return LanguageServerRowState::running;
        }
        starting = starting || state.has_value();
    }
    if (starting)
    {
        return LanguageServerRowState::starting;
    }
    return std::ranges::any_of(languages, [this](core::Language language)
                               { return language_server_given_up(language); })
               ? LanguageServerRowState::stopped
               : LanguageServerRowState::configured;
}

// 設定に行がある行の 2 行目（止めたときは理由・ほかは命令の行）。
void EditorController::describe_configured(LanguageServerRowView &view,
                                           const std::string &command) const
{
    const bool stopped = view.state == LanguageServerRowState::stopped;
    view.detail = stopped ? std::string{core::ui_text(core::UiMessage::settings_lsp_stopped_reason,
                                                      state_.display_language())}
                          : command;
    view.code = !stopped;
    view.action = LanguageServerRowAction::remove;
}

// 表の行 index の表示値（設定に行がある → 見つかった → 探している → 見つからない）。
LanguageServerRowView EditorController::known_server_row(std::size_t index) const
{
    const auto &row = core::known_language_servers()[index];
    const auto &commands = state_.settings().lsp_commands;
    LanguageServerRowView view{std::string{row.title},
                               std::string{row.candidates.front().program},
                               LanguageServerRowState::searching,
                               {},
                               false,
                               LanguageServerRowAction::none};
    if (const auto *command = row_command(commands, row))
    {
        view.server = configured_server(row, *command);
        view.state = configured_state(row.languages);
        describe_configured(view, *command);
        return view;
    }
    if (!found_language_servers_.has_value())
    {
        return view;
    }
    const auto &found = found_language_servers_.value().at(index);
    if (found.has_value())
    {
        view.server = std::string{row.candidates[found.value().first].program};
        view.state = LanguageServerRowState::found;
        view.detail = found_path(found.value().second);
        view.code = true;
        view.action = LanguageServerRowAction::use;
        return view;
    }
    view.state = LanguageServerRowState::missing;
    view.code = !row.install.empty();
    view.detail = view.code ? std::string{row.install}
                            : core::ui_text_filled(core::UiMessage::settings_lsp_install_hint,
                                                   state_.display_language(), {view.server});
    return view;
}

std::vector<LanguageServerRowView> EditorController::language_server_rows() const
{
    std::vector<LanguageServerRowView> rows;
    const auto known = core::known_language_servers();
    for (std::size_t index = 0; index < known.size(); ++index)
    {
        rows.push_back(known_server_row(index));
    }
    const auto &commands = state_.settings().lsp_commands;
    for (const auto &name : unlisted_names(commands))
    {
        const std::string &command = *command_named(commands, name);
        const auto language = core::language_of_name(name);
        LanguageServerRowView view{
            name, executable_stem(command),       LanguageServerRowState::configured, {},
            true, LanguageServerRowAction::remove};
        if (language.has_value())
        {
            const std::array languages{language.value()};
            view.state = configured_state(languages);
        }
        describe_configured(view, command);
        rows.push_back(std::move(view));
    }
    return rows;
}

// 見えている行（rows の添字・畳んでいれば畳んでも出す行だけ）。
std::vector<std::size_t>
EditorController::shown_language_server_rows(const std::vector<LanguageServerRowView> &rows) const
{
    std::vector<std::size_t> shown;
    const bool unfolded = settings_.has_value() && settings_.value().servers_unfolded;
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        if (unfolded || kept_when_folded(index, rows.at(index)))
        {
            shown.push_back(index);
        }
    }
    return shown;
}

// 面を開いたとき（決定 2）。探しが走っていれば重ねて頼まない。候補は表の順に平らに並べる。
void EditorController::request_language_server_search()
{
    if (language_server_search_reading_)
    {
        return;
    }
    std::vector<LanguageServerProbe> probes;
    for (const auto &row : core::known_language_servers())
    {
        for (const auto &candidate : row.candidates)
        {
            probes.push_back(LanguageServerProbe{std::string{candidate.program},
                                                 std::string{candidate.package}});
        }
    }
    language_server_search_reading_ = true;
    ports_.language_server_search.search(std::move(probes));
}

// 合図の道（WorkCompleted）。探しが走っていなければ port を呼ばない。表の行ごとに、表の順で最初に
// 見つかった候補を覚える（数が合わなければどれも見つからない）。
void EditorController::collect_language_server_search()
{
    if (!language_server_search_reading_)
    {
        return;
    }
    auto results = ports_.language_server_search.collect();
    if (!results.has_value())
    {
        return;
    }
    language_server_search_reading_ = false;
    const auto known = core::known_language_servers();
    std::vector<std::optional<std::pair<std::size_t, FoundLanguageServer>>> found;
    std::size_t probe = 0;
    for (const auto &row : known)
    {
        found.push_back(first_found(results.value(), probe, row.candidates.size()));
        probe += row.candidates.size();
    }
    found_language_servers_ = std::move(found);
}

// 「言語サーバー」の節の行 index（language_server_rows の添字・その数なら「ほかの N 言語を見る」）
// を押した（Enter・クリック）。「使う」は行の言語ごとに見つかった候補の命令の行を置き、「外す」は
// 行の言語の命令の行を消す。置くだけで保存はしない（面を閉じるときの 1 回・決定 6）。
void EditorController::act_on_language_server_row(std::size_t index)
{
    const auto rows = language_server_rows();
    if (index >= rows.size())
    {
        unfold_language_server_rows(rows);
        return;
    }
    auto settings = state_.settings();
    switch (rows.at(index).action)
    {
    case LanguageServerRowAction::none:
        return;
    case LanguageServerRowAction::use:
        if (!used_commands(settings.lsp_commands, index))
        {
            return;
        }
        break;
    case LanguageServerRowAction::remove:
        erase_row_commands(settings.lsp_commands, index, rows.at(index).title);
        break;
    }
    preview_settings(std::move(settings));
}

// 表の行 index の見つかった候補の命令の行を、行が受ける言語ごとに置く（書けない文字なら偽）。
bool EditorController::used_commands(std::vector<std::pair<std::string, std::string>> &commands,
                                     std::size_t index) const
{
    if (!found_language_servers_.has_value())
    {
        return false;
    }
    const auto &chosen = found_language_servers_.value().at(index);
    if (!chosen.has_value())
    {
        return false;
    }
    const auto &row = core::known_language_servers()[index];
    const auto &[candidate, found] = chosen.value();
    const auto command = found_command(row.candidates[candidate], found);
    if (!command.has_value())
    {
        return false;
    }
    for (const core::Language language : row.languages)
    {
        put_command(commands, core::language_name(language), command.value());
    }
    return true;
}

// 「ほかの N 言語を見る」。畳んだ行を全部出し、焦点を最初に出た行へ置く。
void EditorController::unfold_language_server_rows(const std::vector<LanguageServerRowView> &rows)
{
    if (!settings_.has_value())
    {
        return;
    }
    const auto shown = shown_language_server_rows(rows);
    std::size_t first = 0;
    while (first < shown.size() && shown.at(first) == first)
    {
        ++first;
    }
    settings_.value().servers_unfolded = true;
    settings_.value().focus = core::settings_rows.size() + first;
}

// 命令の行が変わった言語（「使う」「外す」「元に戻す」・#1945）。その言語の席を止め、失敗の控えを
// 消し、起こすかを決め直す（行があれば今の規則どおり `lsp=on` でその言語の文書があれば起きる）。
void EditorController::follow_language_server_commands(const Commands &before,
                                                       const Commands &after)
{
    if (before == after)
    {
        return;
    }
    for (const auto &name : changed_names(before, after))
    {
        const auto language = core::language_of_name(name);
        if (language.has_value())
        {
            restart_language_servers_of(language.value());
        }
    }
    language_servers_.considered.reset();
}

// language の失敗の控えを消し、その言語の席を止める（行が残っていれば今の規則で起こし直す）。
void EditorController::restart_language_servers_of(core::Language language)
{
    std::erase_if(language_servers_.troubles,
                  [language](const auto &trouble) { return trouble.language == language; });
    for (const LanguageServerSlot slot : language_server_slots)
    {
        if (link_of(slot).language == std::optional{language})
        {
            shut_language_server(slot);
        }
    }
}
} // namespace nenenib::application
