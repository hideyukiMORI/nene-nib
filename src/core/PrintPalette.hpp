#pragma once

#include "Palette.hpp"
#include "SyntaxPalette.hpp"

namespace nenenib::core
{
// 印刷の配色（ADR 0177 の決定 4）。ui はヘッダ・行番号・置き換えた文字の色（muted・gutter）と
// 白地・黒字、body は構文の色（syntax_color が読む表）。どちらも light の既定テーマの値から作り、
// 新しいトークンは足さない。作るのは print_palette の 1 本。どの欄も単独で妥当な色なので公開
// aggregate。
struct PrintPalette
{
    Palette ui;
    SyntaxPalette body;
};
} // namespace nenenib::core
