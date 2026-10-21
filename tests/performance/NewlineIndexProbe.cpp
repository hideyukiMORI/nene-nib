#include "NewlineIndexProbe.hpp"

#include "CountedHeap.hpp"
#include "Milestone.hpp"
#include "NewlineIndex.hpp"
#include "ProbeWorkload.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests::performance
{
namespace
{
constexpr std::size_t index_bytes = std::size_t{16U} << 20U;
constexpr std::size_t lengths_calls = 64U;
// 1 行 60 byte（59 文字と LF）の英文。
constexpr std::string_view prose_line =
    "The quick brown fox jumps over the lazy dog near the river.";
static_assert(prose_line.size() == 59U);

[[nodiscard]] std::string prose_input()
{
    std::string text;
    text.reserve(index_bytes);
    while (text.size() + prose_line.size() + 1U <= index_bytes)
    {
        text += prose_line;
        text += '\n';
    }
    return text;
}

[[nodiscard]] std::size_t newlines_in(std::string_view text)
{
    return static_cast<std::size_t>(std::ranges::count(text, '\n'));
}
} // namespace

std::string newline_index_input(NewlineIndexWorkload workload)
{
    switch (workload)
    {
    case NewlineIndexWorkload::payload_all_lf:
    case NewlineIndexWorkload::lengths_all_lf:
        return std::string(index_bytes, '\n');
    case NewlineIndexWorkload::payload_prose:
        return prose_input();
    case NewlineIndexWorkload::payload_one_line:
        return std::string(index_bytes, 'x');
    }
    std::unreachable();
}

// 索引を 1 回作り、作る間に増えた確保の byte 数を payload として報せる（ADR 0121 の追補の決定 9）。
std::expected<std::uint64_t, ProbeFailure> indexed_payload(NewlineIndexWorkload workload,
                                                           const std::string &input,
                                                           application::TimingPort &timing)
{
    if (input != newline_index_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    start_heap_count();
    const auto index = core::NewlineIndex::from_text(input);
    const auto payload = counted_heap_bytes();
    stop_heap_count();
    timing.mark(core::Milestone::probe_finished);
    if (index.size() != newlines_in(input) || payload <= 0)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto bytes = static_cast<std::uint64_t>(payload);
    std::cerr << "indexPayloadBytes=" << bytes << " inputBytes=" << input.size()
              << " payloadPerMille=" << (bytes * 1000U / input.size()) << '\n';
    return checksum_of(input) ^ bytes ^ index.size();
}

// 両端が塊の途中の窓の lengths を 64 回引く（読む量は両端の欠けた塊の 2 つ・決定 10）。
std::expected<std::uint64_t, ProbeFailure> indexed_lengths(NewlineIndexWorkload workload,
                                                           const std::string &input,
                                                           application::TimingPort &timing)
{
    if (input != newline_index_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto index = core::NewlineIndex::from_text(input);
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t call = 0; call < lengths_calls; ++call)
    {
        const auto lengths = index.lengths(1U + call, input.size() - 1U - call);
        checksum += lengths.head + lengths.longest + lengths.tail + (lengths.closed ? 1U : 0U);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != lengths_calls)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(input) ^ checksum;
}
} // namespace nenenib::tests::performance
