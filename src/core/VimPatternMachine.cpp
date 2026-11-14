#include "VimPatternMachine.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimPattern.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternOpcode.hpp"

#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
// 置き直した終わりの「`\ze` を通っていない」。
constexpr std::size_t no_bound = std::string_view::npos;
// bounded_ のとき積みの 1 項目が持つ語の数（項目と、置き直した始まりと終わり）。
constexpr std::size_t bounded_entry_words = 3;
} // namespace

VimPatternMachine::VimPatternMachine(const VimPattern &pattern, std::string_view line)
    : pattern_(pattern), line_(line), at_(0), transit_(pattern.program_.size()),
      cells_(cells_per_slot * 2 * transit_, 0), bounded_(pattern.bounded_),
      bounds_(bounded_ ? cells_per_slot * 2 * transit_ : 0, 0)
{
    current_.reserve(2 * transit_);
    next_.reserve(2 * transit_);
    pending_.reserve((bounded_ ? bounded_entry_words : 1) * (4 * transit_ + 4));
}

void VimPatternMachine::pushed(std::size_t entry)
{
    pending_.push_back(entry);
    if (bounded_)
    {
        pending_.push_back(carried_begin_);
        pending_.push_back(carried_end_);
    }
}

std::size_t VimPatternMachine::popped()
{
    if (bounded_)
    {
        carried_end_ = pending_.back();
        pending_.pop_back();
        carried_begin_ = pending_.back();
        pending_.pop_back();
    }
    const std::size_t entry = pending_.back();
    pending_.pop_back();
    return entry;
}

void VimPatternMachine::bounds_stored(bool into_next, std::size_t slot)
{
    if (!bounded_)
    {
        return;
    }
    const std::size_t cell = cell_of(into_next, slot);
    bounds_.at(cell) = carried_begin_;
    bounds_.at(cell + 1) = carried_end_;
}

void VimPatternMachine::bounds_loaded(bool into_next, std::size_t slot)
{
    if (!bounded_)
    {
        return;
    }
    const std::size_t cell = cell_of(into_next, slot);
    carried_begin_ = bounds_.at(cell);
    carried_end_ = bounds_.at(cell + 1);
}

// 当たりの範囲。`\zs` は始まりを、`\ze` は終わりを置き直す（`\zs` は前の `\ze` を消す・Vim 9.1 の
// addstate の NFA_ZSTART は複数行の照合で end_lnum を -1 にする・`a\ze\zsbc` は `bc`・実測）。
VimPatternMatch VimPatternMachine::accepted(std::size_t begin) const noexcept
{
    if (!bounded_)
    {
        return VimPatternMatch{begin, at_, begin, at_};
    }
    return VimPatternMatch{carried_begin_, carried_end_ == no_bound ? at_ : carried_end_, begin,
                           at_};
}

std::optional<VimPatternMatch> VimPatternMachine::placed(bool into_next, std::size_t pc,
                                                         std::size_t begin)
{
    pending_.clear();
    pushed(pc << 1U);
    while (!pending_.empty())
    {
        const std::size_t entry = popped();
        const std::size_t here = entry >> 1U;
        if ((entry & 1U) != 0U)
        {
            stamp_of(into_next, here) = done_stamp(into_next);
            continue;
        }
        if (!entered(into_next, here))
        {
            continue;
        }
        const auto match = followed(into_next, here, begin);
        if (match.has_value())
        {
            // 残りの低い優先は捨てる。
            pending_.clear();
            return match;
        }
    }
    return std::nullopt;
}

std::size_t &VimPatternMachine::stamp_of(bool into_next, std::size_t slot)
{
    return cells_.at(cell_of(into_next, slot) + 1);
}

std::size_t &VimPatternMachine::begin_of(bool into_next, std::size_t slot)
{
    return cells_.at(cell_of(into_next, slot));
}

// 升の並びは列の番号ごとに（今の列の始まり・印・次の列の始まり・印）の 4 つ（確保を 1 回にする）。
std::size_t VimPatternMachine::cell_of(bool into_next, std::size_t slot) const noexcept
{
    return (slot * cells_per_slot) + (into_next == current_is_first_ ? 2 : 0);
}

std::size_t VimPatternMachine::done_stamp(bool into_next) const noexcept
{
    return 2 * (into_next ? next_generation_ : current_generation_) + 1;
}

// Vim 9.1 の addstate と同じく、食べる原子・錨・群の開き・`\zs` は、
// 同じ位置で 2 度目に来たら辿らない。split・jump・群の閉じ・`\ze`・当たりは、
// 辿り終えるまで（途中の印の間）は辿り直す（空の周を 1 回だけ通す・
// `\(\|a\)\+` は空の当たりが `a` を食べる道より先・実測）。
// 辿り終えた後は、そこから着ける所にもう着いているので辿らない。
bool VimPatternMachine::entered(bool into_next, std::size_t here)
{
    std::size_t &stamp = stamp_of(into_next, here);
    if (stamp == done_stamp(into_next))
    {
        return false;
    }
    const VimPatternOpcode opcode = pattern_.program_.at(here).opcode;
    if (opcode == VimPatternOpcode::atom || opcode == VimPatternOpcode::open ||
        opcode == VimPatternOpcode::match_begin)
    {
        stamp = done_stamp(into_next);
        return true;
    }
    stamp = done_stamp(into_next) - 1;
    pushed((here << 1U) | 1U);
    return true;
}

void VimPatternMachine::atom_placed(bool into_next, std::size_t here, std::size_t begin)
{
    const VimPatternAtom &atom = pattern_.atoms_.at(pattern_.program_.at(here).first);
    if (VimPattern::is_zero_width(atom.kind))
    {
        if (VimPattern::anchored(line_, atom, at_))
        {
            pushed((here + 1) << 1U);
        }
        return;
    }
    begin_of(into_next, here) = begin;
    bounds_stored(into_next, here);
    (into_next ? next_ : current_).push_back(here);
}

std::optional<VimPatternMatch> VimPatternMachine::followed(bool into_next, std::size_t here,
                                                           std::size_t begin)
{
    const VimPatternInstruction &instruction = pattern_.program_.at(here);
    switch (instruction.opcode)
    {
    case VimPatternOpcode::atom:
        atom_placed(into_next, here, begin);
        return std::nullopt;
    // 先の道を後に積む（先に取り出す）。
    case VimPatternOpcode::split:
        pushed(instruction.second << 1U);
        pushed(instruction.first << 1U);
        return std::nullopt;
    case VimPatternOpcode::jump:
        pushed(instruction.first << 1U);
        return std::nullopt;
    case VimPatternOpcode::open:
    case VimPatternOpcode::close:
        pushed((here + 1) << 1U);
        return std::nullopt;
    // `\zs` は始まりを今の位置に置き、前の `\ze` を消す。`\ze` は終わりを今の位置に置く（複数なら
    // 最後に通った方が効く・ADR 0184 の決定 5）。
    case VimPatternOpcode::match_begin:
        carried_begin_ = at_;
        carried_end_ = no_bound;
        pushed((here + 1) << 1U);
        return std::nullopt;
    case VimPatternOpcode::match_end:
        carried_end_ = at_;
        pushed((here + 1) << 1U);
        return std::nullopt;
    // 終わりが文字の境の当たりだけを一致にする（#1138）。
    case VimPatternOpcode::accept:
        return VimPattern::ends_a_character(line_, at_) ? std::optional{accepted(begin)}
                                                        : std::nullopt;
    }
    std::unreachable();
}

// `.` と集合が 1 文字の残りの結合文字を食べる間（#1146）、候補は transit_ 以上の番号で
// 「target へ向かう途中」として列に残る（VimPatternEvaluation と同じ・優先順を保つ）。
std::optional<VimPatternMatch> VimPatternMachine::arrived(std::size_t target, std::size_t begin)
{
    if (VimPattern::ends_a_character(line_, at_))
    {
        return placed(true, target, begin);
    }
    const std::size_t slot = transit_ + target;
    if (stamp_of(true, slot) != done_stamp(true))
    {
        stamp_of(true, slot) = done_stamp(true);
        begin_of(true, slot) = begin;
        bounds_stored(true, slot);
        next_.push_back(slot);
    }
    return std::nullopt;
}

std::optional<VimPatternMatch> VimPatternMachine::stepped(std::size_t slot, std::size_t begin,
                                                          std::size_t before, char32_t code)
{
    if (slot >= transit_)
    {
        return arrived(slot - transit_, begin);
    }
    const VimPatternAtom &atom = pattern_.atoms_.at(pattern_.program_.at(slot).first);
    if (!pattern_.consumes(line_, atom, before, code))
    {
        return std::nullopt;
    }
    return VimPattern::whole_character(atom.kind) ? arrived(slot + 1, begin)
                                                  : placed(true, slot + 1, begin);
}

void VimPatternMachine::advanced()
{
    const std::size_t before = at_;
    const char32_t code = code_point_at(line_, Offset{before});
    at_ = next_code_point(line_, Offset{before}).value;
    next_.clear();
    for (const std::size_t slot : current_)
    {
        bounds_loaded(false, slot);
        const auto match = stepped(slot, begin_of(false, slot), before, code);
        if (match.has_value())
        {
            found_ = match;
            break;
        }
    }
    // 当たりより優先する候補だけが次の列に残る。後続の低い優先の候補は捨てる。
    current_.swap(next_);
    current_is_first_ = !current_is_first_;
    current_generation_ = std::exchange(next_generation_, next_generation_ + 1);
}

std::optional<VimPatternMatch> VimPatternMachine::matched(std::size_t from)
{
    at_ = from;
    while (true)
    {
        // 始まりは Vim の 1 文字の先頭だけ（結合文字の上からは試さない・#1146）。
        if (!found_.has_value() && VimPattern::ends_a_character(line_, at_))
        {
            carried_begin_ = at_;
            carried_end_ = no_bound;
            found_ = placed(false, 0, at_);
        }
        if (at_ == line_.size())
        {
            return found_;
        }
        advanced();
        if (found_.has_value() && current_.empty())
        {
            return found_;
        }
    }
}
} // namespace nenenib::core
