#include "ExResult.hpp"

#include "BuiltinThemes.hpp"
#include "ExPaletteName.hpp"
#include "ExTabName.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <optional>
#include <system_error>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    const auto first = text.find_first_not_of(' ');
    if (first == std::string_view::npos)
    {
        return {};
    }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

[[nodiscard]] DisplayText theme_message(const EditorSettings &settings, Appearance appearance)
{
    std::string message = "colorscheme=" + std::string(selected_theme(settings, appearance).name);
    if (!settings.theme.has_value())
    {
        message += " (system)";
    }
    return DisplayText::parse(message).value();
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> colorscheme(std::string_view name,
                                                                       EditorSettings settings,
                                                                       Appearance appearance,
                                                                       const ThemeCatalog &themes)
{
    if (name.empty())
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        theme_message(settings, appearance)};
    }
    if (name == "system")
    {
        settings.theme = std::nullopt;
        return ExResult{settings, std::nullopt, std::nullopt, theme_message(settings, appearance)};
    }
    const auto parsed = ThemeName::parse(name);
    if (!parsed)
    {
        return std::unexpected(ExFailure::unknown_theme);
    }
    const auto found = themes.find(parsed.value());
    if (!found)
    {
        return std::unexpected(found.error());
    }
    settings.theme = found.value();
    return ExResult{settings, std::nullopt, std::nullopt, theme_message(settings, appearance)};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> set_font_size(std::string_view value,
                                                                         EditorSettings settings)
{
    const auto size = FontSize::parse(value);
    if (!size)
    {
        return std::unexpected(ExFailure::invalid_font_size);
    }
    settings.font_size = size.value();
    return ExResult{settings, std::nullopt, std::nullopt,
                    DisplayText::parse("fontsize=" + std::string(value)).value()};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> set_gui_font(std::string_view value,
                                                                        EditorSettings settings)
{
    const auto separator = value.rfind(":h");
    if (separator == std::string_view::npos)
    {
        return std::unexpected(ExFailure::invalid_font_family);
    }
    const auto family = DisplayText::parse(trimmed(value.substr(0, separator)));
    if (!family)
    {
        return std::unexpected(ExFailure::invalid_font_family);
    }
    const auto size = FontSize::parse(value.substr(separator + 2));
    if (!size)
    {
        return std::unexpected(ExFailure::invalid_font_size);
    }
    settings.font_size = size.value();
    settings.font_family = family.value();
    return ExResult{settings, std::nullopt, std::nullopt,
                    DisplayText::parse("guifont=" + std::string(value)).value()};
}

// 3 値が増えたらここでコンパイルが落ちる（CPP-002）。
[[nodiscard]] std::string_view highlight_name(VimSearchHighlight highlight)
{
    switch (highlight)
    {
    case VimSearchHighlight::on:
        return "on";
    case VimSearchHighlight::off:
        return "off";
    case VimSearchHighlight::suspended:
        return "suspended";
    }
    std::unreachable();
}

// 検索の強調の 3 値は設定に載せないので settings は空のまま返す（ADR 0037 の決定 1・2）。
[[nodiscard]] ExResult highlight_result(VimSearchHighlight highlight)
{
    return ExResult{
        std::nullopt, highlight, std::nullopt,
        DisplayText::parse("hlsearch=" + std::string(highlight_name(highlight))).value()};
}

// incsearch も設定に載せず、表示名は hlsearch と同じ形（ADR 0041 の決定 6）。
[[nodiscard]] ExResult incsearch_result(bool incsearch)
{
    return ExResult{std::nullopt, std::nullopt, incsearch,
                    DisplayText::parse(incsearch ? "incsearch=on" : "incsearch=off").value()};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
set_option(std::string_view option, const EditorSettings &settings)
{
    if (option.starts_with("fontsize="))
    {
        return set_font_size(option.substr(9), settings);
    }
    if (option.starts_with("guifont="))
    {
        return set_gui_font(option.substr(8), settings);
    }
    if (option == "hlsearch")
    {
        return highlight_result(VimSearchHighlight::on);
    }
    if (option == "nohlsearch")
    {
        return highlight_result(VimSearchHighlight::off);
    }
    if (option == "incsearch")
    {
        return incsearch_result(true);
    }
    if (option == "noincsearch")
    {
        return incsearch_result(false);
    }
    return std::unexpected(ExFailure::unknown_option);
}
// タブの命令の名前の表（ADR 0057 の決定 4・CPP-012）。省略は Vim 9.1 の実測のとおり「最短の形
// から完全な形までの前方一致」で、`tabne` は tabnext（tabnew は完全一致だけ）。`tab` `tabe`
// `tabm` `tabf` はどの行にも当たらず、今までどおり unknown_command になる。
constexpr std::array<ExTabName, 6> tab_names{{{"tabnext", 4, ExTabVerb::next},
                                              {"tabprevious", 4, ExTabVerb::previous},
                                              {"tabNext", 4, ExTabVerb::previous},
                                              {"tabnew", 6, ExTabVerb::open},
                                              {"tabclose", 4, ExTabVerb::close},
                                              {"tabs", 4, ExTabVerb::list}}};

[[nodiscard]] std::optional<ExTabName> tab_name_of(std::string_view name) noexcept
{
    for (const ExTabName &row : tab_names)
    {
        if (name.size() >= row.shortest && row.name.starts_with(name))
        {
            return row;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool is_digit(char letter) noexcept
{
    return letter >= '0' && letter <= '9';
}

[[nodiscard]] bool is_letter(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
}

[[nodiscard]] std::size_t leading(std::string_view text, bool (*accepts)(char) noexcept) noexcept
{
    std::size_t length = 0;
    while (length < text.size() && accepts(text.at(length)))
    {
        ++length;
    }
    return length;
}

// `:tabnext` `:tabprevious` の引数。空か 10 進の数 1 つだけを受け、ほかの形（`+1` `-1` `$`・
// 数でない文字・大きすぎる数）は受けない（決定 4）。
[[nodiscard]] std::expected<std::optional<std::size_t>, ExEvaluationFailure>
tab_number(std::string_view argument)
{
    if (argument.empty())
    {
        return std::optional<std::size_t>{};
    }
    std::size_t number = 0;
    const char *const last = argument.data() + argument.size();
    const auto [end, error] = std::from_chars(argument.data(), last, number);
    if (error != std::errc{} || end != last)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return std::optional<std::size_t>{number};
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> tab_result(const ExTabName &row,
                                                                      std::string_view argument)
{
    const bool counted = row.verb == ExTabVerb::next || row.verb == ExTabVerb::previous;
    if (!counted && !argument.empty())
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    const auto number = tab_number(argument);
    if (!number)
    {
        return std::unexpected(number.error());
    }
    return ExResult{std::nullopt, std::nullopt, std::nullopt, DisplayText::parse(row.name).value(),
                    ExTabRequest{row.verb, number.value()}};
}

// タブの命令（決定 4）。名前が表に当たらなければ値なし（ほかの命令として扱う）。範囲の形
// （`:4tabnext`）と `!` と受けない引数は unsupported_argument。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
tab_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    const auto row = tab_name_of(text.substr(range, letters));
    if (!row.has_value())
    {
        return std::nullopt;
    }
    const std::string_view argument = trimmed(text.substr(range + letters));
    if (range > 0 || argument.starts_with('!'))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return tab_result(row.value(), argument);
}
constexpr std::array<ExPaletteName, 5> palette_names{{
    {"edit", 1, PaletteScope::files},
    {"buffer", 1, PaletteScope::tabs},
    {"ls", 2, PaletteScope::tabs},
    {"files", 5, PaletteScope::tabs},
    {"buffers", 7, PaletteScope::tabs},
}};

// !・範囲・+cmd を普通の名前として消費しない。引数の直接実行はせず、共通の一覧へ渡す。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
palette_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    const auto name = text.substr(range, letters);
    const auto row = std::ranges::find_if(
        palette_names, [name](const ExPaletteName &entry)
        { return name.size() >= entry.shortest && entry.name.starts_with(name); });
    if (row == palette_names.end())
    {
        return std::nullopt;
    }
    const auto suffix = text.substr(range + letters);
    const auto query = trimmed(suffix);
    if (range > 0 || (!suffix.empty() && suffix.front() != ' ') || query.starts_with('!') ||
        query.starts_with('+'))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return ExResult{std::nullopt, std::nullopt,
                    std::nullopt, DisplayText::parse(row->name).value(),
                    std::nullopt, ExPaletteRequest{row->scope, std::string(query)}};
}

// colorscheme と set のほか（Vim の強調の止め方とタブ・ファイルの命令）。どれにも当たらなければ
// unknown_command。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
remaining_command(std::string_view text, std::string_view name, std::string_view argument)
{
    // `:nohlsearch` と短縮形の `:noh` は次の検索まで強調を止めるだけで、設定は変えない。
    if ((name == "nohlsearch" || name == "noh") && argument.empty())
    {
        return highlight_result(VimSearchHighlight::suspended);
    }
    auto tab = tab_command(text);
    if (tab.has_value())
    {
        return std::move(tab).value();
    }
    auto palette = palette_command(text);
    if (palette.has_value())
    {
        return std::move(palette).value();
    }
    return std::unexpected(ExFailure::unknown_command);
}
} // namespace

std::expected<ExResult, ExEvaluationFailure> evaluate_ex(std::string_view text,
                                                         const EditorSettings &settings,
                                                         Appearance system_appearance,
                                                         const ThemeCatalog &themes)
{
    if (text.size() > DisplayText::maximum_bytes)
    {
        return std::unexpected(ExFailure::too_long);
    }
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    if (text.find('|') != std::string_view::npos)
    {
        return std::unexpected(ExFailure::unknown_command);
    }
    text = trimmed(text);
    const auto space = text.find(' ');
    const auto name = text.substr(0, space);
    const auto argument =
        space == std::string_view::npos ? std::string_view{} : trimmed(text.substr(space + 1));
    if (name == "colorscheme")
    {
        return colorscheme(argument, settings, system_appearance, themes);
    }
    if (name == "set")
    {
        return set_option(argument, settings);
    }
    return remaining_command(text, name, argument);
}

std::vector<std::string> ex_command_candidates(const ThemeCatalog &themes)
{
    std::vector<std::string> candidates{
        "colorscheme",     "set fontsize=", "set guifont=",   "set incsearch",
        "set noincsearch", "set hlsearch",  "set nohlsearch", "tabs",
        "tabnew",          "tabnext",       "tabprevious",    "tabclose"};
    for (const ExPaletteName &name : palette_names)
    {
        candidates.emplace_back(name.name);
    }
    for (const auto &name : themes.names())
    {
        candidates.push_back("colorscheme " + name);
    }
    candidates.emplace_back("colorscheme system");
    return candidates;
}

std::vector<std::string> command_completions(std::string_view prefix, const ThemeCatalog &themes)
{
    auto candidates = ex_command_candidates(themes);
    std::erase_if(candidates,
                  [prefix](const std::string &candidate)
                  {
                      return !candidate.starts_with(prefix) ||
                             (candidate.starts_with("colorscheme ") &&
                              !prefix.starts_with("colorscheme "));
                  });
    return candidates;
}

DisplayText ex_failure_message(ExFailure failure)
{
    switch (failure)
    {
    case ExFailure::unknown_command:
        return DisplayText::parse("Unknown command").value();
    case ExFailure::unknown_option:
        return DisplayText::parse("Use set fontsize=, set guifont= or set (no)hlsearch").value();
    case ExFailure::unknown_theme:
        return DisplayText::parse("Unknown colorscheme (Tab lists names)").value();
    case ExFailure::invalid_font_size:
        return DisplayText::parse("Font size must be 8 to 40 pt").value();
    case ExFailure::invalid_font_family:
        return DisplayText::parse("Use set guifont=<name>:h<pt>").value();
    case ExFailure::invalid_text:
        return DisplayText::parse("Command must be a single line of UTF-8 text").value();
    case ExFailure::too_long:
        return DisplayText::parse("Command is limited to 256 bytes").value();
    case ExFailure::unsupported_argument:
        return DisplayText::parse("Not supported").value();
    }
    std::unreachable();
}
} // namespace nenenib::core
