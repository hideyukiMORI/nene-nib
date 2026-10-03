#pragma once

#include "FilePath.hpp"

#include <vector>

namespace nenenib::application
{
// 明示登録したファイル。登録順を保ち、使った順では動かさない（ADR 0063）。
struct FileBookmarks
{
    std::vector<core::FilePath> files;
};
} // namespace nenenib::application
