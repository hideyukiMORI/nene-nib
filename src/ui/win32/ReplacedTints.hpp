#pragma once
#include "BodyGlyphRun.hpp"
#include "ReplacedTint.hpp"

#include <dwrite.h>
#include <optional>
#include <span>
#include <vector>

namespace nenenib::ui::win32
{
// 置き換えた文字の範囲（UTF-16・前から順）を、保持した字形 run から切り抜きと描く run の区間に
// 写す（#531）。HitTestTextRange を範囲ごとに呼ばず、run を 1 回だけ前から歩く。
// - 保持に無い範囲（縦・横に画面外で集めなかった run の中）は返さない（見えていない）。
// - 1 つの切り抜きに描くのは、同じ行で横の範囲（origin と advance の和）が交わる run だけ。
// - 行の上端は uniform な行の高さ line_height の倍数（BodyTextLayouts::set_spacing）。
// 位置を引けない保持（右から左・横倒し・文字の対応が無い・文字の位置が前へ戻る）は nullopt で、
// 呼び手は今までの経路（HitTestTextRange）に戻る。
[[nodiscard]] std::optional<std::vector<ReplacedTint>>
replaced_tints(std::span<const BodyGlyphRun> runs, std::span<const DWRITE_TEXT_RANGE> ranges,
               float line_height);
} // namespace nenenib::ui::win32
