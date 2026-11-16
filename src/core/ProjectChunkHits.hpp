#pragma once

#include "ProjectHit.hpp"
#include "ProjectScan.hpp"

#include <vector>

namespace nenenib::core
{
// 塊 1 つを照合した答え（ADR 0167 の決定 3）。hits は行の順、next は次の塊へ渡す状態。
struct ProjectChunkHits
{
    std::vector<ProjectHit> hits;
    ProjectScan next;
};
} // namespace nenenib::core
