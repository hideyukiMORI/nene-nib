#pragma once

#include "FileFolder.hpp"
#include "FilePath.hpp"
#include "Language.hpp"

#include <optional>

namespace nenenib::application
{
// 言語サーバーの root を探す頼み 1 つ（ADR 0161 の 2026-11-22 の追補・段 S3）。folder は文書の
// フォルダ（探し始める所）、language は印の表を引く言語、ceiling はこれより上へ出ない上限（文書が
// 設定の root の下にあるときの設定の root・それ以外は無し・#1960）。公開 aggregate（CPP-003）。
struct LanguageRootRequest
{
    core::FilePath folder;
    core::Language language;
    std::optional<core::FilePath> ceiling;
};

// 覚えの鍵（フォルダ・言語・上限）が同じか。フォルダの比べは core の same_folder の 1 本（ASCII の
// 大小・`\` `/`・末尾の区切りを畳む・#1944 のレビュー L4・#1963 のレビュー M1 / R1）。
[[nodiscard]] inline bool same_language_root_request(const LanguageRootRequest &left,
                                                     const LanguageRootRequest &right) noexcept
{
    return left.language == right.language && core::same_folder(left.folder, right.folder) &&
           left.ceiling.has_value() == right.ceiling.has_value() &&
           (!left.ceiling.has_value() ||
            core::same_folder(left.ceiling.value(), right.ceiling.value()));
}
} // namespace nenenib::application
