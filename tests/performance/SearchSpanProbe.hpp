#pragma once

#include "ProbeFailure.hpp"
#include "SearchSpanWorkload.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>

namespace nenenib::tests::performance
{
[[nodiscard]] std::string search_span_input(SearchSpanWorkload workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
searched_spans(SearchSpanWorkload workload, const std::string &input,
               application::TimingPort &timing);
} // namespace nenenib::tests::performance
