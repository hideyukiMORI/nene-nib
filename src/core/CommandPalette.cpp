#include "CommandPalette.hpp"

#include <utility>

namespace nenenib::core
{
CommandPalette::CommandPalette(CommandLine input, std::size_t selected, CommandPaletteSource source,
                               std::vector<CommandChoice> tabs)
    : input_(std::move(input)), selected_(selected), source_(source), tabs_(std::move(tabs))
{
}

CommandPalette CommandPalette::opened(ThemeCatalog themes)
{
    return CommandPalette(CommandLine::empty(std::move(themes)).inserted(":").value(), 0,
                          CommandPaletteSource::commands, {});
}

CommandPalette CommandPalette::opened_tabs(std::vector<CommandChoice> tabs, std::size_t active,
                                           ThemeCatalog themes)
{
    const std::size_t selected = active < tabs.size() ? active : 0;
    return CommandPalette(CommandLine::empty(std::move(themes)), selected,
                          CommandPaletteSource::tabs, std::move(tabs));
}

CommandPaletteSource CommandPalette::source() const noexcept
{
    return source_;
}

CommandPalette CommandPalette::with_input(CommandLine input, std::size_t selected) const
{
    return CommandPalette(std::move(input), selected, source_, tabs_);
}

const CommandLine &CommandPalette::input() const noexcept
{
    return input_;
}

std::vector<CommandChoice> CommandPalette::choices() const
{
    switch (source_)
    {
    case CommandPaletteSource::commands:
        return palette_choices(input_);
    case CommandPaletteSource::tabs:
        return tab_list_choices(tabs_, input_.text());
    }
    std::unreachable();
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
    return with_input(next.value(), 0);
}

CommandPalette CommandPalette::moved(CommandEdit direction) const
{
    const auto count = choices().size();
    if (count == 0)
    {
        return *this;
    }
    const auto step = direction == CommandEdit::complete_previous ? count - 1 : 1;
    return with_input(input_, (selected_ + step) % count);
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
        return with_input(input_.edited(edit), 0);
    }
    std::unreachable();
}

CommandPalette CommandPalette::selected_at(std::size_t index) const
{
    if (index >= choices().size())
    {
        return *this;
    }
    return with_input(input_, index);
}

std::expected<CommandPalette, ExFailure> CommandPalette::filled(std::string_view command) const
{
    const auto input = CommandLine::empty(input_.catalog()).inserted(":" + std::string(command));
    if (!input)
    {
        return std::unexpected(input.error());
    }
    return CommandPalette(input.value(), 0, CommandPaletteSource::commands, {});
}
} // namespace nenenib::core
