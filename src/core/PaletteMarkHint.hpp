#pragma once

#include <cstdint>

namespace nenenib::core
{
// 記号を検索欄の案内に出すか（ADR 0060 の 2026-11-22 の追補・#1958）。案内は「手で打って探す入口」
// だけを見せる。always はいつも、language_server は LSP がオン（設定の lsp）のときだけ（診断 `^`・
// LSP の語は LSP がオンの人にだけ見せる）、never は出さない（参照 `&`・記号 `%` は鍵で頼んだ答えを
// 見せる記号で、入口は鍵）。出さない記号も手で打てば出どころを選ぶ。
enum class PaletteMarkHint : std::uint8_t
{
    always,
    language_server,
    never
};
} // namespace nenenib::core
