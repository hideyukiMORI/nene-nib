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

// Up の 1 歩。前が無ければ空（動かない・回り込まない）。
[[nodiscard]] std::optional<HistoryWalk> walked_back(const std::vector<std::string> &entries,
                                                     HistoryWalk walk)
{
    const auto found = previous_match(entries, walk.origin.text, walk.at.value_or(entries.size()));
    if (!found.has_value())
    {
        return std::nullopt;
    }
    walk.at = found;
    return walk;
}

// Down の 1 歩。歩いていなければ空。次が無ければ位置を空にする（歩きを始めたときの入力に戻る）。
[[nodiscard]] std::optional<HistoryWalk> walked_forward(const std::vector<std::string> &entries,
                                                        HistoryWalk walk)
{
    if (!walk.at.has_value())
    {
        return std::nullopt;
    }
    walk.at = next_match(entries, walk.origin.text, walk.at.value() + 1);
    return walk;
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
    switch (direction)
    {
    case CommandEdit::history_previous:
        return walked_back(*entries_, std::move(next));
    case CommandEdit::history_next:
        return walked_forward(*entries_, std::move(next));
    case CommandEdit::left:
    case CommandEdit::right:
    case CommandEdit::home:
    case CommandEdit::end:
    case CommandEdit::backspace:
    case CommandEdit::erase:
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
    case CommandEdit::page_previous:
    case CommandEdit::page_next:
    case CommandEdit::list_first:
    case CommandEdit::list_last:
        return std::nullopt;
    }
    std::unreachable();
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
