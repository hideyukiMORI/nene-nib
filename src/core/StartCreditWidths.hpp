#pragma once

#include <cstdint>

namespace nenenib::core
{
// 起動画面の名乗りの文の幅（物理画素・描画が 1 行のまま測った値・D90）。title は題「NeNe Nib」、
// version は版、copyright と homepage は署名の 2 行。測れなければ 0（その文は描かない）。公開
// aggregate（CPP-003）。
struct StartCreditWidths
{
    std::int32_t title = 0;
    std::int32_t version = 0;
    std::int32_t copyright = 0;
    std::int32_t homepage = 0;
};
} // namespace nenenib::core
