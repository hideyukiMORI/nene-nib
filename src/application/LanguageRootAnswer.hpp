#pragma once

#include "FilePath.hpp"
#include "LanguageRootRequest.hpp"

#include <optional>

namespace nenenib::application
{
// root を探した答え 1 つ（ADR 0161 の 2026-11-22 の追補・段 S3）。request
// は頼んだ（フォルダ・言語）、 found は folder
// から上へ印のどれかがあった最初のフォルダ（無い・読めなければ値なし・controller
// は文書のフォルダへ落とす）。公開 aggregate（CPP-003）。
struct LanguageRootAnswer
{
    LanguageRootRequest request;
    std::optional<core::FilePath> found = std::nullopt;
};
} // namespace nenenib::application
