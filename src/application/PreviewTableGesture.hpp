#pragma once

#include <cstdint>

namespace nenenib::application
{
// 表の横の送りの手の動き（#1883）。wheel は Shift+縦のホイールと横のホイール（量は DIP・右が正）、
// press は棒の上の左の押し（つまみならつかみ、溝なら 1 枠ぶん送る）、drag はつかんだ間の動き、
// release は離し（捕まえを失ったときも）。
enum class PreviewTableGesture : std::uint8_t
{
    wheel,
    press,
    drag,
    release
};
} // namespace nenenib::application
