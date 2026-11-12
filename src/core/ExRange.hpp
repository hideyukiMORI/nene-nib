#pragma once

#include "ExAddress.hpp"

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
// Ex の行の範囲（ADR 0168 の決定 1・ADR 0022 の追補 #1526）。番地の列で、`%` は `1,$` の 2 つ。
// 使うのは後ろの 2 つ（1 つなら同じ行・Vim の do_one_cmd）。本文を知らないので行に解くのは
// engine（vim_ex_lines）。値は単独で妥当なので公開 aggregate（CPP-003）。
struct ExRange
{
    std::vector<ExAddress> addresses;
};

// 行の先の範囲と、その長さ（バイト）。番地を `,` `;` で区切って続けて読む（空の番地も Vim と同じに
// 許し、キャレットの行になる）。範囲が無ければ長さ 0。Ex の行の範囲を読むのはこの 1 本で、命令を
// 切り出す長さ（ExResult の ranged_command）もここから取る。
[[nodiscard]] std::pair<ExRange, std::size_t> parsed_ex_range(std::string_view text);
} // namespace nenenib::core
