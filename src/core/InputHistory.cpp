#include "InputHistory.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
// before より前（古い方）で prefix から始まるいちばん新しい位置。
[[nodiscard]] std::optional<std::size_t> previous_match(const std::vector<std::string> &entries,
                                                        std::string_view prefix, std::size_t before)
{
    for (std::size_t index = before; index > 0; --index)
    {
        if (std::string_view(entries[index - 1]).starts_with(prefix))
        {
            return index - 1;
        }
    }
    return std::nullopt;
}

// from から後ろ（新しい方）で prefix から始まるいちばん古い位置。
[[nodiscard]] std::optional<std::size_t> next_match(const std::vector<std::string> &entries,
                                                    std::string_view prefix, std::size_t from)
{
    for (std::size_t index = from; index < entries.size(); ++index)
    {
        if (std::string_view(entries[index]).starts_with(prefix))
        {
            return index;
        }
    }
    return std::nullopt;
}
} // namespace

InputHistory::InputHistory(std::shared_ptr<const std::vector<std::string>> entries)
    : entries_(std::move(entries))
{
}

InputHistory InputHistory::empty()
{
    return InputHistory(std::make_shared<const std::vector<std::string>>());
}

std::size_t InputHistory::size() const noexcept
{
    return entries_->size();
}

std::string_view InputHistory::at(std::size_t index) const
{
    return (*entries_)[index];
}

InputHistory InputHistory::remembered(std::string_view entry) const
{
    if (entry.empty() || (!entries_->empty() && entries_->back() == entry))
    {
        return *this;
    }
    std::vector<std::string> next;
    const std::size_t kept = entries_->size() < limit ? entries_->size() : limit - 1;
    next.reserve(kept + 1);
    next.assign(entries_->end() - static_cast<std::ptrdiff_t>(kept), entries_->end());
    next.emplace_back(entry);
    return InputHistory(std::make_shared<const std::vector<std::string>>(std::move(next)));
}

std::optional<HistoryWalk> InputHistory::stepped(const InputText &typed,
                                                 const std::optional<HistoryWalk> &walk,
                                                 CommandEdit direction) const
{
    HistoryWalk next = walk.value_or(HistoryWalk{typed, std::nullopt});
    if (direction == CommandEdit::history_previous)
    {
        const auto found =
            previous_match(*entries_, next.origin.text, next.at.value_or(entries_->size()));
        if (!found.has_value())
        {
            return std::nullopt;
        }
        next.at = found;
        return next;
    }
    if (direction != CommandEdit::history_next || !next.at.has_value())
    {
        return std::nullopt;
    }
    // 次が無ければ空（歩きを始めたときの入力に戻る・回り込まない）。
    next.at = next_match(*entries_, next.origin.text, next.at.value() + 1);
    return next;
}

InputText InputHistory::shown(const HistoryWalk &walk) const
{
    if (!walk.at.has_value())
    {
        return walk.origin;
    }
    std::string text = (*entries_)[walk.at.value()];
    const Offset caret{text.size()};
    return InputText{std::move(text), caret};
}
} // namespace nenenib::core
