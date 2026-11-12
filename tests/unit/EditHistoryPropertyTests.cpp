// scope `--edit-history-property` の単体テスト（#1473・ADR 0180）。undo の履歴は固定の件数の塊の
// 不変の列で、状態の写しは束を共有する。塊の境（255 → 256 → 257 件）・塊をまたぐ undo / redo・
// undo の後の別の編集（redo の枝を捨てる）・封じた後の push・空の履歴・古い値が後の push で
// 変わらないことを確かめ、最後に固定 seed のランダムな push（3 つの区切り）・undo・redo・封じるの
// 列を、毎回 std::vector を写す素朴な参照の模型と照合する（件数・位置・改訂番号・undo / redo の
// 答え・適用済みの編集）。空の単位と本文の位置（#1440・ADR 0181）は、空の単位を混ぜた列（seed
// 1440〜1442）で同じ模型と照合し（本文の位置は模型が [0, 位置) の changed を数え直す）、空の単位の
// 畳み方と undo / redo の往復は手で書いた形でも確かめる。
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "HistoryFailure.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "VimChange.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::absorbed;
using core::Edit;
using core::EditBoundary;
using core::EditHistory;
using core::HistoryFailure;
using core::Offset;
using core::TextBuffer;
using core::VimChange;

using Random = std::mt19937_64;

// 区切りで閉じた単位を n 個積む（at は番号・挿入 1 文字・畳まれない）。
[[nodiscard]] Edit numbered(std::size_t index)
{
    return Edit{Offset{index}, "", std::to_string(index), Offset{index}};
}

[[nodiscard]] EditHistory separated(std::size_t count)
{
    EditHistory history = EditHistory::empty();
    for (std::size_t index = 0; index < count; ++index)
    {
        history = history.pushed(numbered(index), EditBoundary::separate);
    }
    return history;
}

// 0 から position までの適用済みの編集と、undo / redo の答えが番号の列と一致するか。
[[nodiscard]] bool numbered_through(const EditHistory &history, std::size_t position)
{
    for (std::size_t index = 0; index < position; ++index)
    {
        if (!(history.applied(index) == std::optional<Edit>{numbered(index)}))
        {
            return false;
        }
    }
    const bool undo_ok =
        position == 0 ? !history.undo().has_value() : history.undo() == numbered(position - 1);
    return history.position() == position && undo_ok && !history.applied(position).has_value();
}

void verify_block_edges()
{
    for (const std::size_t count : {std::size_t{255}, std::size_t{256}, std::size_t{257},
                                    std::size_t{511}, std::size_t{512}, std::size_t{513}})
    {
        const EditHistory history = separated(count);
        expect(history.size() == count && history.revision() == count &&
                   numbered_through(history, count) && !history.redo().has_value(),
               "the history keeps every unit across the 256-entry block edges");
    }
    // 257 件から 2 回戻すと塊の境をまたいで 255 件。redo は 256 件目と 257 件目を順に返す。
    const EditHistory full = separated(257);
    const EditHistory back = full.undone().undone();
    expect(numbered_through(back, 255) && back.size() == 257 && back.redo() == numbered(255) &&
               back.revision() == 259,
           "undo walks back across a block edge without dropping the redo entries");
    const EditHistory forward = back.redone().redone();
    expect(numbered_through(forward, 257) && !forward.redo().has_value() &&
               forward.revision() == 261,
           "redo walks forward across a block edge");
    // 戻した後の別の編集は redo の枝を捨て、塊の境の手前の位置に足す。
    const Edit other{Offset{9000}, "", "z", Offset{9000}};
    const EditHistory branched = back.pushed(other, EditBoundary::separate);
    expect(branched.size() == 256 && branched.position() == 256 && branched.undo() == other &&
               !branched.redo().has_value() && numbered_through(branched.undone(), 255),
           "an edit after undo drops the redo branch at a block edge");
    // 元の値（back と full）は後の push で変わらない（束は共有の不変の値）。
    expect(numbered_through(back, 255) && back.size() == 257 && back.redo() == numbered(255) &&
               numbered_through(full, 257),
           "pushing onto a history leaves the earlier values unchanged");
}

void verify_folding_and_sealing()
{
    // 255 件の後の 256 件目（index 255・塊 0 の最後）にまとめた打鍵を畳む。畳んでも件数は増えず、
    // 改訂番号は進む。塊の頭（index 256 / 512）へ畳む形は verify_folding_at_block_heads。
    EditHistory history = separated(255);
    history = history.pushed(Edit{Offset{1000}, "", "a", Offset{1000}}, EditBoundary::coalesce);
    history = history.pushed(Edit{Offset{1001}, "", "b", Offset{1001}}, EditBoundary::coalesce);
    expect(history.size() == 256 && history.revision() == 257 &&
               history.undo() == Edit{Offset{1000}, "", "ab", Offset{1000}} &&
               numbered_through(history.undone(), 255),
           "a coalesced unit on a block edge folds into one entry");
    // 封じた後の push は畳まれず、封じても改訂番号は変わらない。
    const EditHistory sealed = history.sealed();
    expect(sealed.revision() == history.revision() && sealed.size() == history.size(),
           "sealing keeps the size and the revision");
    const EditHistory after =
        sealed.pushed(Edit{Offset{1002}, "", "c", Offset{1002}}, EditBoundary::coalesce);
    expect(after.size() == 257 && after.undo() == Edit{Offset{1002}, "", "c", Offset{1002}},
           "a push after sealing opens a new unit");
    // 空の履歴。
    const EditHistory empty = EditHistory::empty();
    expect(empty.size() == 0 && empty.position() == 0 && empty.revision() == 0 &&
               empty.undo() == std::unexpected(HistoryFailure::nothing_to_undo) &&
               empty.redo() == std::unexpected(HistoryFailure::nothing_to_redo) &&
               empty.undone().position() == 0 && empty.redone().position() == 0 &&
               !empty.applied(0).has_value(),
           "an empty history has nothing to undo or redo");
}

// 塊の頭（index 256 / 512・kept_with の rest が 0）へ畳む（レビュー V-1481 の
// L1）。新しい塊は畳んだ 1 件だけで、前の塊は共有のまま。
void verify_folding_at_block_heads()
{
    for (const std::size_t count : {std::size_t{256}, std::size_t{512}})
    {
        const EditHistory before = separated(count);
        const EditHistory folded =
            before.pushed(Edit{Offset{1000}, "", "a", Offset{1000}}, EditBoundary::coalesce)
                .pushed(Edit{Offset{1001}, "", "b", Offset{1001}}, EditBoundary::coalesce);
        expect(folded.size() == count + 1 && folded.revision() == count + 2 &&
                   folded.undo() == Edit{Offset{1000}, "", "ab", Offset{1000}} &&
                   numbered_through(folded.undone(), count) && numbered_through(before, count),
               "a coalesced unit at the head of a block folds into one entry");
    }
}

// 本文の値の写しは piece の列を共有し、写した側の編集は元の値を変えない（ADR 0180 の決定 1・3）。
void verify_shared_pieces()
{
    const TextBuffer original = TextBuffer::from_utf8("abc\ndef").value();
    const TextBuffer copy = original;
    const TextBuffer edited = copy.replaced(Offset{1}, Offset{2}, "XY");
    expect(original.text() == "abc\ndef" && copy.text() == "abc\ndef" &&
               edited.text() == "aXYc\ndef" && original.piece_count() == copy.piece_count(),
           "a copied text shares its pieces and an edit of the copy leaves the original alone");
}

// 空の単位（ADR 0181 の決定 3）。本文に触れず、戻り先だけを持つ。
[[nodiscard]] bool empty_unit(const Edit &edit) noexcept
{
    return edit.removed.empty() && edit.inserted.empty();
}

// 素朴な参照の模型の畳み方（EditHistory.cpp の folded と同じ規則を試験の側で書き直したもの）。
// 空の編集は前に畳まず、空の単位へは absorb だけが畳む（ADR 0181 の決定 3）。
[[nodiscard]] std::optional<Edit> model_folded(const Edit &previous, const Edit &edit,
                                               EditBoundary boundary)
{
    if (empty_unit(edit))
    {
        return std::nullopt;
    }
    switch (boundary)
    {
    case EditBoundary::separate:
        return std::nullopt;
    case EditBoundary::coalesce:
        if (empty_unit(previous) || !previous.removed.empty() || !edit.removed.empty() ||
            previous.at.value + previous.inserted.size() != edit.at.value)
        {
            return std::nullopt;
        }
        return Edit{previous.at, previous.removed, previous.inserted + edit.inserted,
                    previous.restore};
    case EditBoundary::absorb:
        return absorbed(previous, edit);
    }
    std::unreachable();
}

// 前の単位に続く位置を選びやすい編集（畳まれる枝を踏む）。
[[nodiscard]] Edit drawn_edit(Random &random, const std::vector<Edit> &edits, std::size_t position)
{
    const std::uint64_t shape = random() % 4U;
    std::size_t at = static_cast<std::size_t>(random() % 64U);
    if (position > 0 && shape < 2U)
    {
        const Edit &previous = edits.at(position - 1);
        const std::size_t end = previous.at.value + previous.inserted.size();
        // 1 つ目の形は直後（畳まれる）、2 つ目は半分の確率で 1 つ手前（absorb の枝）。
        at = shape == 1U && at % 2U == 0U && end > 0 ? end - 1 : end;
    }
    std::string removed = shape == 3U ? std::string(1 + random() % 2U, 'r') : std::string{};
    std::string inserted(random() % 3U, static_cast<char>('a' + random() % 26U));
    return Edit{Offset{at}, std::move(removed), std::move(inserted), Offset{at}};
}

// 素朴な参照の模型（古い実装と同じく、足すたびに vector を写す）。edits の [0, 位置) が適用済み。
// 数は位置・改訂番号・末尾の単位の区切り（boundaries の添字）・塊の頭へ畳んだ回数・畳む歩の多い
// 列か（0 / 1）・空の単位を混ぜる列か（0 / 1）。changes は edits と同じ添字の印（ADR 0181）。
using ModelCounts = std::array<std::size_t, 6>;
using Model = std::tuple<std::vector<Edit>, ModelCounts, std::vector<VimChange>>;
constexpr std::size_t model_position = 0;
constexpr std::size_t model_revision = 1;
constexpr std::size_t model_tail = 2;
constexpr std::size_t model_head_folds = 3;
constexpr std::size_t model_folding = 4;
constexpr std::size_t model_units = 5;
// 製品の塊の件数（EditHistory.cpp の history_block_entries）と同じ値。塊の頭へ畳んだ回数を数える
// ためだけに使い、照合の答えには使わない。
constexpr std::size_t block_entries = 256;
constexpr std::array<EditBoundary, 3> boundaries{EditBoundary::separate, EditBoundary::coalesce,
                                                 EditBoundary::absorb};

void model_pushed(Model &model, const Edit &edit, std::size_t boundary,
                  VimChange change = VimChange::changed)
{
    auto &[edits, counts, changes] = model;
    const auto kept = static_cast<std::ptrdiff_t>(counts.at(model_position));
    std::vector<Edit> next(edits.begin(), edits.begin() + kept);
    std::vector<VimChange> marks(changes.begin(), changes.begin() + kept);
    const auto merged = counts.at(model_tail) == boundary && !next.empty()
                            ? model_folded(next.back(), edit, boundaries.at(boundary))
                            : std::nullopt;
    if (merged.has_value())
    {
        next.back() = merged.value();
        marks.back() = marks.back() == VimChange::changed ? VimChange::changed : change;
        counts.at(model_head_folds) +=
            next.size() - 1 >= block_entries && (next.size() - 1) % block_entries == 0 ? 1U : 0U;
    }
    else
    {
        next.push_back(edit);
        marks.push_back(change);
    }
    changes = std::move(marks);
    edits = std::move(next);
    counts.at(model_position) = edits.size();
    ++counts.at(model_revision);
    counts.at(model_tail) = boundary;
}

// 畳む歩の多い列の push: 3 割が区切り（新しい単位）、7 割が前の単位の直後へ続く coalesce（畳む）。
// 区切りで塊の境を越えた直後の単位にも畳むので、塊の頭への畳みを踏む（レビュー V-1481 の L1）。
[[nodiscard]] EditHistory folding_pushed(Random &random, const EditHistory &history, Model &model)
{
    const std::size_t position = std::get<1>(model).at(model_position);
    const bool separate = random() % 10U < 3U || position == 0;
    const std::size_t at = position == 0 ? 0
                                         : std::get<0>(model).at(position - 1).at.value +
                                               std::get<0>(model).at(position - 1).inserted.size();
    const Edit edit{Offset{at}, "", "f", Offset{at}};
    const std::size_t boundary = separate ? 0U : 1U;
    model_pushed(model, edit, boundary);
    return history.pushed(edit, boundaries.at(boundary));
}

// push でない 1 歩（undo・redo・封じる）。封じるのは畳む歩の多くない列の action 9 だけ。undo は
// action 6〜7（畳む歩の多い列は 8）、残りは redo。
[[nodiscard]] EditHistory moved(std::uint64_t action, const EditHistory &history, Model &model)
{
    auto &[edits, counts, changes] = model;
    const bool folding = counts.at(model_folding) != 0;
    std::size_t &position = counts.at(model_position);
    counts.at(model_tail) = 0;
    if (action == 9U && !folding)
    {
        return history.sealed();
    }
    ++counts.at(model_revision);
    if (action < 8U || (folding && action == 8U))
    {
        position = position == 0 ? 0 : position - 1;
        return history.undone();
    }
    position = position >= edits.size() ? edits.size() : position + 1;
    return history.redone();
}

// 空の単位を混ぜる列の push（ADR 0181）。4 回に 1 回は戻り先だけの空の単位（印は半々）、残りは
// 前の単位の直後へ続く印つきの編集（空の単位へ absorb が畳む枝を踏む）。
[[nodiscard]] EditHistory unit_pushed(Random &random, const EditHistory &history, Model &model)
{
    const auto boundary = static_cast<std::size_t>(random() % boundaries.size());
    const VimChange change = random() % 2U == 0U ? VimChange::changed : VimChange::saved_only;
    Edit edit = drawn_edit(random, std::get<0>(model), std::get<1>(model).at(model_position));
    if (random() % 4U == 0U)
    {
        edit.removed.clear();
        edit.inserted.clear();
    }
    const VimChange marked = empty_unit(edit) ? change : VimChange::changed;
    model_pushed(model, edit, boundary, marked);
    return history.pushed(edit, boundaries.at(boundary), marked);
}

// 1 歩を履歴と模型の両方に施した次の履歴（6 割が push・2 割が undo・1 割ずつ redo
// と封じる。畳む歩の 多い列は 8 割が push・1 割ずつ undo と redo・封じない）。
[[nodiscard]] EditHistory stepped(Random &random, const EditHistory &history, Model &model)
{
    const bool folding = std::get<1>(model).at(model_folding) != 0;
    const std::uint64_t action = random() % 10U;
    if (folding && action < 8U)
    {
        return folding_pushed(random, history, model);
    }
    if (!folding && action < 6U && std::get<1>(model).at(model_units) != 0)
    {
        return unit_pushed(random, history, model);
    }
    if (!folding && action < 6U)
    {
        const auto boundary = static_cast<std::size_t>(random() % boundaries.size());
        const Edit edit =
            drawn_edit(random, std::get<0>(model), std::get<1>(model).at(model_position));
        model_pushed(model, edit, boundary);
        return history.pushed(edit, boundaries.at(boundary));
    }
    return moved(action, history, model);
}

// 模型の本文の位置（[0, 位置) の changed の数を数え直す・ADR 0181 の決定 4）。
[[nodiscard]] std::size_t model_content(const Model &model)
{
    const auto &changes = std::get<2>(model);
    const auto position = static_cast<std::ptrdiff_t>(std::get<1>(model).at(model_position));
    return static_cast<std::size_t>(
        std::count(changes.begin(), changes.begin() + position, VimChange::changed));
}

// 件数・位置・改訂番号・undo / redo の答え・probe 番目の適用済みの編集が模型と一致するか。
[[nodiscard]] bool agrees(const EditHistory &history, const Model &model, std::size_t probe)
{
    const auto &[edits, counts, changes] = model;
    const std::size_t position = counts.at(model_position);
    const bool undo_agrees =
        position == 0 ? !history.undo().has_value() : history.undo() == edits.at(position - 1);
    const bool redo_agrees = position >= edits.size() ? !history.redo().has_value()
                                                      : history.redo() == edits.at(position);
    const bool applied_agrees = probe < position
                                    ? history.applied(probe) == std::optional<Edit>{edits.at(probe)}
                                    : !history.applied(probe).has_value();
    return history.size() == edits.size() && history.position() == position &&
           history.revision() == counts.at(model_revision) && undo_agrees && redo_agrees &&
           applied_agrees && history.content_position() == model_content(model);
}

// 履歴の適用済みの全件と答えが模型と一致するか。
[[nodiscard]] bool wholly_agrees(const EditHistory &history, const Model &model)
{
    bool agreed = agrees(history, model, 0);
    for (std::size_t index = 0; index < std::get<1>(model).at(model_position) && agreed; ++index)
    {
        agreed = history.applied(index) == std::optional<Edit>{std::get<0>(model).at(index)};
    }
    return agreed;
}

// 残した古い値がどれも、その時点の模型の写しと一致するか。
[[nodiscard]] bool snapshots_agree(const std::vector<std::pair<EditHistory, Model>> &snapshots)
{
    bool agreed = true;
    for (const auto &[old_history, old_model] : snapshots)
    {
        agreed = agreed && wholly_agrees(old_history, old_model);
    }
    return agreed;
}

// seed ごとに steps 歩の列を模型と照合し、最後に適用済みの全件を照合する。途中の値（古い履歴）と
// その時点の模型の写しを 97 歩ごとに残し、最後にもう一度照合する（後の push
// が古い値の中身を変えない・ aliasing・レビュー V-1481 の L2）。folding は畳む歩の多い列、units は
// 空の単位を混ぜる列（ADR 0181）。
void verify_against_model(std::uint64_t seed, std::size_t steps, bool folding, bool units = false)
{
    constexpr std::size_t snapshot_every = 97;
    Random random{seed};
    EditHistory history = EditHistory::empty();
    Model model{};
    std::get<1>(model).at(model_folding) = folding ? 1U : 0U;
    std::get<1>(model).at(model_units) = units ? 1U : 0U;
    std::vector<std::pair<EditHistory, Model>> snapshots;
    bool agreed = true;
    for (std::size_t step = 0; step < steps && agreed; ++step)
    {
        history = stepped(random, history, model);
        const std::size_t size = std::get<0>(model).size();
        const std::size_t probe = size == 0 ? 0 : static_cast<std::size_t>(random() % size);
        agreed = agrees(history, model, probe);
        if (step % snapshot_every == 0)
        {
            snapshots.emplace_back(history, model);
        }
    }
    agreed = agreed && wholly_agrees(history, model) && snapshots_agree(snapshots);
    expect(agreed, "random pushes, undo, redo and sealing agree with the copying model");
    expect(!folding || std::get<1>(model).at(model_head_folds) > 0,
           "the folding walk folds a unit at the head of a block");
}

// 空の単位（ADR 0181 の決定 3・4）。saved_only は本文の位置を進めず、changed は進める。空の単位は
// 前の単位に畳まれず、空の単位の後ろの absorb の打鍵は空の単位に畳まれて印は changed になる。
// coalesce の打鍵は空の単位に畳まない。undo / redo は空の単位を 1
// 段として往復し、本文の位置を戻す。
void verify_empty_units()
{
    const Edit typed{Offset{0}, "", "a", Offset{0}};
    const Edit empty{Offset{1}, "", "", Offset{0}};
    const EditHistory base = EditHistory::empty().pushed(typed, EditBoundary::absorb);
    const EditHistory saved = base.pushed(empty, EditBoundary::absorb, VimChange::saved_only);
    expect(saved.size() == 2 && saved.position() == 2 && saved.content_position() == 1 &&
               saved.undo() == empty,
           "a saved_only empty unit is a new undo unit that does not move the content position");
    const EditHistory counted = base.pushed(empty, EditBoundary::separate, VimChange::changed);
    expect(counted.size() == 2 && counted.content_position() == 2,
           "a changed empty unit moves the content position");
    const EditHistory undone = saved.undone();
    expect(undone.position() == 1 && undone.content_position() == 1 && undone.undo() == typed &&
               undone.redone().content_position() == 1 && undone.redone().position() == 2,
           "undo and redo walk an empty unit as one step");
    expect(counted.undone().content_position() == 1 &&
               counted.undone().redone().content_position() == 2,
           "undoing a changed empty unit moves the content position back");
    const EditHistory absorbed_typing =
        saved.pushed(Edit{Offset{1}, "", "b", Offset{1}}, EditBoundary::absorb);
    expect(absorbed_typing.size() == 2 && absorbed_typing.content_position() == 2 &&
               absorbed_typing.undo() == Edit{Offset{1}, "", "b", Offset{0}},
           "typing after an empty unit folds into it and counts as a change");
    const EditHistory open =
        EditHistory::empty().pushed(empty, EditBoundary::coalesce, VimChange::saved_only);
    const EditHistory coalesced =
        open.pushed(Edit{Offset{1}, "", "c", Offset{1}}, EditBoundary::coalesce);
    expect(coalesced.size() == 2 && coalesced.content_position() == 1,
           "ordinary typing does not coalesce into an empty unit");
    expect(base.pushed(empty, EditBoundary::absorb, VimChange::changed).size() == 2,
           "an empty unit never folds into the unit before it");
}
} // namespace

void verify_edit_history_property_scope()
{
    verify_block_edges();
    verify_folding_and_sealing();
    verify_folding_at_block_heads();
    verify_shared_pieces();
    verify_empty_units();
    for (const std::uint64_t seed : {std::uint64_t{1473}, std::uint64_t{1474}, std::uint64_t{1475}})
    {
        verify_against_model(seed, 3000, false);
        verify_against_model(seed, 4000, true);
    }
    for (const std::uint64_t seed : {std::uint64_t{1440}, std::uint64_t{1441}, std::uint64_t{1442}})
    {
        verify_against_model(seed, 3000, false, true);
    }
}
} // namespace nenenib::tests
