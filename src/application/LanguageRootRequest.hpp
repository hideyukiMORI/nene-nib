#pragma once

#include "AsciiCase.hpp"
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

// 同じ root のフォルダか（Windows のパスとして ASCII の大小を区別しない・folder_contains と同じ
// 畳み方・ファイルに触れない・#1944 のレビュー L4）。席の鍵と覚えの鍵が使う。
[[nodiscard]] inline bool same_language_root(const core::FilePath &left,
                                             const core::FilePath &right) noexcept
{
    return core::ascii_equal_ignoring_case(left.text(), right.text());
}

[[nodiscard]] inline bool same_language_root_request(const LanguageRootRequest &left,
                                                     const LanguageRootRequest &right) noexcept
{
    return left.language == right.language && same_language_root(left.folder, right.folder) &&
           left.ceiling.has_value() == right.ceiling.has_value() &&
           (!left.ceiling.has_value() ||
            same_language_root(left.ceiling.value(), right.ceiling.value()));
}
} // namespace nenenib::application
