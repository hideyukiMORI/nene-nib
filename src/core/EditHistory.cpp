#include "EditHistory.hpp"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 履歴の塊の件数（ADR 0180 の決定 2）。足すときに写す末尾の塊はこれ未満の参照。
constexpr std::size_t history_block_entries = 256;

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
    if (edit.at.value == end)
    {
        // 入れた本文の終わりから後ろの元の本文を消した（INSERT の `<Del>`・#1325）。消した本文は
        // 単位が置き換えた本文の続きなので removed の後ろに付き、単位の頭はそのまま。終わりへの
        // 挿入だけの編集は最初の枝が畳むので、ここへ来るのは削除を伴う編集だけ。
        return Edit{previous.at, previous.removed + edit.removed, previous.inserted + edit.inserted,
                    previous.restore};
    }
    return std::nullopt;
}

EditHistory::EditHistory(std::shared_ptr<const Blocks> blocks, std::size_t size,
                         std::size_t position, EditBoundary tail, std::size_t revision)
    : blocks_(std::move(blocks)), size_(size), position_(position), tail_(tail), revision_(revision)
{
}

EditHistory EditHistory::empty()
{
    return EditHistory(std::make_shared<const Blocks>(), 0, 0, EditBoundary::separate, 0);
}

// 写すのは塊の列（count / 塊の件数 + 1 個の参照）と、残す件数が割り切れなければ末尾の塊の頭
// （塊の件数未満の参照）だけ（ADR 0180 の決定 2）。
std::shared_ptr<const EditHistory::Blocks> EditHistory::kept_with(std::size_t count,
                                                                  Entry entry) const
{
    const std::size_t full = count / history_block_entries;
    const std::size_t rest = count % history_block_entries;
    Blocks blocks(blocks_->begin(), blocks_->begin() + static_cast<std::ptrdiff_t>(full));
    Block last;
    last.reserve(rest + 1);
    if (rest > 0)
    {
        const Block &partial = *blocks_->at(full);
        last.assign(partial.begin(), partial.begin() + static_cast<std::ptrdiff_t>(rest));
    }
    last.push_back(std::move(entry));
    blocks.push_back(std::make_shared<const Block>(std::move(last)));
    return std::make_shared<const Blocks>(std::move(blocks));
}

const Edit &EditHistory::entry_at(std::size_t index) const
{
    return *blocks_->at(index / history_block_entries)->at(index % history_block_entries);
}

EditHistory EditHistory::pushed(const Edit &edit, EditBoundary boundary) const
{
    if (tail_ == boundary && position_ > 0)
    {
        auto merged = folded(entry_at(position_ - 1), edit, boundary);
        if (merged.has_value())
        {
            auto blocks =
                kept_with(position_ - 1, std::make_shared<const Edit>(std::move(merged.value())));
            return EditHistory(std::move(blocks), position_, position_, boundary, revision_ + 1);
        }
    }
    // raw 入力の所有移管は受けない。呼出元に残る可変 alias からこの entry を隔離する。
    auto blocks = kept_with(position_, std::make_shared<const Edit>(edit));
    const std::size_t size = position_ + 1;
    return EditHistory(std::move(blocks), size, size, boundary, revision_ + 1);
}

std::expected<Edit, HistoryFailure> EditHistory::undo() const
{
    if (position_ == 0)
    {
        return std::unexpected(HistoryFailure::nothing_to_undo);
    }
    return entry_at(position_ - 1);
}

std::expected<Edit, HistoryFailure> EditHistory::redo() const
{
    if (position_ >= size_)
    {
        return std::unexpected(HistoryFailure::nothing_to_redo);
    }
    return entry_at(position_);
}

std::optional<Edit> EditHistory::applied(std::size_t index) const
{
    if (index >= position_)
    {
        return std::nullopt;
    }
    return entry_at(index);
}

// 封じる・戻す・やり直すは束を共有したまま位置と区切りだけを変える（列を写さない・決定 2）。
EditHistory EditHistory::sealed() const
{
    return EditHistory(blocks_, size_, position_, EditBoundary::separate, revision_);
}

EditHistory EditHistory::undone() const
{
    // 履歴を動かしたら単位は閉じる。戻したあとの入力が、戻す前の入力に混ざらないようにする。
    return EditHistory(blocks_, size_, position_ == 0 ? 0 : position_ - 1, EditBoundary::separate,
                       revision_ + 1);
}

EditHistory EditHistory::redone() const
{
    return EditHistory(blocks_, size_, position_ >= size_ ? size_ : position_ + 1,
                       EditBoundary::separate, revision_ + 1);
}

std::size_t EditHistory::size() const noexcept
{
    return size_;
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
