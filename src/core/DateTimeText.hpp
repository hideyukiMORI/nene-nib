#pragma once

#include "LocalDateTime.hpp"

#include <string>

namespace nenenib::core
{
// F5 で挿入する日時の文字（ADR 0175 の決定 2）。`HH:mm yyyy/MM/dd`（Windows のメモ帳の日本語の
// 既定）で、時・分・月・日は 2 桁、年は 4 桁に 0 で埋める。形はまず 1 つで、ロケールは読まない。
[[nodiscard]] std::string date_time_text(LocalDateTime value);
} // namespace nenenib::core
