#pragma once

#include "BodyLayout.hpp"
#include "BodyTextLayouts.hpp"
#include "EditorSettings.hpp"
#include "LayoutRect.hpp"
#include "PrintLine.hpp"
#include "PrintPage.hpp"
#include "PrintPalette.hpp"

#include <cstddef>
#include <dwrite.h>
#include <span>
#include <string_view>

namespace nenenib::ui::win32
{
// 1 ページを描く入力（ADR 0177 の決定 8）。print control の command list と tests/ui の WIC bitmap
// が同じ draw_print_page に渡す。座標は DIP（dpi 96）。page はこのページの画面行の範囲、number は
// 1 始まりのページ番号、count はページの総数。lines は文書の全行（first_row の昇順）。paper は紙の
// 全体、header はヘッダの 1 行、body は本文の帯（行番号の欄と本文）。layouts は印刷のために作った
// 使い捨ての文字組み（画面の保持とは別の値）、gutter は行番号の書式（行番号を描かないなら null）。
struct PrintPageInput
{
    core::PrintPage page;
    std::size_t number;
    std::size_t count;
    std::span<const PrintLine> lines;
    std::string_view title;
    core::LayoutRect paper;
    core::LayoutRect header;
    core::BodyLayout body;
    const core::EditorSettings &settings;
    BodyTextLayouts &layouts;
    IDWriteFactory &factory;
    IDWriteTextFormat *gutter;
    const core::PrintPalette &palette;
};
} // namespace nenenib::ui::win32
