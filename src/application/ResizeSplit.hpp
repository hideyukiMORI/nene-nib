#pragma once

#include "SplitShare.hpp"

namespace nenenib::application
{
// 2 枠の境界をドラッグで動かす（ADR 0148 の 2026-11-22 の追補・#1941）。使う人の操作。share は窓が
// core::split_share_at で範囲に収めた比で、ダブルクリックは等分（SplitShare{}）、ドラッグ中の Esc
// は押す前の比を送る。controller は状態に置くだけで、ファイルにも設定にも書かない（窓を閉じるまで覚
// え、枠を閉じて分け直すと等分）。1 枠なら何もしない。
struct ResizeSplit
{
    core::SplitShare share;
};
} // namespace nenenib::application
