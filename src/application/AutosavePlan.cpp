#include "AutosavePlan.hpp"

#include "Document.hpp"
#include "DocumentByteLimit.hpp"
#include "DocumentState.hpp"
#include "SaveState.hpp"
#include "UnloadedDocument.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// 1 タブの読み込み済みの値（未保存か・改訂番号・本文のバイト数）。まだ読んでいない文書は値なし。
// 補助の型を足さない（CPP-011）ので組で持ち、読むのは下の 3 つの関数だけ。
using TabRevision = std::tuple<core::SaveState, std::size_t, std::size_t>;

[[nodiscard]] core::SaveState save_of(const TabRevision &tab) noexcept
{
    return std::get<0>(tab);
}

[[nodiscard]] std::size_t revision_number(const TabRevision &tab) noexcept
{
    return std::get<1>(tab);
}

[[nodiscard]] std::size_t bytes_of(const TabRevision &tab) noexcept
{
    return std::get<2>(tab);
}

[[nodiscard]] std::optional<TabRevision>
revision_of(const std::shared_ptr<const DocumentState> &tab)
{
    return TabRevision{save_state_of(tab->document, tab->history.position()),
                       tab->history.revision(), tab->text.size_bytes()};
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
                           state.history().revision(), state.text().size_bytes()};
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1;
    return std::visit([](const auto &tab) { return revision_of(tab); }, state.parked().at(parked));
}

// 決定 2 の表の 1 行。stored は帳面に覚えた改訂番号（無ければ値なし）。
[[nodiscard]] AutosaveStep step_of(const std::optional<TabRevision> &tab,
                                   const std::optional<std::size_t> &stored)
{
    const bool unsaved = tab.has_value() && save_of(tab.value()) != core::SaveState::saved;
    if (unsaved)
    {
        if (stored.has_value() && stored.value() == revision_number(tab.value()))
        {
            return AutosaveStep::keep;
        }
        // 上限を超える本文は書かない（ADR 0159 の決定 7・L-4）。前の退避は消さずに残す。
        return bytes_of(tab.value()) > core::maximum_decoded_document_bytes
                   ? AutosaveStep::oversized
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

bool clears_autosave(const EditorState &state, const EndSession &intent)
{
    if (intent.unsaved_discarded)
    {
        return true;
    }
    for (std::size_t position = 0; position < state.tab_count(); ++position)
    {
        const auto tab = revision_at(state, position);
        if (tab.has_value() && save_of(tab.value()) != core::SaveState::saved)
        {
            return false;
        }
    }
    return true;
}
} // namespace nenenib::application
