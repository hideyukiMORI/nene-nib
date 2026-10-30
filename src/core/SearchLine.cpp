#include "SearchLine.hpp"

#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// preset を置いてキャレットを末尾に置いた入力。1 行の入力にならなければ空。
[[nodiscard]] InputText preset_input(std::string_view preset)
{
    const InputText empty{std::string{}, Offset{0}};
    auto filled = inserted_input(empty, preset);
    return filled ? std::move(filled).value() : empty;
}
} // namespace

SearchLine::SearchLine(InputText input, InputLinePrompt prompt)
    : input_(std::move(input)), prompt_(prompt)
{
}

SearchLine SearchLine::opened(VimSearchDirection direction)
{
    return SearchLine(InputText{std::string{}, Offset{0}}, direction == VimSearchDirection::forward
                                                               ? InputLinePrompt::search_forward
                                                               : InputLinePrompt::search_backward);
}

SearchLine SearchLine::opened_find(std::string_view preset)
{
    return SearchLine(preset_input(preset), InputLinePrompt::find);
}

SearchLine SearchLine::opened_replace_with(std::string_view preset)
{
    return SearchLine(preset_input(preset), InputLinePrompt::replace_with);
}

SearchLine SearchLine::opened_go_to_line()
{
    return SearchLine(InputText{std::string{}, Offset{0}}, InputLinePrompt::go_to_line);
}

std::string_view SearchLine::text() const noexcept
{
    return input_.text;
}

Offset SearchLine::caret() const noexcept
{
    return input_.caret;
}

VimSearchDirection SearchLine::direction() const noexcept
{
    return prompt_ == InputLinePrompt::search_backward ? VimSearchDirection::backward
                                                       : VimSearchDirection::forward;
}

InputLinePrompt SearchLine::prompt() const noexcept
{
    return prompt_;
}

std::expected<SearchLine, ExFailure> SearchLine::inserted(std::string_view text) const
{
    auto next = inserted_input(input_, text);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    return SearchLine(std::move(next).value(), prompt_);
}

SearchLine SearchLine::edited(CommandEdit edit) const
{
    // 補完は無い（却下した選択肢の 4 行目）。Tab の 2 つは文字列もキャレットも変えない。
    return SearchLine(edited_input(input_, edit), prompt_);
}

SearchLine SearchLine::recalled(const InputHistory &history, CommandEdit direction) const
{
    const auto walk = history.stepped(input_, history_walk_, direction);
    if (!walk.has_value())
    {
        return *this;
    }
    SearchLine line(history.shown(walk.value()), prompt_);
    line.history_walk_ = walk;
    return line;
}
} // namespace nenenib::core
