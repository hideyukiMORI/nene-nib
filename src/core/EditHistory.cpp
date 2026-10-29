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
// 畳んだ単位の restore は最初の編集の値のまま（ADR 0052 の決定 3）。
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
        return Edit{previous.at, previous.removed, previous.inserted + edit.inserted,
                    previous.restore};
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
        return Edit{previous.at, previous.removed, std::move(inserted), previous.restore};
    }
    const std::size_t erased = edit.at.value + edit.removed.size();
    if (edit.at.value < previous.at.value && erased >= previous.at.value && erased <= end)
    {
        // 挿入を始めた位置より前から、単位が入れた本文の中（または頭）までを置き換えた。単位の頭が
        // 前へ動き、前の本文は removed の先頭に付き、入れた本文の消された頭は新しい本文に替わる。
        // 削除だけ（Backspace が頭の前を消す）と、回数付き INSERT の流し直しが前の行とつないで
        // から打ち直す置き換え（#1093）がここへ来る。
        const std::size_t before = previous.at.value - edit.at.value;
        return Edit{edit.at, edit.removed.substr(0, before) + previous.removed,
                    edit.inserted + previous.inserted.substr(erased - previous.at.value),
                    previous.restore};
    }
    return std::nullopt;
}

EditHistory::EditHistory(std::vector<Entry> edits, std::size_t position, EditBoundary tail,
                         std::size_t revision)
    : edits_(std::move(edits)), position_(position), tail_(tail), revision_(revision)
{
}

EditHistory EditHistory::empty()
{
    return EditHistory(std::vector<Entry>{}, 0, EditBoundary::separate, 0);
}

EditHistory EditHistory::pushed(const Edit &edit, EditBoundary boundary) const
{
    std::vector<Entry> next(edits_.begin(),
                            edits_.begin() + static_cast<std::ptrdiff_t>(position_));
    if (tail_ == boundary && !next.empty())
    {
        auto merged = folded(*next.back(), edit, boundary);
        if (merged.has_value())
        {
            next.back() = std::make_shared<const Edit>(std::move(merged.value()));
            return EditHistory(std::move(next), position_, boundary, revision_ + 1);
        }
    }
    // raw 入力の所有移管は受けない。呼出元に残る可変 alias からこの entry を隔離する。
    next.push_back(std::make_shared<const Edit>(edit));
    const std::size_t size = next.size();
    return EditHistory(std::move(next), size, boundary, revision_ + 1);
}

std::expected<Edit, HistoryFailure> EditHistory::undo() const
{
    if (position_ == 0)
    {
        return std::unexpected(HistoryFailure::nothing_to_undo);
    }
    return *edits_.at(position_ - 1);
}

std::expected<Edit, HistoryFailure> EditHistory::redo() const
{
    if (position_ >= edits_.size())
    {
        return std::unexpected(HistoryFailure::nothing_to_redo);
    }
    return *edits_.at(position_);
}

std::optional<Edit> EditHistory::applied(std::size_t index) const
{
    if (index >= position_)
    {
        return std::nullopt;
    }
    return *edits_.at(index);
}

EditHistory EditHistory::sealed() const
{
    return EditHistory(edits_, position_, EditBoundary::separate, revision_);
}

EditHistory EditHistory::undone() const
{
    // 履歴を動かしたら単位は閉じる。戻したあとの入力が、戻す前の入力に混ざらないようにする。
    return EditHistory(edits_, position_ == 0 ? 0 : position_ - 1, EditBoundary::separate,
                       revision_ + 1);
}

EditHistory EditHistory::redone() const
{
    return EditHistory(edits_, position_ >= edits_.size() ? edits_.size() : position_ + 1,
                       EditBoundary::separate, revision_ + 1);
}

std::size_t EditHistory::size() const noexcept
{
    return edits_.size();
}

std::size_t EditHistory::position() const noexcept
{
    return position_;
}

std::size_t EditHistory::revision() const noexcept
{
    return revision_;
}
} // namespace nenenib::core
