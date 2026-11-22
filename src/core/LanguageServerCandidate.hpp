#pragma once

#include <string_view>

namespace nenenib::core
{
// よく知られた言語サーバーの 1 つの候補（LSP 段 S4・#1945・ADR 0160 の決定 7 の追補）。program は
// 実行ファイルの名前（拡張子 `.exe` を除く）。package が空でなければ npm の包みで、program は
// その `package.json` の `bin` の名前（`.cmd` の入口は使わず、node.exe とスクリプトの形で書く）。
// arguments は命令の行の実行ファイル（npm ならスクリプト）の後ろに続ける引数（空白で区切った
// 語・`"` を含まない）。
struct LanguageServerCandidate
{
    std::string_view program;
    std::string_view package;
    std::string_view arguments;
};
} // namespace nenenib::core
