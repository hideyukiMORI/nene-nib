#include "CommandLine.hpp"

#include "ExResult.hpp"
#include "InputText.hpp"

#include <utility>

namespace nenenib::core
{
CommandLine::CommandLine(std::string text, Offset caret, ThemeCatalog themes, FontFamilies fonts)
    : text_(std::move(text)), caret_(caret), themes_(std::move(themes)), fonts_(std::move(fonts))
{
}

CommandLine CommandLine::empty(ThemeCatalog themes)
{
    return CommandLine({}, Offset{0}, std::move(themes), FontFamilies::none());
}

const FontFamilies &CommandLine::fonts() const & noexcept
{
    return fonts_;
}

CommandLine CommandLine::with_fonts(FontFamilies fonts) const
{
    CommandLine next = *this;
    next.fonts_ = std::move(fonts);
    return next;
}

const ThemeCatalog &CommandLine::catalog() const & noexcept
{
    return themes_;
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
    const auto &prefix = completion_seed_.has_value() ? completion_seed_.value() : text_;
    return command_completions(prefix, themes_, fonts_);
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
    // 文字列とキャレットの規則は検索の入力行と共用する（ADR 0032 の決定 1・ARC-001）。
    auto next = inserted_input(InputText{text_, caret_}, text);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    const auto caret = next.value().caret;
    return CommandLine(std::move(next).value().text, caret, themes_, fonts_);
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
    CommandLine next(candidates[index], Offset{candidates[index].size()}, themes_, fonts_);
    next.completion_seed_ = completion_seed_.value_or(text_);
    next.completion_index_ = index;
    return next;
}

CommandLine CommandLine::edited(CommandEdit edit) const
{
    if (edit == CommandEdit::complete_next || edit == CommandEdit::complete_previous)
    {
        return completed(edit);
    }
    InputText next = edited_input(InputText{text_, caret_}, edit);
    const auto caret = next.caret;
    return CommandLine(std::move(next.text), caret, themes_, fonts_);
}
} // namespace nenenib::core
