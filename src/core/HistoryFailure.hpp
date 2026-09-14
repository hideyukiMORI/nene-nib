#pragma once

#include <cstdint>

namespace nenenib::core
{
// 履歴の端では「戻せない」が期待される結果である。例外にしない（ARC-010 / CPP-005）。
enum class HistoryFailure : std::uint8_t
{
    nothing_to_undo,
    nothing_to_redo
};
} // namespace nenenib::core
