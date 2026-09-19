#include "CommandPalette.hpp"

#include <utility>

namespace nenenib::core
{
CommandPalette::CommandPalette(CommandLine input, std::size_t selected)
    : input_(std::move(input)), selected_(selected)
{
}

CommandPalette CommandPalette::opened(ThemeCatalog themes)
{
    return CommandPalette(CommandLine::empty(std::move(themes)).inserted(":").value(), 0);
}

const CommandLine &CommandPalette::input() const noexcept
{
    return input_;
}

std::vector<CommandChoice> CommandPalette::choices() const
{
    return palette_choices(input_);
}

std::size_t CommandPalette::selected() const noexcept
{
    return selected_;
}

std::expected<CommandPalette, ExFailure> CommandPalette::inserted(std::string_view text) const
{
    const auto next = input_.inserted(text);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    return CommandPalette(next.value(), 0);
}

CommandPalette CommandPalette::moved(CommandEdit direction) const
{
    const auto count = choices().size();
    if (count == 0)
    {
        return *this;
    }
    const auto step = direction == CommandEdit::complete_previous ? count - 1 : 1;
    return CommandPalette(input_, (selected_ + step) % count);
}

CommandPalette CommandPalette::edited(CommandEdit edit) const
{
    switch (edit)
    {
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
        return moved(edit);
    case CommandEdit::left:
    case CommandEdit::right:
    case CommandEdit::home:
    case CommandEdit::end:
    case CommandEdit::backspace:
    case CommandEdit::erase:
        return CommandPalette(input_.edited(edit), 0);
    }
    std::unreachable();
}

CommandPalette CommandPalette::selected_at(std::size_t index) const
{
    if (index >= choices().size())
    {
        return *this;
    }
    return CommandPalette(input_, index);
}

std::expected<CommandPalette, ExFailure> CommandPalette::filled(std::string_view command) const
{
    const auto input = CommandLine::empty(input_.catalog()).inserted(":" + std::string(command));
    if (!input)
    {
        return std::unexpected(input.error());
    }
    return CommandPalette(input.value(), 0);
}
} // namespace nenenib::core
