#pragma once

#include "FilePath.hpp"
#include "StartEntryKind.hpp"

#include <optional>

namespace nenenib::core
{
// 起動画面の選べる行 1 つ（ADR 0172 の決定 2）。path は recent と bookmark だけが持ち、「はじめる」
// の 3 行は値なし。公開 aggregate（CPP-003）。
struct StartEntry
{
    StartEntryKind kind;
    std::optional<FilePath> path;
};
} // namespace nenenib::core
