#include "VimPatternTrace.hpp"

#include "VimPattern.hpp"
#include "VimPatternInstruction.hpp"
#include "VimPatternOpcode.hpp"

#include <array>
#include <utility>

namespace nenenib::core
{
VimPatternTrace::VimPatternTrace(const VimPattern &pattern, std::string_view line,
                                 VimPatternMatch match)
    : pattern_(pattern), line_(line), match_(match),
      marked_(pattern.program_.size() * (match.end - match.begin + 1), false)
{
    path_.reserve(match.end - match.begin + pattern.program_.size() + 1);
}

std::size_t VimPatternTrace::cell(const VimPatternTraceFrame &frame) const noexcept
{
    return frame.pc * (match_.end - match_.begin + 1) + (frame.at - match_.begin);
}

bool VimPatternTrace::marked_on_entry(std::size_t pc) const
{
    const VimPatternOpcode opcode = pattern_.program_.at(pc).opcode;
    return opcode == VimPatternOpcode::atom || opcode == VimPatternOpcode::open;
}

std::optional<VimPatternTraceFrame> VimPatternTrace::atom_child(const VimPatternTraceFrame &top,
                                                                std::size_t atom) const
{
    const VimPatternAtom &unit = pattern_.atoms_.at(atom);
    if (VimPattern::is_zero_width(unit.kind))
    {
        return VimPattern::anchored(line_, unit, top.at)
                   ? std::optional{VimPatternTraceFrame{top.at, top.pc + 1, 0}}
                   : std::nullopt;
    }
    const auto eaten = pattern_.stepped(line_, unit, top.at);
    if (!eaten.has_value() || eaten.value() > match_.end)
    {
        return std::nullopt;
    }
    return VimPatternTraceFrame{eaten.value(), top.pc + 1, 0};
}

// 入った所の枝（split の後の道は next_child が引く）。
std::optional<VimPatternTraceFrame>
VimPatternTrace::first_child(const VimPatternTraceFrame &top) const
{
    const VimPatternInstruction &instruction = pattern_.program_.at(top.pc);
    switch (instruction.opcode)
    {
    case VimPatternOpcode::atom:
        return atom_child(top, instruction.first);
    case VimPatternOpcode::split:
    case VimPatternOpcode::jump:
        return VimPatternTraceFrame{top.at, instruction.first, 0};
    case VimPatternOpcode::open:
    case VimPatternOpcode::close:
        return VimPatternTraceFrame{top.at, top.pc + 1, 0};
    case VimPatternOpcode::accept:
        return std::nullopt;
    }
    std::unreachable();
}

std::optional<VimPatternTraceFrame> VimPatternTrace::next_child(VimPatternTraceFrame &top) const
{
    const VimPatternInstruction &instruction = pattern_.program_.at(top.pc);
    const std::uint8_t phase = top.phase;
    ++top.phase;
    if (phase == 0)
    {
        return first_child(top);
    }
    // split の後の道（先の道が着かなかった後）。
    if (phase == 1 && instruction.opcode == VimPatternOpcode::split)
    {
        return VimPatternTraceFrame{top.at, instruction.second, 0};
    }
    return std::nullopt;
}

// 群の閉じを通るたびに、その周の開きからの範囲を群の値にする（最後の周が残る）。
void VimPatternTrace::captures_recorded(VimPatternCaptures &captures) const
{
    std::array<std::size_t, vim_pattern_group_limit> starts{};
    for (const VimPatternTraceFrame &frame : path_)
    {
        const VimPatternInstruction &instruction = pattern_.program_.at(frame.pc);
        if (instruction.first >= vim_pattern_group_limit)
        {
            continue;
        }
        if (instruction.opcode == VimPatternOpcode::open)
        {
            starts.at(instruction.first) = frame.at;
        }
        if (instruction.opcode == VimPatternOpcode::close)
        {
            captures.at(instruction.first + 1) =
                VimPatternMatch{starts.at(instruction.first), frame.at};
        }
    }
}

// 入ったばかりの段の升が印つきなら真（試さない）。入ったときに印を付ける升はここで付ける。
bool VimPatternTrace::cut(const VimPatternTraceFrame &top)
{
    const std::size_t here = cell(top);
    if (marked_.at(here))
    {
        return true;
    }
    marked_.at(here) = marked_on_entry(top.pc);
    return false;
}

bool VimPatternTrace::accepted(const VimPatternTraceFrame &top) const
{
    return pattern_.program_.at(top.pc).opcode == VimPatternOpcode::accept &&
           top.at == match_.end && VimPattern::ends_a_character(line_, top.at);
}

bool VimPatternTrace::walked(VimPatternCaptures &captures)
{
    path_.push_back(VimPatternTraceFrame{match_.begin, 0, 0});
    while (!path_.empty())
    {
        VimPatternTraceFrame &top = path_.back();
        const bool entered = top.phase == 0;
        if (entered && cut(top))
        {
            path_.pop_back();
            continue;
        }
        if (entered && accepted(top))
        {
            captures_recorded(captures);
            return true;
        }
        const auto child = next_child(top);
        if (child.has_value())
        {
            path_.push_back(child.value());
            continue;
        }
        // 着かなかった升には印を付けて 2 度試さない。
        marked_.at(cell(path_.back())) = true;
        path_.pop_back();
    }
    return false;
}
} // namespace nenenib::core
