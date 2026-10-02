#pragma once

#include "FilePath.hpp"
#include "FolderProgress.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// フォルダの列挙が読めた分（ADR 0062 の決定 6）。
// ticket は要求の券、files はフルパスで OS が返す順、progress は続きがあるか。
// 値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct FolderBatch
{
    std::uint64_t ticket;
    std::vector<core::FilePath> files;
    FolderProgress progress;
};
} // namespace nenenib::application
