#pragma once

namespace nenenib::tests::performance
{
// #1125: エクスプローラーの面を出した状態の 1 打鍵の費用（ADR 0162 の追補）。shown は面を出して
// 1,000 件のフォルダを読んだ後、hidden は面を消した同じ打鍵（比べるための対）。
enum class ExplorerFrameWorkload
{
    typing_shown,
    typing_hidden,
};
} // namespace nenenib::tests::performance
