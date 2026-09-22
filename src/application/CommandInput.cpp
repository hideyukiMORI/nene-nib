#include "CommandInput.hpp"

namespace nenenib::application
{
namespace
{
// 検索は補完も候補も持たないので、見え方の列は空になる（ADR 0032 の決定 1）。
[[nodiscard]] core::InputLineView view_of(const core::SearchLine &line)
{
    return core::InputLineView{line.direction() == core::VimSearchDirection::forward
                                   ? core::InputLinePrompt::search_forward
                                   : core::InputLinePrompt::search_backward,
                               std::string(line.text()),
                               line.caret(),
                               {},
                               std::nullopt};
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

// 補完を持つ入力行の見え方。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] core::InputLineView view_of(const core::CommandLine &line)
{
    return core::InputLineView{prompt_of(line), std::string(line.text()), line.caret(),
                               line.completions(), line.completion_index()};
}

[[nodiscard]] core::InputLineView view_of(const core::CommandPalette &palette)
{
    const core::CommandLine &line = palette.input();
    return core::InputLineView{prompt_of(palette), std::string(line.text()), line.caret(),
                               line.completions(), line.completion_index()};
}
} // namespace

core::InputLineView input_line_of(const CommandInput &input)
{
    return std::visit([](const auto &value) { return view_of(value); }, input);
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
