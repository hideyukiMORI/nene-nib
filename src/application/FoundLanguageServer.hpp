#pragma once

#include "FilePath.hpp"

#include <optional>

namespace nenenib::application
{
// 探す口が見つけた 1 つの候補（LSP 段 S4・#1945）。executable は起こす実行ファイルの絶対パス
// （npm の物は node.exe）、script は npm の物のスクリプトの絶対パス（ほかは値なし）。
struct FoundLanguageServer
{
    core::FilePath executable;
    std::optional<core::FilePath> script;
};
} // namespace nenenib::application
