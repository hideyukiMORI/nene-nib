#include "CommandPalette.hpp"

#include "PaletteMarks.hpp"
#include "PaletteQuery.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 結果の 1 件の読み方は結果の種類ごとに 1 つ（std::visit で選ぶ・種類が増えたらここが落ちる）。
[[nodiscard]] const CommandChoice &row_of(const std::vector<std::size_t> &positions,
                                          const std::vector<CommandChoice> &entries,
                                          std::size_t index)
{
    return entries.at(positions.at(index));
}

[[nodiscard]] const CommandChoice &row_of(const std::vector<CommandChoice> &choices,
                                          const std::vector<CommandChoice> & /*entries*/,
                                          std::size_t index)
{
    return choices.at(index);
}
} // namespace

CommandPalette::CommandPalette(CommandLine input, std::size_t selected, Entries entries,
                               std::shared_ptr<const Result> result)
    : input_(std::move(input)), selected_(selected), entries_(std::move(entries)),
      result_(std::move(result))
{
}

CommandPalette CommandPalette::opened(std::vector<CommandChoice> entries, std::string_view input,
                                      std::size_t selected, ThemeCatalog themes)
{
    const auto empty = CommandLine::empty(std::move(themes));
    return filtered(empty.inserted(input).value_or(empty),
                    std::make_shared<const std::vector<CommandChoice>>(std::move(entries)))
        .selected_at(selected);
}

CommandPalette CommandPalette::filtered(CommandLine input, Entries entries)
{
    auto result = std::make_shared<const Result>(result_of(input, *entries));
    return CommandPalette(std::move(input), 0, std::move(entries), std::move(result));
}

CommandPalette::Result CommandPalette::result_of(const CommandLine &input,
                                                 const std::vector<CommandChoice> &entries)
{
    const PaletteQuery query = palette_query_of(input.text());
    switch (query.scope)
    {
    case PaletteScope::commands:
        // palette_choices は先頭の `:` を自分で剥がす（結果は Ctrl+P が `:`
        // で開いていた頃と同じ）。
        return Result{palette_choices(input)};
    case PaletteScope::files:
    case PaletteScope::tabs:
    case PaletteScope::history:
        return Result{listed_positions(entries, query.scope, query.query)};
    }
    std::unreachable();
}

CommandPalette CommandPalette::reselected(std::size_t selected) const
{
    return CommandPalette(input_, selected, entries_, result_);
}

PaletteScope CommandPalette::scope() const noexcept
{
    return palette_query_of(input_.text()).scope;
}

const CommandLine &CommandPalette::input() const noexcept
{
    return input_;
}

std::size_t CommandPalette::count() const noexcept
{
    return std::visit([](const auto &result) noexcept { return result.size(); }, *result_);
}

const CommandChoice &CommandPalette::row(std::size_t index) const
{
    return std::visit([this, index](const auto &result) -> const CommandChoice &
                      { return row_of(result, *entries_, index); }, *result_);
}

std::optional<CommandChoice> CommandPalette::choice_at(std::size_t index) const
{
    if (index >= count())
    {
        return std::nullopt;
    }
    return row(index);
}

std::vector<CommandChoice> CommandPalette::rows(std::size_t first, std::size_t limit) const
{
    const std::size_t total = count();
    std::vector<CommandChoice> listed;
    if (first >= total)
    {
        return listed;
    }
    const std::size_t last = first + std::min(limit, total - first);
    listed.reserve(last - first);
    for (std::size_t index = first; index < last; ++index)
    {
        listed.push_back(row(index));
    }
    return listed;
}

bool CommandPalette::shares_result_with(const CommandPalette &other) const noexcept
{
    return result_ == other.result_;
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
    return filtered(next.value(), entries_);
}

CommandPalette CommandPalette::moved(CommandEdit direction) const
{
    const auto total = count();
    if (total == 0)
    {
        return *this;
    }
    const auto step = direction == CommandEdit::complete_previous ? total - 1 : 1;
    return reselected((selected_ + step) % total);
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
        return filtered(input_.edited(edit), entries_);
    }
    std::unreachable();
}

CommandPalette CommandPalette::selected_at(std::size_t index) const
{
    if (index >= count())
    {
        return *this;
    }
    return reselected(index);
}

// 候補の列を持ったまま、入力を `:<command>` にする（ADR 0060 の決定 5）。
std::expected<CommandPalette, ExFailure> CommandPalette::filled(std::string_view command) const
{
    const auto input = CommandLine::empty(input_.catalog()).inserted(":" + std::string(command));
    if (!input)
    {
        return std::unexpected(input.error());
    }
    return filtered(input.value(), entries_);
}
} // namespace nenenib::core
