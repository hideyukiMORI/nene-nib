// scope `--tabs-property` の単体テスト（#935・ADR 0056 / 0057 / 0058 / 0059 / 0137 の決定 3・
// ADR 0042 決定 2）。固定 seed の決定的な操作の列（切り替え・歩く・歩きの確定・新しいタブ・閉じる・
// 前回のタブの復元・自動退避から戻した文書・読む・外す）を EditorState と素朴な模型の両方に当て、
// 各歩で次を見る。
//   1. 使った順は帯の位置の置換で、歩いていなければ先頭がアクティブ。
//   2. 退避番号は本数ぶんで重複せず、閉じた・外した番号は戻らない（模型の通し番号と一致）。
//   3. 脇の束の数 + 1 が本数で、操作が触っていないタブの束は同じ参照。
//   4. アクティブはまだ読んでいない文書にならず、範囲の外・読んでいないタブ・隣が読んでいない
//      アクティブを閉じる、は状態を変えない。
//   5. 帯の順（タブの札）・使った順・アクティブ・歩いている印が模型と一致する。
// 模型は EditorState.hpp の注釈の規則を std::vector の列で手で動かしたもの。タブの札は退避番号と
// 同じ通し番号をパス `C:\t\<番号>.txt` に書いて持たせ、帯の位置ごとにパスで照合する。
// 使った順の歩き（tab_recency_walked）と行き先（tab_destination）は素朴な式と全数で比べる。
#include "Appearance.hpp"
#include "AutosaveId.hpp"
#include "Column.hpp"
#include "Document.hpp"
#include "DocumentState.hpp"
#include "DocumentView.hpp"
#include "EditHistory.hpp"
#include "EditMode.hpp"
#include "EditorState.hpp"
#include "FilePath.hpp"
#include "LineNumber.hpp"
#include "Offset.hpp"
#include "ParkedTab.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "Selection.hpp"
#include "TabDestination.hpp"
#include "TabJump.hpp"
#include "TabJumpDirection.hpp"
#include "TabRecency.hpp"
#include "TabStep.hpp"
#include "TabTitle.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "TextPosition.hpp"
#include "UnloadedDocument.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace app = nenenib::application;
namespace core = nenenib::core;
using app::EditorState;

using Random = std::mt19937_64;
// 模型のタブ 1 本: 札（= 退避番号）と、読み込み済みか。
using Tab = std::pair<std::uint64_t, bool>;
using Band = std::vector<Tab>;
using Order = std::vector<std::size_t>;
// 模型: 帯の順のタブ・アクティブの位置・使った順・歩いている印・次の番号・閉じた番号の集合。
using Model = std::tuple<Band, std::size_t, Order, bool, std::uint64_t, std::set<std::uint64_t>>;
using World = std::pair<EditorState, Model>;
// 操作の前の脇の束の参照（退避番号から）と模型。
using Before = std::pair<std::map<std::uint64_t, const void *>, Model>;
// 操作の 1 つ: 表 operations の行の番号と引数。
using Operation = std::pair<std::size_t, std::size_t>;
using Case = std::vector<Operation>;
using Check = std::string_view (*)(const Case &);

// Debug で 1 秒以内に収まる回数（#935）。50 倍の走りは rounds を 50 にして手元で回し、commit
// しない。
constexpr std::size_t rounds = 1U;
constexpr std::size_t family_cases = 96U * rounds;
constexpr std::size_t coverage_cases = 40U;
constexpr std::size_t operation_limit = 64U;
constexpr std::size_t tab_limit = 16U;

// 操作の結果の枝（生成器がどの規則まで届いたかを数える添字）。
constexpr std::size_t outcome_changed = 0U;
constexpr std::size_t outcome_out_of_range = 1U;
constexpr std::size_t outcome_unread = 2U;
constexpr std::size_t outcome_last_tab = 3U;
constexpr std::size_t outcome_self = 4U;
constexpr std::size_t outcome_settled = 5U;
constexpr std::size_t outcome_idle = 6U;
constexpr std::size_t outcome_loaded = 7U;
constexpr std::size_t outcome_dropped = 8U;
constexpr std::size_t outcome_count = 9U;

template <typename M> [[nodiscard]] auto &band_of(M &model)
{
    return std::get<0>(model);
}
template <typename M> [[nodiscard]] auto &active_of(M &model)
{
    return std::get<1>(model);
}
template <typename M> [[nodiscard]] auto &order_of(M &model)
{
    return std::get<2>(model);
}
template <typename M> [[nodiscard]] auto &walking_of(M &model)
{
    return std::get<3>(model);
}
template <typename M> [[nodiscard]] auto &next_of(M &model)
{
    return std::get<4>(model);
}
template <typename M> [[nodiscard]] auto &retired_of(M &model)
{
    return std::get<5>(model);
}

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] std::ptrdiff_t distance_of(std::size_t index)
{
    return static_cast<std::ptrdiff_t>(index);
}

// ---------------------------------------------------------------- 素朴な使った順

// tab を先頭へ動かす。列に無ければ何もしない。
void touched(Order &order, std::size_t tab)
{
    const auto found = std::ranges::find(order, tab);
    if (found == order.end())
    {
        return;
    }
    order.erase(found);
    order.insert(order.begin(), tab);
}

// 位置 tab に新しいタブができた: それ以上は 1 つ後ろへずれ、新しいタブは先頭。
void opened(Order &order, std::size_t tab)
{
    for (std::size_t &position : order)
    {
        position += position >= tab ? 1U : 0U;
    }
    order.insert(order.begin(), tab);
}

// 位置 tab が閉じた: それより後ろは 1 つ前へずれる。
void closed(Order &order, std::size_t tab)
{
    std::erase(order, tab);
    for (std::size_t &position : order)
    {
        position -= position > tab ? 1U : 0U;
    }
}

// ---------------------------------------------------------------- 札と文書

[[nodiscard]] std::string label_of(std::uint64_t id)
{
    return "C:\\t\\" + std::to_string(id) + ".txt";
}

[[nodiscard]] core::FilePath path_of(std::uint64_t id)
{
    auto parsed = core::FilePath::parse(label_of(id));
    return std::move(parsed).value();
}

[[nodiscard]] app::DocumentView view_of(const core::FilePath &path)
{
    return app::DocumentView{core::tab_title_for(path, core::SaveState::saved), path,
                             core::TextEncoding::utf8, core::SaveState::saved, std::nullopt};
}

[[nodiscard]] app::Document document_of(std::uint64_t id)
{
    return app::Document{path_of(id), core::TextEncoding::utf8, std::size_t{0}};
}

[[nodiscard]] app::DocumentState bundle_of(std::uint64_t id)
{
    const core::FilePath path = path_of(id);
    return app::DocumentState{core::TextBuffer::empty(),
                              core::collapsed_at(core::Offset{0}),
                              core::EditHistory::empty(),
                              document_of(id),
                              core::LineNumber{1},
                              std::nullopt,
                              std::nullopt,
                              view_of(path)};
}

[[nodiscard]] app::UnloadedDocument unloaded_of(std::uint64_t id)
{
    const core::FilePath path = path_of(id);
    return app::UnloadedDocument{path, core::TextPosition{core::LineNumber{1}, core::Column{1}},
                                 core::LineNumber{1}, view_of(path)};
}

[[nodiscard]] const World &fresh()
{
    static const World world{EditorState::create(core::Appearance::dark, core::EditMode::ordinary)
                                 .with_document(document_of(0U)),
                             Model{Band{{0U, true}}, 0U, Order{0U}, false, 1U, {}}};
    return world;
}

// ---------------------------------------------------------------- 操作（状態と模型の両方へ）

// 引数を帯の位置に畳む。本数と本数 + 1 は範囲の外。
[[nodiscard]] std::size_t position_from(const World &world, std::size_t argument)
{
    return argument % (world.first.tab_count() + 2U);
}

// 帯の位置 position がまだ読んでいないタブか（アクティブ・範囲の外・読み込み済みは偽）。
[[nodiscard]] bool unread_at(const Model &model, std::size_t position)
{
    const Band &band = band_of(model);
    return position < band.size() && position != active_of(model) && !band.at(position).second;
}

[[nodiscard]] std::size_t switch_model(Model &model, std::size_t position)
{
    if (position >= band_of(model).size())
    {
        return outcome_out_of_range;
    }
    const bool self = position == active_of(model);
    if (!self && !band_of(model).at(position).second)
    {
        return outcome_unread;
    }
    active_of(model) = position;
    touched(order_of(model), position);
    walking_of(model) = false;
    return self ? outcome_self : outcome_changed;
}

[[nodiscard]] std::size_t switch_to(World &world, std::size_t argument)
{
    const std::size_t position = position_from(world, argument);
    world.first = world.first.with_switched(position);
    return switch_model(world.second, position);
}

[[nodiscard]] std::size_t walk_model(Model &model, std::size_t position)
{
    if (position >= band_of(model).size())
    {
        return outcome_out_of_range;
    }
    const bool self = position == active_of(model);
    if (!self && !band_of(model).at(position).second)
    {
        return outcome_unread;
    }
    active_of(model) = position;
    walking_of(model) = true;
    return self ? outcome_self : outcome_changed;
}

[[nodiscard]] std::size_t walk_to(World &world, std::size_t argument)
{
    const std::size_t position = position_from(world, argument);
    world.first = world.first.with_walked(position);
    return walk_model(world.second, position);
}

[[nodiscard]] std::size_t settle(World &world, std::size_t)
{
    world.first = world.first.with_walk_settled();
    Model &model = world.second;
    if (!walking_of(model))
    {
        return outcome_idle;
    }
    touched(order_of(model), active_of(model));
    walking_of(model) = false;
    return outcome_settled;
}

// 新しいタブはアクティブの右に入り、次の番号を取る（札は番号のパスを後から書く）。
[[nodiscard]] std::size_t open_new(World &world, std::size_t)
{
    if (world.first.tab_count() >= tab_limit)
    {
        return outcome_idle;
    }
    Model &model = world.second;
    const std::uint64_t id = next_of(model)++;
    const std::size_t at = active_of(model) + 1U;
    world.first = world.first.with_new_tab().with_document(document_of(id));
    touched(order_of(model), active_of(model));
    opened(order_of(model), at);
    band_of(model).insert(std::next(band_of(model).begin(), distance_of(at)), Tab{id, true});
    active_of(model) = at;
    walking_of(model) = false;
    return outcome_changed;
}

// 閉じた後のアクティブの位置。値なしは「隣がまだ読んでいないアクティブ」で閉じない。
[[nodiscard]] std::optional<std::size_t> active_after_close(const Model &model,
                                                            std::size_t position)
{
    const std::size_t active = active_of(model);
    if (position != active)
    {
        return position < active ? active - 1U : active;
    }
    const std::size_t neighbour = active + 1U < band_of(model).size() ? active + 1U : active - 1U;
    if (!band_of(model).at(neighbour).second)
    {
        return std::nullopt;
    }
    return neighbour > position ? position : neighbour;
}

[[nodiscard]] std::size_t close_at(World &world, std::size_t argument)
{
    const std::size_t position = position_from(world, argument);
    world.first = world.first.with_closed(position);
    Model &model = world.second;
    Band &band = band_of(model);
    if (position >= band.size())
    {
        return outcome_out_of_range;
    }
    if (band.size() == 1U)
    {
        return outcome_last_tab;
    }
    const auto next_active = active_after_close(model, position);
    if (!next_active.has_value())
    {
        return outcome_unread;
    }
    retired_of(model).insert(band.at(position).first);
    band.erase(std::next(band.begin(), distance_of(position)));
    closed(order_of(model), position);
    active_of(model) = next_active.value();
    touched(order_of(model), active_of(model));
    walking_of(model) = false;
    return outcome_changed;
}

// 帯の右端へ count 本を足し、振った番号を返す（復元と自動退避から戻した文書の共通）。使った順は
// 足したものを順に先頭へ置き、最後にアクティブを先頭へ戻す。歩いている間でもアクティブを先頭へ
// 戻す（印は立ったまま）のは製品のとおり。どちらも起動と自動退避から戻すときだけ呼ばれ、
// controller の道で歩きの途中に呼ばれることは無い。ADR 0058 の「歩きの途中は列を入れ替えない」は
// with_walked の規則で、ここには掛からない（#935 の設計の答え）。
[[nodiscard]] std::vector<std::uint64_t> appended_ids(Model &model, std::size_t count, bool loaded)
{
    std::vector<std::uint64_t> ids;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::uint64_t id = next_of(model)++;
        opened(order_of(model), band_of(model).size());
        band_of(model).emplace_back(id, loaded);
        ids.push_back(id);
    }
    touched(order_of(model), active_of(model));
    return ids;
}

[[nodiscard]] std::size_t count_from(const World &world, std::size_t argument)
{
    return std::min(argument % 3U, tab_limit - world.first.tab_count());
}

[[nodiscard]] std::size_t restore(World &world, std::size_t argument)
{
    const std::size_t count = count_from(world, argument);
    std::vector<app::UnloadedDocument> documents;
    for (const std::uint64_t id : appended_ids(world.second, count, false))
    {
        documents.push_back(unloaded_of(id));
    }
    world.first = world.first.with_restored(std::move(documents));
    return count == 0U ? outcome_idle : outcome_changed;
}

[[nodiscard]] std::size_t append(World &world, std::size_t argument)
{
    const std::size_t count = count_from(world, argument);
    std::vector<app::DocumentState> documents;
    for (const std::uint64_t id : appended_ids(world.second, count, true))
    {
        documents.push_back(bundle_of(id));
    }
    world.first = world.first.with_appended(std::move(documents));
    return count == 0U ? outcome_idle : outcome_changed;
}

[[nodiscard]] std::size_t refused_outcome(const Model &model, std::size_t position)
{
    return position >= band_of(model).size() ? outcome_out_of_range : outcome_idle;
}

// 読んだ束は同じ札（同じファイル）を持つ。範囲の外には使われない札を渡す。
[[nodiscard]] std::size_t load(World &world, std::size_t argument)
{
    const std::size_t position = position_from(world, argument);
    Model &model = world.second;
    const std::uint64_t id =
        position < band_of(model).size() ? band_of(model).at(position).first : 999U;
    world.first = world.first.with_loaded(position, bundle_of(id));
    if (!unread_at(model, position))
    {
        return refused_outcome(model, position);
    }
    band_of(model).at(position).second = true;
    return outcome_loaded;
}

[[nodiscard]] std::size_t drop(World &world, std::size_t argument)
{
    const std::size_t position = position_from(world, argument);
    world.first = world.first.with_dropped(position);
    Model &model = world.second;
    if (!unread_at(model, position))
    {
        return refused_outcome(model, position);
    }
    Band &band = band_of(model);
    retired_of(model).insert(band.at(position).first);
    band.erase(std::next(band.begin(), distance_of(position)));
    closed(order_of(model), position);
    active_of(model) -= position < active_of(model) ? 1U : 0U;
    return outcome_dropped;
}

using Apply = std::size_t (*)(World &, std::size_t);
// 切り替え・歩き・閉じるを多めにして、読んでいないタブと範囲の外に当たる組を作る。
constexpr std::array<Apply, 14> operations{
    switch_to, switch_to, walk_to, walk_to, walk_to, settle, open_new,
    close_at,  close_at,  restore, restore, append,  load,   drop,
};

// ---------------------------------------------------------------- 各歩の検査

// 不変量 1: 使った順は帯の位置の置換で、歩いていなければ先頭がアクティブ。
[[nodiscard]] std::string_view recency_violation(const EditorState &state)
{
    const auto order = state.recency().order();
    if (order.size() != state.tab_count())
    {
        return "the recency order does not hold every tab";
    }
    std::vector<bool> seen(order.size(), false);
    for (const std::size_t position : order)
    {
        if (position >= seen.size() || seen.at(position))
        {
            return "the recency order is not a permutation of the band positions";
        }
        seen.at(position) = true;
    }
    return state.tab_walking() || order.front() == state.active_tab()
               ? std::string_view{}
               : "the most recent tab is not the active tab outside a walk";
}

// 不変量 2: 退避番号は本数ぶんで重複せず、閉じた番号は戻らず、模型の通し番号と一致する。
[[nodiscard]] std::string_view ids_violation(const World &world)
{
    const auto &ids = world.first.autosave_ids();
    const Band &band = band_of(world.second);
    if (ids.size() != world.first.tab_count() || ids.size() != band.size())
    {
        return "autosave_ids does not have one number per tab";
    }
    std::set<std::uint64_t> seen;
    for (std::size_t position = 0; position < ids.size(); ++position)
    {
        const std::uint64_t id = ids.at(position).value;
        if (!seen.insert(id).second)
        {
            return "an autosave number is given to two tabs";
        }
        if (retired_of(world.second).contains(id))
        {
            return "a closed or dropped autosave number comes back";
        }
        if (band.at(position).first != id)
        {
            return "autosave_ids differs from the serial numbers the model gave";
        }
    }
    return {};
}

[[nodiscard]] std::string_view parked_label(const app::ParkedTab &tab)
{
    return std::visit(
        [](const auto &parked) -> std::string_view
        {
            using Parked = std::decay_t<decltype(*parked)>;
            if constexpr (std::is_same_v<Parked, app::DocumentState>)
            {
                return parked->document.path.has_value() ? parked->document.path.value().text()
                                                         : std::string_view{};
            }
            else
            {
                return parked->path.text();
            }
        },
        tab);
}

[[nodiscard]] std::string_view active_label(const EditorState &state)
{
    const auto &path = state.document().path;
    return path.has_value() ? path.value().text() : std::string_view{};
}

// 帯の位置 position のタブが模型の札と形（読み込み済みか）を持つか。
[[nodiscard]] bool tab_matches(const World &world, std::size_t position)
{
    const EditorState &state = world.first;
    const Tab &tab = band_of(world.second).at(position);
    if (position == state.active_tab())
    {
        return tab.second && active_label(state) == label_of(tab.first);
    }
    const std::size_t parked = position < state.active_tab() ? position : position - 1U;
    const app::ParkedTab &found = state.parked().at(parked);
    const bool loaded = std::holds_alternative<std::shared_ptr<const app::DocumentState>>(found);
    return loaded == tab.second && parked_label(found) == label_of(tab.first);
}

// 不変量 3 の本数・4・5 の帯の順: 脇の束の数 + 1 が本数で、どの位置も模型の札と形を持ち、まだ
// 読んでいない文書を答えるのはアクティブではない読んでいないタブだけ（範囲の外は値なし）。
[[nodiscard]] std::string_view band_violation(const World &world)
{
    const EditorState &state = world.first;
    if (state.parked().size() + 1U != state.tab_count())
    {
        return "the parked tabs plus the active tab are not the tab count";
    }
    for (std::size_t position = 0; position < state.tab_count(); ++position)
    {
        if (!tab_matches(world, position))
        {
            return "a tab in the band differs from the model in label or loaded form";
        }
    }
    for (std::size_t position = 0; position < state.tab_count() + 2U; ++position)
    {
        if (state.unloaded_at(position).has_value() != unread_at(world.second, position))
        {
            return "unloaded_at answers for a tab that is active, loaded or out of range";
        }
    }
    return {};
}

// 不変量 5: アクティブ・使った順・歩いている印が模型と一致する。
[[nodiscard]] std::string_view model_violation(const World &world)
{
    const EditorState &state = world.first;
    const Model &model = world.second;
    if (state.active_tab() != active_of(model))
    {
        return "the active tab differs from the model";
    }
    const auto order = state.recency().order();
    if (!std::ranges::equal(order, order_of(model)))
    {
        return "the recency order differs from the model";
    }
    return state.tab_walking() == walking_of(model) ? std::string_view{}
                                                    : "the walking mark differs from the model";
}

[[nodiscard]] const void *pointer_of(const app::ParkedTab &tab)
{
    return std::visit([](const auto &parked) -> const void * { return parked.get(); }, tab);
}

// 脇の束を退避番号から参照へ写す。
[[nodiscard]] std::map<std::uint64_t, const void *> parked_pointers(const EditorState &state)
{
    std::map<std::uint64_t, const void *> pointers;
    for (std::size_t index = 0; index < state.parked().size(); ++index)
    {
        const std::size_t position = index < state.active_tab() ? index : index + 1U;
        pointers.emplace(state.autosave_ids().at(position).value,
                         pointer_of(state.parked().at(index)));
    }
    return pointers;
}

[[nodiscard]] bool loaded_in(const Model &model, std::uint64_t id)
{
    const auto found = std::ranges::find(band_of(model), id, &Tab::first);
    return found != band_of(model).end() && found->second;
}

// 操作の前に覚えておく分: 脇の束の参照と模型（状態の写しは持たない）。
[[nodiscard]] Before before_of(const World &world)
{
    return Before{parked_pointers(world.first), world.second};
}

// 不変量 3 の参照: 前後とも脇にあり読み込みの形が変わっていないタブの束は同じ参照のまま。
[[nodiscard]] std::string_view pointer_violation(const Before &before, const World &after)
{
    const auto &earlier = before.first;
    for (const auto &[id, pointer] : parked_pointers(after.first))
    {
        const auto found = earlier.find(id);
        if (found != earlier.end() && loaded_in(before.second, id) == loaded_in(after.second, id) &&
            found->second != pointer)
        {
            return "a parked tab the operation did not touch got another bundle";
        }
    }
    return {};
}

[[nodiscard]] std::string_view step_violation(const Before &before, const World &after)
{
    std::string_view reason = recency_violation(after.first);
    reason = reason.empty() ? ids_violation(after) : reason;
    reason = reason.empty() ? band_violation(after) : reason;
    reason = reason.empty() ? model_violation(after) : reason;
    return reason.empty() ? pointer_violation(before, after) : reason;
}

// 操作の列を当て、最初に破れた検査の理由を返す。
[[nodiscard]] std::string_view case_violation(const Case &input)
{
    World world = fresh();
    for (const auto &[row, argument] : input)
    {
        const Before before = before_of(world);
        static_cast<void>(operations.at(row % operations.size())(world, argument));
        const std::string_view reason = step_violation(before, world);
        if (!reason.empty())
        {
            return reason;
        }
    }
    return {};
}

[[nodiscard]] Case random_case(Random &random)
{
    Case input;
    const std::size_t steps = 1U + below(random, operation_limit);
    for (std::size_t index = 0; index < steps; ++index)
    {
        input.emplace_back(below(random, operations.size()), below(random, 4U * tab_limit));
    }
    return input;
}

// 破れた組を縮める: 操作を 1 つずつ外し、破れが残るなら外したままにする。
void minimize(Case &input, Check check)
{
    for (std::size_t index = input.size(); index-- > 0;)
    {
        Case smaller = input;
        smaller.erase(std::next(smaller.begin(), distance_of(index)));
        if (!check(smaller).empty())
        {
            input = std::move(smaller);
        }
    }
}

void report(const char *family, Case input, Check check)
{
    minimize(input, check);
    const std::string_view reason = check(input);
    std::fprintf(stderr, "  %s: %.*s\n    operations:", family, static_cast<int>(reason.size()),
                 reason.data());
    for (const auto &[row, argument] : input)
    {
        std::fprintf(stderr, " %zu/%zu", row % operations.size(), argument);
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

// ---------------------------------------------------------------- 歩きと行き先

// 素朴な使った順と製品の TabRecency を同じ操作で作る（本数 1〜16）。
[[nodiscard]] std::pair<core::TabRecency, Order> random_recency(Random &random)
{
    core::TabRecency recency = core::TabRecency::single();
    Order order{0U};
    const std::size_t steps = below(random, 48U);
    for (std::size_t index = 0; index < steps; ++index)
    {
        const std::size_t kind = below(random, 3U);
        const std::size_t tab = below(random, order.size() + 2U);
        if (kind == 0U && order.size() < tab_limit && tab <= order.size())
        {
            recency = core::tab_recency_opened(recency, tab);
            opened(order, tab);
        }
        else if (kind == 1U && tab < order.size() && order.size() > 1U)
        {
            recency = core::tab_recency_closed(recency, tab);
            closed(order, tab);
        }
        else
        {
            recency = core::tab_recency_touched(recency, tab);
            touched(order, tab);
        }
    }
    return {recency, order};
}

// from から一歩ぶんの素朴な答え。next は列の次（より前に使った）、previous は前。端は折り返す。
[[nodiscard]] std::optional<std::size_t> naive_walk(const Order &order, std::size_t from,
                                                    core::TabStep step)
{
    const auto found = std::ranges::find(order, from);
    if (found == order.end())
    {
        return std::nullopt;
    }
    const auto index = static_cast<std::size_t>(std::distance(order.begin(), found));
    const std::size_t count = order.size();
    return step == core::TabStep::next ? order.at((index + 1U) % count)
                                       : order.at((index + count - 1U) % count);
}

[[nodiscard]] bool walk_round_trips(const core::TabRecency &recency, std::size_t from)
{
    const auto next = core::tab_recency_walked(recency, from, core::TabStep::next);
    if (!next.has_value())
    {
        return false;
    }
    const auto back = core::tab_recency_walked(recency, next.value(), core::TabStep::previous);
    return back.has_value() && back.value() == from;
}

// 両向きの一歩が素朴な答えと一致し、列にある位置は next の後の previous で戻る。
[[nodiscard]] bool walks_agree(const core::TabRecency &recency, const Order &order,
                               std::size_t from)
{
    const bool same = core::tab_recency_walked(recency, from, core::TabStep::next) ==
                          naive_walk(order, from, core::TabStep::next) &&
                      core::tab_recency_walked(recency, from, core::TabStep::previous) ==
                          naive_walk(order, from, core::TabStep::previous);
    return same && (from >= order.size() || walk_round_trips(recency, from));
}

[[nodiscard]] std::size_t broken_walks()
{
    Random random{9351U};
    std::size_t broken = 0;
    for (std::size_t round = 0; round < 400U * rounds; ++round)
    {
        const auto [recency, order] = random_recency(random);
        broken += std::ranges::equal(recency.order(), order) ? 0U : 1U;
        for (std::size_t from = 0; from < order.size() + 2U; ++from)
        {
            broken += walks_agree(recency, order, from) ? 0U : 1U;
        }
    }
    return broken;
}

// 行き先の素朴な式（TabDestination.hpp の注釈・ADR 0057 の決定 1）。
[[nodiscard]] std::optional<std::size_t> naive_destination(const core::TabJump &jump,
                                                           std::size_t active, std::size_t count)
{
    if (active >= count)
    {
        return std::nullopt;
    }
    const auto signed_count = static_cast<std::int64_t>(count);
    const auto from = static_cast<std::int64_t>(active);
    if (jump.direction == core::TabJumpDirection::forward)
    {
        if (!jump.count.has_value())
        {
            return static_cast<std::size_t>((from + 1) % signed_count);
        }
        const std::size_t number = jump.count.value();
        return number >= 1U && number <= count ? std::optional<std::size_t>{number - 1U}
                                               : std::nullopt;
    }
    const auto steps = static_cast<std::int64_t>(jump.count.value_or(1U));
    if (steps == 0)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(((from - steps) % signed_count + signed_count) % signed_count);
}

[[nodiscard]] std::size_t broken_destinations_at(std::size_t count, std::size_t active)
{
    std::size_t broken = 0;
    for (const core::TabJumpDirection direction :
         {core::TabJumpDirection::forward, core::TabJumpDirection::backward})
    {
        for (std::size_t number = 0; number <= 41U; ++number)
        {
            // 41 は回数なしの印（回数は 0〜40）。
            const core::TabJump jump{direction, number == 41U ? std::optional<std::size_t>{}
                                                              : std::optional<std::size_t>{number}};
            broken +=
                core::tab_destination(jump, active, count) == naive_destination(jump, active, count)
                    ? 0U
                    : 1U;
        }
    }
    return broken;
}

// 本数 1〜16・今の位置は範囲の外 1 つまで・回数なしと回数 0〜40 を全数で比べる。
[[nodiscard]] std::size_t broken_destinations()
{
    std::size_t broken = 0;
    for (std::size_t count = 1; count <= tab_limit; ++count)
    {
        for (std::size_t active = 0; active <= count; ++active)
        {
            broken += broken_destinations_at(count, active);
        }
    }
    return broken;
}

// ---------------------------------------------------------------- 生成器と検査そのもの

// 生成器が「変えない」側の規則（範囲の外・読んでいないタブ・最後の 1 本・アクティブ自身・歩いて
// いない確定）と、読む・外す・歩きの確定の全部に届き、本数が多くなる。どれかが 0 なら性質の試験は
// その枝を見ていない。
void verify_generator_reaches_the_rules()
{
    Random random{9352U};
    std::array<std::size_t, outcome_count> outcomes{};
    std::size_t largest = 0;
    for (std::size_t index = 0; index < coverage_cases; ++index)
    {
        const Case input = random_case(random);
        World world = fresh();
        for (const auto &[row, argument] : input)
        {
            ++outcomes.at(operations.at(row % operations.size())(world, argument));
            largest = std::max(largest, world.first.tab_count());
        }
    }
    expect(std::ranges::all_of(outcomes, [](std::size_t count) { return count > 0U; }) &&
               largest >= tab_limit / 2U,
           "the generated cases reach every unchanged rule, loading, dropping, settling and "
           "many tabs");
}

void verify_generator_is_deterministic()
{
    Random left{9353U};
    Random right{9353U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && random_case(left) == random_case(right);
    }
    expect(same, "the generator gives the same cases for the same seed");
}

[[nodiscard]] World world_of(const Case &input)
{
    World world = fresh();
    for (const auto &[row, argument] : input)
    {
        static_cast<void>(operations.at(row % operations.size())(world, argument));
    }
    return world;
}

// 「変えない」側の規則の回帰の組（行の番号は operations の添字・引数は本数 + 2 で畳む）。
void verify_known_cases()
{
    // 復元で [0 u1]、アクティブ 0 を閉じると右隣が読んでいないので閉じない。
    const Case unread_neighbour{{9U, 1U}, {7U, 0U}};
    const World kept = world_of(unread_neighbour);
    expect(case_violation(unread_neighbour).empty() && kept.first.tab_count() == 2U,
           "closing the active tab whose neighbour is not read changes nothing");
    // 読んでいないタブへは切り替えも歩きもしない。
    const Case unread_target{{9U, 1U}, {0U, 1U}, {2U, 1U}};
    const World stayed = world_of(unread_target);
    expect(case_violation(unread_target).empty() && stayed.first.active_tab() == 0U &&
               !stayed.first.tab_walking(),
           "switching or walking to a tab not read yet changes nothing");
    // 読んでから外すと番号は戻らず、新しいタブは次の番号を取る。
    const Case dropped{{9U, 2U}, {13U, 1U}, {12U, 1U}, {6U, 0U}};
    const World after = world_of(dropped);
    expect(case_violation(dropped).empty() && after.first.autosave_ids().size() == 3U &&
               after.first.autosave_ids().at(1).value == 3U,
           "a dropped tab's number is not reused and the new tab takes the next number");
    // 最後の 1 本と範囲の外は閉じない。
    const Case last{{7U, 0U}, {7U, 1U}};
    expect(case_violation(last).empty() && world_of(last).first.tab_count() == 1U,
           "the last tab and positions out of range are not closed");
    // 歩いている間に閉じると歩きは確定し、新しいアクティブが先頭。
    const Case walked{{6U, 0U}, {6U, 0U}, {2U, 0U}, {7U, 1U}};
    expect(case_violation(walked).empty() && !world_of(walked).first.tab_walking(),
           "closing a tab during a walk settles the walk");
}

// 検査そのものが破れを見つける（模型の 1 欄を書き換えると落ちる）。
void verify_checks_catch_a_broken_model()
{
    const World world = world_of({{6U, 0U}, {6U, 0U}, {9U, 1U}, {2U, 0U}});
    expect(step_violation(before_of(world), world).empty(),
           "the planted world keeps every invariant");
    World swapped = world;
    std::ranges::reverse(order_of(swapped.second));
    World reused = world;
    retired_of(reused.second).insert(band_of(reused.second).front().first);
    World relabelled = world;
    band_of(relabelled.second).back().second = true;
    World moved = world;
    active_of(moved.second) = 1U;
    expect(!model_violation(swapped).empty() && !ids_violation(reused).empty() &&
               !band_violation(relabelled).empty() && !model_violation(moved).empty(),
           "the checks catch a wrong recency, a reused number, a wrong form and a wrong active");
}
} // namespace

void verify_tabs_property_scope()
{
    verify_generator_is_deterministic();
    verify_generator_reaches_the_rules();
    verify_known_cases();
    verify_checks_catch_a_broken_model();
    expect(broken_cases("tabs keep their invariants", 9354U, case_violation) == 0U,
           "switching, walking, opening, closing, restoring, loading and dropping tabs keep the "
           "recency, the autosave numbers, the parked bundles and the model");
    expect(broken_walks() == 0U,
           "tab_recency_walked follows the naive order, wraps at the ends and round trips");
    expect(broken_destinations() == 0U,
           "tab_destination follows the naive formula for 1 to 16 tabs and counts 0 to 40");
}
} // namespace nenenib::tests
