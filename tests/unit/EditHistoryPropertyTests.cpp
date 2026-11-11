// scope `--edit-history-property` の単体テスト（#1473・ADR 0180）。undo の履歴は固定の件数の塊の
// 不変の列で、状態の写しは束を共有する。塊の境（255 → 256 → 257 件）・塊をまたぐ undo / redo・
// undo の後の別の編集（redo の枝を捨てる）・封じた後の push・空の履歴・古い値が後の push で
// 変わらないことを確かめ、最後に固定 seed のランダムな push（3 つの区切り）・undo・redo・封じるの
// 列を、毎回 std::vector を写す素朴な参照の模型と照合する（件数・位置・改訂番号・undo / redo の
// 答え・適用済みの編集）。
#include "Edit.hpp"
#include "EditBoundary.hpp"
#include "EditHistory.hpp"
#include "HistoryFailure.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <random>
#include <string>
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
    // 256 件目（塊の先頭）にまとめた打鍵を畳む。畳んでも件数は増えず、改訂番号は進む。
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

// 素朴な参照の模型の畳み方（EditHistory.cpp の folded と同じ規則を試験の側で書き直したもの）。
[[nodiscard]] std::optional<Edit> model_folded(const Edit &previous, const Edit &edit,
                                               EditBoundary boundary)
{
    switch (boundary)
    {
    case EditBoundary::separate:
        return std::nullopt;
    case EditBoundary::coalesce:
        if (!previous.removed.empty() || !edit.removed.empty() ||
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
// 数の 3 つは位置・改訂番号・末尾の単位の区切り（boundaries の添字）。
using ModelCounts = std::array<std::size_t, 3>;
using Model = std::pair<std::vector<Edit>, ModelCounts>;
constexpr std::size_t model_position = 0;
constexpr std::size_t model_revision = 1;
constexpr std::size_t model_tail = 2;
constexpr std::array<EditBoundary, 3> boundaries{EditBoundary::separate, EditBoundary::coalesce,
                                                 EditBoundary::absorb};

void model_pushed(Model &model, const Edit &edit, std::size_t boundary)
{
    auto &[edits, counts] = model;
    std::vector<Edit> next(edits.begin(),
                           edits.begin() + static_cast<std::ptrdiff_t>(counts.at(model_position)));
    const auto merged = counts.at(model_tail) == boundary && !next.empty()
                            ? model_folded(next.back(), edit, boundaries.at(boundary))
                            : std::nullopt;
    if (merged.has_value())
    {
        next.back() = merged.value();
    }
    else
    {
        next.push_back(edit);
    }
    edits = std::move(next);
    counts.at(model_position) = edits.size();
    ++counts.at(model_revision);
    counts.at(model_tail) = boundary;
}

// 1 歩を履歴と模型の両方に施した次の履歴（6 割が push・2 割が undo・1 割ずつ redo と封じる）。
[[nodiscard]] EditHistory stepped(Random &random, const EditHistory &history, Model &model)
{
    auto &[edits, counts] = model;
    const std::uint64_t action = random() % 10U;
    std::size_t &position = counts.at(model_position);
    if (action < 6U)
    {
        const auto boundary = static_cast<std::size_t>(random() % boundaries.size());
        const Edit edit = drawn_edit(random, edits, position);
        model_pushed(model, edit, boundary);
        return history.pushed(edit, boundaries.at(boundary));
    }
    counts.at(model_tail) = 0;
    if (action == 9U)
    {
        return history.sealed();
    }
    ++counts.at(model_revision);
    if (action < 8U)
    {
        position = position == 0 ? 0 : position - 1;
        return history.undone();
    }
    position = position >= edits.size() ? edits.size() : position + 1;
    return history.redone();
}

// 件数・位置・改訂番号・undo / redo の答え・probe 番目の適用済みの編集が模型と一致するか。
[[nodiscard]] bool agrees(const EditHistory &history, const Model &model, std::size_t probe)
{
    const auto &[edits, counts] = model;
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
           applied_agrees;
}

// seed ごとに steps 歩の列を模型と照合し、最後に適用済みの全件を照合する。
void verify_against_model(std::uint64_t seed, std::size_t steps)
{
    Random random{seed};
    EditHistory history = EditHistory::empty();
    Model model{};
    bool agreed = true;
    for (std::size_t step = 0; step < steps && agreed; ++step)
    {
        history = stepped(random, history, model);
        const std::size_t size = model.first.size();
        const std::size_t probe = size == 0 ? 0 : static_cast<std::size_t>(random() % size);
        agreed = agrees(history, model, probe);
    }
    for (std::size_t index = 0; index < model.second.at(model_position) && agreed; ++index)
    {
        agreed = history.applied(index) == std::optional<Edit>{model.first.at(index)};
    }
    expect(agreed, "random pushes, undo, redo and sealing agree with the copying model");
}
} // namespace

void verify_edit_history_property_scope()
{
    verify_block_edges();
    verify_folding_and_sealing();
    verify_shared_pieces();
    for (const std::uint64_t seed : {std::uint64_t{1473}, std::uint64_t{1474}, std::uint64_t{1475}})
    {
        verify_against_model(seed, 3000);
    }
}
} // namespace nenenib::tests
