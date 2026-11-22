#pragma once

#include <string>

namespace nenenib::application
{
// 探す口に渡す 1 つの候補（LSP 段 S4・#1945）。program は実行ファイルの名前（`.exe` を除く）。
// package が空でなければ npm の包みの名前で、program はその `bin` の名前（adapter は `.cmd` の
// 入口を読まず、包みの `package.json` からスクリプトを引いて node.exe と組にする）。
struct LanguageServerProbe
{
    std::string program;
    std::string package;
};
} // namespace nenenib::application
