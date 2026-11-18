#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include "WordTableWorkload.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
// 入力は workload の短い名札（文書は probe の中で固定の生成から作る・ファイルを読まない）。
[[nodiscard]] std::string word_table_input(WordTableWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
word_table_completion(WordTableWorkload workload, const std::string &input,
                      application::TimingPort &timing);
} // namespace nenenib::tests::performance
