#pragma once

#include <cstdint>

namespace nenenib::core
{
// 本文が保存済みかどうか。履歴の位置と保存時点の位置の比較から導く（ADR 0010 の決定 7）。
enum class SaveState : std::uint8_t
{
    saved,
    modified
};
} // namespace nenenib::core
