#pragma once

namespace nenenib::core
{
// JSON の null（ADR 0160 の決定 4）。JsonValue の選択肢の 1 つで、値を持たない。
struct JsonNull
{
};

[[nodiscard]] constexpr bool operator==(JsonNull /*left*/, JsonNull /*right*/) noexcept
{
    return true;
}
} // namespace nenenib::core
