#pragma once

#include "FilePath.hpp"
#include "FolderEntry.hpp"
#include "FolderProgress.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// フォルダの列挙が読めた分（ADR 0062 の決定 6）。
// ticket は要求の券、files はフルパスで OS が返す順、progress は続きがあるか。entries は要求が
// FolderListingKind::entries のときだけ埋まる直下の項目で（ADR 0162 の決定 5）、そのとき files
// は空。 値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct FolderBatch
{
    std::uint64_t ticket;
    std::vector<core::FilePath> files;
    FolderProgress progress;
    std::vector<FolderEntry> entries = {};
};
} // namespace nenenib::application
