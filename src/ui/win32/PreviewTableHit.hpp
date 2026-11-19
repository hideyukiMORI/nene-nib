#pragma once

#include "PreviewTableSpan.hpp"

#include <cstddef>
#include <cstdint>

namespace nenenib::ui::win32
{
// 点の下の枠より広い表（#1883）。table は表の通し番号、span は最後に描いた枠の表の寸法（DIP・
// 意図に添える）、at は点の溝の左端からの位置（DIP・溝の外は溝の外の値のまま）、bar は点が棒の帯
// （最後の行の下の空きと棒）の上か。公開 aggregate（CPP-003）。
struct PreviewTableHit
{
    std::size_t table;
    core::PreviewTableSpan span;
    std::int32_t at;
    bool bar;
};
} // namespace nenenib::ui::win32
