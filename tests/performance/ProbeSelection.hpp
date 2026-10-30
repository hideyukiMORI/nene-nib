#pragma once

#include "CommandInputWorkload.hpp"
#include "CompletionPrefixWorkload.hpp"
#include "ExCompletionWorkload.hpp"
#include "ExplorerFrameWorkload.hpp"
#include "FileNameWorkload.hpp"
#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "LexerWorkload.hpp"
#include "MarkdownWorkload.hpp"
#include "NewlineIndexWorkload.hpp"
#include "OperationChoiceWorkload.hpp"
#include "PaletteChoiceWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "SearchSpanWorkload.hpp"
#include "StatusItemsWorkload.hpp"
#include "TabDisplayWorkload.hpp"
#include "TextObjectWorkload.hpp"

#include <variant>

namespace nenenib::tests::performance
{
using ProbeSelection =
    std::variant<ProbeWorkload, FrameRowsWorkload, SearchSnapshotWorkload, PreviewCaretWorkload,
                 FrameDocumentWorkload, StatusItemsWorkload, FileNameWorkload, TabDisplayWorkload,
                 CommandInputWorkload, ExCompletionWorkload, CompletionPrefixWorkload,
                 PaletteChoiceWorkload, OperationChoiceWorkload, SearchSpanWorkload,
                 TextObjectWorkload, MarkdownWorkload, NewlineIndexWorkload, LexerWorkload,
                 ExplorerFrameWorkload>;

[[nodiscard]] std::optional<ProbeSelection> workload_of(std::string_view name) noexcept;
[[nodiscard]] std::string input_of(ProbeSelection workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeSelection workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
