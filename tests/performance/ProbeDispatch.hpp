#pragma once

#include "FrameRowsWorkload.hpp"
#include "ProbeDispatchRow.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeDispatch =
    std::variant<ProbeDispatchRow<ProbeWorkload>, ProbeDispatchRow<FrameRowsWorkload>,
                 ProbeDispatchRow<SearchSnapshotWorkload>>;
} // namespace nenenib::tests::performance
