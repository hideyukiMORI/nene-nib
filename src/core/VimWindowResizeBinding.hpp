#pragma once

#include "PaneDimension.hpp"
#include "PaneResize.hpp"

namespace nenenib::core
{
// `Ctrl-w` の後ろの鍵 → 枠の大きさの変え方の表の 1 行（CPP-012・ADR 0148 の 2026-11-22 の追補の
// 決定 4）。回数が無いときは uncounted、あるときは counted（`_` `|` は回数があればその数ちょうど・
// 無ければ最大。ほかは同じ値で、回数は升の数）。
struct VimWindowResizeBinding
{
    char32_t key;
    PaneDimension dimension;
    PaneResize uncounted;
    PaneResize counted;
};
} // namespace nenenib::core
