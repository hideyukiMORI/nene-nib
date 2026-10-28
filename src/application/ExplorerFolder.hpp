#pragma once

#include "ExplorerEntry.hpp"
#include "FilePath.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// エクスプローラーが頼んで読み終えていないフォルダの控え（ADR 0162 の決定 5）。ticket は要求の券
// （Ctrl+P の面と共用の通し番号）、folder は頼んだフォルダの経路（届いたときに木の節を経路で引き
// 直す）、entries は今の券で届いた分。最後の batch で木へ置いて控えを消す。公開
// aggregate（CPP-003）。
struct ExplorerFolder
{
    std::uint64_t ticket;
    core::FilePath folder;
    std::vector<core::ExplorerEntry> entries;
};
} // namespace nenenib::application
