#pragma once

#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeDispatchRow.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "StatusItemsWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeDispatch =
    std::variant<ProbeDispatchRow<ProbeWorkload>, ProbeDispatchRow<FrameRowsWorkload>,
                 ProbeDispatchRow<SearchSnapshotWorkload>, ProbeDispatchRow<PreviewCaretWorkload>,
                 ProbeDispatchRow<FrameDocumentWorkload>, ProbeDispatchRow<StatusItemsWorkload>>;
} // namespace nenenib::tests::performance
