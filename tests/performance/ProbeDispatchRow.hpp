#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::tests::performance
{
template <typename Workload> struct ProbeDispatchRow
{
    Workload workload;
    std::string_view name;
    std::string (*input)(Workload);
    std::expected<std::uint64_t, ProbeFailure> (*run)(Workload, const std::string &,
                                                      application::TimingPort &);
};
} // namespace nenenib::tests::performance
