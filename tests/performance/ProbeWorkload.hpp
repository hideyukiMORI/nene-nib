#pragma once

#include "ProbeFailure.hpp"
#include "TimingPort.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
enum class ProbeWorkload : std::uint8_t
{
    controller_open,
    buffer_create,
    controller_insert,
    display_long,
    insert_after_delete_small,
    insert_after_delete_large
};

[[nodiscard]] std::optional<ProbeWorkload> workload_of(std::string_view name) noexcept;
[[nodiscard]] std::string input_of(ProbeWorkload workload);
[[nodiscard]] std::uint64_t checksum_of(std::string_view bytes) noexcept;
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeWorkload workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
