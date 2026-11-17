#pragma once

#include "Column.hpp"

#include <cstdint>
#include <string>

namespace nenenib::core
{
// 見えている 1 行の上のイージーモーションの印（ADR 0163 の決定 6）。column は跳び先の文字の桁
// （LineView.colors と同じ code point の序数・行頭が Column{1}）、label はまだ打っていない印の
// 文字（打った接頭辞を除いた残り）、cells は跳び先の文字の升の数（全角は 2）。札は跳び先の左肩に
// 置く大きさの決まった札なので、renderer は cells を札の幅に使わない（D99・ADR 0163 の追補）。
// 値は単独で妥当な公開 aggregate（CPP-003）。
struct MotionLabel
{
    Column column;
    std::string label;
    std::uint8_t cells;
};

[[nodiscard]] inline bool operator==(const MotionLabel &left, const MotionLabel &right)
{
    return left.column == right.column && left.label == right.label && left.cells == right.cells;
}
} // namespace nenenib::core
