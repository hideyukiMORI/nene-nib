#pragma once

namespace nenenib::ui::win32
{
// 表の行の横の寸法（#1883・描画先の物理画素）。view は表を見せる枠の幅（行の原点＝字下げの位置から
// 枠の右の余白まで）、width は表の幅（列の幅と左右の余白の和）。描く所・当たり・高さの部分和が
// preview_table_band の 1 本で作る。公開 aggregate（CPP-003）。
struct PreviewTableBand
{
    float view;
    float width;
};
} // namespace nenenib::ui::win32
