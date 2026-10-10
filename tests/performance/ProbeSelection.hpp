#pragma once

#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "StatusItemsWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeSelection =
    std::variant<ProbeWorkload, FrameRowsWorkload, SearchSnapshotWorkload, PreviewCaretWorkload,
                 FrameDocumentWorkload, StatusItemsWorkload>;

[[nodiscard]] std::optional<ProbeSelection> workload_of(std::string_view name) noexcept;
[[nodiscard]] std::string input_of(ProbeSelection workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeSelection workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
