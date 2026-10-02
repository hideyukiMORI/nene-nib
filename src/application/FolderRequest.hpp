#pragma once

#include "FilePath.hpp"

#include <cstdint>

namespace nenenib::application
{
// 同じフォルダを読む要求（ADR 0062 の決定 6・ADR 0004 の不変の要求値）。
// folder は直下を読むフォルダ。ticket は application が進める券で、
// 届いた batch がどの要求のものかを見分ける。公開 aggregate（CPP-003）。
struct FolderRequest
{
    core::FilePath folder;
    std::uint64_t ticket;
};
} // namespace nenenib::application
