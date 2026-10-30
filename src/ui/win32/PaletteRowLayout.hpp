#pragma once

#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 面の行の 1 欄の保持（#900）。key は PaletteRowKey の欄（書式・役・DPI・欄の幅と高さ・文字列）を
// 1 本のバイト列に畳んだもので、保持の索引はこの文字列を借りて引く（#1125・O(1)）。
struct PaletteRowLayout final
{
    std::string key;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
