#include "VimPatternCapture.hpp"

#include "VimPattern.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::core
{
VimPatternCapture::VimPatternCapture(const VimPattern &pattern, std::string_view line,
                                     VimPatternMatch match)
    : pattern_(pattern), line_(line), match_(match),
      positions_(pattern.atoms_.size() + 1, match.begin),
      failed_((pattern.atoms_.size() + 1) * (match.end - match.begin + 1), false)
{
}

std::size_t VimPatternCapture::cell(const VimPatternCaptureFrame &frame) const noexcept
{
    return frame.index * (match_.end - match_.begin + 1) + (frame.at - match_.begin);
}

bool VimPatternCapture::arrived(const VimPatternCaptureFrame &frame) const noexcept
{
    return frame.at == match_.end && VimPattern::ends_a_character(line_, frame.at);
}

std::optional<VimPatternCaptureFrame> VimPatternCapture::next_child(VimPatternCaptureFrame &top)
{
    const VimPatternAtom &atom = pattern_.atoms_.at(top.index);
    const std::uint8_t phase = top.phase;
    ++top.phase;
    const auto next = static_cast<std::uint32_t>(top.index + 1);
    if (VimPattern::is_zero_width(atom.kind))
    {
        return zero_width_child(atom, top, phase);
    }
    // `*` の 0 回の枝は 1 文字食べる枝の後（照合器と同じ優先順）。
    if (phase == 1 && atom.repeated)
    {
        return VimPatternCaptureFrame{top.at, next, 0};
    }
    const auto eaten = phase == 0 ? consumed(atom, top.at) : std::nullopt;
    if (eaten.has_value())
    {
        return VimPatternCaptureFrame{eaten.value(), atom.repeated ? top.index : next, 0};
    }
    return atom.repeated && phase == 0 ? next_child(top) : std::nullopt;
}

std::optional<VimPatternCaptureFrame>
VimPatternCapture::zero_width_child(const VimPatternAtom &atom, const VimPatternCaptureFrame &top,
                                    std::uint8_t phase) const
{
    if (phase != 0 || !VimPattern::anchored(line_, atom, top.at))
    {
        return std::nullopt;
    }
    return VimPatternCaptureFrame{top.at, static_cast<std::uint32_t>(top.index + 1), 0};
}

std::optional<std::size_t> VimPatternCapture::consumed(const VimPatternAtom &atom,
                                                       std::size_t at) const
{
    const auto stepped = pattern_.stepped(line_, atom, at);
    return stepped.has_value() && stepped.value() <= match_.end ? stepped : std::nullopt;
}

void VimPatternCapture::positions_recorded(const std::vector<VimPatternCaptureFrame> &path)
{
    std::vector<bool> placed(positions_.size(), false);
    for (const VimPatternCaptureFrame &frame : path)
    {
        if (!placed.at(frame.index))
        {
            placed.at(frame.index) = true;
            positions_.at(frame.index) = frame.at;
        }
    }
}

bool VimPatternCapture::walked()
{
    const auto last = static_cast<std::uint32_t>(pattern_.atoms_.size());
    std::vector<VimPatternCaptureFrame> path{VimPatternCaptureFrame{match_.begin, 0, 0}};
    while (!path.empty())
    {
        VimPatternCaptureFrame &top = path.back();
        const bool entered = top.phase == 0;
        if (entered && top.index == last && arrived(top))
        {
            positions_recorded(path);
            return true;
        }
        const auto child =
            !failed_.at(cell(top)) && top.index < last ? next_child(top) : std::nullopt;
        if (child.has_value())
        {
            path.push_back(child.value());
            continue;
        }
        failed_.at(cell(path.back())) = true;
        path.pop_back();
    }
    return false;
}
} // namespace nenenib::core
