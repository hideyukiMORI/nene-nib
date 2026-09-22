#include "SearchLine.hpp"

#include <utility>

namespace nenenib::core
{
SearchLine::SearchLine(InputText input, VimSearchDirection direction)
    : input_(std::move(input)), direction_(direction)
{
}

SearchLine SearchLine::opened(VimSearchDirection direction)
{
    return SearchLine(InputText{std::string{}, Offset{0}}, direction);
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
    return direction_;
}

std::expected<SearchLine, ExFailure> SearchLine::inserted(std::string_view text) const
{
    const auto next = inserted_input(input_, text);
    if (!next)
    {
        return std::unexpected(next.error());
    }
    return SearchLine(next.value(), direction_);
}

SearchLine SearchLine::edited(CommandEdit edit) const
{
    // 補完は無い（却下した選択肢の 4 行目）。Tab の 2 つは文字列もキャレットも変えない。
    return SearchLine(edited_input(input_, edit), direction_);
}
} // namespace nenenib::core
