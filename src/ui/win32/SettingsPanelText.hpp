#pragma once

#include "DisplayLanguage.hpp"

#include <cstdint>
#include <dwrite.h>

namespace nenenib::ui::win32
{
// 設定の面の文字組みを作る道具（施主決定 D102・#1836）。書式は SettingsPanelFormats が作る 7 つで、
// どれも段落の中央・折り返さない・左寄せ（中央と右寄せは painter が幅を測って置く）。
// - title は見出し「設定」（17 DIP の semibold）。strong は行の名前と要約の 1 行目（13 DIP の
//   semibold）。heading は節の見出しと段の印の語（11.5 DIP の bold）。note は説明・要約の 2 行目・
//   下の帯の文（11.5 DIP）。label は札・組・ボタン・段の上下の値（12 DIP の semibold）。value は
//   選ぶ欄の値と探す欄の文字（12.5 DIP）。mono は `:set` の名前・鍵の表示・命令の行（等幅 11
//   DIP）。
// - dpi は保持の鍵と寸法の倍率。language は文を引く言語（frame の値）。
// 書式は借用で、持ち主は renderer（試験では試験の関数）。公開 aggregate（CPP-003）。
struct SettingsPanelText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *title;
    IDWriteTextFormat *strong;
    IDWriteTextFormat *heading;
    IDWriteTextFormat *note;
    IDWriteTextFormat *label;
    IDWriteTextFormat *value;
    IDWriteTextFormat *mono;
    std::uint32_t dpi;
    core::DisplayLanguage language;
};
} // namespace nenenib::ui::win32
