#include "CommandInput.hpp"

#include "CommandChoice.hpp"
#include "CommandChoiceKind.hpp"
#include "FontFamilies.hpp"
#include "PaletteScope.hpp"

#include <string>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
// プロンプトの文字はこの表だけが決める。写し先が足りなければコンパイルが落ちる（CPP-002）。
// 検索の行は `/` `?` と通常モードの「検索: 」「行: 」（ADR 0174 の決定 3）。
// 行が持つプロンプトの 1 つ。
[[nodiscard]] core::InputLinePrompt prompt_of(const core::SearchLine &line)
{
    return line.prompt();
}

[[nodiscard]] core::InputLinePrompt prompt_of(const core::CommandLine &)
{
    return core::InputLinePrompt::ex;
}

[[nodiscard]] core::InputLinePrompt prompt_of(const core::CommandPalette &)
{
    return core::InputLinePrompt::palette;
}

// 検索は補完も候補も持たないので、見え方の列は空になる（ADR 0032 の決定 1）。
[[nodiscard]] core::InputLineView view_of(const core::SearchLine &line)
{
    return core::InputLineView{
        prompt_of(line), std::string(line.text()), line.caret(), {}, std::nullopt};
}

// 補完を持つ入力行の見え方。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] core::InputLineView view_of(const core::CommandLine &line)
{
    return core::InputLineView{
        prompt_of(line),    std::string(line.text()), line.caret(),
        line.completions(), line.completion_index(),  line.completion_first()};
}

// Ex の補完を持つのは `:` の出どころだけ。ほかの出どころの入力は候補の絞り込み（ADR 0060 の
// 決定 5）。
[[nodiscard]] std::vector<std::string> completions_of(const core::CommandPalette &palette)
{
    switch (palette.scope())
    {
    case core::PaletteScope::commands:
        return palette.input().completions();
    case core::PaletteScope::files:
    case core::PaletteScope::tabs:
    case core::PaletteScope::bookmarks:
    case core::PaletteScope::history:
    case core::PaletteScope::folder:
    case core::PaletteScope::operations:
        return {};
    }
    std::unreachable();
}

[[nodiscard]] bool wants_fonts(const core::CommandLine &line)
{
    return core::completes_font_family(line.text());
}

[[nodiscard]] bool wants_fonts(const core::CommandPalette &palette)
{
    return core::completes_font_family(palette.input().text());
}

[[nodiscard]] bool wants_fonts(const core::SearchLine & /*line*/)
{
    return false;
}

[[nodiscard]] CommandInput fonts_given(const core::CommandLine &line,
                                       const core::FontFamilies &fonts)
{
    return line.with_fonts(fonts);
}

[[nodiscard]] CommandInput fonts_given(const core::CommandPalette &palette,
                                       const core::FontFamilies &fonts)
{
    return palette.with_fonts(fonts);
}

[[nodiscard]] CommandInput fonts_given(const core::SearchLine &line,
                                       const core::FontFamilies & /*fonts*/)
{
    return line;
}

[[nodiscard]] core::InputLineView view_of(const core::CommandPalette &palette)
{
    const core::CommandLine &line = palette.input();
    return core::InputLineView{
        prompt_of(palette),      std::string(line.text()), line.caret(),
        completions_of(palette), line.completion_index(),  line.completion_first()};
}

// 履歴の上下の写し。写し先が足りなければコンパイルが落ちる（CPP-002）。
[[nodiscard]] CommandInput recalled_of(const core::CommandLine &line, const core::InputHistory &ex,
                                       const core::InputHistory & /*search*/,
                                       core::CommandEdit direction)
{
    return line.recalled(ex, direction);
}

[[nodiscard]] CommandInput recalled_of(const core::SearchLine &line,
                                       const core::InputHistory & /*ex*/,
                                       const core::InputHistory &search,
                                       core::CommandEdit direction)
{
    // 「行: 」は検索ではないので検索の履歴を歩かない（ADR 0174 の決定 8）。
    if (line.prompt() == core::InputLinePrompt::go_to_line)
    {
        return line;
    }
    return line.recalled(search, direction);
}

[[nodiscard]] CommandInput recalled_of(const core::CommandPalette &palette,
                                       const core::InputHistory & /*ex*/,
                                       const core::InputHistory & /*search*/,
                                       core::CommandEdit /*direction*/)
{
    return palette;
}

[[nodiscard]] std::optional<std::string> focused_of(const core::SearchLine & /*line*/)
{
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> focused_of(const core::CommandLine &line)
{
    if (!line.completion_index().has_value())
    {
        return std::nullopt;
    }
    return std::string(line.text());
}

[[nodiscard]] std::optional<std::string> focused_of(const core::CommandPalette &palette)
{
    const auto chosen = palette.choice_at(palette.selected());
    if (!chosen.has_value())
    {
        return std::nullopt;
    }
    switch (chosen.value().kind)
    {
    case core::CommandChoiceKind::execute:
        return chosen.value().command;
    case core::CommandChoiceKind::open:
    case core::CommandChoiceKind::operate:
    case core::CommandChoiceKind::fill:
        return std::nullopt;
    }
    std::unreachable();
}
} // namespace

core::InputLineView input_line_of(const CommandInput &input)
{
    return std::visit([](const auto &value) { return view_of(value); }, input);
}

core::InputLinePrompt input_line_prompt_of(const CommandInput &input)
{
    return std::visit([](const auto &value) { return prompt_of(value); }, input);
}

std::expected<CommandInput, core::ExFailure> inserted_command(const CommandInput &input,
                                                              std::string_view text)
{
    return std::visit(
        [text](const auto &value) -> std::expected<CommandInput, core::ExFailure>
        {
            auto next = value.inserted(text);
            if (!next)
            {
                return std::unexpected(next.error());
            }
            return CommandInput{std::move(next).value()};
        },
        input);
}

CommandInput edited_command(const CommandInput &input, core::CommandEdit edit)
{
    return std::visit([edit](const auto &value) -> CommandInput { return value.edited(edit); },
                      input);
}

CommandInput recalled_command(const CommandInput &input, const core::InputHistory &ex,
                              const core::InputHistory &search, core::CommandEdit direction)
{
    return std::visit([&ex, &search, direction](const auto &value)
                      { return recalled_of(value, ex, search, direction); }, input);
}

std::optional<std::string> focused_command_of(const CommandInput &input)
{
    return std::visit([](const auto &value) { return focused_of(value); }, input);
}

bool wants_font_families(const CommandInput &input)
{
    return std::visit([](const auto &value) { return wants_fonts(value); }, input);
}

CommandInput with_font_families(const CommandInput &input, const core::FontFamilies &fonts)
{
    return std::visit([&fonts](const auto &value) { return fonts_given(value, fonts); }, input);
}
} // namespace nenenib::application
