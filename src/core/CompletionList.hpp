#pragma once

#include "CompletionCandidate.hpp"
#include "OffsetRange.hpp"

#include <vector>

namespace nenenib::core
{
// 補完の候補の列（ADR 0169 の決定 2）。prefix は本文の接頭辞の範囲（end はキャレット・空の接頭辞
// では begin == end）で、選んだ語はこの範囲と置き換える。candidates は向きの順（最初の Ctrl-n /
// Ctrl-p で入る語が先頭）で、重複は無く、completion_candidate_limit 個まで。
struct CompletionList
{
    OffsetRange prefix;
    std::vector<CompletionCandidate> candidates;
};
} // namespace nenenib::core
