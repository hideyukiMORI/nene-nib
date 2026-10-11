#pragma once
#include "PaletteChoiceWorkload.hpp"
#include "ProbeFailure.hpp"
#include "TimingPort.hpp"
#include <cstdint>
#include <expected>
#include <string>
namespace nenenib::tests::performance
{
[[nodiscard]] std::string palette_choice_input(PaletteChoiceWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_palette_choice(PaletteChoiceWorkload workload, const std::string &input,
                      application::TimingPort &timing);
} // namespace nenenib::tests::performance
