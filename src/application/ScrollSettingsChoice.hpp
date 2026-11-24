#pragma once

#include <cstddef>

namespace nenenib::application
{
// 開いている選ぶ欄の一覧の送り（いちばん上に見える選択肢の位置）を top へ置く（#1926・
// ホイールと、選ぼうとしている行を見える所へ寄せる追従）。値は ui が配置
// （core::settings_choice_layout の 1 本）から求める。列の外は最後の位置へ切り詰め、
// 見える行の数での上限は配置が切り詰める。一覧が開いていなければ何もしない。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ScrollSettingsChoice
{
    std::size_t top;
};
} // namespace nenenib::application
