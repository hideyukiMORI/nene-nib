#pragma once

#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"

#include <optional>

namespace nenenib::core
{
// ジャンプの列と変更の列を歩いた結果（ADR 0152 の追補 #1037）。marks は歩いた後のマーク（列の
// 片付け・今の位置の積み・番号の移動を含み、失敗でも片付けと積みは残る・Vim の movemark と同じ）。
// target は着く位置で、列の端を越える・列が空なら無い（Vim の E662 E663 E664 と鳴らすだけの失敗）。
// どちらも単独で妥当な公開 aggregate（CPP-003）。
struct VimJumpMove
{
    VimMarks marks;
    std::optional<VimMarkPosition> target;
};
} // namespace nenenib::core
