#pragma once

#include "LayoutRect.hpp"

#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 描画器の固定枠に閉じるステータスの文字組み（ADR 0070）。formatは同一性の比較だけに使い、
// UI書式を作り直す前に全枠を消す。所有するlayoutを外へ貸し出さない。
struct StatusTextLayout
{
    std::string text;
    core::LayoutRect area{};
    IDWriteTextFormat *format = nullptr;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
