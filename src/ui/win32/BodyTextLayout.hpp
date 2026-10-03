#pragma once

#include "LayoutRect.hpp"

#include <dwrite.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 描画器だけが所有する本文の文字組み。本文の正本ではなく、全表示文字列と幾何が一致する
// ときだけ使うCOM資源。書式変更で全て破棄する（ADR 0069）。
struct BodyTextLayout
{
    std::string text;
    core::LayoutRect area;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
};
} // namespace nenenib::ui::win32
