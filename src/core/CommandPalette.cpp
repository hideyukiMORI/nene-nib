#include "CommandPalette.hpp"

#include "PaletteMarks.hpp"
#include "PaletteQuery.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
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

// 選んでいた候補の列の中の位置（ADR 0062 の決定 16）。結果が空なら無し。設定のコマンドの結果は
// 列に依らないので無し。
[[nodiscard]] std::optional<std::size_t> entry_position(const std::vector<std::size_t> &positions,
                                                        std::size_t selected)
{
    if (selected >= positions.size())
    {
        return std::nullopt;
    }
    return positions.at(selected);
}

[[nodiscard]] std::optional<std::size_t>
entry_position(const std::vector<CommandChoice> & /*choices*/, std::size_t /*selected*/)
{
    return std::nullopt;
}

// 伸ばした後の結果の中で、列の位置 kept の候補の番号。無ければ先頭。
[[nodiscard]] std::size_t carried_index(const std::vector<std::size_t> &positions,
                                        std::optional<std::size_t> kept, std::size_t /*selected*/)
{
    if (!kept.has_value())
    {
        return 0;
    }
    const auto found = std::ranges::find(positions, kept.value());
    return found == positions.end()
               ? 0
               : static_cast<std::size_t>(std::ranges::distance(positions.begin(), found));
}

// 設定のコマンドの結果は列に依らないので、選択の番号はそのまま。
[[nodiscard]] std::size_t carried_index(const std::vector<CommandChoice> & /*choices*/,
                                        std::optional<std::size_t> /*kept*/, std::size_t selected)
{
    return selected;
}
} // namespace

CommandPalette::CommandPalette(CommandLine input, std::size_t selected, EditMode mode,
                               Entries entries, std::shared_ptr<const Result> result)
    : input_(std::move(input)), selected_(selected), mode_(mode), entries_(std::move(entries)),
      result_(std::move(result))
{
}

CommandPalette CommandPalette::opened(std::vector<CommandChoice> entries, std::string_view input,
                                      EditMode mode, ThemeCatalog themes)
{
    const auto empty = CommandLine::empty(std::move(themes));
    return filtered(empty.inserted(input).value_or(empty), mode,
                    std::make_shared<const std::vector<CommandChoice>>(std::move(entries)));
}

CommandPalette CommandPalette::filtered(CommandLine input, EditMode mode, Entries entries)
{
    auto result = std::make_shared<const Result>(result_of(input, mode, *entries));
    return CommandPalette(std::move(input), 0, mode, std::move(entries), std::move(result));
}

CommandPalette::Result CommandPalette::result_of(const CommandLine &input, EditMode mode,
                                                 const std::vector<CommandChoice> &entries)
{
    const PaletteQuery query = palette_query_of(input.text());
    switch (query.scope)
    {
    case PaletteScope::commands:
        // palette_choices は先頭の `:` を自分で剥がす（結果は Ctrl+P が `:`
        // で開いていた頃と同じ）。
        return Result{palette_choices(input)};
    case PaletteScope::operations:
        return Result{operation_choices(query.query, mode)};
    case PaletteScope::files:
    case PaletteScope::tabs:
    case PaletteScope::bookmarks:
    case PaletteScope::history:
    case PaletteScope::folder:
        return Result{listed_positions(entries, query.scope, query.query)};
    }
    std::unreachable();
}

CommandPalette CommandPalette::reselected(std::size_t selected) const
{
    return CommandPalette(input_, selected, mode_, entries_, result_);
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
    return filtered(next.value(), mode_, entries_);
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
        return filtered(input_.edited(edit), mode_, entries_);
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
    return filtered(input.value(), mode_, entries_);
}

CommandPalette CommandPalette::extended(std::vector<CommandChoice> more) const
{
    if (more.empty())
    {
        return *this;
    }
    auto grown = std::make_shared<std::vector<CommandChoice>>();
    grown->reserve(entries_->size() + more.size());
    grown->insert(grown->end(), entries_->begin(), entries_->end());
    grown->insert(grown->end(), std::make_move_iterator(more.begin()),
                  std::make_move_iterator(more.end()));
    const auto kept = std::visit([this](const auto &result)
                                 { return entry_position(result, selected_); }, *result_);
    const CommandPalette next = filtered(input_, mode_, std::move(grown));
    const auto index =
        std::visit([this, kept](const auto &result)
                   { return carried_index(result, kept, selected_); }, *next.result_);
    return next.reselected(index);
}
} // namespace nenenib::core
