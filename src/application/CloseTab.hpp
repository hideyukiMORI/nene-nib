#pragma once

#include <cstddef>

namespace nenenib::application
{
// 帯の位置 index（0 始まり）のタブを閉じる（ADR 0056 の決定 6）。確認はしない（確認は ui）。
// 最後の 1 つなら状態を変えず、EditorFrame.closing を立てる（D22）。範囲の外なら何もしない。
struct CloseTab
{
    std::size_t index;
};
} // namespace nenenib::application
