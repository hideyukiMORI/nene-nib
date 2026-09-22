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

// プロンプトの文字はこの表だけが決める。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] core::InputLinePrompt prompt_of(const core::CommandLine &)
{
    return core::InputLinePrompt::ex;
}

[[nodiscard]] core::InputLinePrompt prompt_of(const core::CommandPalette &)
{
    return core::InputLinePrompt::palette;
}
} // namespace

core::InputLineView input_line_of(const CommandInput &input)
{
    return std::visit(
        [](const auto &value) -> core::InputLineView
        {
            const core::CommandLine &line = line_of(value);
            return core::InputLineView{prompt_of(value), std::string(line.text()), line.caret(),
                                       line.completions(), line.completion_index()};
        },
        input);
}

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
