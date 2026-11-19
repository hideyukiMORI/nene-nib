#pragma once

#include <cstdint>

namespace nenenib::core
{
// 表の横の送りの寸法（#1883・DIP）。view は表を見せる枠の幅（字下げの位置から右の余白まで）、
// table は表の幅（列の幅と左右の余白の和）。測るのは ui で、application は意図に添えて受け取る
// （帯の幅 TitleBarWidth と同じく ui が測った寸法を渡す形）。どちらも単独で妥当な値なので公開
// aggregate（CPP-003）。
struct PreviewTableSpan
{
    std::int32_t view;
    std::int32_t table;
};
} // namespace nenenib::core
