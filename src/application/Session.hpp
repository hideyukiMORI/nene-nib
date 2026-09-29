#pragma once

#include "SessionTab.hpp"

#include <cstddef>
#include <vector>

namespace nenenib::application
{
// 前回のタブ（ADR 0059 の決定 1）。tabs は帯の順でパスのあるタブだけ（無題は入れない・D9）、
// active は tabs の中の位置（一覧が空なら 0）。公開 aggregate（CPP-003）。
struct Session
{
    std::vector<SessionTab> tabs;
    std::size_t active;
};
} // namespace nenenib::application
