#include "EditHistory.hpp"

#include <cstddef>
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

// 区切り方ごとの畳み方。separate は畳まない＝新しい単位を開く（ADR 0009 / ADR 0015）。
[[nodiscard]] std::optional<Edit> folded(const Edit &previous, const Edit &edit,
                                         EditBoundary boundary)
{
    switch (boundary)
    {
    case EditBoundary::separate:
        return std::nullopt;
    case EditBoundary::coalesce:
        if (!joinable(previous, edit))
        {
            return std::nullopt;
        }
        return Edit{previous.at, previous.removed, previous.inserted + edit.inserted};
    case EditBoundary::absorb:
        return absorbed(previous, edit);
    }
    std::unreachable();
}
} // namespace

std::optional<Edit> absorbed(const Edit &previous, const Edit &edit)
{
    const std::size_t end = previous.at.value + previous.inserted.size();
    if (edit.at.value >= previous.at.value && edit.at.value <= end &&
        edit.removed.size() <= end - edit.at.value)
    {
        std::string inserted = previous.inserted;
        inserted.replace(edit.at.value - previous.at.value, edit.removed.size(), edit.inserted);
        return Edit{previous.at, previous.removed, std::move(inserted)};
    }
    if (!edit.inserted.empty())
    {
        return std::nullopt;
    }
    const std::size_t erased = edit.at.value + edit.removed.size();
    if (erased == previous.at.value)
    {
        // 挿入を始めた位置より前を消した。単位の頭が前へ動き、消した本文が removed の先頭に付く。
        return Edit{edit.at, edit.removed + previous.removed, previous.inserted};
    }
    return std::nullopt;
}

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
    if (tail_ == boundary && !next.empty())
    {
        const auto merged = folded(next.back(), edit, boundary);
        if (merged.has_value())
        {
            next.back() = merged.value();
            return EditHistory(std::move(next), position_, boundary);
        }
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
