#include "CommandPalette.hpp"

#include "PaletteMarks.hpp"
#include "PaletteQuery.hpp"

#include <string>
#include <utility>

namespace nenenib::core
{
CommandPalette::CommandPalette(CommandLine input, std::size_t selected,
                               std::shared_ptr<const std::vector<CommandChoice>> entries)
    : input_(std::move(input)), selected_(selected), entries_(std::move(entries))
{
}

CommandPalette CommandPalette::opened(std::vector<CommandChoice> entries, std::string_view input,
                                      std::size_t selected, ThemeCatalog themes)
{
    const auto empty = CommandLine::empty(std::move(themes));
    const CommandPalette palette(
        empty.inserted(input).value_or(empty), 0,
        std::make_shared<const std::vector<CommandChoice>>(std::move(entries)));
    return palette.selected_at(selected);
}

PaletteScope CommandPalette::scope() const noexcept
{
    return palette_query_of(input_.text()).scope;
}

CommandPalette CommandPalette::with_input(CommandLine input, std::size_t selected) const
{
    return CommandPalette(std::move(input), selected, entries_);
}

const CommandLine &CommandPalette::input() const noexcept
{
    return input_;
}

std::vector<CommandChoice> CommandPalette::choices() const
{
    const PaletteQuery query = palette_query_of(input_.text());
    switch (query.scope)
    {
    case PaletteScope::commands:
        // palette_choices は先頭の `:` を自分で剥がす（結果は Ctrl+P が `:`
        // で開いていた頃と同じ）。
        return palette_choices(input_);
    case PaletteScope::files:
    case PaletteScope::tabs:
    case PaletteScope::history:
        return listed_choices(*entries_, query.scope, query.query);
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

// 候補の列を持ったまま、入力を `:<command>` にする（ADR 0060 の決定 5）。
std::expected<CommandPalette, ExFailure> CommandPalette::filled(std::string_view command) const
{
    const auto input = CommandLine::empty(input_.catalog()).inserted(":" + std::string(command));
    if (!input)
    {
        return std::unexpected(input.error());
    }
    return CommandPalette(input.value(), 0, entries_);
}
} // namespace nenenib::core
