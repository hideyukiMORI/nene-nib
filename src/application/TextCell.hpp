#pragma once

namespace nenenib::application
{
// 格子の 1 升の物理画素（ADR 0140 の決定 4・5）。width は升の幅、height は画面行の高さ。
// どちらも文字組みの境界（TextLayoutPort::cell）が測った値。
struct TextCell
{
    float width;
    float height;
};
} // namespace nenenib::application
