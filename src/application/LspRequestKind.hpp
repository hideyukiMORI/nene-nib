#pragma once

#include <cstdint>

namespace nenenib::application
{
// controller が送って答えを待つ要求の種類（ADR 0160 の決定 5・6）。段 L0 は寿命の 2 つだけで、
// 答えの写し先はこの種類で決める。増えたら写し先の switch がコンパイルで落ちる（CPP-002）。
enum class LspRequestKind : std::uint8_t
{
    initialize,
    shutdown
};
} // namespace nenenib::application
