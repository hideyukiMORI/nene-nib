#include "VimJumpLists.hpp"

#include "VimJumpList.hpp"
#include "VimJumpMove.hpp"
#include "VimMarkEdits.hpp"
#include "VimMarkPosition.hpp"
#include "VimMarks.hpp"

#include <cstddef>
#include <optional>
#include <utility>

namespace nenenib::core
{
namespace
{
// 同じ行の変更を 1 つの位置にまとめる桁の幅（Vim の changed_common が textwidth 0 で使う 79）。
constexpr std::size_t change_columns = 79;

// 末尾に 1 つ空ける。満ちていれば最も古い位置を捨てて詰める。
[[nodiscard]] VimJumpList with_room(VimJumpList list) noexcept
{
    if (list.count < vim_jump_list_limit)
    {
        return list;
    }
    for (std::size_t at = 1; at < list.count; ++at)
    {
        list.entries.at(at - 1) = list.entries.at(at);
    }
    --list.count;
    return list;
}

[[nodiscard]] bool repeated_later(const VimJumpList &jumps, std::size_t from) noexcept
{
    for (std::size_t later = from + 1; later < jumps.count; ++later)
    {
        if (jumps.entries.at(later).line == jumps.entries.at(from).line)
        {
            return true;
        }
    }
    return false;
}

// 変更の列に新しい位置を足すか（列が空か、最後の位置と違う行か、79 桁より離れているか）。
[[nodiscard]] bool distant(const VimJumpList &changes, const VimMarkPosition &at) noexcept
{
    if (changes.count == 0)
    {
        return true;
    }
    const VimMarkPosition &last = changes.entries.at(changes.count - 1);
    return !(last.line == at.line) || last.byte + change_columns < at.byte ||
           at.byte + change_columns < last.byte;
}

[[nodiscard]] VimJumpMove reached(VimMarks marks, std::size_t index)
{
    VimJumpList &list = marks.jumps;
    list.index = index;
    const VimMarkPosition target = list.entries.at(index);
    return VimJumpMove{std::move(marks), target};
}
} // namespace

VimJumpList vim_jump_appended(const VimJumpList &jumps, const VimMarkPosition &at) noexcept
{
    VimJumpList next = with_room(jumps);
    next.entries.at(next.count) = at;
    ++next.count;
    next.index = next.count;
    return next;
}

VimJumpList vim_jumps_cleaned(const VimJumpList &jumps) noexcept
{
    VimJumpList next{};
    next.index = jumps.index;
    std::size_t kept = 0;
    for (std::size_t from = 0; from < jumps.count; ++from)
    {
        if (jumps.index == from)
        {
            next.index = kept;
        }
        if (!repeated_later(jumps, from))
        {
            next.entries.at(kept) = jumps.entries.at(from);
            ++kept;
        }
    }
    if (jumps.index == jumps.count)
    {
        next.index = kept;
    }
    next.count = kept;
    return next;
}

VimJumpMove vim_jump_moved(const VimMarks &marks, const VimMarkPosition &caret, bool older,
                           std::size_t count)
{
    VimMarks next = marks;
    next.jumps = vim_jumps_cleaned(marks.jumps);
    const VimJumpList &list = next.jumps;
    // 回数は SIZE_MAX まで来るので、足さずに引いて比べる（index は count を越えない）。
    const bool beyond = older ? count > list.index : count >= list.count - list.index;
    if (list.count == 0 || beyond)
    {
        return VimJumpMove{std::move(next), std::nullopt};
    }
    // 歩く前に末尾の後ろにいたら、今の位置を飛ぶ鍵と同じに積んでその 1 つ前から歩く。
    if (list.index == list.count)
    {
        next = vim_marks_jumped(next, caret);
        --next.jumps.index;
        if (older && count > next.jumps.index)
        {
            return VimJumpMove{std::move(next), std::nullopt};
        }
    }
    const std::size_t index = older ? next.jumps.index - count : next.jumps.index + count;
    return reached(std::move(next), index);
}

VimJumpMove vim_change_moved(const VimMarks &marks, bool older, std::size_t count)
{
    const VimJumpList &list = marks.changes;
    const std::size_t at = list.index;
    if (list.count == 0 || (older && at == 0) || (!older && at + 1 == list.count))
    {
        return VimJumpMove{marks, std::nullopt};
    }
    std::size_t index = list.count - 1;
    if (older)
    {
        index = count > at ? 0 : at - count;
    }
    else if (count < list.count - at)
    {
        index = at + count;
    }
    VimMarks next = marks;
    next.changes.index = index;
    const VimMarkPosition target = next.changes.entries.at(index);
    return VimJumpMove{std::move(next), target};
}

VimMarks vim_change_recorded(const VimMarks &marks, const VimMarkPosition &at, bool started)
{
    VimMarks next = marks;
    VimJumpList &list = next.changes;
    if (started)
    {
        next.change_started = true;
    }
    if ((next.change_started || list.count == 0) && distant(list, at))
    {
        next.change_started = false;
        list = with_room(list);
        ++list.count;
    }
    list.entries.at(list.count - 1) = at;
    list.index = list.count;
    return next;
}
} // namespace nenenib::core
