#pragma once

#include <cstddef>
#include <optional>

namespace nenenib::application
{
// マウスを載せた面の行（#1390・ADR 0056 の決定 3 と同じ形の PointTitleBar の写し）。line は見えて
// いる行の列（ExplorerView.rows）の位置で、ui は first_visible を足さない（画面の位置で持つ）。
// ルート名の行と面の外は値なし。変わったときだけ ui が送り、窓を出たら値なし。使う人の操作では
// ない（not_user_operation）。
struct HoverExplorer
{
    std::optional<std::size_t> line;
};
} // namespace nenenib::application
