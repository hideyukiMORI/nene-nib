#include "LastResortPlan.hpp"

#include "AutosavePlan.hpp"

namespace nenenib::application
{
IntentMark intent_mark_of(const EditorState &state)
{
    return IntentMark{state.autosave_ids().at(state.active_tab()), state.history().revision(),
                      state.tab_count()};
}

std::vector<std::pair<AutosaveId, AutosaveStep>> last_resort_plan(const EditorState &state,
                                                                  const AutosaveLedger &ledger,
                                                                  const IntentMark &entered,
                                                                  std::size_t depth)
{
    auto plan = autosave_plan(state, ledger);
    if (depth == 0)
    {
        return plan;
    }
    const IntentMark now = intent_mark_of(state);
    if (!(now.active == entered.active) || now.tabs != entered.tabs)
    {
        return {};
    }
    if (now.revision == entered.revision)
    {
        return plan;
    }
    for (auto &[id, step] : plan)
    {
        if (id == entered.active)
        {
            step = AutosaveStep::keep;
        }
    }
    return plan;
}
} // namespace nenenib::application
