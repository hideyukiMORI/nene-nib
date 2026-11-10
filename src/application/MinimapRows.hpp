#pragma once

#include "MinimapBlock.hpp"
#include "MinimapWindow.hpp"

#include <cstdint>
#include <vector>

namespace nenenib::application
{
// ミニマップの窓の行の塊（ADR 0171 の決定 3・4）。controller の MinimapCache が 1 つ持ち、編集の
// たびに書き換えて serial を進める（窓を作り直すときは別の値を作る）。renderer は frame で借りて
// 描くときだけ読み、指す先の同一性と serial で自分の bitmap と比べる。rows は window の行の順。
// どの欄も単独で妥当な値なので公開 aggregate。
struct MinimapRows
{
    core::MinimapWindow window;
    std::vector<core::MinimapRow> rows;
    std::uint64_t serial;
};
} // namespace nenenib::application
