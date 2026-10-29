#pragma once

#include "ExplorerKeyKind.hpp"

namespace nenenib::application
{
// 面に焦点があるときに打った鍵（ADR 0162 の決定 4）。面が消えている・ルートが無いなら何もしない。
struct ExplorerKey
{
    ExplorerKeyKind key;
};
} // namespace nenenib::application
