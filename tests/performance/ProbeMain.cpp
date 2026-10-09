#include "ProbeWorkload.hpp"

#include "Utf16.hpp"
#include "Win32TimingAdapter.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>

namespace
{
[[nodiscard]] std::optional<std::size_t> iterations_of(std::string_view text) noexcept
{
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 ||
        value > 2048)
    {
        return std::nullopt;
    }
    return value;
}

void report(std::string_view workload, std::size_t iterations, std::string_view input,
            std::uint64_t checksum)
{
    std::cout << "{\"schema\":1,\"workload\":\"" << workload << "\",\"iterations\":" << iterations
              << ",\"warmup\":1,\"inputBytes\":" << input.size()
              << ",\"inputHashAlgorithm\":\"fnv1a64\",\"inputHash\":\""
              << nenenib::tests::performance::checksum_of(input) << "\",\"outputChecksum\":\""
              << checksum << "\",\"commit\":\"" << NIB_PROBE_COMMIT
              << "\",\"dirty\":" << NIB_PROBE_DIRTY << ",\"compilerVersion\":\""
              << NIB_PROBE_COMPILER << "\",\"configuration\":\"" << NIB_PROBE_CONFIGURATION
              << "\",\"toolchainSha256\":\"" << NIB_PROBE_TOOLCHAIN_HASH << "\"}\n";
}
} // namespace

// argv: workload / iterations / private timing output. No window, clock or file API here.
int main(int argc, char **argv)
{
    namespace probe = nenenib::tests::performance;
    if (argc != 4)
    {
        std::cerr << "Probe: specify workload, iterations (1..2048), timing output\n";
        return 2;
    }
    const auto workload = probe::workload_of(argv[1]);
    const auto iterations = iterations_of(argv[2]);
    const auto path = nenenib::core::to_utf16(argv[3]);
    if (!workload.has_value() || !iterations.has_value() || !path.has_value() ||
        path.value().empty())
    {
        std::cerr << "Probe: invalid workload, iterations or UTF-8 output path\n";
        return 2;
    }
    const std::string input = probe::input_of(workload.value());
    nenenib::adapters::win32::Win32TimingAdapter timing;
    const auto warmup = probe::run_workload(workload.value(), input, timing);
    if (!warmup.has_value())
    {
        std::cerr << "Probe: warmup result differs from the fixed workload\n";
        return 1;
    }
    timing.bind(path.value());
    for (std::size_t iteration = 0; iteration < iterations.value(); ++iteration)
    {
        const auto result = probe::run_workload(workload.value(), input, timing);
        if (!result.has_value() || result.value() != warmup.value())
        {
            std::cerr << "Probe: observed result differs from warmup\n";
            return 1;
        }
    }
    report(argv[1], iterations.value(), input, warmup.value());
    return 0;
}
