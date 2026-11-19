#pragma once

#include "PreviewTableGesture.hpp"
#include "PreviewTableSpan.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::application
{
// プレビューの枠より広い表を横に送る（#1883・施主決定 D107 の案 A）。table は表の通し番号
// （core::PreviewTableRow::table）、gesture は手の動き、at は wheel なら送る量（DIP・右が正）、
// press と drag なら点の溝の左端からの位置（DIP）、release では使わない。span は ui が最後に描いた
// 枠で測った表の寸法（列の幅を知るのは ui だけなので意図に添える・帯の幅 TitleBarWidth と同じ形）。
// 送りの量・つかみ・端への寄せは application が決め、ui は点の下の表と寸法を送るだけ。使う人の
// 操作（もう 1 つの枠の縦の送り ScrollPane と同じ側）。
struct ScrollPreviewTable
{
    std::size_t table;
    PreviewTableGesture gesture;
    std::int32_t at;
    core::PreviewTableSpan span;
};
} // namespace nenenib::application
