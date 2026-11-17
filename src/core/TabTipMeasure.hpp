#pragma once

#include <cstdint>

namespace nenenib::core
{
// タブの札の文を測った大きさ（物理画素）と DPI（#1789・ADR 0056 の #1789 の追補）。ui が
// DirectWrite で測った値を tab_tip_rect に渡す。3 つの欄は単独で妥当なので公開 aggregate
// （CPP-003）。
struct TabTipMeasure
{
    std::int32_t width;
    std::int32_t height;
    std::uint32_t dpi;
};
} // namespace nenenib::core
