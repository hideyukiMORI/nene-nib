#pragma once

#include <cstdint>

namespace nenenib::application
{
// 窓が閉じていく理由（ADR 0059 の決定 3）。window_closed は窓を閉じる・OS の終了で、開いている
// タブのうちパスのあるものを全部覚える。last_tab_closed は使う人が最後の 1 本を閉じた（D22）ので
// 空の一覧を覚える。
enum class SessionEnd : std::uint8_t
{
    window_closed,
    last_tab_closed
};
} // namespace nenenib::application
