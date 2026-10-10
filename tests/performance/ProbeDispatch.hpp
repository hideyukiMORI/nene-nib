#pragma once

#include "FrameRowsWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeDispatchRow.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeDispatch =
    std::variant<ProbeDispatchRow<ProbeWorkload>, ProbeDispatchRow<FrameRowsWorkload>,
                 ProbeDispatchRow<SearchSnapshotWorkload>, ProbeDispatchRow<PreviewCaretWorkload>>;
} // namespace nenenib::tests::performance
