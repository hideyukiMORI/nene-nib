#pragma once

#include "FilePath.hpp"

#include <vector>

namespace nenenib::application
{
// 閉じたファイルの履歴（ADR 0060 の決定 8）。files は新しい順で、同じファイルは 1 つだけ。
// 時刻は持たない。公開 aggregate（CPP-003）。
struct FileHistory
{
    std::vector<core::FilePath> files;
};
} // namespace nenenib::application
