#pragma once

#include "KeycapStyle.hpp"

#include <cstdint>
#include <dwrite.h>
#include <string_view>

namespace nenenib::ui::win32
{
// 起動画面の文字組みを作る道具（ADR 0172 の決定 5・追補 2026-11-11 の 20〜22）。
// dwrite は layout を作る factory、title は題（UI の字体・本文の字の 1.5 倍）、
// item は行の字（名前・場所・「はじめる」の名前と補足・エクスプローラーの面と同じ書式）、
// heading は節の見出しと空の知らせ（UI の字体・左寄せ）、
// key と key_label は鍵の枠の字と説明（本文の案内と同じ書式）、
// keycap は鍵の枠の寸法と色（本文の案内と同じ値）、
// new_key・open_key・folder_key は「はじめる」の 3 行の鍵の表示名（操作の表の new_tab・open_file・
// choose_explorer_root を今のモードの shown_chord で引いた値・鍵が無ければ空で枠を描かない・Vim の
// 「ファイルを開く」は Ctrl+Shift+O・#1480）、
// dpi は保持の鍵と寸法の倍率。書式はどれも借用で、持ち主は renderer（試験では試験の関数）。
// 公開 aggregate（CPP-003）。
struct StartScreenText
{
    IDWriteFactory *dwrite;
    IDWriteTextFormat *title;
    IDWriteTextFormat *item;
    IDWriteTextFormat *heading;
    IDWriteTextFormat *key;
    IDWriteTextFormat *key_label;
    KeycapStyle keycap;
    std::string_view new_key;
    std::string_view open_key;
    std::string_view folder_key;
    std::uint32_t dpi;
};
} // namespace nenenib::ui::win32
