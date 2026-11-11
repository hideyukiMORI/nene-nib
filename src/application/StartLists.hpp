#pragma once

#include "FilePath.hpp"

#include <vector>

namespace nenenib::application
{
// 起動画面の 2 つの節の材料（ADR 0172 の決定 2・3）。recent は閉じたファイルの履歴の新しい順、
// bookmarks は登録順で、どちらも読んだまま（件数は core の start_entries が切る）。読めなかった方は
// 空。公開 aggregate（CPP-003）。
struct StartLists
{
    std::vector<core::FilePath> recent;
    std::vector<core::FilePath> bookmarks;
};
} // namespace nenenib::application
