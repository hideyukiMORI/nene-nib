#pragma once

#include "ProjectSearchHit.hpp"
#include "ProjectSearchProgress.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// 全文検索が届けた分（ADR 0167 の決定 4・FolderBatch と同じ形）。ticket は要求の券、hits は木を
// 歩いた順の当たり（前の batch の続き・並べ替えない）、progress は要求からの累計の進み具合。
// 公開 aggregate（CPP-003）。
struct ProjectSearchBatch
{
    std::uint64_t ticket;
    std::vector<ProjectSearchHit> hits;
    ProjectSearchProgress progress;
};
} // namespace nenenib::application
