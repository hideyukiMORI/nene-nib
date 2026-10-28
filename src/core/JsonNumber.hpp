#pragma once

#include <cstdint>
#include <optional>

namespace nenenib::core
{
// JSON の数（ADR 0160 の決定 4）。値は double の範囲に限り、-2^53〜2^53 の整数でもあるときだけ
// integer に同じ値を持つ（LSP の id と行と桁を double を経ずに読むため）。両方をそろえて作るのは
// json_number の 1 本で、parse_json もこれを通る。
struct JsonNumber
{
    double value;
    std::optional<std::int64_t> integer;
};

// double の値から JsonNumber を作る。integer は値が ±2^53 の内の整数のときだけ持つ。
[[nodiscard]] JsonNumber json_number(double value) noexcept;

// 整数から JsonNumber を作る。±2^53 を超える整数は double だけを持つ（json_number と同じ規則）。
[[nodiscard]] JsonNumber json_integer(std::int64_t value) noexcept;

[[nodiscard]] bool operator==(const JsonNumber &left, const JsonNumber &right) noexcept;
} // namespace nenenib::core
