#pragma once
#include "OperationChoiceWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include <cstdint>
#include <expected>
#include <string>
namespace nenenib::tests::performance
{
[[nodiscard]] std::string operation_choice_input(OperationChoiceWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_operation_choice(OperationChoiceWorkload workload, const std::string &input,
                        application::TimingPort &timing);
} // namespace nenenib::tests::performance
