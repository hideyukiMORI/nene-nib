#pragma once

#include "GuideContext.hpp"

#include <cstdint>

namespace nenenib::core
{
// 案内の配置への問い（ADR 0079・ADR 0078 の追補・#1122）。key_width は空の文書の案内の 3 行の
// 鍵の箱のうち最も広いものの幅（物理画素）で、ui が鍵の表示名を DirectWrite の layout で測って
// 左右の余白を足して渡す。core は字の幅を知らず、64 DIP より狭ければ 64 DIP の列にする。
struct GuideRequest
{
    GuideContext context;
    std::int32_t key_width;
};
} // namespace nenenib::core
