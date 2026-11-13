#include "VimPatternMachine.hpp"

#include "Offset.hpp"
#include "Utf8.hpp"
#include "VimPattern.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternOpcode.hpp"

#include <utility>

namespace nenenib::core
{
VimPatternMachine::VimPatternMachine(const VimPattern &pattern, std::string_view line)
    : pattern_(pattern), line_(line), at_(0), transit_(pattern.program_.size()),
      current_begins_(2 * transit_, 0), next_begins_(2 * transit_, 0),
      current_stamps_(2 * transit_, 0), next_stamps_(2 * transit_, 0)
{
    current_.reserve(2 * transit_);
    next_.reserve(2 * transit_);
    pending_.reserve(4 * transit_ + 4);
}

std::optional<VimPatternMatch> VimPatternMachine::placed(bool into_next, std::size_t pc,
                                                         std::size_t begin)
{
    pending_.clear();
    pending_.push_back(pc << 1U);
    while (!pending_.empty())
    {
        const std::size_t entry = pending_.back();
        pending_.pop_back();
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
    return (into_next ? next_stamps_ : current_stamps_).at(slot);
}

std::size_t VimPatternMachine::done_stamp(bool into_next) const noexcept
{
    return 2 * (into_next ? next_generation_ : current_generation_) + 1;
}

// Vim 9.1 の addstate と同じく、食べる原子・錨・群の開きは、
// 同じ位置で 2 度目に来たら辿らない。split・jump・群の閉じ・当たりは、
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
    if (opcode == VimPatternOpcode::atom || opcode == VimPatternOpcode::open)
    {
        stamp = done_stamp(into_next);
        return true;
    }
    stamp = done_stamp(into_next) - 1;
    pending_.push_back((here << 1U) | 1U);
    return true;
}

void VimPatternMachine::atom_placed(bool into_next, std::size_t here, std::size_t begin)
{
    const VimPatternAtom &atom = pattern_.atoms_.at(pattern_.program_.at(here).first);
    if (VimPattern::is_zero_width(atom.kind))
    {
        if (VimPattern::anchored(line_, atom, at_))
        {
            pending_.push_back((here + 1) << 1U);
        }
        return;
    }
    (into_next ? next_begins_ : current_begins_).at(here) = begin;
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
        pending_.push_back(instruction.second << 1U);
        pending_.push_back(instruction.first << 1U);
        return std::nullopt;
    case VimPatternOpcode::jump:
        pending_.push_back(instruction.first << 1U);
        return std::nullopt;
    case VimPatternOpcode::open:
    case VimPatternOpcode::close:
        pending_.push_back((here + 1) << 1U);
        return std::nullopt;
    // 終わりが文字の境の当たりだけを一致にする（#1138）。
    case VimPatternOpcode::accept:
        return VimPattern::ends_a_character(line_, at_) ? std::optional{VimPatternMatch{begin, at_}}
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
    if (next_stamps_.at(slot) != done_stamp(true))
    {
        next_stamps_.at(slot) = done_stamp(true);
        next_begins_.at(slot) = begin;
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
        const auto match = stepped(slot, current_begins_.at(slot), before, code);
        if (match.has_value())
        {
            found_ = match;
            break;
        }
    }
    // 当たりより優先する候補だけが次の列に残る。後続の低い優先の候補は捨てる。
    current_.swap(next_);
    current_begins_.swap(next_begins_);
    current_stamps_.swap(next_stamps_);
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
