#pragma once

#include "CommandInputWorkload.hpp"
#include "ExCompletionWorkload.hpp"
#include "FileNameWorkload.hpp"
#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeDispatchRow.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "StatusItemsWorkload.hpp"
#include "TabDisplayWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeDispatch =
    std::variant<ProbeDispatchRow<ProbeWorkload>, ProbeDispatchRow<FrameRowsWorkload>,
                 ProbeDispatchRow<SearchSnapshotWorkload>, ProbeDispatchRow<PreviewCaretWorkload>,
                 ProbeDispatchRow<FrameDocumentWorkload>, ProbeDispatchRow<StatusItemsWorkload>,
                 ProbeDispatchRow<FileNameWorkload>, ProbeDispatchRow<TabDisplayWorkload>,
                 ProbeDispatchRow<CommandInputWorkload>, ProbeDispatchRow<ExCompletionWorkload>>;
} // namespace nenenib::tests::performance
