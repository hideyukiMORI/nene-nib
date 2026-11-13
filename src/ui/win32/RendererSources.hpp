#pragma once

#include "BodyTextLayouts.hpp"
#include "DeviceWarmup.hpp"
#include "DisplayLanguage.hpp"

namespace nenenib::ui::win32
{
// renderer が借りる合成ルートの持ち物。本文の文字組みの保持（ADR 0114）と、worker が device を
// 先に作る箱（ADR 0131）。窓は renderer を作るたびにこの 2 つと、そのときの表示言語を渡す
// （画面の文字の書式を最初から正しい locale で 1 回だけ作る・ADR 0123 の 2026-11-13 の追補の
// 決定 9・V-1598 の C-L5）。
struct RendererSources final
{
    BodyTextLayouts &text_layouts;
    DeviceWarmup &warmup;
    core::DisplayLanguage language;
};
} // namespace nenenib::ui::win32
