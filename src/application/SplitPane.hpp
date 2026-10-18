#pragma once

#include "SplitLayout.hpp"

namespace nenenib::application
{
// 本文を 2 つの枠に分ける（ADR 0148 の決定 1・6）。もう 1 つの枠は今の文書を同じキャレットと
// スクロールで出す。既に 2 枠なら 1 行知らせて何もしない。none は何もしない。
struct SplitPane
{
    core::SplitLayout layout;
};
} // namespace nenenib::application
