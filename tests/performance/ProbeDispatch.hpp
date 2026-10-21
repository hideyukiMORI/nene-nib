#pragma once

#include "CommandInputWorkload.hpp"
#include "CompletionPrefixWorkload.hpp"
#include "ExCompletionWorkload.hpp"
#include "FileNameWorkload.hpp"
#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "LexerWorkload.hpp"
#include "MarkdownWorkload.hpp"
#include "NewlineIndexWorkload.hpp"
#include "OperationChoiceWorkload.hpp"
#include "PaletteChoiceWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeDispatchRow.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "SearchSpanWorkload.hpp"
#include "StatusItemsWorkload.hpp"
#include "TabDisplayWorkload.hpp"
#include "TextObjectWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeDispatch =
    std::variant<ProbeDispatchRow<ProbeWorkload>, ProbeDispatchRow<FrameRowsWorkload>,
                 ProbeDispatchRow<SearchSnapshotWorkload>, ProbeDispatchRow<PreviewCaretWorkload>,
                 ProbeDispatchRow<FrameDocumentWorkload>, ProbeDispatchRow<StatusItemsWorkload>,
                 ProbeDispatchRow<FileNameWorkload>, ProbeDispatchRow<TabDisplayWorkload>,
                 ProbeDispatchRow<CommandInputWorkload>, ProbeDispatchRow<ExCompletionWorkload>,
                 ProbeDispatchRow<CompletionPrefixWorkload>,
                 ProbeDispatchRow<PaletteChoiceWorkload>, ProbeDispatchRow<OperationChoiceWorkload>,
                 ProbeDispatchRow<SearchSpanWorkload>, ProbeDispatchRow<TextObjectWorkload>,
                 ProbeDispatchRow<MarkdownWorkload>, ProbeDispatchRow<NewlineIndexWorkload>,
                 ProbeDispatchRow<LexerWorkload>>;
} // namespace nenenib::tests::performance
