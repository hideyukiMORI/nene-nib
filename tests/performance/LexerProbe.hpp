#pragma once

#include "LexerWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string lexer_input(LexerWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
lexed_lines(LexerWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
