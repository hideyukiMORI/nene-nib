#pragma once

#include "TitleBarTarget.hpp"

#include <optional>

namespace nenenib::application
{
// マウスを載せた帯の要素（ADR 0056 の決定 3・9）。変わったときだけ ui が送り、窓を出たら nullopt。
// pressed は帯の上でボタンを押したとき（タブの × と中ボタン・#1789）。押した時点でタブの札を消す
// （Windows の標準のツールチップと同じ・D82）。載せた要素は target のまま。
struct PointTitleBar
{
    std::optional<core::TitleBarTarget> target;
    bool pressed = false;
};
} // namespace nenenib::application
