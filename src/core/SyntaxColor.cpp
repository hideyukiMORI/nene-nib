#include "SyntaxColor.hpp"

#include <utility>

namespace nenenib::core
{
RgbColor syntax_color(const SyntaxPalette &palette, SyntaxRole role) noexcept
{
    switch (role)
    {
    case SyntaxRole::comment:
        return palette.comment;
    case SyntaxRole::keyword:
        return palette.keyword;
    case SyntaxRole::string:
        return palette.string;
    case SyntaxRole::number:
        return palette.number;
    case SyntaxRole::type:
        return palette.type;
    case SyntaxRole::function:
        return palette.function;
    case SyntaxRole::constant:
        return palette.constant;
    case SyntaxRole::operators:
        return palette.operators;
    case SyntaxRole::error:
        return palette.error;
    case SyntaxRole::warning:
        return palette.warning;
    }
    std::unreachable();
}
} // namespace nenenib::core
