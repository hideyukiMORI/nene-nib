#pragma once

#include "FilePath.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// 文書を読む要求（ADR 0159 の決定 1・ADR 0004 の不変の要求値）。ticket は application が進める
// 通し番号で、届いた結果がどの要求のものかを見分ける。maximum_bytes は application が決める上限
// （値の正本は core の 1 か所・ADR 0010 の決定 12）。公開 aggregate（CPP-003）。
struct DocumentReadRequest
{
    core::FilePath path;
    std::uint64_t ticket;
    std::size_t maximum_bytes;
};
} // namespace nenenib::application
