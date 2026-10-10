#pragma once

#include "ProbeWorkload.hpp"

namespace nenenib::tests::performance
{
struct ProbeDispatch
{
    ProbeWorkload workload;
    std::string_view name;
    std::string (*input)(ProbeWorkload);
    std::expected<std::uint64_t, ProbeFailure> (*run)(ProbeWorkload, const std::string &,
                                                      application::TimingPort &);
};
} // namespace nenenib::tests::performance
