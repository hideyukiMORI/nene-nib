#pragma once

#include "FilePath.hpp"

#include <string>
#include <vector>

namespace nenenib::application
{
// 言語サーバーを起こす要求（ADR 0160 の決定 1）。executable は設定に書いた実行ファイルの経路、
// arguments は引数（adapter が 1 本の命令行に引用して並べる）、root は子の作業フォルダ。公開
// aggregate（CPP-003）。
struct ServerLaunch
{
    core::FilePath executable;
    std::vector<std::string> arguments;
    core::FilePath root;
};
} // namespace nenenib::application
