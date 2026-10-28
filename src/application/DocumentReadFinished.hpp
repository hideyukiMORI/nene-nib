#pragma once

#include "FileFailure.hpp"
#include "OpenedDocument.hpp"

#include <cstdint>
#include <expected>

namespace nenenib::application
{
// 読み込みの終わり（ADR 0159 の決定 2・6）。成功は揃った文書、失敗は値（取消は結果を出さない）。
// 公開 aggregate（CPP-003）。
struct DocumentReadFinished
{
    std::uint64_t ticket;
    std::expected<OpenedDocument, FileFailure> document;
};
} // namespace nenenib::application
