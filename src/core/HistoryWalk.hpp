#pragma once

#include "InputText.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::core
{
// 入力行の履歴の歩き（#725・ADR 0022 決定 2 の追補）。origin は歩きを始めたときの入力（接頭辞と
// 戻り先）、at は見せている履歴の位置（古い方から 0）。at が空なら origin を見せている。
struct HistoryWalk
{
    InputText origin;
    std::optional<std::size_t> at;
};
} // namespace nenenib::core
