#pragma once

#include "PreviewPoint.hpp"
#include "SelectionAnchoring.hpp"

namespace nenenib::application
{
// 全面プレビューの描いた文字の上のマウス（ADR 0155 の改訂 18）。point は ui が最後に描いた枠の
// layout の hit test で求めた文字の位置（ARC-011）。collapse
// は押した点に空の選択を置き（選び始め）、 extend
// は選び始めた端を保って動く端をそこへ（ドラッグ・Shift を押しながらの押し）。使う人の
// 操作で、全面でなければ何もしない。
struct PointPreview
{
    core::PreviewPoint point;
    core::SelectionAnchoring anchoring;
};
} // namespace nenenib::application
