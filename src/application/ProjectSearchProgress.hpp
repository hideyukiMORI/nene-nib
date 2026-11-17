#pragma once

#include "ProjectSearchPhase.hpp"

#include <cstddef>

namespace nenenib::application
{
// 全文検索の進み具合（ADR 0167 の決定 4・5・ADR 0189 の D96 の「探しています… N / M ファイル」）。
// searched は探し終えたファイルの数（飛ばしたファイルを含めない）、found は今までに見つけた探す
// ファイルの数（木を歩くにつれて増える・searched 以上）。どちらも要求からの累計で、batch ごとの
// 差ではない。値だけを運ぶ公開 aggregate（CPP-003）。
struct ProjectSearchProgress
{
    ProjectSearchPhase phase;
    std::size_t searched;
    std::size_t found;
};
} // namespace nenenib::application
