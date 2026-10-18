#pragma once

#include "AutosaveId.hpp"
#include "CaretAffinity.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VimWantedColumn.hpp"

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// もう 1 つの枠の受け身の表示（ADR 0148 の決定 2）。tab は帯の順でもパスでもない退避番号で
// 文書を名指しし（autosave_ids と同じ・閉じても再利用しない）、残りはその文書の上の表示の欄。
// 型は DocumentState の同じ欄と VimState.wanted_column に揃える。どの欄も単独で妥当な値なので
// 公開 aggregate（CPP-003）。比較は非メンバーで書く。
struct PaneView
{
    AutosaveId tab;
    core::Offset caret;
    core::CaretAffinity affinity = core::CaretAffinity::leading;
    core::LineNumber first_visible;
    std::size_t first_row = 0;
    float horizontal = 0;
    std::optional<core::VimWantedColumn> wanted_column;
};

[[nodiscard]] constexpr bool operator==(const PaneView &left, const PaneView &right) noexcept
{
    return left.tab == right.tab && left.caret == right.caret && left.affinity == right.affinity &&
           left.first_visible == right.first_visible && left.first_row == right.first_row &&
           left.horizontal == right.horizontal && left.wanted_column == right.wanted_column;
}
} // namespace nenenib::application
