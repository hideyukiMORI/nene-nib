#pragma once

#include "PreviewResult.hpp"

#include <cstdint>

namespace nenenib::application
{
// 1 つの要求の答え（ADR 0189 の決定 4）。券は要求の券。値だけを運ぶ公開 aggregate（CPP-003）。
struct PreviewBatch
{
    std::uint64_t ticket;
    PreviewResult lines;
};
} // namespace nenenib::application
