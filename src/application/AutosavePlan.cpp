#include "AutosavePlan.hpp"

#include "Document.hpp"
#include "DocumentState.hpp"
#include "SaveState.hpp"
#include "UnloadedDocument.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// 1 タブの読み込み済みの値（未保存か・改訂番号）。まだ読んでいない文書は値なし。
using TabRevision = std::pair<core::SaveState, std::size_t>;

[[nodiscard]] std::optional<TabRevision>
revision_of(const std::shared_ptr<const DocumentState> &tab)
{
    return TabRevision{save_state_of(tab->document, tab->history.position()),
                       tab->history.revision()};
}

[[nodiscard]] std::optional<TabRevision>
revision_of(const std::shared_ptr<const UnloadedDocument> &)
{
    return std::nullopt;
}

// 帯の位置 position のタブの値。脇の束は帯の位置からアクティブを抜いた順。写し先が足りなければ
// std::visit が落ちる（CPP-002）。
[[nodiscard]] std::optional<TabRevision> revision_at(const EditorState &state, std::size_t position)
{
    if (position == state.active_tab())
    {
        return TabRevision{save_state_of(state.document(), state.history().position()),
                           state.history().revision()};
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::visit([](const auto &tab) { return revision_of(tab); }, state.parked().at(parked));
}

// 決定 2 の表の 1 行。stored は帳面に覚えた改訂番号（無ければ値なし）。
[[nodiscard]] AutosaveStep step_of(const std::optional<TabRevision> &tab,
                                   const std::optional<std::size_t> &stored)
{
    const bool unsaved = tab.has_value() && tab.value().first != core::SaveState::saved;
    if (unsaved)
    {
        return stored.has_value() && stored.value() == tab.value().second ? AutosaveStep::keep
                                                                          : AutosaveStep::store;
    }
    return stored.has_value() ? AutosaveStep::discard : AutosaveStep::keep;
}

[[nodiscard]] std::optional<std::size_t> stored_of(const AutosaveLedger &ledger, AutosaveId id)
{
    const auto found = ledger.find(id);
    if (found == ledger.end())
    {
        return std::nullopt;
    }
    return found->second;
}
} // namespace

std::vector<std::pair<AutosaveId, AutosaveStep>> autosave_plan(const EditorState &state,
                                                               const AutosaveLedger &ledger)
{
    std::vector<std::pair<AutosaveId, AutosaveStep>> plan;
    const auto &ids = state.autosave_ids();
    std::set<AutosaveId> present;
    for (std::size_t position = 0; position < ids.size(); ++position)
    {
        const AutosaveId id = ids.at(position);
        present.insert(id);
        plan.emplace_back(id, step_of(revision_at(state, position), stored_of(ledger, id)));
    }
    // 帳面に残っていて帯に無い番号は閉じたタブ（決定 2）。
    for (const auto &entry : ledger)
    {
        if (!present.contains(entry.first))
        {
            plan.emplace_back(entry.first, AutosaveStep::discard);
        }
    }
    return plan;
}
} // namespace nenenib::application
