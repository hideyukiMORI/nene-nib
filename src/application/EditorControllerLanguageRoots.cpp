#include "EditorController.hpp"

#include "FileFolder.hpp"
#include "FilePath.hpp"
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

// アクティブな文書の root（ADR 0161 の 2026-11-22 の追補）。(a) 設定の root があり文書がその下に
// ある → 設定の root。(b) 文書のフォルダから上へ印を探した答えがあればそのフォルダ。(c) 探して
// 何も無ければ文書のフォルダ。答えをまだ覚えていなければ seeking。パスの無い文書は設定の root
// （あれば）か untitled。ファイルに触れない（覚えた答えを引くだけ）。
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
    if (configured.has_value() && core::folder_contains(configured.value(), path.value()))
    {
        return configured.value();
    }
    auto folder = core::folder_of(path.value());
    if (!folder.has_value())
    {
        return std::unexpected(LanguageRootGap::untitled);
    }
    const LanguageRootRequest request{std::move(folder).value(), language};
    const auto &roots = language_servers_.roots;
    const auto known =
        std::ranges::find_if(roots, [&request](const LanguageRootAnswer &answer)
                             { return same_language_root_request(answer.request, request); });
    if (known == roots.end())
    {
        return std::unexpected(LanguageRootGap::seeking);
    }
    return known->found.value_or(request.folder);
}

// 設定に language の命令の行があるか（命令の無い言語のためには root を探さない）。
bool EditorController::has_language_server_command(core::Language language) const
{
    const std::string_view name = core::language_name(language);
    return std::ranges::any_of(state_.settings().lsp_commands,
                               [name](const auto &entry) { return entry.first == name; });
}

// アクティブな文書の（フォルダ・言語）の印を探す頼みを 1 回だけ出す（答えを待つ間は 2
// 回出さない）。
void EditorController::seek_language_root(core::Language language)
{
    const auto &path = state_.document().path;
    auto folder = path.has_value() ? core::folder_of(path.value()) : std::nullopt;
    if (!folder.has_value())
    {
        return;
    }
    LanguageRootRequest request{std::move(folder).value(), language};
    auto &seeking = language_servers_.seeking;
    if (std::ranges::any_of(seeking, [&request](const LanguageRootRequest &asked)
                            { return same_language_root_request(asked, request); }))
    {
        return;
    }
    seeking.push_back(request);
    ports_.language_roots.find(std::move(request));
}

// WorkCompleted の道。頼んでいなければ port を呼ばない。答えは（フォルダ・言語）で覚え（上限を
// 超えたら古い方から捨てる）、起こすかを決め直す。言語サーバーの出来事を集めるより前に呼ぶので、
// 答えで起きた子の最初の出来事は同じ合図で読む。
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
    servers.considered.reset();
    reconcile_language_server();
}
} // namespace nenenib::application
