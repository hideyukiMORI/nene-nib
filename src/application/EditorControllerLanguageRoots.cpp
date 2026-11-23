#include "EditorController.hpp"

#include "FileFolder.hpp"
#include "FilePath.hpp"
#include "HeldLanguageRoot.hpp"
#include "Language.hpp"
#include "LanguageRootAnswer.hpp"
#include "LanguageRootGap.hpp"
#include "LanguageRootRequest.hpp"
#include "LanguageServers.hpp"
#include "LanguageTable.hpp"

#include <algorithm>
#include <expected>
#include <optional>
#include <string_view>
#include <utility>

namespace nenenib::application
{
// ---------------------------------------------------------------- 言語サーバーの root（段 S3）

namespace
{
// path の文書の root を探す頼み（文書のフォルダから・#1960）。文書が設定の root の下にあれば設定の
// root を上限にする（開いたフォルダより上へ出ない）。フォルダの無いパスは値なし。document_root と
// seek_language_root が同じ頼みを作る 1 本。
[[nodiscard]] std::optional<LanguageRootRequest>
language_root_request(const std::optional<core::FilePath> &configured, const core::FilePath &path,
                      core::Language language)
{
    auto folder = core::folder_of(path);
    if (!folder.has_value())
    {
        return std::nullopt;
    }
    auto ceiling = configured.has_value() && core::folder_contains(configured.value(), path)
                       ? configured
                       : std::nullopt;
    return LanguageRootRequest{std::move(folder).value(), language, std::move(ceiling)};
}
} // namespace

// アクティブな文書の root（ADR 0161 の 2026-11-22 の追補と #1960 の追補）。文書のフォルダから上へ
// 印を探した答えがあればそのフォルダ（設定の root があり文書がその下にあれば、設定の root より
// 上へは出ない・直下の文書は探さずに設定の root）。探して何も無ければ、設定の root の下の文書は
// 設定の root、ほかは文書のフォルダ。
// 答えをまだ覚えていなければ seeking。パスの無い文書は設定の root（あれば）か untitled。ファイルに
// 触れない（覚えた答えを引くだけ）。
std::expected<core::FilePath, LanguageRootGap>
EditorController::document_root(core::Language language) const
{
    const auto &configured = state_.settings().root;
    const auto &path = state_.document().path;
    if (!path.has_value())
    {
        if (configured.has_value())
        {
            return configured.value();
        }
        return std::unexpected(LanguageRootGap::untitled);
    }
    const auto request = language_root_request(configured, path.value(), language);
    if (!request.has_value())
    {
        return std::unexpected(LanguageRootGap::untitled);
    }
    // 文書が設定の root の直下にあれば、印があってもなくても root は設定の root（探さない）。
    // 直下かは core の same_folder（folder_contains と同じ畳み方・`C:\work\` `C:/work` も直下）。
    const auto &ceiling = request.value().ceiling;
    if (ceiling.has_value() && core::same_folder(ceiling.value(), request.value().folder))
    {
        return ceiling.value();
    }
    const auto &roots = language_servers_.roots;
    const auto known = std::ranges::find_if(
        roots, [&request](const LanguageRootAnswer &answer)
        { return same_language_root_request(answer.request, request.value()); });
    if (known == roots.end())
    {
        return std::unexpected(LanguageRootGap::seeking);
    }
    return known->found.value_or(ceiling.value_or(request.value().folder));
}

// held が今の（言語・文書のパス・設定の root・覚えの改訂）と同じ鍵か。
bool EditorController::holds_language_root(core::Language language) const
{
    const auto &held = language_servers_.held;
    const auto &path = state_.document().path;
    return held.has_value() && path.has_value() && held.value().language == language &&
           held.value().answers == language_servers_.answers && held.value().path == path.value() &&
           held.value().configured == state_.settings().root;
}

// 1 回決めた root が今も同じ鍵なら読み、違えば決め直す（打鍵ごとの費用は鍵の比べだけ・L1）。
std::expected<core::FilePath, LanguageRootGap>
EditorController::held_root(core::Language language) const
{
    const auto &held = language_servers_.held;
    if (held.has_value() && holds_language_root(language))
    {
        return held.value().root;
    }
    return document_root(language);
}

// reconcile の入口。鍵が変わったときだけ document_root で決め直して持つ（パスの無い文書と、まだ
// 決まらない文書は持たない）。
void EditorController::hold_language_root(core::Language language)
{
    if (holds_language_root(language))
    {
        return;
    }
    auto &held = language_servers_.held;
    held.reset();
    const auto &path = state_.document().path;
    auto root = document_root(language);
    if (!path.has_value() || !root.has_value())
    {
        return;
    }
    held = HeldLanguageRoot{.language = language,
                            .path = path.value(),
                            .configured = state_.settings().root,
                            .answers = language_servers_.answers,
                            .root = std::move(root).value()};
}

// 設定に language の命令の行があるか（命令の無い言語のためには root を探さない）。
bool EditorController::has_language_server_command(core::Language language) const
{
    const std::string_view name = core::language_name(language);
    return std::ranges::any_of(state_.settings().lsp_commands,
                               [name](const auto &entry) { return entry.first == name; });
}

// アクティブな文書の（フォルダ・言語・上限）の印を探す頼みを 1 回だけ出す（答えを待つ間は 2
// 回出さない）。
void EditorController::seek_language_root(core::Language language)
{
    const auto &path = state_.document().path;
    auto made = path.has_value()
                    ? language_root_request(state_.settings().root, path.value(), language)
                    : std::nullopt;
    if (!made.has_value())
    {
        return;
    }
    LanguageRootRequest request = std::move(made).value();
    auto &seeking = language_servers_.seeking;
    if (std::ranges::any_of(seeking, [&request](const LanguageRootRequest &asked)
                            { return same_language_root_request(asked, request); }))
    {
        return;
    }
    seeking.push_back(request);
    ports_.language_roots.find(std::move(request));
}

// WorkCompleted の道。頼んでいなければ port を呼ばない。答えは（フォルダ・言語・探す上限）で
// 覚え（数の上限を超えたら古い方から捨てる）、起こすかを決め直す。言語サーバーの出来事を集める
// より前に呼ぶので、答えで起きた子の最初の出来事は同じ合図で読む。
void EditorController::collect_language_roots()
{
    auto &servers = language_servers_;
    if (servers.seeking.empty())
    {
        return;
    }
    auto answers = ports_.language_roots.collect();
    if (answers.empty())
    {
        return;
    }
    for (LanguageRootAnswer &answer : answers)
    {
        std::erase_if(servers.seeking, [&answer](const LanguageRootRequest &asked)
                      { return same_language_root_request(asked, answer.request); });
        std::erase_if(servers.roots, [&answer](const LanguageRootAnswer &known)
                      { return same_language_root_request(known.request, answer.request); });
        if (servers.roots.size() >= language_root_memory)
        {
            servers.roots.erase(servers.roots.begin());
        }
        servers.roots.push_back(std::move(answer));
    }
    ++servers.answers;
    servers.considered.reset();
    reconcile_language_server();
}
} // namespace nenenib::application
