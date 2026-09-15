#include "EditHistory.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// まとめてよいのは「削除を伴わない入力が、直前の入力の直後に続いたとき」だけ。
// 移動・削除・貼り付け・改行は EditBoundary::separate で来て単位を閉じる（ADR 0009 の決定 3）。
[[nodiscard]] bool joinable(const Edit &previous, const Edit &edit) noexcept
{
    return previous.removed.empty() && edit.removed.empty() &&
           previous.at.value + previous.inserted.size() == edit.at.value;
}
} // namespace

EditHistory::EditHistory(std::vector<Edit> edits, std::size_t position, EditBoundary tail)
    : edits_(std::move(edits)), position_(position), tail_(tail)
{
}

EditHistory EditHistory::empty()
{
    return EditHistory(std::vector<Edit>{}, 0, EditBoundary::separate);
}

EditHistory EditHistory::pushed(const Edit &edit, EditBoundary boundary) const
{
    std::vector<Edit> next(edits_.begin(), edits_.begin() + static_cast<std::ptrdiff_t>(position_));
    const bool joins = boundary == EditBoundary::coalesce && tail_ == EditBoundary::coalesce &&
                       !next.empty() && joinable(next.back(), edit);
    if (joins)
    {
        next.back().inserted += edit.inserted;
        return EditHistory(std::move(next), position_, boundary);
    }
    next.push_back(edit);
    const std::size_t size = next.size();
    return EditHistory(std::move(next), size, boundary);
}

std::expected<Edit, HistoryFailure> EditHistory::undo() const
{
    if (position_ == 0)
    {
        return std::unexpected(HistoryFailure::nothing_to_undo);
    }
    return edits_.at(position_ - 1);
}

std::expected<Edit, HistoryFailure> EditHistory::redo() const
{
    if (position_ >= edits_.size())
    {
        return std::unexpected(HistoryFailure::nothing_to_redo);
    }
    return edits_.at(position_);
}

EditHistory EditHistory::sealed() const
{
    return EditHistory(edits_, position_, EditBoundary::separate);
}

EditHistory EditHistory::undone() const
{
    // 履歴を動かしたら単位は閉じる。戻したあとの入力が、戻す前の入力に混ざらないようにする。
    return EditHistory(edits_, position_ == 0 ? 0 : position_ - 1, EditBoundary::separate);
}

EditHistory EditHistory::redone() const
{
    return EditHistory(edits_, position_ >= edits_.size() ? edits_.size() : position_ + 1,
                       EditBoundary::separate);
}

std::size_t EditHistory::size() const noexcept
{
    return edits_.size();
}

std::size_t EditHistory::position() const noexcept
{
    return position_;
}
} // namespace nenenib::core
