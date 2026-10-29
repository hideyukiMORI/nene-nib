// scope `--autosave-plan-property` の単体テスト（#930・ADR 0137 の決定 2・3・5・9・ADR 0059・
// ADR 0042 決定 2）。固定 seed の決定的な操作の列（打鍵・まとめた打鍵・undo・redo・保存・新しい
// タブ・まだ読んでいないタブ・切り替え・閉じる）で EditorState を作り、帳面（タブごとの前回の改訂・
// 無い・違う改訂・閉じた番号の残骸）を添えて、autosave_plan と clears_autosave の性質を見る。
// 帳面を計画どおりに直す規則は EditorController::autosave_now と store_planned から写した:
// store はそのタブの今の改訂番号を覚え、discard は番号を外し、keep は何もしない。
// store_autosave の失敗（ADR 0137 の決定 4: 失敗した番号を帳面から外し次の合図で書き直す）は
// 写していない。この試験の帳面は書き込みが全部成功した後の形だけを見る。
// 製品の step_of と autosave_plan を使い捨てに壊して落ちることを手元で確かめた（#930 の R2・commit
// しない）: store と keep の入れ替え・改訂番号を履歴の位置に替える・残骸の discard を keep
// にする、の 3 本とも 落ちる（位置の変異は branched の組と不変量 3 で落ちる）。 未保存かどうかは
// Document.hpp の save_state_of の注釈（ADR 0010 の決定 7）の「保存時点の位置と
// 履歴の位置が一致すれば保存済み」を、タブの束から素朴に読む。
#include "Appearance.hpp"
#include "AutosaveId.hpp"
#include "AutosaveLedger.hpp"
#include "AutosavePlan.hpp"
#include "AutosaveStep.hpp"
#include "Column.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditMode.hpp"
#include "EditorState.hpp"
#include "EndSession.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SessionEnd.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "UnloadedDocument.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::AutosaveId;
using app::AutosaveLedger;
using app::AutosaveStep;
using app::EditorState;

using Random = std::mt19937_64;
using Plan = std::vector<std::pair<AutosaveId, AutosaveStep>>;
// 操作の 1 つ: 表 operations の行の番号と引数。
using Operation = std::pair<std::size_t, std::size_t>;
// 帳面の 1 行の作り方: 的（帯の位置か、tab_limit 以上なら残骸の番号）・形（今の改訂・違う改訂・
// 任意の値）・添える値。
using LedgerRecipe = std::array<std::size_t, 3>;
using Case = std::pair<std::vector<Operation>, std::vector<LedgerRecipe>>;
// 作った状態と、模型が数えた帯の順の退避番号と、次に振られるはずの番号。
using Built = std::pair<EditorState, std::pair<std::vector<AutosaveId>, std::uint64_t>>;
// 読み込み済みのタブの値（未保存か・改訂番号）。まだ読んでいないタブは値なし。
using TabValue = std::optional<std::pair<bool, std::size_t>>;
using Check = std::string_view (*)(const Case &);

// Debug で 1 秒以内に収まる回数（#930）。50 倍の走りは rounds を 50 にして手元で回し、commit
// しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 72U * rounds;
constexpr std::size_t operation_limit = 48U;
constexpr std::size_t ledger_limit = 12U;
constexpr std::size_t tab_limit = 16U;
constexpr std::size_t ledger_exact = 0U;
constexpr std::size_t ledger_shifted = 1U;

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] const EditorState &fresh()
{
    static const EditorState state =
        EditorState::create(core::Appearance::dark, core::EditMode::ordinary);
    return state;
}

[[nodiscard]] app::UnloadedDocument unloaded()
{
    const core::FilePath path = sample_path();
    return app::UnloadedDocument{
        path, core::TextPosition{core::LineNumber{1}, core::Column{1}}, core::LineNumber{1},
        app::DocumentView{core::tab_title_for(path, core::SaveState::saved), path,
                          core::TextEncoding::utf8, core::SaveState::saved, std::nullopt}};
}

[[nodiscard]] EditorState typed(const EditorState &state, core::EditBoundary boundary)
{
    const std::size_t at = state.history().position();
    return state.with_history(
        state.history().pushed(core::Edit{core::Offset{at}, "", "x", core::Offset{at}}, boundary));
}

void type_coalesced(Built &built, std::size_t)
{
    built.first = typed(built.first, core::EditBoundary::coalesce);
}

void type_separated(Built &built, std::size_t)
{
    built.first = typed(built.first, core::EditBoundary::separate);
}

void undo(Built &built, std::size_t)
{
    built.first = built.first.with_history(built.first.history().undone());
}

void redo(Built &built, std::size_t)
{
    built.first = built.first.with_history(built.first.history().redone());
}

void save(Built &built, std::size_t)
{
    built.first = built.first.with_document(
        app::Document{sample_path(), core::TextEncoding::utf8, built.first.history().position()});
}

// 新しいタブはアクティブの右に入り、次の番号を取る。
void open_new(Built &built, std::size_t)
{
    if (built.first.tab_count() >= tab_limit)
    {
        return;
    }
    auto &ids = built.second.first;
    const std::size_t at = built.first.active_tab() + 1;
    built.first = built.first.with_new_tab();
    ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(at), AutosaveId{built.second.second++});
}

// まだ読んでいないタブは帯の右端に入り、次の番号を取る。
void restore(Built &built, std::size_t)
{
    if (built.first.tab_count() >= tab_limit)
    {
        return;
    }
    built.first = built.first.with_restored({unloaded()});
    built.second.first.push_back(AutosaveId{built.second.second++});
}

void switch_to(Built &built, std::size_t argument)
{
    built.first = built.first.with_switched(argument % built.first.tab_count());
}

// 閉じたかどうかは本数で見る（最後の 1 つ・隣がまだ読んでいないアクティブは何も変えない）。
void close(Built &built, std::size_t argument)
{
    const std::size_t position = argument % built.first.tab_count();
    const std::size_t before = built.first.tab_count();
    built.first = built.first.with_closed(position);
    if (built.first.tab_count() + 1 == before)
    {
        auto &ids = built.second.first;
        ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(position));
    }
}

using Apply = void (*)(Built &, std::size_t);
// 打鍵を多めにして、まとめた打鍵と undo の後の別の編集で位置が同じ値に戻る組を作る。
constexpr std::array<Apply, 12> operations{
    type_coalesced, type_coalesced, type_separated, type_separated, undo,      redo,
    save,           open_new,       open_new,       restore,        switch_to, close,
};

[[nodiscard]] Built built_of(const std::vector<Operation> &steps)
{
    Built built{fresh(), {{AutosaveId{0}}, 1U}};
    for (const auto &[row, argument] : steps)
    {
        operations.at(row % operations.size())(built, argument);
    }
    return built;
}

[[nodiscard]] TabValue value_of(const std::shared_ptr<const app::DocumentState> &tab)
{
    const auto &saved = tab->document.saved_position;
    const bool unsaved = !saved.has_value() || saved.value() != tab->history.position();
    return std::pair{unsaved, tab->history.revision()};
}

[[nodiscard]] TabValue value_of(const std::shared_ptr<const app::UnloadedDocument> &)
{
    return std::nullopt;
}

// 帯の順の各タブの値。脇の束は帯の位置からアクティブを抜いた順（EditorState::parked の注釈）。
[[nodiscard]] std::vector<TabValue> tab_values(const EditorState &state)
{
    std::vector<TabValue> values;
    for (std::size_t position = 0; position < state.tab_count(); ++position)
    {
        if (position == state.active_tab())
        {
            const auto &saved = state.document().saved_position;
            const bool unsaved = !saved.has_value() || saved.value() != state.history().position();
            values.emplace_back(std::pair{unsaved, state.history().revision()});
            continue;
        }
        const std::size_t parked = position < state.active_tab() ? position : position - 1;
        values.push_back(
            std::visit([](const auto &tab) { return value_of(tab); }, state.parked().at(parked)));
    }
    return values;
}

[[nodiscard]] std::size_t revision_or_zero(const TabValue &value)
{
    return value.has_value() ? value.value().second : 0U;
}

[[nodiscard]] AutosaveLedger ledger_of(const EditorState &state,
                                       const std::vector<LedgerRecipe> &recipes)
{
    const auto &ids = state.autosave_ids();
    const std::vector<TabValue> values = tab_values(state);
    AutosaveLedger ledger;
    for (const auto &[target, form, extra] : recipes)
    {
        if (target >= tab_limit)
        {
            ledger.insert_or_assign(AutosaveId{target}, extra);
            continue;
        }
        const std::size_t position = target % ids.size();
        const std::size_t now = revision_or_zero(values.at(position));
        const std::size_t stored =
            form == ledger_exact ? now : (form == ledger_shifted ? now + 1U + extra : extra);
        ledger.insert_or_assign(ids.at(position), stored);
    }
    return ledger;
}

[[nodiscard]] std::optional<std::size_t> position_of(const EditorState &state, AutosaveId id)
{
    const auto &ids = state.autosave_ids();
    for (std::size_t position = 0; position < ids.size(); ++position)
    {
        if (ids.at(position) == id)
        {
            return position;
        }
    }
    return std::nullopt;
}

// 帯の番号 id のタブの今の改訂番号（帯に無ければ 0）。
[[nodiscard]] std::size_t revision_in(const EditorState &state, const std::vector<TabValue> &values,
                                      AutosaveId id)
{
    const auto position = position_of(state, id);
    return position.has_value() ? revision_or_zero(values.at(position.value())) : 0U;
}

// 帳面を計画どおりに直す（EditorController::autosave_now の写し）。
[[nodiscard]] AutosaveLedger followed(const EditorState &state, AutosaveLedger ledger,
                                      const Plan &plan)
{
    const std::vector<TabValue> values = tab_values(state);
    for (const auto &[id, step] : plan)
    {
        switch (step)
        {
        case AutosaveStep::store:
            ledger.insert_or_assign(id, revision_in(state, values, id));
            break;
        case AutosaveStep::discard:
            ledger.erase(id);
            break;
        case AutosaveStep::keep:
        case AutosaveStep::oversized:
            break;
        }
    }
    return ledger;
}

// 計画の index 番目の番号が、帯の順の番号か（帯の外なら帳面の残骸か）。
[[nodiscard]] std::string_view entry_naming_violation(const EditorState &state,
                                                      const AutosaveLedger &ledger,
                                                      std::size_t index, AutosaveId id)
{
    const auto &ids = state.autosave_ids();
    if (index < ids.size())
    {
        return ids.at(index) == id ? std::string_view{}
                                   : "the plan does not follow autosave_ids in band order";
    }
    return !position_of(state, id).has_value() && ledger.contains(id)
               ? std::string_view{}
               : "the plan names a tab that is neither in the band nor in the ledger";
}

// 不変量 1: 計画の番号は重複せず、帯の番号は帯の順で全部載り、残りは帳面の残骸だけ。
[[nodiscard]] std::string_view naming_violation(const EditorState &state,
                                                const AutosaveLedger &ledger, const Plan &plan)
{
    std::set<AutosaveId> seen;
    for (std::size_t index = 0; index < plan.size(); ++index)
    {
        const AutosaveId id = plan.at(index).first;
        if (!seen.insert(id).second)
        {
            return "a tab is named twice in the plan";
        }
        const std::string_view reason = entry_naming_violation(state, ledger, index, id);
        if (!reason.empty())
        {
            return reason;
        }
    }
    return plan.size() >= state.autosave_ids().size() ? std::string_view{}
                                                      : "the plan leaves out a tab of the band";
}

// 不変量 2 の store の側: 読み込み済み・未保存・帳面の改訂と違う、の 3 つが揃うときだけ書く。
[[nodiscard]] bool storable(const TabValue &value, const AutosaveLedger &ledger, AutosaveId id)
{
    if (!value.has_value() || !value.value().first)
    {
        return false;
    }
    const auto found = ledger.find(id);
    return found == ledger.end() || found->second != value.value().second;
}

// 不変量 2 の表: 書くのは storable なタブだけで、それ以外は（未保存で変わっていないタブを除き）
// 帳面にあれば消し、無ければ何もしない。帯の外の番号は閉じたタブの残骸。
[[nodiscard]] AutosaveStep expected_step(const std::optional<std::size_t> &position,
                                         const TabValue &value, const AutosaveLedger &ledger,
                                         AutosaveId id)
{
    if (position.has_value() && storable(value, ledger, id))
    {
        return AutosaveStep::store;
    }
    const bool unsaved = value.has_value() && value.value().first;
    return !unsaved && ledger.contains(id) ? AutosaveStep::discard : AutosaveStep::keep;
}

[[nodiscard]] std::string_view step_violation(const EditorState &state,
                                              const AutosaveLedger &ledger, const Plan &plan)
{
    const std::vector<TabValue> values = tab_values(state);
    for (const auto &[id, step] : plan)
    {
        const auto position = position_of(state, id);
        const TabValue value = position.has_value() ? values.at(position.value()) : TabValue{};
        const AutosaveStep expected = expected_step(position, value, ledger, id);
        if (step != expected)
        {
            return expected == AutosaveStep::store
                       ? "an unsaved tab changed since its store is not stored"
                       : "a tab is stored or discarded against the table of ADR 0137";
        }
    }
    return {};
}

// 不変量 3: 帳面を計画どおりに直して計画し直すと、帯の全部が keep で残骸が無い。
[[nodiscard]] std::string_view fixed_point_violation(const EditorState &state,
                                                     const AutosaveLedger &ledger, const Plan &plan)
{
    const Plan again = app::autosave_plan(state, followed(state, ledger, plan));
    if (again.size() != state.tab_count())
    {
        return "a remnant survives the ledger that followed the plan";
    }
    for (const auto &entry : again)
    {
        if (entry.second != AutosaveStep::keep)
        {
            return "planning again after following the plan is not empty";
        }
    }
    return {};
}

// 模型の番号の列: 帯の番号は振った順の通し番号で、閉じた番号は戻らない。
[[nodiscard]] std::string_view numbering_violation(const Built &built)
{
    return built.first.autosave_ids() == built.second.first
               ? std::string_view{}
               : "autosave_ids differs from the serial numbers the tabs were given";
}

[[nodiscard]] std::string_view plan_violation(const Case &input)
{
    const Built built = built_of(input.first);
    const EditorState &state = built.first;
    const AutosaveLedger ledger = ledger_of(state, input.second);
    const Plan plan = app::autosave_plan(state, ledger);
    // 不変量 4: 同じ入力で同じ計画（同じ列から作り直した状態でも同じ）。
    if (app::autosave_plan(state, ledger) != plan ||
        app::autosave_plan(built_of(input.first).first, ledger) != plan)
    {
        return "the same state and ledger give another plan";
    }
    std::string_view reason = numbering_violation(built);
    reason = reason.empty() ? naming_violation(state, ledger, plan) : reason;
    reason = reason.empty() ? step_violation(state, ledger, plan) : reason;
    return reason.empty() ? fixed_point_violation(state, ledger, plan) : reason;
}

// clears_autosave:
// 「いいえ」で捨てたら真。そうでなければ、読み込み済みのタブに未保存が無いときだけ真。
[[nodiscard]] std::string_view clears_violation(const Case &input)
{
    const EditorState state = built_of(input.first).first;
    bool unsaved = false;
    for (const TabValue &value : tab_values(state))
    {
        unsaved = unsaved || (value.has_value() && value.value().first);
    }
    const app::EndSession forced{app::SessionEnd::window_closed};
    const app::EndSession discarded{.reason = app::SessionEnd::window_closed,
                                    .unsaved_discarded = true};
    if (!app::clears_autosave(state, discarded))
    {
        return "unsaved tabs the user chose not to save keep the autosave";
    }
    return app::clears_autosave(state, forced) == !unsaved
               ? std::string_view{}
               : "clears_autosave disagrees with the unsaved tabs that were read";
}

[[nodiscard]] Case random_case(Random &random)
{
    Case input;
    const std::size_t steps = below(random, operation_limit + 1U);
    for (std::size_t index = 0; index < steps; ++index)
    {
        input.first.emplace_back(below(random, operations.size()), below(random, tab_limit));
    }
    const std::size_t entries = below(random, ledger_limit + 1U);
    for (std::size_t index = 0; index < entries; ++index)
    {
        // 的の 4 分の 1 は残骸（閉じたタブの番号と、振られていない番号の両方に当たる）。
        const std::size_t target =
            below(random, 4U) == 0U ? below(random, 64U) : below(random, 16U);
        input.second.push_back({target, below(random, 3U), below(random, 8U)});
    }
    return input;
}

// 破れた組を縮める: 操作と帳面の行を 1 つずつ外し、破れが残るなら外したままにする。
template <typename Item> void minimize_side(Case &input, std::vector<Item> Case::*side, Check check)
{
    for (std::size_t index = (input.*side).size(); index-- > 0;)
    {
        Case smaller = input;
        (smaller.*side).erase((smaller.*side).begin() + static_cast<std::ptrdiff_t>(index));
        if (!check(smaller).empty())
        {
            input = std::move(smaller);
        }
    }
}

void report(const char *family, Case input, Check check)
{
    const std::string_view reason = check(input);
    minimize_side(input, &Case::first, check);
    minimize_side(input, &Case::second, check);
    std::fprintf(stderr, "  %s: %.*s\n    operations:", family, static_cast<int>(reason.size()),
                 reason.data());
    for (const auto &[row, argument] : input.first)
    {
        std::fprintf(stderr, " %zu/%zu", row % operations.size(), argument);
    }
    std::fprintf(stderr, "\n    ledger:");
    for (const auto &[target, form, extra] : input.second)
    {
        std::fprintf(stderr, " %zu/%zu/%zu", target, form, extra);
    }
    std::fprintf(stderr, "\n");
}

// 生成した組のうち性質を破ったものの数。最初の 1 つは縮めて出す。
[[nodiscard]] std::size_t broken_cases(const char *family, std::uint64_t seed, Check check)
{
    Random random{seed};
    std::size_t broken = 0;
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        const Case input = random_case(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(family, input, check);
        }
    }
    return broken;
}

// 計画の 1 行が決定 2 の表のどの枝か。添字は branch_* の順で、clears_autosave の真偽が続く。
constexpr std::size_t branch_store = 0U;
constexpr std::size_t branch_unchanged = 1U;
constexpr std::size_t branch_untouched = 2U;
constexpr std::size_t branch_unread = 3U;
constexpr std::size_t branch_remnant = 4U;
constexpr std::size_t branch_saved = 5U;
constexpr std::size_t branch_clears = 6U;
constexpr std::size_t branch_keeps = 7U;
using Branches = std::array<std::size_t, 8>;

[[nodiscard]] std::size_t branch_of(const std::optional<std::size_t> &position,
                                    const TabValue &value, AutosaveStep step)
{
    const bool unsaved = value.has_value() && value.value().first;
    std::size_t branch = branch_store;
    switch (step)
    {
    // 生成する本文は上限より十分小さいので oversized は出ない（出れば下の神託との比較が落ちる）。
    case AutosaveStep::store:
    case AutosaveStep::oversized:
        branch = branch_store;
        break;
    case AutosaveStep::keep:
        branch = unsaved ? branch_unchanged : branch_untouched;
        break;
    case AutosaveStep::discard:
        branch = !position.has_value() ? branch_remnant
                                       : (value.has_value() ? branch_saved : branch_unread);
        break;
    }
    return branch;
}

void count_branches(const Case &input, Branches &branches)
{
    const EditorState state = built_of(input.first).first;
    const std::vector<TabValue> values = tab_values(state);
    for (const auto &[id, step] : app::autosave_plan(state, ledger_of(state, input.second)))
    {
        const auto position = position_of(state, id);
        const TabValue value = position.has_value() ? values.at(position.value()) : TabValue{};
        ++branches.at(branch_of(position, value, step));
    }
    const bool clears =
        app::clears_autosave(state, app::EndSession{app::SessionEnd::window_closed});
    ++branches.at(clears ? branch_clears : branch_keeps);
}

// 生成器が決定 2 の表の枝を全部覆う（store・未保存で変わっていない keep・帳面に無い keep・
// 読んでいないタブの行の discard・閉じた残骸の discard・保存済みの行の discard）と、
// clears_autosave の真偽の両方が出る。どれかが 0 なら性質の試験はその枝を見ていない（#930 の R1）。
void verify_generator_reaches_the_table()
{
    Random random{9301U};
    Branches branches{};
    std::size_t largest = 0;
    for (std::size_t index = 0; index < family_cases; ++index)
    {
        const Case input = random_case(random);
        largest = std::max(largest, built_of(input.first).first.tab_count());
        count_branches(input, branches);
    }
    expect(std::ranges::all_of(branches, [](std::size_t count) { return count > 0U; }) &&
               largest >= tab_limit / 2U,
           "the generated states reach every row of the table, both answers of clears_autosave "
           "and many tabs");
}

void verify_generator_is_deterministic()
{
    Random left{9302U};
    Random right{9302U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && random_case(left) == random_case(right);
    }
    expect(same, "the generator gives the same cases for the same seed");
}

// まとめた打鍵と undo の後の別の編集で位置が同じ値に戻っても、改訂で比べて書き直す（回帰の組）。
void verify_known_cases()
{
    const Case folded{{{0U, 0U}}, {{0U, ledger_exact, 0U}}};
    expect(plan_violation(folded).empty(), "a tab stored at its revision stays kept");
    const Case branched{{{2U, 0U}, {4U, 0U}, {2U, 0U}}, {{0U, ledger_shifted, 0U}}};
    expect(plan_violation(branched).empty(),
           "another edit after undo at the same position is planned by revision");
    const Case closed{{{7U, 0U}, {2U, 0U}, {11U, 1U}}, {{17U, 0U, 3U}, {1U, ledger_exact, 0U}}};
    expect(plan_violation(closed).empty(), "the remnant of a closed tab is discarded once");
    const Case unread{{{9U, 0U}}, {{1U, ledger_exact, 0U}}};
    expect(plan_violation(unread).empty() && clears_violation(unread).empty(),
           "a tab not read yet is never stored and never keeps the autosave");
}
// 検査そのものが破れを見つける（計画の 1 行を書き換えると落ちる）。
void verify_checks_catch_a_broken_plan()
{
    const EditorState state = built_of({{0U, 0U}, {7U, 0U}, {9U, 0U}}).first;
    const AutosaveLedger ledger = ledger_of(state, {{20U, 0U, 1U}});
    Plan plan = app::autosave_plan(state, ledger);
    expect(plan.size() == 4U && plan.at(0).second == AutosaveStep::store,
           "the planted case stores the typed tab and discards the remnant");
    Plan kept = plan;
    kept.at(0).second = AutosaveStep::keep;
    Plan doubled = plan;
    doubled.at(1).first = doubled.at(0).first;
    expect(!step_violation(state, ledger, kept).empty() &&
               !naming_violation(state, ledger, doubled).empty() &&
               !naming_violation(state, ledger, Plan(plan.begin(), plan.begin() + 1)).empty(),
           "the checks catch a skipped store, a doubled name and a missing tab");
}
} // namespace

void verify_autosave_plan_property_scope()
{
    verify_generator_is_deterministic();
    verify_generator_reaches_the_table();
    verify_known_cases();
    verify_checks_catch_a_broken_plan();
    expect(broken_cases("autosave_plan keeps its invariants", 9303U, plan_violation) == 0U,
           "autosave_plan names band tabs once, stores only changed unsaved tabs and settles");
    expect(broken_cases("clears_autosave follows the unsaved tabs", 9304U, clears_violation) == 0U,
           "clears_autosave is true only without unsaved tabs that were read or after discarding");
}
} // namespace nenenib::tests
