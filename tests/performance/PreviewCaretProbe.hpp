#pragma once

#include "PreviewCaretWorkload.hpp"
#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
[[nodiscard]] std::string preview_caret_input(PreviewCaretWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
probed_preview_caret(PreviewCaretWorkload workload, const std::string &input,
                     application::TimingPort &timing);
} // namespace nenenib::tests::performance
