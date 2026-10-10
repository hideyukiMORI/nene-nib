#include "VimPatternEvaluation.hpp"

#include "Utf8.hpp"
#include "VimPattern.hpp"

#include <algorithm>

namespace nenenib::core
{
VimPatternEvaluation::VimPatternEvaluation(const VimPattern &pattern, std::string_view line)
    : pattern_(pattern), line_(line), at_(0), current_begins_(pattern.atoms_.size() + 1),
      next_begins_(pattern.atoms_.size() + 1)
{
    current_.reserve(pattern.atoms_.size());
    next_.reserve(pattern.atoms_.size());
}

std::optional<VimPatternMatch>
VimPatternEvaluation::expanded(std::vector<std::size_t> &order,
                               std::vector<std::optional<std::size_t>> &begins, std::size_t index,
                               std::size_t begin)
{
    // 幅のない遷移は原子indexを増やすだけ。同位置で先に来た候補を優先する。
    while (!begins.at(index).has_value())
    {
        begins.at(index) = begin;
        if (index == pattern_.atoms_.size())
        {
            return VimPatternMatch{begin, at_};
        }
        const VimPatternAtom &atom = pattern_.atoms_.at(index);
        const bool zero_width = VimPattern::is_zero_width(atom.kind);
        if (zero_width && !VimPattern::anchored(line_, atom, at_))
        {
            return std::nullopt;
        }
        if (zero_width)
        {
            ++index;
            continue;
        }
        // 星の消費を先に置き、0回で先へ進む候補を後から展開する。
        order.push_back(index);
        if (!atom.repeated)
        {
            return std::nullopt;
        }
        ++index;
    }
    return std::nullopt;
}

void VimPatternEvaluation::advanced()
{
    const std::size_t before = at_;
    const char32_t code = code_point_at(line_, Offset{before});
    at_ = next_code_point(line_, Offset{before}).value;
    next_.clear();
    std::ranges::fill(next_begins_, std::nullopt);
    for (const std::size_t index : current_)
    {
        const VimPatternAtom &atom = pattern_.atoms_.at(index);
        const auto begin = current_begins_.at(index);
        if (!begin.has_value() || !pattern_.consumes(atom, code))
        {
            continue;
        }
        const auto match =
            expanded(next_, next_begins_, atom.repeated ? index : index + 1, begin.value());
        if (match.has_value())
        {
            found_ = match;
            break;
        }
    }
    // 一致より優先する消費候補だけがnextに残る。後続の低優先候補は捨てる。
    current_.swap(next_);
    current_begins_.swap(next_begins_);
}

std::optional<VimPatternMatch> VimPatternEvaluation::matched(std::size_t from)
{
    at_ = from;
    while (true)
    {
        if (!found_.has_value())
        {
            found_ = expanded(current_, current_begins_, 0, at_);
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
