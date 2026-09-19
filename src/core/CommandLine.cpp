#include "CommandLine.hpp"

#include "ExResult.hpp"
#include "Utf8.hpp"

#include <utility>

namespace nenenib::core
{
CommandLine::CommandLine(std::string text, Offset caret) : text_(std::move(text)), caret_(caret) {}

CommandLine CommandLine::empty()
{
    return CommandLine({}, Offset{0});
}

std::string_view CommandLine::text() const noexcept
{
    return text_;
}

Offset CommandLine::caret() const noexcept
{
    return caret_;
}

std::vector<std::string> CommandLine::completions() const
{
    return command_completions(completion_seed_.value_or(text_));
}

std::optional<std::size_t> CommandLine::completion_index() const noexcept
{
    if (!completion_seed_.has_value())
    {
        return std::nullopt;
    }
    return completion_index_;
}

std::expected<CommandLine, ExFailure> CommandLine::inserted(std::string_view text) const
{
    if (text.size() > DisplayText::maximum_bytes - text_.size())
    {
        return std::unexpected(ExFailure::too_long);
    }
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    std::string next = text_;
    next.insert(caret_.value, text);
    return CommandLine(std::move(next), Offset{caret_.value + text.size()});
}

CommandLine CommandLine::completed(CommandEdit direction) const
{
    const auto candidates = completions();
    if (candidates.empty())
    {
        return *this;
    }
    const bool backwards = direction == CommandEdit::complete_previous;
    std::size_t index = backwards ? candidates.size() - 1 : 0;
    if (completion_seed_.has_value())
    {
        index = (completion_index_ + (backwards ? candidates.size() - 1 : 1)) % candidates.size();
    }
    CommandLine next(candidates[index], Offset{candidates[index].size()});
    next.completion_seed_ = completion_seed_.value_or(text_);
    next.completion_index_ = index;
    return next;
}

CommandLine CommandLine::edited(CommandEdit edit) const
{
    CommandLine next(text_, caret_);
    const auto before = previous_code_point(text_, caret_);
    const auto after = next_code_point(text_, caret_);
    switch (edit)
    {
    case CommandEdit::left:
        next.caret_ = before;
        break;
    case CommandEdit::right:
        next.caret_ = after;
        break;
    case CommandEdit::home:
        next.caret_ = Offset{0};
        break;
    case CommandEdit::end:
        next.caret_ = Offset{text_.size()};
        break;
    case CommandEdit::backspace:
        next.text_.erase(before.value, caret_.value - before.value);
        next.caret_ = before;
        break;
    case CommandEdit::erase:
        next.text_.erase(caret_.value, after.value - caret_.value);
        break;
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
        return completed(edit);
    }
    return next;
}
} // namespace nenenib::core
