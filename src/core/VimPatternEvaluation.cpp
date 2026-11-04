#include "VimPatternEvaluation.hpp"

#include "Utf8.hpp"
#include "VimPattern.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
// 終わりが文字の境（ends_a_character）の当たりだけを一致にする（#1138）。
[[nodiscard]] std::optional<VimPatternMatch> finished_match(bool on_boundary, std::size_t begin,
                                                            std::size_t end) noexcept
{
    if (!on_boundary)
    {
        return std::nullopt;
    }
    return VimPatternMatch{begin, end};
}
} // namespace

VimPatternEvaluation::VimPatternEvaluation(const VimPattern &pattern, std::string_view line)
    : pattern_(pattern), line_(line), at_(0), skipping_(pattern.atoms_.size() + 1),
      current_begins_(2 * skipping_), next_begins_(2 * skipping_)
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
            return finished_match(VimPattern::ends_a_character(line_, at_), begin, at_);
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

// `.` と集合が 1 文字の残りの結合文字を食べる間（#1146）、候補は skipping_ 以上の番号で
// 「target へ向かう途中」として列に残る（Vim の NFA の add_off と同じ・優先順を保つ）。
std::optional<VimPatternMatch> VimPatternEvaluation::arrived(std::size_t target, std::size_t begin)
{
    if (VimPattern::ends_a_character(line_, at_))
    {
        return expanded(next_, next_begins_, target, begin);
    }
    const std::size_t slot = skipping_ + target;
    if (!next_begins_.at(slot).has_value())
    {
        next_begins_.at(slot) = begin;
        next_.push_back(slot);
    }
    return std::nullopt;
}

// 列の 1 候補に code を食べさせて次の列へ置く。途中の印の候補は食べずに target へ向かう。
std::optional<VimPatternMatch> VimPatternEvaluation::stepped(std::size_t index, std::size_t begin,
                                                             char32_t code)
{
    if (index >= skipping_)
    {
        return arrived(index - skipping_, begin);
    }
    const VimPatternAtom &atom = pattern_.atoms_.at(index);
    if (!pattern_.consumes(atom, code))
    {
        return std::nullopt;
    }
    const std::size_t target = atom.repeated ? index : index + 1;
    return VimPattern::whole_character(atom.kind) ? arrived(target, begin)
                                                  : expanded(next_, next_begins_, target, begin);
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
        const auto begin = current_begins_.at(index);
        if (!begin.has_value())
        {
            continue;
        }
        const auto match = stepped(index, begin.value(), code);
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
        // 始まりは Vim の 1 文字の先頭だけ（結合文字の上からは試さない・#1146）。
        if (!found_.has_value() && VimPattern::ends_a_character(line_, at_))
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
