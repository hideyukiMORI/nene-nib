#pragma once

#include "FilePath.hpp"
#include "Language.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::application
{
// アクティブな文書について 1 回決めた root（ADR 0161 の 2026-11-22 の追補・段 S3・#1944 のレビュー
// L1）。決めたときの言語・文書のパス・設定の root・覚えの改訂（answers）がどれも今と同じときだけ
// root を読み、どれかが違えば document_root で決め直す（打鍵ごとに文書のフォルダを作って覚えの列を
// 引かない）。公開 aggregate（CPP-003）。
struct HeldLanguageRoot
{
    core::Language language;
    core::FilePath path;
    std::optional<core::FilePath> configured;
    std::uint64_t answers;
    core::FilePath root;
};
} // namespace nenenib::application
