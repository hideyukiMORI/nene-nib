#pragma once

#include "FilePath.hpp"

namespace nenenib::application
{
// 操作「ルートのフォルダを選ぶ」（ADR 0162 の決定 7）で ui のダイアログが選んだフォルダ。
// controller は `:set root=<path>` と同じく設定 root に書き、木を作り直す。使う人の操作。
struct ExplorerRootChosen
{
    core::FilePath folder;
};
} // namespace nenenib::application
