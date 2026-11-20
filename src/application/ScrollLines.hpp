#pragma once

#include <cstdint>

namespace nenenib::application
{
// ホイールなどによる縦スクロール。正が下、負が上（ADR 0009 の決定 6）。parts は 1 行に満たない
// 量（1/core::scroll_fraction_units 行・lines と同じ向き・絶対値はその未満）で、高分解能の
// ホイールとタッチパッドの小さな delta を運ぶ（#1898）。text の表示の本文は端数として持ち、
// 端数を持たない送り先（格子・プレビュー）は controller が行に貯めてから送る。
struct ScrollLines
{
    std::int32_t lines;
    std::int32_t parts = 0;
};
} // namespace nenenib::application
