#pragma once

#include "Language.hpp"
#include "LanguageServerCandidate.hpp"

#include <span>
#include <string_view>

namespace nenenib::core
{
// よく知られた言語サーバーの表の 1 行（LSP 段 S4・#1945）。title は設定の行の言語の名前（固有名で
// 訳さない）、languages はこの行が命令の行（`lsp.<name>.command`）を書く Nib の言語（C と C++ の
// ように 1 つのサーバーが受ける言語をまとめる）、candidates は探す順の候補（最初に見つかった物を
// 出す）、install は見つからないときに見せる入れ方の 1 行（命令そのもの・訳さない・空なら決まった
// 文で「入れて PATH に通す」と案内する）。
struct KnownLanguageServer
{
    std::string_view title;
    std::span<const Language> languages;
    std::span<const LanguageServerCandidate> candidates;
    std::string_view install;
};
} // namespace nenenib::core
