#pragma once

#include "CommandInputWorkload.hpp"
#include "CompletionPrefixWorkload.hpp"
#include "EditedHistoryWorkload.hpp"
#include "ExCompletionWorkload.hpp"
#include "ExLinesWorkload.hpp"
#include "ExplorerFrameWorkload.hpp"
#include "FileNameWorkload.hpp"
#include "FrameDocumentWorkload.hpp"
#include "FrameRowsWorkload.hpp"
#include "LexerWorkload.hpp"
#include "MarkdownWorkload.hpp"
#include "NewlineIndexWorkload.hpp"
#include "OperationChoiceWorkload.hpp"
#include "PaletteChoiceWorkload.hpp"
#include "PatternCostWorkload.hpp"
#include "PatternProgramWorkload.hpp"
#include "PieceListWorkload.hpp"
#include "PreviewCaretWorkload.hpp"
#include "ProbeWorkload.hpp"
#include "SearchObjectWorkload.hpp"
#include "SearchSnapshotWorkload.hpp"
#include "SearchSpanWorkload.hpp"
#include "StatusItemsWorkload.hpp"
#include "SubstituteWorkload.hpp"
#include "TabDisplayWorkload.hpp"
#include "TextObjectWorkload.hpp"
#include "VimBlockWorkload.hpp"
#include "WordObjectWorkload.hpp"

#include <span>
#include <string_view>
#include <variant>

namespace nenenib::tests::performance
{
using ProbeSelection =
    std::variant<ProbeWorkload, FrameRowsWorkload, SearchSnapshotWorkload, PreviewCaretWorkload,
                 FrameDocumentWorkload, StatusItemsWorkload, FileNameWorkload, TabDisplayWorkload,
                 CommandInputWorkload, ExCompletionWorkload, CompletionPrefixWorkload,
                 PaletteChoiceWorkload, OperationChoiceWorkload, SearchSpanWorkload,
                 TextObjectWorkload, MarkdownWorkload, NewlineIndexWorkload, LexerWorkload,
                 ExplorerFrameWorkload, VimBlockWorkload, WordObjectWorkload, EditedHistoryWorkload,
                 PieceListWorkload, SubstituteWorkload, PatternCostWorkload, PatternProgramWorkload,
                 SearchObjectWorkload, ExLinesWorkload>;

[[nodiscard]] std::optional<ProbeSelection> workload_of(std::string_view name) noexcept;
// 表 ProbeDispatchRow の全 workload の名前（表の順・#1604 の --expectations が流す）。
[[nodiscard]] std::span<const std::string_view> workload_names() noexcept;
[[nodiscard]] std::string input_of(ProbeSelection workload);
[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
run_workload(ProbeSelection workload, const std::string &input, application::TimingPort &timing);
} // namespace nenenib::tests::performance
