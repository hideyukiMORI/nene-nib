#include "EditHistory.hpp"

#include <algorithm>
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

// 本文に触れない空の単位か（ADR 0181 の決定 3）。`ix<BS><Esc>` の畳んだ単位もこの形になる。
[[nodiscard]] bool is_empty_edit(const Edit &edit) noexcept
{
    return edit.removed.empty() && edit.inserted.empty();
}

// 畳んだ単位の印。どちらかが本文を変えたと数えるなら、畳んだ単位も数える。
[[nodiscard]] VimChange merged_change(VimChange previous, VimChange edit) noexcept
{
    return previous == VimChange::changed || edit == VimChange::changed ? VimChange::changed
                                                                        : VimChange::saved_only;
}

// 印が数える分（content_position の足し引き）。
[[nodiscard]] std::size_t counted(VimChange change) noexcept
{
    switch (change)
    {
    case VimChange::changed:
        return 1;
    case VimChange::saved_only:
        return 0;
    }
    std::unreachable();
}

// 区切り方ごとの畳み方。separate は畳まない＝新しい単位を開く（ADR 0009 / ADR 0015）。
// 畳んだ単位の restore は最初の編集の値のまま（ADR 0052 の決定 3）。空の編集（空の単位）は前の
// 単位に畳まず、空の単位へは INSERT の absorb だけが畳む（ADR 0181 の決定 3）。
[[nodiscard]] std::optional<Edit> folded(const Edit &previous, const Edit &edit,
                                         EditBoundary boundary)
{
    if (is_empty_edit(edit))
    {
        return std::nullopt;
    }
    switch (boundary)
    {
    case EditBoundary::separate:
        return std::nullopt;
    case EditBoundary::coalesce:
        if (is_empty_edit(previous) || !joinable(previous, edit))
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
                         std::size_t position, EditBoundary tail, std::size_t revision,
                         std::size_t content)
    : blocks_(std::move(blocks)), size_(size), position_(position), tail_(tail),
      revision_(revision), content_(content)
{
}

EditHistory EditHistory::empty()
{
    return EditHistory(std::make_shared<const Blocks>(), 0, 0, EditBoundary::separate, 0, 0);
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

const HistoryUnit &EditHistory::entry_at(std::size_t index) const
{
    return *blocks_->at(index / history_block_entries)->at(index % history_block_entries);
}

EditHistory EditHistory::pushed(const Edit &edit, EditBoundary boundary) const
{
    return pushed(edit, boundary, VimChange::changed);
}

EditHistory EditHistory::pushed(const Edit &edit, EditBoundary boundary, VimChange change) const
{
    if (tail_ == boundary && position_ > 0)
    {
        const HistoryUnit &previous = entry_at(position_ - 1);
        auto merged = folded(previous.edit, edit, boundary);
        if (merged.has_value())
        {
            const VimChange joined = merged_change(previous.change, change);
            const std::size_t content = content_ - counted(previous.change) + counted(joined);
            auto blocks = kept_with(position_ - 1, std::make_shared<const HistoryUnit>(HistoryUnit{
                                                       std::move(merged.value()), joined}));
            return EditHistory(std::move(blocks), position_, position_, boundary, revision_ + 1,
                               content);
        }
    }
    // raw 入力の所有移管は受けない。呼出元に残る可変 alias からこの entry を隔離する。
    auto blocks =
        kept_with(position_, std::make_shared<const HistoryUnit>(HistoryUnit{edit, change}));
    const std::size_t size = position_ + 1;
    return EditHistory(std::move(blocks), size, size, boundary, revision_ + 1,
                       content_ + counted(change));
}

std::expected<Edit, HistoryFailure> EditHistory::undo() const
{
    if (position_ == 0)
    {
        return std::unexpected(HistoryFailure::nothing_to_undo);
    }
    return entry_at(position_ - 1).edit;
}

std::expected<Edit, HistoryFailure> EditHistory::redo() const
{
    if (position_ >= size_)
    {
        return std::unexpected(HistoryFailure::nothing_to_redo);
    }
    return entry_at(position_).edit;
}

std::optional<Edit> EditHistory::applied(std::size_t index) const
{
    if (index >= position_)
    {
        return std::nullopt;
    }
    return entry_at(index).edit;
}

// 封じる・戻す・やり直すは束を共有したまま位置と区切りだけを変える（列を写さない・決定 2）。
// 本文の位置は戻した・やり直した 1 件の印だけ足し引きする（ADR 0181 の決定 4）。
EditHistory EditHistory::sealed() const
{
    return EditHistory(blocks_, size_, position_, EditBoundary::separate, revision_, content_);
}

EditHistory EditHistory::undone() const
{
    // 履歴を動かしたら単位は閉じる。戻したあとの入力が、戻す前の入力に混ざらないようにする。
    if (position_ == 0)
    {
        return EditHistory(blocks_, size_, 0, EditBoundary::separate, revision_ + 1, content_);
    }
    return EditHistory(blocks_, size_, position_ - 1, EditBoundary::separate, revision_ + 1,
                       content_ - counted(entry_at(position_ - 1).change));
}

EditHistory EditHistory::redone() const
{
    if (position_ >= size_)
    {
        return EditHistory(blocks_, size_, size_, EditBoundary::separate, revision_ + 1, content_);
    }
    return EditHistory(blocks_, size_, position_ + 1, EditBoundary::separate, revision_ + 1,
                       content_ + counted(entry_at(position_).change));
}

// 本文に触れず、本文を変えたとも数えない単位か（saved_only の空の単位・ADR 0181 の追補 9）。
// 件の参照だけを読み、Edit を写さない。
bool EditHistory::silent_at(std::size_t index) const
{
    const HistoryUnit &unit = entry_at(index);
    return unit.change == VimChange::saved_only && is_empty_edit(unit.edit);
}

// その向きに続く saved_only の空の単位を飛ばした位置。changed の空の単位と本文を変える単位の手前
// （または履歴の端）で止まる。
std::size_t EditHistory::past_silent(HistoryDirection direction) const
{
    std::size_t position = position_;
    switch (direction)
    {
    case HistoryDirection::undo:
        while (position > 0 && silent_at(position - 1))
        {
            --position;
        }
        return position;
    case HistoryDirection::redo:
        while (position < size_ && silent_at(position))
        {
            ++position;
        }
        return position;
    }
    std::unreachable();
}

// 位置だけを動かす。飛ばす件は counted が 0 なので本文の位置（content_）は写したまま変わらない。
// 改訂番号は undone / redone と同じく 1 件ごとに 1 増える。端まで saved_only の空の単位だけなら
// 端の失敗（Windows の標準に「何も起きない取り消し」は無い・D82）。
std::expected<EditHistory, HistoryFailure>
EditHistory::past_saved_only_units(HistoryDirection direction) const
{
    const bool undo = direction == HistoryDirection::undo;
    const std::size_t position = past_silent(direction);
    if (position == (undo ? 0 : size_))
    {
        return std::unexpected(undo ? HistoryFailure::nothing_to_undo
                                    : HistoryFailure::nothing_to_redo);
    }
    if (position == position_)
    {
        return *this;
    }
    const std::size_t skipped = undo ? position_ - position : position - position_;
    return EditHistory(blocks_, size_, position, EditBoundary::separate, revision_ + skipped,
                       content_);
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

EditHistory EditHistory::revised_beyond(std::size_t seen) const
{
    return EditHistory(blocks_, size_, position_, tail_, std::max(revision_, seen + 1), content_);
}

std::size_t EditHistory::content_position() const noexcept
{
    return content_;
}
} // namespace nenenib::core
