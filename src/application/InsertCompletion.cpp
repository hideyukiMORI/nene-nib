#include "InsertCompletion.hpp"

#include "CompletionCandidates.hpp"
#include "CompletionTables.hpp"

#include <vector>

namespace nenenib::application
{
core::CompletionList insert_completion_list(const EditorState &state,
                                            core::CompletionDirection direction)
{
    const std::vector<const core::WordTable *> others = state.parked_word_tables();
    const auto &own = state.word_table();
    return core::completion_list(
        state.text(), state.selection().caret, direction,
        core::CompletionTables{own.has_value() ? &own.value() : nullptr, others});
}
} // namespace nenenib::application
