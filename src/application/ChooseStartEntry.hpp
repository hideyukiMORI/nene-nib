#pragma once

#include <cstddef>

namespace nenenib::application
{
// 起動画面の行のクリック（ADR 0172 の決定 4・追補 2026-11-11 の 5）。index は StartScreenView の
// entries の位置。ui は押した行と離した行が同じときだけ送る（ADR 0056 の決定 9 の形）。選びの有無に
// 依らず、その行を実行する。起動画面が出ていないか、範囲の外なら何もしない。
struct ChooseStartEntry
{
    std::size_t index;
};
} // namespace nenenib::application
