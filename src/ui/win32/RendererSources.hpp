#pragma once

#include "BodyTextLayouts.hpp"
#include "DeviceWarmup.hpp"

namespace nenenib::ui::win32
{
// renderer が借りる合成ルートの持ち物。本文の文字組みの保持（ADR 0114）と、worker が device を
// 先に作る箱（ADR 0131）。窓は renderer を作るたびにこの 2 つを渡す。
struct RendererSources final
{
    BodyTextLayouts &text_layouts;
    DeviceWarmup &warmup;
};
} // namespace nenenib::ui::win32
