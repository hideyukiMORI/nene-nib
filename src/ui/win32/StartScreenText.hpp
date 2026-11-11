#pragma once

#include "KeycapStyle.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string_view>

namespace nenenib::ui::win32
{
// 起動画面の文字組みを作る道具（ADR 0172 の決定 5）。dwrite は layout を作る factory、title は題
// （UI の字体・本文の字の 1.5 倍）、item は項目（本文の字体と大きさ・文字単位の省略記号）、heading
// は節の見出しと空の知らせ（UI の字体・左寄せ）、side は「はじめる」の右端の補足（UI の字体・
// 右寄せ）、key と key_label は鍵の枠の字と説明（本文の案内と同じ書式）、keycap は鍵の枠の寸法と
// 色（本文の案内と同じ値）、dpi は保持の鍵と寸法の倍率。書式はどれも借用で、持ち主は renderer
// （試験では試験の関数）。公開 aggregate（CPP-003）。
struct StartScreenText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *title;
    IDWriteTextFormat *item;
    IDWriteTextFormat *heading;
    IDWriteTextFormat *side;
    IDWriteTextFormat *key;
    IDWriteTextFormat *key_label;
    KeycapStyle keycap;
    std::string_view open_key;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
