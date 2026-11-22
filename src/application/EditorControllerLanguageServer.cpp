#include "EditorController.hpp"

#include "DisplayText.hpp"
#include "Document.hpp"
#include "DocumentSync.hpp"
#include "DocumentUri.hpp"
#include "FileFolder.hpp"
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "LanguageServerEvent.hpp"
#include "LanguageServerLink.hpp"
#include "LanguageServerLog.hpp"
#include "LanguageServerLogKind.hpp"
#include "LanguageServerPhase.hpp"
#include "LanguageServerSlot.hpp"
#include "LanguageServerTrouble.hpp"
#include "LanguageServers.hpp"
#include "LanguageTable.hpp"
#include "LspDiagnostics.hpp"
#include "LspIncoming.hpp"
#include "LspJsonRead.hpp"
#include "LspMessageType.hpp"
#include "LspMessages.hpp"
#include "LspNotification.hpp"
#include "LspPositionEncoding.hpp"
#include "LspProgress.hpp"
#include "LspProgressValue.hpp"
#include "LspRequest.hpp"
#include "LspRequestKind.hpp"
#include "LspResponse.hpp"
#include "LspShowMessage.hpp"
#include "PendingLspContext.hpp"
#include "PendingLspRequest.hpp"
#include "SaveNotice.hpp"
#include "ServerAbilities.hpp"
#include "ServerCommand.hpp"
#include "ServerErrorOutput.hpp"
#include "ServerEvent.hpp"
#include "ServerFailure.hpp"
#include "ServerMessage.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::application
{
namespace
{
[[nodiscard]] core::JsonValue text_list(std::initializer_list<std::string_view> texts)
{
    core::JsonArray list;
    for (const std::string_view text : texts)
    {
        list.push_back(core::JsonValue{std::string{text}});
    }
    return core::JsonValue{std::move(list)};
}

// client の `textDocument` の能力（ADR 0190 の決定 3）。補完はスニペットを受けず説明は平文、hover
// は 平文と markdown、定義は LocationLink を受ける。
[[nodiscard]] core::JsonValue text_document_capabilities()
{
    core::JsonObject item;
    item.emplace_back("snippetSupport", core::JsonValue{false});
    item.emplace_back("documentationFormat", text_list({"plaintext"}));
    core::JsonObject completion;
    completion.emplace_back("completionItem", core::JsonValue{std::move(item)});
    core::JsonObject hover;
    hover.emplace_back("contentFormat", text_list({"plaintext", "markdown"}));
    core::JsonObject definition;
    definition.emplace_back("linkSupport", core::JsonValue{true});
    core::JsonObject text_document;
    text_document.emplace_back("completion", core::JsonValue{std::move(completion)});
    text_document.emplace_back("hover", core::JsonValue{std::move(hover)});
    text_document.emplace_back("definition", core::JsonValue{std::move(definition)});
    return core::JsonValue{std::move(text_document)};
}

// `initialize` の params（決定 5）。位置の単位は utf-8 と utf-16 を申し出る。rootUri は起こす
// 要求の root（設定の root か最初に開いた文書のフォルダ）の URI（ADR 0161 の決定 1）。能力は
// ADR 0190 の決定 3 と、`window.workDoneProgress`（ADR 0160 の 2026-11-22 の追補）。
[[nodiscard]] core::JsonValue initialize_params(const std::string &root_uri)
{
    core::JsonObject general;
    general.emplace_back("positionEncodings", text_list({"utf-8", "utf-16"}));
    // 進み具合の `$/progress` を受ける（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。
    core::JsonObject window;
    window.emplace_back("workDoneProgress", core::JsonValue{true});
    core::JsonObject capabilities;
    capabilities.emplace_back("general", core::JsonValue{std::move(general)});
    capabilities.emplace_back("textDocument", text_document_capabilities());
    capabilities.emplace_back("window", core::JsonValue{std::move(window)});
    core::JsonObject params;
    params.emplace_back("processId", core::JsonValue{core::JsonNull{}});
    params.emplace_back("rootUri", core::JsonValue{root_uri});
    params.emplace_back("capabilities", core::JsonValue{std::move(capabilities)});
    return core::JsonValue{std::move(params)};
}

// `initialize` の答えの `capabilities`（無い・object でなければ nullptr）。
[[nodiscard]] const core::JsonObject *answered_capabilities(const core::JsonValue &result)
{
    const auto *object = core::json_object_of(result);
    return object != nullptr ? core::json_object_member(*object, "capabilities") : nullptr;
}

// `capabilities.positionEncoding`（決定 5）。"utf-8" のときだけ utf8 で、無い・ほかの値は LSP の
// 既定の utf16。
[[nodiscard]] core::LspPositionEncoding chosen_encoding(const core::JsonObject *capabilities)
{
    const auto *name = capabilities != nullptr
                           ? core::json_text_member(*capabilities, "positionEncoding")
                           : nullptr;
    return name != nullptr && *name == "utf-8" ? core::LspPositionEncoding::utf8
                                               : core::LspPositionEncoding::utf16;
}

// 能力の欄が true か object（`HoverOptions` など）なら持つ（ADR 0190 の決定 3）。
[[nodiscard]] bool provided(const core::JsonObject &capabilities, std::string_view key)
{
    const auto member = core::json_member(capabilities, key);
    if (!member.has_value())
    {
        return false;
    }
    const auto &value = member.value().get().value;
    const auto *flag = std::get_if<bool>(&value);
    return (flag != nullptr && *flag) || std::holds_alternative<core::JsonObject>(value);
}

// `TextDocumentSyncKind` の数（0 None・1 Full・2 Incremental）。知らない値は仕様の既定の none。
[[nodiscard]] DocumentSync sync_of_kind(std::optional<std::uint64_t> kind) noexcept
{
    constexpr std::uint64_t full_kind = 1;
    constexpr std::uint64_t incremental_kind = 2;
    return kind == std::optional{full_kind}          ? DocumentSync::full
           : kind == std::optional{incremental_kind} ? DocumentSync::incremental
                                                     : DocumentSync::none;
}

// `TextDocumentSyncOptions.save`（true か `SaveOptions`・`includeText` が true なら本文を添える）。
[[nodiscard]] SaveNotice save_of(const core::JsonObject &options)
{
    const auto member = core::json_member(options, "save");
    if (!member.has_value())
    {
        return SaveNotice::none;
    }
    const auto &value = member.value().get().value;
    if (const auto *flag = std::get_if<bool>(&value))
    {
        return *flag ? SaveNotice::plain : SaveNotice::none;
    }
    const auto *save = std::get_if<core::JsonObject>(&value);
    if (save == nullptr)
    {
        return SaveNotice::none;
    }
    const auto text = core::json_member(*save, "includeText");
    const auto *include = text.has_value() ? std::get_if<bool>(&text.value().get().value) : nullptr;
    return include != nullptr && *include ? SaveNotice::with_text : SaveNotice::plain;
}

// `capabilities.textDocumentSync`（ADR 0161 の 2026-11-20 の追補）。数の形は種類だけで save は
// none、`TextDocumentSyncOptions` は `change` と `save`。無ければ仕様の既定の none。
void read_document_sync(ServerAbilities &abilities, const core::JsonObject &capabilities)
{
    const auto member = core::json_member(capabilities, "textDocumentSync");
    if (!member.has_value())
    {
        return;
    }
    const core::JsonValue &value = member.value().get();
    if (const auto *options = core::json_object_of(value))
    {
        abilities.sync = sync_of_kind(core::json_count_member(*options, "change"));
        abilities.save = save_of(*options);
        return;
    }
    abilities.sync = sync_of_kind(core::json_count_of(value));
}

// `capabilities` → サーバーの能力（ADR 0190 の決定 3）。補完は `completionProvider` の object が
// あるときで、`triggerCharacters` の文字列だけを届いた順に持つ。
[[nodiscard]] ServerAbilities server_abilities(const core::JsonObject *capabilities)
{
    if (capabilities == nullptr)
    {
        return {};
    }
    ServerAbilities abilities{.completion = false,
                              .trigger_characters = {},
                              .hover = provided(*capabilities, "hoverProvider"),
                              .definition = provided(*capabilities, "definitionProvider"),
                              .sync = DocumentSync::none,
                              .save = SaveNotice::none};
    read_document_sync(abilities, *capabilities);
    const auto *completion = core::json_object_member(*capabilities, "completionProvider");
    if (completion == nullptr)
    {
        return abilities;
    }
    abilities.completion = true;
    const auto triggers = core::json_member(*completion, "triggerCharacters");
    const auto *list = triggers.has_value()
                           ? std::get_if<core::JsonArray>(&triggers.value().get().value)
                           : nullptr;
    for (const core::JsonValue &entry : list != nullptr ? *list : core::JsonArray{})
    {
        if (const auto *text = std::get_if<std::string>(&entry.value))
        {
            abilities.trigger_characters.push_back(*text);
        }
    }
    return abilities;
}

// `$/cancelRequest` の params（決定 6）。
[[nodiscard]] core::JsonValue cancel_params(std::int64_t id)
{
    core::JsonObject params;
    params.emplace_back("id", core::JsonValue{core::json_integer(id)});
    return core::JsonValue{std::move(params)};
}

// 起こせなかった理由 → 1 行の知らせ（ADR 0160 の 2026-11-20 の追補）。見つからない・絶対パスで
// ない・`.cmd` / `.bat` を分け、ほかは「起こせませんでした」。
[[nodiscard]] core::UiMessage start_failure_message(ServerFailure failure) noexcept
{
    switch (failure)
    {
    case ServerFailure::not_found:
        return core::UiMessage::notice_language_server_not_found;
    case ServerFailure::not_absolute:
        return core::UiMessage::notice_language_server_not_absolute;
    case ServerFailure::script_refused:
        return core::UiMessage::notice_language_server_script;
    case ServerFailure::cannot_start:
    case ServerFailure::pipe_failed:
    case ServerFailure::write_failed:
    case ServerFailure::read_failed:
    case ServerFailure::message_too_long:
    case ServerFailure::header_too_long:
        return core::UiMessage::notice_language_server_not_started;
    }
    return core::UiMessage::notice_language_server_not_started;
}

// 1 行の知らせ（ADR 0160 の決定 6）。言語の名前を置き場 {0} に入れる。
[[nodiscard]] core::DisplayText server_notice(core::UiMessage message, core::Language language,
                                              core::DisplayLanguage display_language)
{
    return core::DisplayText::parse(
               core::ui_text_filled(message, display_language, {core::language_name(language)}))
        .value();
}

// 要求の種類 → JSON-RPC の method（1 か所）。
[[nodiscard]] std::string_view lsp_method_of(LspRequestKind kind) noexcept
{
    switch (kind)
    {
    case LspRequestKind::initialize:
        return "initialize";
    case LspRequestKind::shutdown:
        return "shutdown";
    case LspRequestKind::completion:
        return "textDocument/completion";
    case LspRequestKind::hover:
        return "textDocument/hover";
    case LspRequestKind::definition:
        return "textDocument/definition";
    }
    return "initialize";
}

// 知らせの type → 記録の行の印（ADR 0160 の 2026-11-22 の追補）。
[[nodiscard]] LanguageServerLogKind log_kind_of(core::LspMessageType type) noexcept
{
    switch (type)
    {
    case core::LspMessageType::error:
        return LanguageServerLogKind::error;
    case core::LspMessageType::warning:
        return LanguageServerLogKind::warning;
    case core::LspMessageType::info:
        return LanguageServerLogKind::info;
    case core::LspMessageType::log:
        return LanguageServerLogKind::log;
    }
    return LanguageServerLogKind::log;
}

// 起動を待つ段（`initialize` の答えの前）。
[[nodiscard]] constexpr bool starting_phase(LanguageServerPhase phase) noexcept
{
    switch (phase)
    {
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
        return true;
    case LanguageServerPhase::idle:
    case LanguageServerPhase::running:
    case LanguageServerPhase::shutting_down:
        return false;
    }
    return false;
}

// 席の番号 → awake の印のビット。
[[nodiscard]] constexpr std::uint8_t slot_bit(LanguageServerSlot slot) noexcept
{
    return static_cast<std::uint8_t>(1U << slot_index(slot));
}

// 子が居て、まだ止めに向かっていない段（文書を受けられる席）。
[[nodiscard]] constexpr bool serving_phase(LanguageServerPhase phase) noexcept
{
    switch (phase)
    {
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
    case LanguageServerPhase::running:
        return true;
    case LanguageServerPhase::idle:
    case LanguageServerPhase::shutting_down:
        return false;
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------- 言語サーバー（ADR 0160）

// 起きているべき子と今の子を合わせる 1 本（決定 6・7・2026-11-20 の追補）。オフで子が居なければ
// 欄を 2 つ読むだけで抜ける（決定 9 の 1 打鍵の費用）。オンでは、アクティブな文書を受ける席が
// 無ければ起こすかを決め、running の席ごとに文書を合わせる。言語の違うタブ・命令の無い言語の
// タブへ移っても、どの席も止めない（段 S2）。
void EditorController::reconcile_language_server()
{
    auto &servers = language_servers_;
    const bool on = state_.settings().lsp;
    if (!on && servers.awake == 0)
    {
        return;
    }
    if (!on)
    {
        for (const LanguageServerSlot slot : language_server_slots)
        {
            shut_language_server(slot);
        }
        return;
    }
    const core::Language language = state_.language();
    hold_language_root(language);
    auto serving = serving_slot(language);
    if (!serving.has_value())
    {
        consider_language_server(language);
        serving = serving_slot(language);
    }
    if (serving.has_value())
    {
        link_of(serving.value()).used = ++servers.uses;
    }
    for (const LanguageServerSlot slot : language_server_slots)
    {
        if (link_of(slot).phase == LanguageServerPhase::running)
        {
            sync_language_server_documents(slot, serving == std::optional{slot});
        }
    }
}

// language の文書を受ける席（段 S2 の root の鍵）。子が居て止めに向かっていない同じ言語の席で、
// 実際に起きた root が今の文書に対していま決まる root（document_root・段 S3）と同じ席だけ
// （`:set root=.` が同じフォルダなら同じ席が受け続ける・#1916 のレビュー M1）。root を探す答えを
// 待つ間はどの席も受けない。パスも設定の root も無い文書は同じ言語のどの席も受ける（段 S2
// のまま）。 2 つ以上なら最後に使った席。
std::optional<LanguageServerSlot> EditorController::serving_slot(core::Language language) const
{
    const auto root = held_root(language);
    if (!root.has_value() && root.error() == LanguageRootGap::seeking)
    {
        return std::nullopt;
    }
    const auto &links = language_servers_.links;
    std::optional<LanguageServerSlot> best;
    for (const LanguageServerSlot slot : language_server_slots)
    {
        const LanguageServerLink &link = links.at(slot_index(slot));
        if (!serving_phase(link.phase) || link.language != std::optional{language} ||
            (root.has_value() &&
             !(link.root.has_value() && same_language_root(link.root.value(), root.value()))))
        {
            continue;
        }
        if (!best.has_value() || links.at(slot_index(best.value())).used < link.used)
        {
            best = slot;
        }
    }
    return best;
}

// アクティブな文書を受ける席のつながり（能力・位置の単位・段）。無ければ空のつながり。
const LanguageServerLink &EditorController::active_language_server() const
{
    if (language_servers_.awake == 0)
    {
        return language_servers_.idle;
    }
    const auto slot = serving_slot(state_.language());
    return slot.has_value() ? language_servers_.links.at(slot_index(slot.value()))
                            : language_servers_.idle;
}

LanguageServerLink &EditorController::link_of(LanguageServerSlot slot)
{
    return language_servers_.links.at(slot_index(slot));
}

// 言語の失敗の控え（無ければ 0 回で足す）。
LanguageServerTrouble &EditorController::trouble_of(core::Language language)
{
    auto &troubles = language_servers_.troubles;
    const auto found = std::ranges::find_if(troubles, [language](const auto &trouble)
                                            { return trouble.language == language; });
    if (found != troubles.end())
    {
        return *found;
    }
    return troubles.emplace_back(LanguageServerTrouble{.language = language});
}

bool EditorController::language_server_given_up(core::Language language) const
{
    return std::ranges::any_of(language_servers_.troubles, [language](const auto &trouble)
                               { return trouble.language == language && trouble.given_up; });
}

// `:set lsp` と設定の面でオンにしたとき（決定 7）。全部の言語の失敗の数と given_up を消し、
// 起こすかを決め直す。
void EditorController::resume_language_servers()
{
    language_servers_.troubles.clear();
    language_servers_.considered.reset();
}

// 受ける席が無いとき、この（言語・パス・設定の root）でまだ決めていなければ起こすかを決める（同じ
// 文書では打鍵ごとに設定を引き直さない・root が変われば決め直す・#1916 のレビュー M1）。空いた席が
// 無ければ最も前に使った席を譲らせ、空けば起こす（running の席は `shutdown` の答えの後の reconcile
// で起こす）。送れない文書（無題・上限を超えた本文）のためには譲らせず、決めたことにもしない
// （送れる形になった打鍵で決め直す・#1916 のレビュー R3）。
void EditorController::consider_language_server(core::Language language)
{
    auto &servers = language_servers_;
    auto key = std::tuple{language, state_.document().path, state_.settings().root};
    if (servers.considered == key)
    {
        return;
    }
    if (!free_slot().has_value() && !language_server_sendable())
    {
        return;
    }
    servers.considered = std::move(key);
    // 諦めた言語でも root は探す（同じ言語の生きている別の root の席が受けられる文書を受ける・#1944
    // のレビュー R3）。止めるのは新しい子を起こすことだけ。
    auto launch = wanted_language_server(language);
    if (!launch.has_value() || language_server_given_up(language))
    {
        return;
    }
    auto slot = free_slot();
    if (!slot.has_value())
    {
        yield_language_server();
        slot = free_slot();
    }
    if (slot.has_value())
    {
        start_language_server(slot.value(), language, std::move(launch).value());
    }
}

std::optional<LanguageServerSlot> EditorController::free_slot() const
{
    const auto &links = language_servers_.links;
    const auto found = std::ranges::find_if(
        language_server_slots, [&links](LanguageServerSlot slot)
        { return links.at(slot_index(slot)).phase == LanguageServerPhase::idle; });
    return found != language_server_slots.end() ? std::optional{*found} : std::nullopt;
}

// 席が満ちたとき（段 S2）。止めに向かっている席があればその終わりを待ち、無ければアクティブな
// 文書で最も前に使った席を `shutdown` して譲らせる（文書は `didClose`）。
void EditorController::yield_language_server()
{
    const auto &links = language_servers_.links;
    if (std::ranges::any_of(links, [](const LanguageServerLink &link)
                            { return link.phase == LanguageServerPhase::shutting_down; }))
    {
        return;
    }
    const auto oldest =
        std::ranges::min_element(language_server_slots, {}, [&links](LanguageServerSlot slot)
                                 { return links.at(slot_index(slot)).used; });
    shut_language_server(*oldest);
}

// 設定に書いた命令だけを起こす（決定 7）。root は document_root（段 S3）。命令が無い・読めない・
// root が決まらなければ値なしで、印を探す答えがまだなら頼みを出す（答えの合図で決め直す）。
std::optional<ServerLaunch> EditorController::wanted_language_server(core::Language language)
{
    if (!has_language_server_command(language))
    {
        return std::nullopt;
    }
    const auto root = document_root(language);
    if (!root.has_value())
    {
        if (root.error() == LanguageRootGap::seeking)
        {
            seek_language_root(language);
        }
        return std::nullopt;
    }
    return language_server_launch(language, root.value());
}

// 言語の名前で lsp_commands を引き、root で起こす要求にする（命令が無い・読めなければ値なし）。
std::optional<ServerLaunch>
EditorController::language_server_launch(core::Language language, const core::FilePath &root) const
{
    const auto &commands = state_.settings().lsp_commands;
    const std::string_view name = core::language_name(language);
    const auto command =
        std::ranges::find_if(commands, [name](const auto &entry) { return entry.first == name; });
    if (command == commands.end())
    {
        return std::nullopt;
    }
    return server_launch_of(command->second, root);
}

// 起こしてすぐ戻る（決定 1）。起こせなければ 1 行知らせてその言語だけ止まる（設定の誤りは何度
// 起こしても同じなので自動では起こし直さない・`:set lsp` で再開）。root は起こした作業フォルダ
// （受ける席の鍵）。
void EditorController::start_language_server(LanguageServerSlot slot, core::Language language,
                                             ServerLaunch launch)
{
    auto &servers = language_servers_;
    auto &link = link_of(slot);
    const core::FilePath root = launch.root;
    link.root_uri = document_uri(root);
    const auto started = ports_.language_server.start(slot, std::move(launch));
    if (!started)
    {
        trouble_of(language).given_up = true;
        state_ = state_.with_command_message(server_notice(start_failure_message(started.error()),
                                                           language, state_.display_language()));
        return;
    }
    link.phase = LanguageServerPhase::starting;
    link.language = language;
    link.pending.clear();
    link.root = root;
    link.used = ++servers.uses;
    servers.awake = static_cast<std::uint8_t>(servers.awake | slot_bit(slot));
}

// 前の子の答えは残さない（落ちた・再起動の道も止める道を通る・#1825 のレビュー R4）。消すのは
// アクティブな文書を受ける・開いた席のときだけで、ほかの言語の席が落ちても答えは残す（R1）。
void EditorController::drop_language_answers_of(LanguageServerSlot slot)
{
    if (serving_slot(state_.language()) != std::optional{slot} &&
        holding_slot() != std::optional{slot})
    {
        return;
    }
    const auto &answers = state_.language_answers();
    if (answers.completion.has_value() || answers.hover.has_value() ||
        answers.definition.has_value())
    {
        state_ = state_.with_language_answers({});
    }
}

// 子を待たずに止める（追補）。止める道（tick の「答えない」・`shutdown` の答えなし・答えの後・
// lsp をオフ・席を譲る・落ちた・窓を閉じる）は全部ここを通る。port の stop が返すその席の
// まだ集めていない出来事は、標準エラーと logMessage だけを記録に写してから捨てる（#1950 の
// レビュー M1）。同じ集めの残りは collect_language_server が同じ形で記録にだけ写す。
void EditorController::stop_language_server(LanguageServerSlot slot)
{
    auto &servers = language_servers_;
    auto &link = link_of(slot);
    const core::Language language = slot_language(slot);
    drop_language_answers_of(slot);
    close_language_server_documents(slot, false);
    for (const ServerEvent &event : ports_.language_server.stop(slot))
    {
        log_unread(slot, language, event);
    }
    // 標準エラーの改行の無い最後の行（落ちる前の 1 行など）を閉じる
    // （ADR 0160 の 2026-11-22 の追補）。
    servers.log.end_output(slot, language);
    link.phase = LanguageServerPhase::idle;
    link.language.reset();
    link.pending.clear();
    link.abilities = {};
    link.progress.clear();
    servers.considered.reset();
    servers.retired = true;
    servers.awake = static_cast<std::uint8_t>(servers.awake & ~slot_bit(slot));
    ++link.generation;
}

// `shutdown` を送り、答えで `exit` を送って止める（決定 6・追補）。`initialize` の答えの前は
// 何も送れないので、そのまま止める。
void EditorController::shut_language_server(LanguageServerSlot slot)
{
    auto &link = link_of(slot);
    switch (link.phase)
    {
    case LanguageServerPhase::idle:
    case LanguageServerPhase::shutting_down:
        return;
    case LanguageServerPhase::starting:
    case LanguageServerPhase::initializing:
        stop_language_server(slot);
        return;
    case LanguageServerPhase::running:
        drop_language_answers_of(slot);
        link.pending.clear();
        close_language_server_documents(slot, true);
        send_lsp_request(slot, LspRequestKind::shutdown, core::JsonValue{core::JsonNull{}});
        link.phase = LanguageServerPhase::shutting_down;
        return;
    }
}

// 要求 id は controller の通し番号で、席をまたいで一意（決定 5）。method は種類から引く。答えを
// 照合する文脈と共に、その席の答えを待つ列に載せる（ADR 0190 の決定 1）。
void EditorController::send_lsp_request(LanguageServerSlot slot, LspRequestKind kind,
                                        const core::JsonValue &params, PendingLspContext context)
{
    const std::int64_t id = language_servers_.next_id++;
    ports_.language_server.send(slot, core::lsp_request_body(id, lsp_method_of(kind), params));
    link_of(slot).pending.push_back(PendingLspRequest{id, kind, std::move(context)});
}

// 答えを待つ要求 1 つを取り消す通知（決定 6・ADR 0190 の決定 1）。列から外すのは呼び出し元。
void EditorController::send_lsp_cancel(LanguageServerSlot slot, std::int64_t id)
{
    ports_.language_server.send(slot,
                                core::lsp_notification_body("$/cancelRequest", cancel_params(id)));
}

// 子が終わった・話せない・答えない（決定 6）。止めて 1 行知らせ、その言語の上限までは同じ席・
// 同じ root で起こし直す。失敗の数は言語ごと（ほかの言語の席は止めない・段 S2）。
void EditorController::language_server_failed(LanguageServerSlot slot, core::UiMessage message)
{
    const auto &link = link_of(slot);
    const core::Language language = link.language.value_or(state_.language());
    const auto root = link.root;
    stop_language_server(slot);
    auto &trouble = trouble_of(language);
    ++trouble.failures;
    if (trouble.failures > language_server_restart_limit)
    {
        trouble.given_up = true;
        state_ = state_.with_command_message(server_notice(
            core::UiMessage::notice_language_server_given_up, language, state_.display_language()));
        return;
    }
    state_ =
        state_.with_command_message(server_notice(message, language, state_.display_language()));
    auto launch = root.has_value() ? language_server_launch(language, root.value()) : std::nullopt;
    if (launch.has_value())
    {
        start_language_server(slot, language, std::move(launch).value());
    }
}

// WorkCompleted の道（決定 1）。子が居なければ port を呼ばない。止めた席の同じ集めの残りと、
// 子の居ない席の出来事は読まず、標準エラーと logMessage だけを集めた時の言語で記録に写し
// （#1950 のレビュー M1）、その席の改行の無い残りを最後に閉じる。
void EditorController::collect_language_server()
{
    const auto &links = language_servers_.links;
    if (language_servers_.awake == 0)
    {
        return;
    }
    const auto events = ports_.language_server.collect();
    std::array<std::uint64_t, language_server_slot_limit> generations{};
    std::ranges::transform(links, generations.begin(),
                           [](const LanguageServerLink &link) { return link.generation; });
    std::array<core::Language, language_server_slot_limit> languages{};
    std::ranges::transform(language_server_slots, languages.begin(),
                           [this](LanguageServerSlot slot) { return slot_language(slot); });
    std::array<bool, language_server_slot_limit> unread{};
    for (const LanguageServerEvent &event : events)
    {
        const std::size_t index = slot_index(event.slot);
        if (links.at(index).generation != generations.at(index) ||
            links.at(index).phase == LanguageServerPhase::idle)
        {
            log_unread(event.slot, languages.at(index), event.event);
            unread.at(index) = true;
            continue;
        }
        std::visit([this, slot = event.slot](const auto &value)
                   { this->receive_server(slot, value); }, event.event);
    }
    for (const LanguageServerSlot slot : language_server_slots)
    {
        if (unread.at(slot_index(slot)))
        {
            language_servers_.log.end_output(slot, languages.at(slot_index(slot)));
        }
    }
}

namespace
{
// 本文が showMessage / logMessage の通知なら種類と文（#1950 のレビュー M1）。ほかは値なし。
[[nodiscard]] std::optional<std::pair<core::LspMessageType, std::string>>
unread_logged_message(std::string_view body)
{
    const auto value = core::parse_json(body);
    if (!value)
    {
        return std::nullopt;
    }
    const auto incoming = core::lsp_incoming(value.value());
    if (!incoming)
    {
        return std::nullopt;
    }
    const auto *notification = std::get_if<core::LspNotification>(&incoming.value());
    return notification == nullptr ? std::nullopt : core::lsp_logged_message(*notification);
}
} // namespace

void EditorController::log_unread(LanguageServerSlot slot, core::Language language,
                                  const ServerEvent &event)
{
    auto &log = language_servers_.log;
    if (const auto *output = std::get_if<ServerErrorOutput>(&event))
    {
        log.add_output(slot, language, output->bytes);
        return;
    }
    const auto *message = std::get_if<ServerMessage>(&event);
    const auto logged = message == nullptr ? std::nullopt : unread_logged_message(message->body);
    if (logged.has_value())
    {
        log.add_message(slot, language, log_kind_of(logged.value().first), logged.value().second);
    }
}

void EditorController::receive_server(LanguageServerSlot slot, const ServerStarted & /*started*/)
{
    auto &link = link_of(slot);
    if (link.phase != LanguageServerPhase::starting)
    {
        return;
    }
    link.phase = LanguageServerPhase::initializing;
    send_lsp_request(slot, LspRequestKind::initialize, initialize_params(link.root_uri));
}

// 読めない本文は捨てる（段 L0 は寿命の答えだけを読む）。
void EditorController::receive_server(LanguageServerSlot slot, const ServerMessage &message)
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
    std::visit([this, slot](const auto &item) { this->receive_lsp(slot, item); }, incoming.value());
}

void EditorController::receive_server(LanguageServerSlot slot, const ServerExited & /*exited*/)
{
    end_language_server(slot);
}

void EditorController::receive_server(LanguageServerSlot slot, const ServerFailed & /*failed*/)
{
    end_language_server(slot);
}

// 標準エラーの塊は記録へ足すだけ（ADR 0160 の 2026-11-22 の追補）。
void EditorController::receive_server(LanguageServerSlot slot, const ServerErrorOutput &output)
{
    language_servers_.log.add_output(slot, slot_language(slot), output.bytes);
}

core::Language EditorController::slot_language(LanguageServerSlot slot) const
{
    return language_servers_.links.at(slot_index(slot)).language.value_or(state_.language());
}

// 子の終わり（決定 6）。`shutdown` を送った後なら頼んだ終わりで、合わせ直す（譲った席ならここで
// 次の言語が起きる）。ほかは失敗。
void EditorController::end_language_server(LanguageServerSlot slot)
{
    if (link_of(slot).phase == LanguageServerPhase::shutting_down)
    {
        stop_language_server(slot);
        reconcile_language_server();
        return;
    }
    language_server_failed(slot, core::UiMessage::notice_language_server_ended);
}

// 答えはその席の答えを待つ列だけで引く（ほかの席の id と取り違えない）。
void EditorController::receive_lsp(LanguageServerSlot slot, const core::LspResponse &response)
{
    auto &link = link_of(slot);
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
    const PendingLspRequest request = *pending;
    link.pending.erase(pending);
    switch (request.kind)
    {
    case LspRequestKind::initialize:
        if (!response.result)
        {
            language_server_failed(slot, core::UiMessage::notice_language_server_not_started);
            return;
        }
        link.encoding = chosen_encoding(answered_capabilities(response.result.value()));
        link.abilities = server_abilities(answered_capabilities(response.result.value()));
        ports_.language_server.send(
            slot, core::lsp_notification_body("initialized", core::JsonValue{core::JsonObject{}}));
        link.phase = LanguageServerPhase::running;
        // 続けて失敗した回数なので、initialized まで届いたら 0 に戻す（決定 6・言語ごと）。
        trouble_of(link.language.value_or(state_.language())).failures = 0;
        return;
    case LspRequestKind::shutdown:
        ports_.language_server.send(
            slot, core::lsp_notification_body("exit", core::JsonValue{core::JsonNull{}}));
        stop_language_server(slot);
        reconcile_language_server();
        return;
    case LspRequestKind::completion:
    case LspRequestKind::hover:
    case LspRequestKind::definition:
        receive_answer(slot, request, response);
        settle_language_answers(request.kind);
        return;
    }
}

// サーバーからの通知（ADR 0161 の決定 5）。診断はその席が開いた文書の値へ写す。`$/progress` は
// 席の進み具合へ（ADR 0160 の 2026-11-22 の追補）。`window/showMessage` と `window/logMessage` は
// どの type も記録に置き（追補）、`window/showMessage` の Error と Warning だけ 1 行の知らせ
// （lsp_shown_notice）にする。ほかの通知は捨てる。
void EditorController::receive_lsp(LanguageServerSlot slot,
                                   const core::LspNotification &notification)
{
    if (notification.method == core::lsp_publish_diagnostics_method)
    {
        receive_diagnostics(slot, notification);
        return;
    }
    if (auto progress = core::lsp_progress_of(notification))
    {
        receive_progress(slot, std::move(progress).value());
        return;
    }
    if (const auto logged = core::lsp_logged_message(notification))
    {
        language_servers_.log.add_message(slot, slot_language(slot),
                                          log_kind_of(logged.value().first), logged.value().second);
    }
    auto notice = core::lsp_shown_notice(notification);
    if (notice.has_value())
    {
        state_ = state_.with_command_message(std::move(notice));
    }
}

// サーバーからの要求は MethodNotFound で断る（決定 5）。`window/workDoneProgress/create` だけは
// result: null で受ける（ADR 0160 の 2026-11-22 の追補・token は最初の begin で覚える）。
void EditorController::receive_lsp(LanguageServerSlot slot, const core::LspRequest &request)
{
    ports_.language_server.send(slot, request.method == core::lsp_progress_create_method
                                          ? core::lsp_null_result_body(request.id)
                                          : core::lsp_method_not_found_body(request.id));
}

namespace
{
// 1 通を受けた後の token の今の値（追補）。begin はそのまま、report は
// 前の値（previous）の題を残して文と百分率だけを直す（前の値の無い report は
// 値なし）、end は値なし（外す）。
[[nodiscard]] std::optional<core::LspProgressValue>
progress_after(const core::LspProgressValue *previous, core::LspProgressValue value)
{
    switch (value.kind)
    {
    case core::LspProgressKind::begin:
        return value;
    case core::LspProgressKind::report:
        if (previous == nullptr)
        {
            return std::nullopt;
        }
        value.title = previous->title;
        value.message = value.message.has_value() ? value.message : previous->message;
        value.percentage = value.percentage.has_value() ? value.percentage : previous->percentage;
        return value;
    case core::LspProgressKind::end:
        return std::nullopt;
    }
    return std::nullopt;
}
} // namespace

// token ごとの今の値（追補）。begin は前の同じ token を置き換えて後ろへ、report は題を残して文と
// 百分率だけを直して後ろへ（知らない token は捨てる）、end は外す。列は上限で最も前に動いたものを
// 忘れる。
void EditorController::receive_progress(LanguageServerSlot slot, core::LspProgressValue value)
{
    auto &progress = link_of(slot).progress;
    const auto found = std::ranges::find_if(progress, [&value](const core::LspProgressValue &entry)
                                            { return entry.token == value.token; });
    const bool known = found != progress.end();
    auto next = progress_after(known ? &*found : nullptr, std::move(value));
    if (known)
    {
        progress.erase(found);
    }
    if (!next.has_value())
    {
        return;
    }
    progress.push_back(std::move(next).value());
    if (progress.size() > language_server_progress_limit)
    {
        progress.erase(progress.begin());
    }
}

std::optional<core::DisplayText> EditorController::language_server_status() const
{
    if (language_servers_.awake == 0)
    {
        return std::nullopt;
    }
    const auto slot = serving_slot(state_.language());
    if (!slot.has_value())
    {
        return std::nullopt;
    }
    const LanguageServerLink &link = language_servers_.links.at(slot_index(slot.value()));
    const std::string_view name = core::language_name(state_.language());
    if (starting_phase(link.phase))
    {
        return ui_notice(core::UiMessage::status_language_server_starting, {name});
    }
    if (link.phase != LanguageServerPhase::running || link.progress.empty())
    {
        return std::nullopt;
    }
    return core::lsp_progress_text(name, link.progress.back());
}

bool EditorController::language_server_served() const
{
    return language_servers_.awake != 0 && serving_slot(state_.language()).has_value();
}

void EditorController::add_language_server_status(EditorFrame &result) const
{
    result.language_server_status = language_server_status();
    result.language_server_served = language_server_served();
}

// 記録の写しを無題のタブに開く（追補）。たまった出来事を先に集める（標準エラーは
// 合図を出さずにたまる）。置き方はファイルを開く道と同じ place_opened の 1 本（何も書いて
// いない無題なら置き換え、ほかは右に新しいタブ・#1950 のレビュー L4）。開いたタブは無題で
// 未変更なので、閉じても「保存しますか」を
// 聞かず、自動退避も書かない（save_state_of が saved・autosave_plan が keep）。
// 無題は言語が plain なので言語サーバーも起こさない。
void EditorController::open_language_server_log()
{
    collect_language_server();
    if (language_servers_.log.empty())
    {
        state_ = state_.with_command_message(
            ui_notice(core::UiMessage::notice_language_server_log_empty));
        return;
    }
    auto text = core::TextBuffer::from_utf8(language_servers_.log.text());
    if (!text)
    {
        return;
    }
    place_opened(std::move(text).value(),
                 Document{std::nullopt, core::TextEncoding::utf8, std::size_t{0}});
}

void EditorController::accept(const OpenLanguageServerLog &)
{
    open_language_server_log();
}

// 古さの合図（決定 6・追補）。子が居ないときは port を呼ばない。2 回の tick を越えた要求は
// 席ごとに捨てる。退く子は adapter に片付けを頼む。
void EditorController::accept(const LanguageServerTick &)
{
    repaint_ = false;
    auto &servers = language_servers_;
    if (servers.retired)
    {
        servers.retired = false;
        ports_.language_server.expire_retired();
    }
    for (const LanguageServerSlot slot : language_server_slots)
    {
        age_lsp_requests(slot);
    }
}

// 席 slot の答えを待つ要求の古さを 1 つ進め、2 回の tick を越えた要求を捨てる。
void EditorController::age_lsp_requests(LanguageServerSlot slot)
{
    auto &pending = link_of(slot).pending;
    std::vector<PendingLspRequest> stale;
    for (PendingLspRequest &request : pending)
    {
        ++request.ticks;
        if (request.ticks >= language_server_stale_ticks)
        {
            stale.push_back(request);
        }
    }
    std::erase_if(pending, [](const PendingLspRequest &request)
                  { return request.ticks >= language_server_stale_ticks; });
    for (const PendingLspRequest &request : stale)
    {
        expire_lsp_request(slot, request);
    }
}

// 古い要求 1 つ（決定 6）。`initialize` は `$/cancelRequest` を送って捨て、答えない子として
// 失敗に数える。`shutdown` は `exit` を送らずに止める（依頼書・追補）。補完・hover・定義へは
// `$/cancelRequest` を送って捨てるだけで、知らせは出さない（ADR 0190 の決定 1）。
void EditorController::expire_lsp_request(LanguageServerSlot slot, const PendingLspRequest &request)
{
    switch (request.kind)
    {
    case LspRequestKind::initialize:
        send_lsp_cancel(slot, request.id);
        repaint_ = true;
        language_server_failed(slot, core::UiMessage::notice_language_server_silent);
        return;
    case LspRequestKind::shutdown:
        stop_language_server(slot);
        reconcile_language_server();
        return;
    case LspRequestKind::completion:
    case LspRequestKind::hover:
    case LspRequestKind::definition:
        send_lsp_cancel(slot, request.id);
        return;
    }
}

// `:set lsp` `:set nolsp` `:set lsp?`（決定 7）。オンは止まった後の手での再開も兼ね、全部の言語の
// 失敗の数と given_up を消す。どの形も今の値を 1 行で知らせる。保存できなければ断る。
void EditorController::answer_lsp(core::ExLspRequest request)
{
    auto settings = state_.settings();
    switch (request)
    {
    case core::ExLspRequest::log:
        // `:LspLog` は設定を変えない（ADR 0160 の 2026-11-22 の追補）。
        open_language_server_log();
        return;
    case core::ExLspRequest::enable:
        settings.lsp = true;
        resume_language_servers();
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

// 窓が閉じていく（追補）。`shutdown` の答えは待てないので全部の席を止めるだけで、子は adapter の
// Job Object が片付ける。
void EditorController::end_language_server_session()
{
    for (const LanguageServerSlot slot : language_server_slots)
    {
        if (link_of(slot).phase != LanguageServerPhase::idle)
        {
            stop_language_server(slot);
        }
    }
}
} // namespace nenenib::application
