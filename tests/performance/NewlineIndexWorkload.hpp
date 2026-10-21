#pragma once

#include <cstdint>

namespace nenenib::tests::performance
{
// 改行の索引の payload（索引が持つ確保の byte 数）を 16 MiB の 3 本の本文で数える（#792・ADR 0121
// の追補の決定 9）。lengths_all_lf は全 LF の本文で両端が塊の途中の lengths を 64 回引く。
enum class NewlineIndexWorkload : std::uint8_t
{
    payload_all_lf,
    payload_prose,
    payload_one_line,
    lengths_all_lf
};
} // namespace nenenib::tests::performance
