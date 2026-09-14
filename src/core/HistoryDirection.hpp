#pragma once

#include <cstdint>

namespace nenenib::core
{
// 履歴をどちらへ動かすか。Ctrl+Z と Ctrl+Y が写る閉じた集合（CPP-002）。
enum class HistoryDirection : std::uint8_t
{
    undo,
    redo
};
} // namespace nenenib::core
