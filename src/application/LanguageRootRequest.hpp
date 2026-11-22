#pragma once

#include "FilePath.hpp"
#include "Language.hpp"

namespace nenenib::application
{
// 言語サーバーの root を探す頼み 1 つ（ADR 0161 の 2026-11-22 の追補・段 S3）。folder は文書の
// フォルダ（探し始める所）、language は印の表を引く言語。公開 aggregate（CPP-003）。
struct LanguageRootRequest
{
    core::FilePath folder;
    core::Language language;
};

[[nodiscard]] inline bool same_language_root_request(const LanguageRootRequest &left,
                                                     const LanguageRootRequest &right) noexcept
{
    return left.language == right.language && left.folder.text() == right.folder.text();
}
} // namespace nenenib::application
