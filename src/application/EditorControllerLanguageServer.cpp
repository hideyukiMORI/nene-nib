#include "EditorController.hpp"

#include "DisplayText.hpp"
#include "FileFolder.hpp"
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageTable.hpp"
#include "LspIncoming.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspPositionEncoding.hpp"
#include "LspRequest.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "PendingLspRequest.hpp"
#include "ServerCommand.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
// 1 行の知らせ（ADR 0160 の決定 6）。言語の名前を後ろに付ける。
constexpr std::string_view server_unstarted_message = "言語サーバーを起こせませんでした: ";
constexpr std::string_view server_ended_message = "言語サーバーが終わりました: ";
constexpr std::string_view server_silent_message = "言語サーバーが答えません: ";
constexpr std::string_view server_given_up_message =
    "言語サーバーを止めました（:set lsp で再開）: ";

// `initialize` の params（決定 5）。位置の単位は utf-8 と utf-16 を申し出る。作業フォルダは
// 起こす要求の root で渡すので rootUri は null（段 L1 で文書の URI と一緒に決める）。
[[nodiscard]] core::JsonValue initialize_params()
{
    core::JsonObject general;
    general.emplace_back("positionEncodings",
                         core::JsonValue{core::JsonArray{core::JsonValue{std::string{"utf-8"}},
                                                         core::JsonValue{std::string{"utf-16"}}}});
    core::JsonObject capabilities;
    capabilities.emplace_back("general", core::JsonValue{std::move(general)});
    core::JsonObject params;
    params.emplace_back("processId", core::JsonValue{core::JsonNull{}});
    params.emplace_back("rootUri", core::JsonValue{core::JsonNull{}});
    params.emplace_back("capabilities", core::JsonValue{std::move(capabilities)});
    return core::JsonValue{std::move(params)};
}

// object の鍵 key の object。無い・object でなければ nullptr。
[[nodiscard]] const core::JsonObject *member_object(const core::JsonObject &object,
                                                    std::string_view key)
{
    const auto member = core::json_member(object, key);
    return member.has_value() ? std::get_if<core::JsonObject>(&member.value().get().value)
                              : nullptr;
}

// `initialize` の答えの `capabilities.positionEncoding`（決定 5）。"utf-8" のときだけ utf8 で、
// 無い・ほかの値は LSP の既定の utf16。
[[nodiscard]] core::LspPositionEncoding chosen_encoding(const core::JsonValue &result)
{
    const auto *object = std::get_if<core::JsonObject>(&result.value);
    const auto *capabilities = object != nullptr ? member_object(*object, "capabilities") : nullptr;
    if (capabilities == nullptr)
    {
        return core::LspPositionEncoding::utf16;
    }
    const auto encoding = core::json_member(*capabilities, "positionEncoding");
    const auto *name =
        encoding.has_value() ? std::get_if<std::string>(&encoding.value().get().value) : nullptr;
    return name != nullptr && *name == "utf-8" ? core::LspPositionEncoding::utf8
                                               : core::LspPositionEncoding::utf16;
}

// `$/cancelRequest` の params（決定 6）。
[[nodiscard]] core::JsonValue cancel_params(std::int64_t id)
{
    core::JsonObject params;
    params.emplace_back("id", core::JsonValue{core::json_integer(id)});
    return core::JsonValue{std::move(params)};
}

[[nodiscard]] core::DisplayText server_notice(std::string_view head, core::Language language)
{
    return core::DisplayText::parse(std::string{head} + std::string{core::language_name(language)})
        .value();
}
} // namespace

// ---------------------------------------------------------------- 言語サーバー（ADR 0160）

// 起きているべき子と今の子を合わせる 1 本（決定 6・7）。オフで子が居なければ欄を 2 つ読むだけで
// 抜ける（決定 9 の 1 打鍵の費用）。オンでも同じ言語の子が居れば port を呼ばない。
void EditorController::reconcile_language_server()
{
    auto &link = language_server_;
    const bool on = state_.settings().lsp && !link.given_up;
    if (!on && link.phase == LanguageServerPhase::idle)
    {
        return;
    }
    const core::Language language = state_.language();
    switch (link.phase)
    {
    case LanguageServerPhase::idle:
        consider_language_server(language);
        return;
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
    case LanguageServerPhase::running:
        if (!on || link.language != std::optional<core::Language>{language})
        {
            shut_language_server();
        }
        return;
    case LanguageServerPhase::shutting_down:
        return;
    }
}

// 子が居ないとき、この言語でまだ決めていなければ起こすかを決める（同じ言語では打鍵ごとに設定を
// 引き直さない）。
void EditorController::consider_language_server(core::Language language)
{
    auto &link = language_server_;
    if (link.considered == std::optional<core::Language>{language})
    {
        return;
    }
    link.considered = language;
    auto launch = wanted_language_server(language);
    if (launch.has_value())
    {
        start_language_server(language, std::move(launch).value());
    }
}

// 設定に書いた命令だけを起こす（決定 7）。言語の名前で lsp_commands を引き、root は設定の root
// か文書のフォルダ。命令が無い・読めない・root が決まらなければ値なし。
std::optional<ServerLaunch> EditorController::wanted_language_server(core::Language language) const
{
    const auto &settings = state_.settings();
    const std::string_view name = core::language_name(language);
    const auto command = std::ranges::find_if(settings.lsp_commands, [name](const auto &entry)
                                              { return entry.first == name; });
    if (command == settings.lsp_commands.end())
    {
        return std::nullopt;
    }
    const auto &path = state_.document().path;
    const auto root = settings.root.has_value() ? settings.root
                      : path.has_value()        ? core::folder_of(path.value())
                                                : std::nullopt;
    if (!root.has_value())
    {
        return std::nullopt;
    }
    return server_launch_of(command->second, root.value());
}

// 起こしてすぐ戻る（決定 1）。起こせなければ 1 行知らせて止まる（設定の誤りは何度起こしても
// 同じなので自動では起こし直さない・`:set lsp` で再開）。
void EditorController::start_language_server(core::Language language, ServerLaunch launch)
{
    auto &link = language_server_;
    const auto started = ports_.language_server.start(std::move(launch));
    if (!started)
    {
        link.given_up = true;
        state_ = state_.with_command_message(server_notice(server_unstarted_message, language));
        return;
    }
    link.phase = LanguageServerPhase::starting;
    link.language = language;
    link.pending.clear();
}

// 子を待たずに止める（追補）。stop は出来事の列も捨てるので、同じ集めの残りは読まない。
void EditorController::stop_language_server()
{
    auto &link = language_server_;
    ports_.language_server.stop();
    link.phase = LanguageServerPhase::idle;
    link.language.reset();
    link.pending.clear();
    link.considered.reset();
    link.retired = true;
    ++link.generation;
}

// `shutdown` を送り、答えで `exit` を送って止める（決定 6・追補）。`initialize` の答えの前は
// 何も送れないので、そのまま止める。
void EditorController::shut_language_server()
{
    auto &link = language_server_;
    switch (link.phase)
    {
    case LanguageServerPhase::idle:
    case LanguageServerPhase::shutting_down:
        return;
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
        stop_language_server();
        return;
    case LanguageServerPhase::running:
        link.pending.clear();
        send_lsp_request(LspRequestKind::shutdown, "shutdown", core::JsonValue{core::JsonNull{}});
        link.phase = LanguageServerPhase::shutting_down;
        return;
    }
}

// 要求 id は controller の通し番号（決定 5）。答えを待つ列に載せる。
void EditorController::send_lsp_request(LspRequestKind kind, std::string_view method,
                                        const core::JsonValue &params)
{
    auto &link = language_server_;
    const std::int64_t id = link.next_id++;
    ports_.language_server.send(core::lsp_request_body(id, method, params));
    link.pending.push_back(PendingLspRequest{id, kind});
}

// 子が終わった・話せない・答えない（決定 6）。止めて 1 行知らせ、上限までは同じ言語で起こし直す。
void EditorController::language_server_failed(std::string_view head)
{
    auto &link = language_server_;
    const core::Language language = link.language.value_or(state_.language());
    stop_language_server();
    ++link.failures;
    if (link.failures > language_server_restart_limit)
    {
        link.given_up = true;
        state_ = state_.with_command_message(server_notice(server_given_up_message, language));
        return;
    }
    state_ = state_.with_command_message(server_notice(head, language));
    auto launch = wanted_language_server(language);
    if (launch.has_value())
    {
        link.considered = language;
        start_language_server(language, std::move(launch).value());
    }
}

// WorkCompleted の道（決定 1）。子が居なければ port を呼ばない。止めた後の残りは読まない。
void EditorController::collect_language_server()
{
    if (language_server_.phase == LanguageServerPhase::idle)
    {
        return;
    }
    const auto events = ports_.language_server.collect();
    const std::uint64_t generation = language_server_.generation;
    for (const ServerEvent &event : events)
    {
        if (language_server_.generation != generation)
        {
            return;
        }
        std::visit([this](const auto &value) { this->receive_server(value); }, event);
    }
}

void EditorController::receive_server(const ServerStarted & /*started*/)
{
    auto &link = language_server_;
    if (link.phase != LanguageServerPhase::starting)
    {
        return;
    }
    link.phase = LanguageServerPhase::initializing;
    send_lsp_request(LspRequestKind::initialize, "initialize", initialize_params());
}

// 読めない本文は捨てる（段 L0 は寿命の答えだけを読む）。
void EditorController::receive_server(const ServerMessage &message)
{
    const auto value = core::parse_json(message.body);
    if (!value)
    {
        return;
    }
    const auto incoming = core::lsp_incoming(value.value());
    if (!incoming)
    {
        return;
    }
    std::visit([this](const auto &item) { this->receive_lsp(item); }, incoming.value());
}

void EditorController::receive_server(const ServerExited & /*exited*/)
{
    end_language_server();
}

void EditorController::receive_server(const ServerFailed & /*failed*/)
{
    end_language_server();
}

// 子の終わり（決定 6）。`shutdown` を送った後なら頼んだ終わりで、合わせ直す。ほかは失敗。
void EditorController::end_language_server()
{
    if (language_server_.phase == LanguageServerPhase::shutting_down)
    {
        stop_language_server();
        reconcile_language_server();
        return;
    }
    language_server_failed(server_ended_message);
}

void EditorController::receive_lsp(const core::LspResponse &response)
{
    auto &link = language_server_;
    const auto *id = std::get_if<std::int64_t>(&response.id);
    if (id == nullptr)
    {
        return;
    }
    const auto pending = std::ranges::find_if(link.pending, [id](const PendingLspRequest &request)
                                              { return request.id == *id; });
    if (pending == link.pending.end())
    {
        return;
    }
    const LspRequestKind kind = pending->kind;
    link.pending.erase(pending);
    switch (kind)
    {
    case LspRequestKind::initialize:
        if (!response.result)
        {
            language_server_failed(server_unstarted_message);
            return;
        }
        link.encoding = chosen_encoding(response.result.value());
        ports_.language_server.send(
            core::lsp_notification_body("initialized", core::JsonValue{core::JsonObject{}}));
        link.phase = LanguageServerPhase::running;
        return;
    case LspRequestKind::shutdown:
        ports_.language_server.send(
            core::lsp_notification_body("exit", core::JsonValue{core::JsonNull{}}));
        stop_language_server();
        reconcile_language_server();
        return;
    }
}

// サーバーからの通知は段 L0 では読まない（`window/showMessage` の表示は L1・決定 10）。
void EditorController::receive_lsp(const core::LspNotification & /*notification*/) {}

// サーバーからの要求は MethodNotFound で断る（決定 5）。
void EditorController::receive_lsp(const core::LspRequest &request)
{
    ports_.language_server.send(core::lsp_method_not_found_body(request.id));
}

// 古さの合図（決定 6・追補）。子が居ないときは port を呼ばない。2 回の tick を越えた要求は
// 捨てる。退く子は adapter に片付けを頼む。
void EditorController::accept(const LanguageServerTick &)
{
    repaint_ = false;
    auto &link = language_server_;
    if (link.retired)
    {
        link.retired = false;
        ports_.language_server.expire_retired();
    }
    std::vector<PendingLspRequest> stale;
    for (PendingLspRequest &request : link.pending)
    {
        ++request.ticks;
        if (request.ticks >= language_server_stale_ticks)
        {
            stale.push_back(request);
        }
    }
    std::erase_if(link.pending, [](const PendingLspRequest &request)
                  { return request.ticks >= language_server_stale_ticks; });
    for (const PendingLspRequest &request : stale)
    {
        expire_lsp_request(request);
    }
}

// 古い要求 1 つ（決定 6）。`initialize` は `$/cancelRequest` を送って捨て、答えない子として
// 失敗に数える。`shutdown` は `exit` を送らずに止める（依頼書・追補）。
void EditorController::expire_lsp_request(const PendingLspRequest &request)
{
    switch (request.kind)
    {
    case LspRequestKind::initialize:
        ports_.language_server.send(
            core::lsp_notification_body("$/cancelRequest", cancel_params(request.id)));
        repaint_ = true;
        language_server_failed(server_silent_message);
        return;
    case LspRequestKind::shutdown:
        stop_language_server();
        reconcile_language_server();
        return;
    }
}

// `:set lsp` `:set nolsp` `:set lsp?`（決定 7）。オンは止まった後の手での再開も兼ね、失敗の
// 数を 0 に戻す。どの形も今の値を 1 行で知らせる。保存できなければ断る。
void EditorController::answer_lsp(core::ExLspRequest request)
{
    auto settings = state_.settings();
    switch (request)
    {
    case core::ExLspRequest::enable:
        settings.lsp = true;
        language_server_.failures = 0;
        language_server_.given_up = false;
        language_server_.considered.reset();
        break;
    case core::ExLspRequest::disable:
        settings.lsp = false;
        break;
    case core::ExLspRequest::show:
        break;
    }
    if (!persist_settings(std::move(settings)))
    {
        reject(core::DisplayText::parse("Settings could not be saved").value());
        return;
    }
    state_ = state_.with_command_message(
        core::DisplayText::parse(state_.settings().lsp ? "lsp=on" : "lsp=off").value());
    reconcile_language_server();
}

// 窓が閉じていく（追補）。`shutdown` の答えは待てないので止めるだけで、子は adapter の Job
// Object が片付ける。
void EditorController::end_language_server_session()
{
    if (language_server_.phase != LanguageServerPhase::idle)
    {
        stop_language_server();
    }
}
} // namespace nenenib::application
