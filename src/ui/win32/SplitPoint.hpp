#pragma once

#include "SplitExtent.hpp"
#include "SplitShare.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// 境界の押しの道（split_step）が 1 通のメッセージで読む点と今の寸法（ADR 0148 の 2026-11-22
// の追補の 決定 3）。coordinate は点の分割の向きの座標（client の物理画素）、start
// は点の下の見えている境界の
// 始まり（split_divider_start_under・押しとダブルクリックのときだけ値）、extent は今の寸法（押して
// いる間の動きと離しのときだけ値・1 枠なら値なし）、current は今の比。どの欄も単独で妥当な値なので
// 公開 aggregate（CPP-003）。
struct SplitPoint
{
    std::int32_t coordinate = 0;
    std::optional<std::int32_t> start = std::nullopt;
    std::optional<core::SplitExtent> extent = std::nullopt;
    core::SplitShare current{};
};
} // namespace nenenib::ui::win32
