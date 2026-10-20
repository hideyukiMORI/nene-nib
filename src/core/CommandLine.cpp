#include "CommandLine.hpp"

#include "AsciiCase.hpp"
#include "CommandLayout.hpp"
#include "ExResult.hpp"
#include "InputText.hpp"
#include "ListWindow.hpp"

#include <algorithm>
#include <utility>

namespace nenenib::core
{
namespace
{
// UTF-8 の続きのバイト（10xxxxxx）。共通の接頭辞をコードポイントの途中で切らないために読む。
[[nodiscard]] constexpr bool continuation_byte(char byte) noexcept
{
    return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U;
}

// 候補の共通の接頭辞の長さ（ADR 0150 の決定 3）。ASCII の大文字小文字は畳んで比べ（#696 の
// AsciiCase）、コードポイントの途中では切らない。埋める文字は最初の候補のバイト（候補の表示の
// 大文字小文字）。
[[nodiscard]] std::size_t common_prefix_size(const std::vector<std::string> &candidates)
{
    const std::string_view head = candidates.front();
    std::size_t size = head.size();
    for (const std::string &other : candidates)
    {
        const std::string_view kept = head.substr(0, size);
        const auto found =
            std::ranges::mismatch(kept, std::string_view(other), {}, ascii_folded, ascii_folded);
        size = static_cast<std::size_t>(found.in1 - kept.begin());
    }
    while (size > 0 && size < head.size() && continuation_byte(head[size]))
    {
        --size;
    }
    return size;
}
// 巡回の次の位置。at が空（打った元の文字）なら先頭（逆は末尾）、端の次は空（元の文字）。
[[nodiscard]] std::optional<std::size_t> cycled_index(std::optional<std::size_t> at,
                                                      std::size_t count, bool backwards) noexcept
{
    const std::size_t last = count - 1;
    if (!at.has_value())
    {
        return backwards ? last : 0;
    }
    if (at.value() == (backwards ? 0 : last))
    {
        return std::nullopt;
    }
    return backwards ? at.value() - 1 : at.value() + 1;
}
} // namespace

CommandLine::CommandLine(std::string text, Offset caret, ThemeCatalog themes, FontFamilies fonts)
    : text_(std::move(text)), caret_(caret), completion_rows_(command_completion_row_limit),
      themes_(std::move(themes)), fonts_(std::move(fonts))
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

std::size_t CommandLine::completion_first() const noexcept
{
    return completion_first_;
}

std::size_t CommandLine::completion_rows() const noexcept
{
    return completion_rows_;
}

CommandLine CommandLine::with_completion_rows(std::size_t rows) const
{
    CommandLine next = *this;
    next.completion_rows_ = std::max(rows, std::size_t{1});
    next.completion_first_ =
        list_window_first(completion_first_, completion_index().value_or(completion_first_),
                          next.completion_rows_, completions().size());
    return next;
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
    CommandLine line(std::move(next).value().text, caret, themes_, fonts_);
    line.completion_rows_ = completion_rows_;
    return line;
}

CommandLine CommandLine::completed(CommandEdit direction) const
{
    const auto candidates = completions();
    if (candidates.empty())
    {
        return *this;
    }
    if (completion_seed_.has_value())
    {
        return cycled(candidates, direction);
    }
    const auto common = common_prefix_size(candidates);
    if (common <= text_.size())
    {
        return cycled(candidates, direction);
    }
    std::string filled = candidates.front().substr(0, common);
    const Offset caret{filled.size()};
    CommandLine next(std::move(filled), caret, themes_, fonts_);
    next.completion_rows_ = completion_rows_;
    return next;
}

// 巡回の 1 歩。元の文字 → 先頭（Shift+Tab は末尾）→ … → 末尾（Shift+Tab は先頭）→ 元の文字。
CommandLine CommandLine::cycled(const std::vector<std::string> &candidates,
                                CommandEdit direction) const
{
    const auto index = cycled_index(completion_seed_.has_value() ? completion_index_ : std::nullopt,
                                    candidates.size(), direction == CommandEdit::complete_previous);
    std::string seed = completion_seed_.value_or(text_);
    std::string shown = index.has_value() ? candidates[index.value()] : seed;
    const Offset caret{shown.size()};
    CommandLine next(std::move(shown), caret, themes_, fonts_);
    next.completion_seed_ = std::move(seed);
    next.completion_index_ = index;
    next.completion_rows_ = completion_rows_;
    next.completion_first_ = list_window_first(completion_first_, index.value_or(completion_first_),
                                               completion_rows_, candidates.size());
    return next;
}

CommandLine CommandLine::edited(CommandEdit edit) const
{
    switch (edit)
    {
    case CommandEdit::complete_next:
    case CommandEdit::complete_previous:
        return completed(edit);
    case CommandEdit::page_previous:
    case CommandEdit::page_next:
    case CommandEdit::list_first:
    case CommandEdit::list_last:
        // `:` の行の一覧の送りは無い（ADR 0150 の決定 3）。
    case CommandEdit::history_previous:
    case CommandEdit::history_next:
        // 履歴は recalled が歩く（履歴を持つのは application の状態・#725）。
        return *this;
    case CommandEdit::left:
    case CommandEdit::right:
    case CommandEdit::home:
    case CommandEdit::end:
    case CommandEdit::backspace:
    case CommandEdit::erase:
        break;
    }
    InputText next = edited_input(InputText{text_, caret_}, edit);
    const auto caret = next.caret;
    CommandLine line(std::move(next.text), caret, themes_, fonts_);
    line.completion_rows_ = completion_rows_;
    return line;
}

CommandLine CommandLine::recalled(const InputHistory &history, CommandEdit direction) const
{
    const auto walk = history.stepped(InputText{text_, caret_}, history_walk_, direction);
    if (!walk.has_value())
    {
        return *this;
    }
    InputText shown = history.shown(walk.value());
    const auto caret = shown.caret;
    CommandLine line(std::move(shown.text), caret, themes_, fonts_);
    line.completion_rows_ = completion_rows_;
    line.history_walk_ = walk;
    return line;
}
} // namespace nenenib::core
