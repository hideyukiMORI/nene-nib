#pragma once

#include "KeyMapEmit.hpp"
#include "KeyMapPass.hpp"
#include "KeyMapPending.hpp"

#include <variant>

namespace nenenib::core
{
// key_map_step の答えの閉じた和型（ADR 0176 の決定 2）。読む所は std::visit。
using KeyMapStep = std::variant<KeyMapEmit, KeyMapPending, KeyMapPass>;
} // namespace nenenib::core
