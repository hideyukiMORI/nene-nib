#pragma once

#include <cstdint>

namespace nenenib::application
{
// 設定の面の行の領域の送りを置く（ホイールと、焦点の行を見える所へ寄せる追従・#1836）。top は上に
// 隠す高さ（物理画素）で、ui が配置（core::settings_layout の 1 本）から求めた値を送る。負は 0。
// 上限は配置が切り詰める（application は行の高さを知らない）。面が開いていなければ何もしない。
// 使う人の操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ScrollSettings
{
    std::int32_t top;
};
} // namespace nenenib::application
