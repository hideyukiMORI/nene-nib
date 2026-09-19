#include "CommandInput.hpp"

namespace nenenib::application
{
namespace
{
[[nodiscard]] const core::CommandLine &line_of(const core::CommandLine &line)
{
    return line;
}

[[nodiscard]] const core::CommandLine &line_of(const core::CommandPalette &palette)
{
    return palette.input();
}
} // namespace

const core::CommandLine &command_line_of(const CommandInput &input)
{
    return std::visit([](const auto &value) -> const core::CommandLine & { return line_of(value); },
                      input);
}

std::expected<CommandInput, core::ExFailure> inserted_command(const CommandInput &input,
                                                              std::string_view text)
{
    return std::visit(
        [text](const auto &value) -> std::expected<CommandInput, core::ExFailure>
        {
            const auto next = value.inserted(text);
            if (!next)
            {
                return std::unexpected(next.error());
            }
            return CommandInput{next.value()};
        },
        input);
}

CommandInput edited_command(const CommandInput &input, core::CommandEdit edit)
{
    return std::visit([edit](const auto &value) -> CommandInput { return value.edited(edit); },
                      input);
}
} // namespace nenenib::application
