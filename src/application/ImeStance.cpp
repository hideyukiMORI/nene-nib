#include "ImeStance.hpp"

#include <utility>
#include <variant>

namespace nenenib::application
{
namespace
{
// 入力の種類ごとの構え。写し先が足りなければ std::visit がコンパイルで落ちる（CPP-002）。
[[nodiscard]] ImeStance stance_of(const core::CommandLine &) noexcept
{
    return ImeStance::closed;
}

[[nodiscard]] ImeStance stance_of(const core::SearchLine &) noexcept
{
    return ImeStance::closed;
}

[[nodiscard]] ImeStance stance_of(const core::CommandPalette &) noexcept
{
    return ImeStance::closed_once;
}

// 入力が無いときはモードが決める（ADR 0014 の決定 5・今までの ui の ime_blocked と同じ表）。
[[nodiscard]] ImeStance stance_of(core::EditMode mode, core::VimMode vim_mode) noexcept
{
    switch (mode)
    {
    case core::EditMode::ordinary:
        return ImeStance::as_left;
    case core::EditMode::vim:
        break;
    }
    switch (vim_mode)
    {
    case core::VimMode::insert:
        return ImeStance::as_left;
    case core::VimMode::normal:
    case core::VimMode::visual:
    case core::VimMode::visual_line:
    case core::VimMode::visual_block:
        return ImeStance::closed;
    }
    std::unreachable();
}
} // namespace

ImeStance ime_stance_of(core::EditMode mode, core::VimMode vim_mode,
                        const std::optional<CommandInput> &input)
{
    if (!input.has_value())
    {
        return stance_of(mode, vim_mode);
    }
    return std::visit([](const auto &value) { return stance_of(value); }, input.value());
}
} // namespace nenenib::application
