#pragma once

#include <cstddef>

namespace nenenib::application
{
// 開いている選ぶ欄の一覧の「選ぼうとしている位置」を choice へ置く（#1926）。鍵の ↑ ↓・
// Home / End・PgUp / PgDn（端で止まる・回らない）とマウスを載せた行。行き先は ui が
// 表示値から求める。列の外は最後の位置へ切り詰める。値は変えない（決める前に見せる
// プレビューは入れない・採用案）。一覧が開いていなければ何もしない。使う人の操作なので
// keeps_tab_walk にも begin_intent の表にも書かない。
struct PointSettingsChoice
{
    std::size_t choice;
};
} // namespace nenenib::application
