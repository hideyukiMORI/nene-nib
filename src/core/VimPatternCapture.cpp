#include "VimPatternCapture.hpp"

#include "VimPattern.hpp"

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

bool VimPatternCapture::walked(std::size_t index, std::size_t at)
{
    const std::size_t cell = index * (match_.end - match_.begin + 1) + (at - match_.begin);
    if (failed_.at(cell))
    {
        return false;
    }
    positions_.at(index) = at;
    const bool arrived = index == pattern_.atoms_.size()
                             ? at == match_.end && VimPattern::ends_a_character(line_, at)
                             : stepped_on(index, at);
    if (!arrived)
    {
        failed_.at(cell) = true;
    }
    return arrived;
}

bool VimPatternCapture::stepped_on(std::size_t index, std::size_t at)
{
    const VimPatternAtom &atom = pattern_.atoms_.at(index);
    if (VimPattern::is_zero_width(atom.kind))
    {
        return VimPattern::anchored(line_, atom, at) && walked(index + 1, at);
    }
    const std::vector<std::size_t> positions = reached(atom, at);
    // 繰り返しの無い原子は 1 歩食べた所だけ、`*` は長い方から 0 回まで。
    const std::size_t shortest = atom.repeated ? 0 : 1;
    for (std::size_t tried = positions.size(); tried > shortest; --tried)
    {
        if (walked(index + 1, positions.at(tried - 1)))
        {
            return true;
        }
    }
    return false;
}

std::vector<std::size_t> VimPatternCapture::reached(const VimPatternAtom &atom,
                                                    std::size_t at) const
{
    std::vector<std::size_t> positions{at};
    for (auto next = pattern_.stepped(line_, atom, at);
         next.has_value() && next.value() <= match_.end;
         next = atom.repeated ? pattern_.stepped(line_, atom, positions.back()) : std::nullopt)
    {
        positions.push_back(next.value());
    }
    return positions;
}
} // namespace nenenib::core
