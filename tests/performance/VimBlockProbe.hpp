#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include "VimBlockWorkload.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string vim_block_input(VimBlockWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_block_inserted(VimBlockWorkload workload, const std::string &input,
                   application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_block_replaced(VimBlockWorkload workload, const std::string &input,
                   application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_block_put(VimBlockWorkload workload, const std::string &input, application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_block_repeated(VimBlockWorkload workload, const std::string &input,
                   application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_insert_word_cut(VimBlockWorkload workload, const std::string &input,
                    application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_counted_left(VimBlockWorkload workload, const std::string &input,
                 application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_counted_right(VimBlockWorkload workload, const std::string &input,
                  application::TimingPort &timing);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
vim_sentence_moved(VimBlockWorkload workload, const std::string &input,
                   application::TimingPort &timing);
} // namespace nenenib::tests::performance
