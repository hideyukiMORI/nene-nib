#include "ExResult.hpp"

#include "ExDocumentName.hpp"
#include "Minimap.hpp"

#include "AsciiCase.hpp"
#include "BuiltinThemes.hpp"
#include "DisplayLanguageNames.hpp"
#include "DocumentFormat.hpp"
#include "DocumentFormatKind.hpp"
#include "ExLineRequest.hpp"
#include "ExMapCommand.hpp"
#include "ExPaletteName.hpp"
#include "ExRange.hpp"
#include "ExRelativeLineRequest.hpp"
#include "ExSubstituteCommand.hpp"
#include "ExTabName.hpp"
#include "ExTabRequest.hpp"
#include "ExTabVerb.hpp"
#include "ExWindowName.hpp"
#include "InputText.hpp"
#include "LanguageTable.hpp"
#include "Utf8.hpp"
#include "VimSearchHighlight.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <tuple>
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
                    clipped_display_text("fontsize=" + std::string(value)).value()};
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
                    clipped_display_text("guifont=" + std::string(value)).value()};
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

// `:set clipboard=` の値（ADR 0051 の追補（#848）の決定 A）。`unnamed` と `unnamedplus` の `,`
// 区切りの任意の組（1 つ以上）だけを受ける。Windows では `"*` と `"+` が同じ OS のクリップボード
// なので、どちらも同じ「無名と OS を同期する」になる。
[[nodiscard]] bool unnamed_clipboard_value(std::string_view value) noexcept
{
    while (true)
    {
        const std::size_t comma = value.find(',');
        const std::string_view item = value.substr(0, comma);
        if (item != "unnamed" && item != "unnamedplus")
        {
            return false;
        }
        if (comma == std::string_view::npos)
        {
            return true;
        }
        value.remove_prefix(comma + 1);
    }
}

// 無名と OS の同期も設定に載せない（決定 A）。報せは今の値を知る controller が
// `clipboard=unnamed` / `clipboard=` に書き換えるので、ここでは名前だけ。
[[nodiscard]] ExResult clipboard_result(ExClipboardRequest request)
{
    return ExResult{.settings = std::nullopt,
                    .highlight = std::nullopt,
                    .incsearch = std::nullopt,
                    .message = DisplayText::parse("clipboard").value(),
                    .clipboard = request};
}

// `clipboard=<値>` と `clipboard?`（決定 A）。`clipboard=` は外す。ほかの値は
// unsupported_argument。clipboard の option でなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
clipboard_option(std::string_view option)
{
    if (option == "clipboard?")
    {
        return clipboard_result(ExClipboardRequest::show);
    }
    constexpr std::string_view name = "clipboard=";
    if (!option.starts_with(name))
    {
        return std::nullopt;
    }
    const std::string_view value = option.substr(name.size());
    if (value.empty())
    {
        return clipboard_result(ExClipboardRequest::disable);
    }
    if (!unnamed_clipboard_value(value))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return clipboard_result(ExClipboardRequest::enable);
}

[[nodiscard]] ExResult guide_result(EditorSettings settings, GuideVisibility guide)
{
    settings.guide = guide;
    switch (guide)
    {
    case GuideVisibility::shown:
        return ExResult{settings, std::nullopt, std::nullopt,
                        DisplayText::parse("guide=on").value()};
    case GuideVisibility::hidden:
        return ExResult{settings, std::nullopt, std::nullopt,
                        DisplayText::parse("guide=off").value()};
    }
    std::unreachable();
}

[[nodiscard]] ExResult wrap_result(EditorSettings settings, LineWrapping wrapping)
{
    settings.wrapping = wrapping;
    switch (wrapping)
    {
    case LineWrapping::wrap:
        return ExResult{settings, std::nullopt, std::nullopt,
                        DisplayText::parse("wrap=on").value()};
    case LineWrapping::no_wrap:
        return ExResult{settings, std::nullopt, std::nullopt,
                        DisplayText::parse("wrap=off").value()};
    }
    std::unreachable();
}

// 表示形式は文書ごとの値で設定に載せないので settings は空のまま返す（ADR 0140 の決定 3）。
[[nodiscard]] ExResult display_result(DisplayForm display)
{
    switch (display)
    {
    case DisplayForm::text:
        return ExResult{.settings = std::nullopt,
                        .highlight = std::nullopt,
                        .incsearch = std::nullopt,
                        .message = DisplayText::parse("display=text").value(),
                        .display = display};
    case DisplayForm::grid:
        return ExResult{.settings = std::nullopt,
                        .highlight = std::nullopt,
                        .incsearch = std::nullopt,
                        .message = DisplayText::parse("display=grid").value(),
                        .display = display};
    }
    std::unreachable();
}

// 言語の option の名前（ADR 0154 の決定 4）。完全な形と Vim の短い形の 2 つで、補完の候補も同じ表。
constexpr std::array<std::string_view, 2> language_options{"filetype", "ft"};

// 言語は文書ごとの値で設定に載せないので settings は空のまま返す（ADR 0154 の決定 1・4）。
// 問いの文言は今の値を知る controller が `filetype=<名前>` に書き換えるので、ここでは名前だけ。
[[nodiscard]] ExResult language_result(std::optional<Language> language)
{
    const auto message = language.has_value()
                             ? "filetype=" + std::string(language_name(language.value()))
                             : std::string("filetype");
    return ExResult{.settings = std::nullopt,
                    .highlight = std::nullopt,
                    .incsearch = std::nullopt,
                    .message = DisplayText::parse(message).value(),
                    .language = ExLanguageRequest{language}};
}

// `filetype=<名前>` `ft=<名前>` と `filetype?` `ft?`。名前が言語の表に無ければ E474（Vim は任意の
// 文字列を受けるが、Nib の言語は閉じている・決定 4 と 8）。言語の option でなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
language_option(std::string_view option)
{
    for (const auto name : language_options)
    {
        if (!option.starts_with(name))
        {
            continue;
        }
        const auto rest = option.substr(name.size());
        if (rest == "?")
        {
            return language_result(std::nullopt);
        }
        if (!rest.starts_with('='))
        {
            continue;
        }
        const auto language = language_of_name(rest.substr(1));
        if (!language.has_value())
        {
            return std::unexpected(ExFailure::invalid_argument);
        }
        return language_result(language);
    }
    return std::nullopt;
}

// 保存の形の option の名前（ADR 0010 の追補 #1049 の決定 2 の (b)）。完全な形と Vim の短い形。
constexpr std::array<std::string_view, 2> encoding_options{"fileencoding", "fenc"};
constexpr std::array<std::string_view, 2> line_ending_options{"fileformat", "ff"};

// 保存の形は文書の値で設定に載せない。問いの文言は今の値を知る controller が `fileencoding=<名前>`
// `fileformat=<名前>` に書き換えるので、ここでは欄の名前だけ。
// 欄の名前（完全な形）。
[[nodiscard]] std::string_view format_option_name(DocumentFormatKind kind) noexcept
{
    switch (kind)
    {
    case DocumentFormatKind::encoding:
        return encoding_options.front();
    case DocumentFormatKind::line_ending:
        return line_ending_options.front();
    }
    std::unreachable();
}

[[nodiscard]] ExResult format_result(DocumentFormatKind kind, std::optional<DocumentFormat> chosen)
{
    return ExResult{.settings = std::nullopt,
                    .highlight = std::nullopt,
                    .incsearch = std::nullopt,
                    .message = DisplayText::parse(format_option_name(kind)).value(),
                    .format = ExFormatRequest{kind, chosen}};
}

[[nodiscard]] std::optional<DocumentFormat> format_named(DocumentFormatKind kind,
                                                         std::string_view value)
{
    switch (kind)
    {
    case DocumentFormatKind::encoding:
        return encoding_of_option_name(value).transform([](TextEncoding encoding)
                                                        { return DocumentFormat{encoding}; });
    case DocumentFormatKind::line_ending:
        return line_ending_of_option_name(value).transform([](LineEnding ending)
                                                           { return DocumentFormat{ending}; });
    }
    std::unreachable();
}

// `name=<値>` と `name?`。値が表に無ければ E474（Vim は任意の文字コードを受けるが、Nib の文字コード
// は閉じている）。欄の名前でなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
format_option_of(std::string_view option, DocumentFormatKind kind,
                 std::span<const std::string_view> names)
{
    for (const auto name : names)
    {
        if (!option.starts_with(name))
        {
            continue;
        }
        const auto rest = option.substr(name.size());
        if (rest == "?")
        {
            return format_result(kind, std::nullopt);
        }
        if (!rest.starts_with('='))
        {
            continue;
        }
        const auto chosen = format_named(kind, rest.substr(1));
        if (!chosen.has_value())
        {
            return std::unexpected(ExFailure::invalid_argument);
        }
        return format_result(kind, chosen);
    }
    return std::nullopt;
}

// 文書の値の option（言語・保存の形）。どれにも当たらなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
document_option(std::string_view option)
{
    auto language = language_option(option);
    if (language.has_value())
    {
        return language;
    }
    auto encoding = format_option_of(option, DocumentFormatKind::encoding, encoding_options);
    if (encoding.has_value())
    {
        return encoding;
    }
    return format_option_of(option, DocumentFormatKind::line_ending, line_ending_options);
}

// 設定に載せない名前の option（検索の強調・incsearch・表示形式・言語・クリップボード）。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> unsaved_option(std::string_view option)
{
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
    if (option == "display=text")
    {
        return display_result(DisplayForm::text);
    }
    if (option == "display=grid")
    {
        return display_result(DisplayForm::grid);
    }
    auto document = document_option(option);
    if (document.has_value())
    {
        return std::move(document).value();
    }
    auto clipboard = clipboard_option(option);
    if (clipboard.has_value())
    {
        return std::move(clipboard).value();
    }
    if (option == "noclipboard")
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return std::unexpected(ExFailure::unknown_option);
}

[[nodiscard]] std::expected<FilePath, ExFailure> document_path(std::string_view text);

// `explorer=on` `explorer=off`（ADR 0162 の決定 7）。`set wrap` と同じ形の文言。
[[nodiscard]] DisplayText explorer_message(bool shown)
{
    return DisplayText::parse(shown ? "explorer=on" : "explorer=off").value();
}

[[nodiscard]] ExResult explorer_result(EditorSettings settings, bool shown)
{
    settings.explorer = shown;
    return ExResult{std::move(settings), std::nullopt, std::nullopt, explorer_message(shown)};
}

// `root=<経路>`。無ければ `root=`。256 バイトを超える経路は末尾を code point 境界で落として「…」。
[[nodiscard]] DisplayText root_message(const std::optional<FilePath> &root)
{
    std::string text = "root=";
    if (root.has_value())
    {
        text += root.value().text();
    }
    auto parsed = clipped_display_text(text);
    if (!parsed)
    {
        return DisplayText::parse("root").value();
    }
    return std::move(parsed).value();
}

// `set root=<経路>` と `set root=`（ルートを決めていない状態へ戻す）。経路の読みは `:w <file>` と
// 同じ document_path の 1 本（展開しない・`\ ` だけ外す）。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> root_result(std::string_view value,
                                                                       EditorSettings settings)
{
    if (value.empty())
    {
        settings.root = std::nullopt;
    }
    else
    {
        auto path = document_path(value);
        if (!path)
        {
            return std::unexpected(path.error());
        }
        settings.root = std::move(path).value();
    }
    const DisplayText message = root_message(settings.root);
    return ExResult{std::move(settings), std::nullopt, std::nullopt, message};
}

// エクスプローラーの option（ADR 0162 の決定 7）。`explorer` `root` は保存する設定で
// settings に載る。問い（`?`）は今の値を文言にするだけで settings は空。
// `root=.` は controller が解く要求。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
explorer_option(std::string_view option, const EditorSettings &settings)
{
    if (option == "explorer" || option == "noexplorer")
    {
        return explorer_result(settings, option == "explorer");
    }
    if (option == "explorer?")
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        explorer_message(settings.explorer)};
    }
    if (option == "root?")
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt, root_message(settings.root)};
    }
    if (option == "root=.")
    {
        return ExResult{.settings = std::nullopt,
                        .highlight = std::nullopt,
                        .incsearch = std::nullopt,
                        .message = DisplayText::parse("root=.").value(),
                        .root = ExRootRequest::active_document_folder};
    }
    if (!option.starts_with("root="))
    {
        return std::nullopt;
    }
    return root_result(option.substr(5), settings);
}

// `lsp` `nolsp` `lsp?`（ADR 0160 の決定 7）。保存と起動・停止は今の値を知る controller が行い、
// 報せも controller が `lsp=on` / `lsp=off` に書き換えるので、ここでは名前だけ。lsp の option で
// なければ空。
[[nodiscard]] std::optional<ExResult> lsp_option(std::string_view option)
{
    std::optional<ExLspRequest> request;
    if (option == "lsp")
    {
        request = ExLspRequest::enable;
    }
    if (option == "nolsp")
    {
        request = ExLspRequest::disable;
    }
    if (option == "lsp?")
    {
        request = ExLspRequest::show;
    }
    if (!request.has_value())
    {
        return std::nullopt;
    }
    return ExResult{.settings = std::nullopt,
                    .highlight = std::nullopt,
                    .incsearch = std::nullopt,
                    .message = DisplayText::parse("lsp").value(),
                    .lsp = request};
}

// `language=<名前>` `language?` `language`（ADR 0123 の追補 2026-11-13 の決定 8）。名前は
// listed_display_language_choices の 4 つだけで、ほか（列の無い `ko` `es` `de` と表に無い名前）は
// E474。保存と観測と解決は今の値を知る controller が行い、報せも controller が書くので、ここでは
// 名前だけ。language の option でなければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
display_language_option(std::string_view option)
{
    constexpr std::string_view name = "language";
    if (!option.starts_with(name))
    {
        return std::nullopt;
    }
    const auto rest = option.substr(name.size());
    std::optional<DisplayLanguageChoice> chosen;
    if (rest.starts_with('='))
    {
        chosen = display_language_choice_named(rest.substr(1));
        if (!chosen.has_value() ||
            std::ranges::find(listed_display_language_choices, chosen.value()) ==
                listed_display_language_choices.end())
        {
            return std::unexpected(ExFailure::invalid_argument);
        }
    }
    else if (!rest.empty() && rest != "?")
    {
        return std::nullopt;
    }
    return ExResult{.settings = std::nullopt,
                    .highlight = std::nullopt,
                    .incsearch = std::nullopt,
                    .message = DisplayText::parse(name).value(),
                    .display_language = ExDisplayLanguageRequest{chosen}};
}

// 検索の大小の option の名前（#1041）。長い名前と Vim の短い名前（`ic` `scs`）。
constexpr std::array<std::string_view, 2> ignorecase_names{"ignorecase", "ic"};
constexpr std::array<std::string_view, 2> smartcase_names{"smartcase", "scs"};

[[nodiscard]] DisplayText switch_message(std::string_view name, bool on)
{
    return DisplayText::parse(std::string(name) + (on ? "=on" : "=off")).value();
}

// 名前 `name`・`no<name>`・`<name>?` のどれかなら、その真偽か問い（値が無い）。
[[nodiscard]] std::optional<std::optional<bool>>
switch_request(std::string_view option, std::span<const std::string_view> names)
{
    for (const std::string_view name : names)
    {
        if (option == name)
        {
            return std::optional<bool>{true};
        }
        if (option.starts_with("no") && option.substr(2) == name)
        {
            return std::optional<bool>{false};
        }
        if (option.ends_with('?') && option.substr(0, option.size() - 1) == name)
        {
            return std::optional<bool>{std::nullopt};
        }
    }
    return std::nullopt;
}

// `set ic` `set noic` `set ic?` と `scs` の同じ 3 つ（長い名前も・#1041）。保存する設定に載せ、
// 文言は `set wrap` と同じ `ignorecase=on` の形。問いは今の値を文言にするだけで settings は空。
[[nodiscard]] std::optional<ExResult> case_option(std::string_view option,
                                                  const EditorSettings &settings)
{
    const auto ignorecase = switch_request(option, ignorecase_names);
    const auto smartcase = switch_request(option, smartcase_names);
    if (!ignorecase.has_value() && !smartcase.has_value())
    {
        return std::nullopt;
    }
    const bool ignoring = ignorecase.has_value();
    const std::optional<bool> wanted = ignoring ? ignorecase.value() : smartcase.value();
    const std::string_view name = ignoring ? ignorecase_names.front() : smartcase_names.front();
    if (!wanted.has_value())
    {
        const bool current = ignoring ? settings.ignorecase : settings.smartcase;
        return ExResult{std::nullopt, std::nullopt, std::nullopt, switch_message(name, current)};
    }
    EditorSettings changed = settings;
    if (ignoring)
    {
        changed.ignorecase = wanted.value();
    }
    else
    {
        changed.smartcase = wanted.value();
    }
    return ExResult{std::move(changed), std::nullopt, std::nullopt,
                    switch_message(name, wanted.value())};
}

// 行番号の相対表示の名前（#1041）。`set rnu` `set nornu` `set rnu?` と長い名前。文言は
// `relativenumber=on` の形。
constexpr std::array<std::string_view, 2> relativenumber_names{"relativenumber", "rnu"};

[[nodiscard]] std::optional<ExResult> relativenumber_option(std::string_view option,
                                                            const EditorSettings &settings)
{
    const auto request = switch_request(option, relativenumber_names);
    if (!request.has_value())
    {
        return std::nullopt;
    }
    const std::string_view name = relativenumber_names.front();
    if (!request.value().has_value())
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        switch_message(name, settings.relativenumber)};
    }
    const bool wanted = request.value().value();
    EditorSettings changed = settings;
    changed.relativenumber = wanted;
    return ExResult{std::move(changed), std::nullopt, std::nullopt, switch_message(name, wanted)};
}

// 追従の余白の名前（#1041）。`set so=N` `set so?` `set so`（数の option の名前だけは問い・
// Vim と同じ）と長い名前。値は core の scrolloff_of の 1 本で読み、範囲の外は E474 と同じ失敗。
constexpr std::array<std::string_view, 2> scrolloff_names{"scrolloff", "so"};

[[nodiscard]] DisplayText scrolloff_message(std::size_t value)
{
    return DisplayText::parse(std::string(scrolloff_names.front()) + "=" + std::to_string(value))
        .value();
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
scrolloff_result(std::string_view value, const EditorSettings &settings)
{
    const auto parsed = scrolloff_of(value);
    if (!parsed.has_value())
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    EditorSettings changed = settings;
    changed.scrolloff = parsed.value();
    return ExResult{std::move(changed), std::nullopt, std::nullopt,
                    scrolloff_message(parsed.value())};
}

[[nodiscard]] bool asks_for(std::string_view option, std::string_view name) noexcept
{
    return option == name || (option.ends_with('?') && option.substr(0, option.size() - 1) == name);
}

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
scrolloff_option(std::string_view option, const EditorSettings &settings)
{
    for (const std::string_view name : scrolloff_names)
    {
        if (asks_for(option, name))
        {
            return ExResult{std::nullopt, std::nullopt, std::nullopt,
                            scrolloff_message(settings.scrolloff)};
        }
        if (option.starts_with(name) && option.substr(name.size()).starts_with('='))
        {
            return scrolloff_result(option.substr(name.size() + 1), settings);
        }
    }
    return std::nullopt;
}

// ミニマップ（ADR 0171 の決定 1）。`set minimap` `set nominimap` `set minimap?`。設定 v4 の
// features の名前を足す・外すだけで、文言は `minimap=on` の形（Vim
// に無い名前なので短い名前は無い）。
constexpr std::array<std::string_view, 1> minimap_names{minimap_feature};

[[nodiscard]] std::optional<ExResult> minimap_option(std::string_view option,
                                                     const EditorSettings &settings)
{
    const auto request = switch_request(option, minimap_names);
    if (!request.has_value())
    {
        return std::nullopt;
    }
    if (!request.value().has_value())
    {
        return ExResult{std::nullopt, std::nullopt, std::nullopt,
                        switch_message(minimap_feature, minimap_shown(settings))};
    }
    const bool wanted = request.value().value();
    return ExResult{minimap_set(settings, wanted), std::nullopt, std::nullopt,
                    switch_message(minimap_feature, wanted)};
}

// 画面の見え方の option（行番号の相対表示と追従の余白・#1041、ミニマップ・ADR 0171）。どれにも
// 当たらなければ無し。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
view_option(std::string_view option, const EditorSettings &settings)
{
    auto relative = relativenumber_option(option, settings);
    if (relative.has_value())
    {
        return std::expected<ExResult, ExEvaluationFailure>{std::move(relative).value()};
    }
    auto minimap = minimap_option(option, settings);
    if (minimap.has_value())
    {
        return std::expected<ExResult, ExEvaluationFailure>{std::move(minimap).value()};
    }
    return scrolloff_option(option, settings);
}

// Vim が値を置いた後に窓の先頭を決め直す option の名前（ADR 0019 の #1524 の追補）。Nib が
// `:set` で受ける option のうち、Vim 9.1（v9.1.0004 の optiondefs.h）で `P_RWIN`（`wrap`
// `relativenumber`）か `P_RALL`（`hlsearch` `scrolloff`）を持つものだけ。`check_redraw` が
// `changed_window_setting` を呼ぶ旗はこの 2 つと `P_RBUF` で、Nib の受ける option に `P_RBUF` は
// 無い。ほかの option（`ignorecase` `smartcase` `incsearch` `clipboard` `filetype` `fileformat`、
// 端末の Vim では `check_redraw` を通らない `guifont`）と Vim に無い Nib だけの option は載せない
// （Vim 9.1 の実測・`D:/NeNeNib/scripts/20261112/1524/probe.py`）。option ごとに分岐を書かない。
constexpr std::array<std::string_view, 6> window_setting_names{"wrap",     "relativenumber", "rnu",
                                                               "hlsearch", "scrolloff",      "so"};

// `name` `no<name>` `<name>=<値>` のどれか（問いの `?` と値の無い数の名前は呼び手が除く）。
[[nodiscard]] bool names_window_setting(std::string_view option) noexcept
{
    const std::string_view name = option.substr(0, option.find('='));
    return std::ranges::any_of(
        window_setting_names, [name](std::string_view row)
        { return name == row || (name.starts_with("no") && name.substr(2) == row); });
}

// 値を置いた `:set` だけが決め直す。問い（`set rnu?` `set so`）は設定も強調も変えない答えなので、
// 値を置いたかは答えの欄で見る（同じ値を置き直す `set nornu` も設定を載せるので真）。
[[nodiscard]] ExResult with_window_recheck(ExResult result, std::string_view option)
{
    result.rechecks_window = (result.settings.has_value() || result.highlight.has_value()) &&
                             names_window_setting(option);
    return result;
}

// 真偽の option の今の値を読む関数（設定の正本の欄か、設定に載せない Vim の状態から）。
using SwitchReader = bool (*)(const EditorSettings &, const ExWindowState &) noexcept;
// 表の 1 行: 長い名前・Vim の短い名前（無ければ空）・今の値。
using SwitchOption = std::tuple<std::string_view, std::string_view, SwitchReader>;

// `:set` の真偽の option（長い名前と Vim の短い名前・#1525）。`{name}!` `no{name}!` `inv{name}` の
// 反転と `{name}&` の既定値はこの表の 1 か所から引く（名前と値を同じ行に置く・レビュー L3）。
// hlsearch と incsearch の値は設定に無い Vim の状態（ExWindowState）から読む（レビュー L4）。
constexpr std::array<SwitchOption, 10> switch_options{{
    {"wrap", "", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.wrapping == LineWrapping::wrap; }},
    {"guide", "", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.guide == GuideVisibility::shown; }},
    {"ignorecase", "ic", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.ignorecase; }},
    {"smartcase", "scs", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.smartcase; }},
    {"relativenumber", "rnu", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.relativenumber; }},
    {minimap_feature, "", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return minimap_shown(settings); }},
    {"explorer", "", [](const EditorSettings &settings, const ExWindowState &) noexcept
     { return settings.explorer; }},
    {"lsp", "",
     [](const EditorSettings &settings, const ExWindowState &) noexcept { return settings.lsp; }},
    // `:noh` の後（suspended）も 'hlsearch' は立っている（Vim の no_hlsearch は別の旗）。
    {"hlsearch", "hls", [](const EditorSettings &, const ExWindowState &window) noexcept
     { return window.highlight != VimSearchHighlight::off; }},
    {"incsearch", "is",
     [](const EditorSettings &, const ExWindowState &window) noexcept { return window.incsearch; }},
}};

// 表の真偽の option の行。表に無ければ空。
[[nodiscard]] std::optional<SwitchOption> switch_named(std::string_view name) noexcept
{
    for (const auto &option : switch_options)
    {
        const auto &[long_name, short_name, reader] = option;
        if (name == long_name || (!short_name.empty() && name == short_name))
        {
            return option;
        }
    }
    return std::nullopt;
}

// 表の行の長い名前の `name` か `noname`（今の読みの形）。
[[nodiscard]] std::string switch_text(const SwitchOption &option, bool on)
{
    const std::string name(std::get<0>(option));
    return on ? name : "no" + name;
}

// 表の行の今の値。
[[nodiscard]] bool switch_value(const SwitchOption &option, const EditorSettings &settings,
                                const ExWindowState &window) noexcept
{
    return std::get<2>(option)(settings, window);
}

// `{name}!` `no{name}!` `inv{name}` を `name` か `noname` へ（Vim 9.1 の実測: 真偽の option は
// 反転する。数の option の `!` は E488、`inv` は E474、`invno{name}` は E518 で、どれも表に当たら
// ないので後の読みが unknown_option で断る）。
[[nodiscard]] std::optional<std::string>
toggled_text(std::string_view option, const EditorSettings &settings, const ExWindowState &window)
{
    std::string_view name;
    if (option.ends_with('!'))
    {
        name = option.substr(0, option.size() - 1);
        if (!switch_named(name).has_value() && name.starts_with("no"))
        {
            name.remove_prefix(2);
        }
    }
    else if (option.starts_with("inv"))
    {
        name = option.substr(3);
    }
    const auto found = switch_named(name);
    if (!found.has_value())
    {
        return std::nullopt;
    }
    return switch_text(found.value(), !switch_value(found.value(), settings, window));
}

// 既定値へ戻す接尾辞（Vim 9.1 の実測: `&` `&vim` `&vi` はどれも既定値へ戻す）。
constexpr std::array<std::string_view, 3> default_suffixes{"&vim", "&vi", "&"};

// 字の大きさの 10 進（小数 1 桁まで・FontSize::parse が読める形）。core は libm を呼ばないので
// 0.1 pt の整数で丸める。
[[nodiscard]] std::string points_text(FontSize size)
{
    const auto tenths = static_cast<std::int64_t>((size.points() * 10.0F) + 0.5F);
    std::string text = std::to_string(tenths / 10);
    if (tenths % 10 != 0)
    {
        text += "." + std::to_string(tenths % 10);
    }
    return text;
}

// `{name}&`（`&vim` `&vi`）を既定値の書き方へ（#1525）。既定値は保存する設定の正本
// default_editor_settings と、Vim の状態の既定（ExWindowState の初期値・クリップボード）。文書の
// 値（filetype・fileencoding・fileformat・display）は受けない（後の読みが unknown_option で断る）。
// `language&` は既定の system へ（ADR 0123 の追補 2026-11-13 の決定 8・controller が OS を 1
// 回観測する）。
[[nodiscard]] std::optional<std::string> reset_text(std::string_view option)
{
    const auto suffix = std::ranges::find_if(default_suffixes, [option](std::string_view value)
                                             { return option.ends_with(value); });
    if (suffix == default_suffixes.end())
    {
        return std::nullopt;
    }
    const std::string_view name = option.substr(0, option.size() - suffix->size());
    const EditorSettings defaults = default_editor_settings();
    const auto switched = switch_named(name);
    if (switched.has_value())
    {
        return switch_text(switched.value(), switch_value(switched.value(), defaults,
                                                          ExWindowState{Appearance::dark}));
    }
    if (std::ranges::find(scrolloff_names, name) != scrolloff_names.end())
    {
        return std::string(scrolloff_names.front()) + "=" + std::to_string(defaults.scrolloff);
    }
    if (name == "clipboard")
    {
        return std::string("clipboard=");
    }
    if (name == "root" && !defaults.root.has_value())
    {
        return std::string("root=");
    }
    if (name == "fontsize")
    {
        return "fontsize=" + points_text(defaults.font_size);
    }
    if (name == "language")
    {
        return "language=" + std::string(display_language_choice_name(defaults.ui_language));
    }
    if (name == "guifont")
    {
        return "guifont=" + std::string(defaults.font_family.text()) + ":h" +
               points_text(defaults.font_size);
    }
    return std::nullopt;
}

// 保存する設定のうち、置いて答えるのが controller の option（`lsp`・`language`）。どれにも当たら
// なければ空。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
answered_option(std::string_view option)
{
    auto lsp = lsp_option(option);
    if (lsp.has_value())
    {
        return std::move(lsp).value();
    }
    return display_language_option(option);
}

[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
read_set_option(std::string_view option, const EditorSettings &settings)
{
    auto view = view_option(option, settings);
    if (view.has_value())
    {
        return std::move(view).value();
    }
    auto letter_case = case_option(option, settings);
    if (letter_case.has_value())
    {
        return std::move(letter_case).value();
    }
    if (option == "wrap")
    {
        return wrap_result(settings, LineWrapping::wrap);
    }
    if (option == "nowrap")
    {
        return wrap_result(settings, LineWrapping::no_wrap);
    }
    if (option == "guide")
    {
        return guide_result(settings, GuideVisibility::shown);
    }
    if (option == "noguide")
    {
        return guide_result(settings, GuideVisibility::hidden);
    }
    if (option.starts_with("fontsize="))
    {
        return set_font_size(option.substr(9), settings);
    }
    if (option.starts_with("guifont="))
    {
        return set_gui_font(option.substr(8), settings);
    }
    auto explorer = explorer_option(option, settings);
    if (explorer.has_value())
    {
        return std::move(explorer).value();
    }
    auto answered = answered_option(option);
    if (answered.has_value())
    {
        return std::move(answered).value();
    }
    return unsaved_option(option);
}

// 反転と既定値の書き方を今の読みの形へ 1 段だけ写してから読む（#1525）。写した形は `&` `!`
// `inv` を持たない。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
set_option(std::string_view option, const EditorSettings &settings, const ExWindowState &window)
{
    const auto rewritten =
        toggled_text(option, settings, window).or_else([option] { return reset_text(option); });
    // 窓の決め直し（#1524）は書き換えた後の形で見る（`so&` `hls!` `invwrap` も値を置く・Vim の
    // do_set は反転と既定値でも check_redraw を通る）。
    const std::string_view read =
        rewritten.has_value() ? std::string_view{rewritten.value()} : option;
    return read_set_option(read, settings)
        .transform([read](ExResult result)
                   { return with_window_recheck(std::move(result), read); });
}

// 命令の名前の省略は Vim 9.1 のとおり「最短の形から完全な形までの前方一致」の 1 つ。タブ・一覧・
// 保存と終了の表と `:nohlsearch` が同じ判定を使う（Issue #501）。
[[nodiscard]] bool abbreviates(std::string_view input, std::string_view name,
                               std::size_t shortest) noexcept
{
    return input.size() >= shortest && name.starts_with(input);
}

// `:noh[lsearch]`（最短は `noh`・`:no` は Vim では noremap）。
constexpr std::string_view nohlsearch_name = "nohlsearch";
constexpr std::size_t nohlsearch_shortest = 3;

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
        if (abbreviates(name, row.name, row.shortest))
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
// 窓の命令の名前の表（ADR 0148 の決定 5）。`:on[ly]` は Vim と同じ最短の形。
constexpr std::array<ExWindowName, 1> window_names{{{"only", 2, ExWindowCommand::only}}};

// 窓の命令。名前が表に当たらなければ値なし（ほかの命令として扱う）。範囲の形（`:2only`）と
// 引数（`!` を含む）は unsupported_argument。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
window_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    const auto name = text.substr(range, letters);
    const auto row = std::ranges::find_if(
        window_names, [name](const ExWindowName &entry)
        { return name.size() >= entry.shortest && entry.name.starts_with(name); });
    if (row == window_names.end())
    {
        return std::nullopt;
    }
    if (range > 0 || !trimmed(text.substr(range + letters)).empty())
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt,
                    DisplayText::parse(row->name).value()};
    result.window = row->command;
    return result;
}

// `:ha[rdcopy]`（ADR 0177 の決定 1・最短は Vim と同じ `ha`）。アクティブな文書の印刷の要求で、
// 設定は変えない。引数（`> file.ps` の PostScript など）・`!`・範囲の形は unsupported_argument
// （決定 9 の後続）。名前が当たらなければ値なし（ほかの命令として扱う）。
constexpr std::string_view hardcopy_name = "hardcopy";
constexpr std::size_t hardcopy_shortest = 2;

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
print_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    if (!abbreviates(text.substr(range, letters), hardcopy_name, hardcopy_shortest))
    {
        return std::nullopt;
    }
    if (range > 0 || !trimmed(text.substr(range + letters)).empty())
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt,
                    DisplayText::parse(hardcopy_name).value()};
    result.print = ExPrintRequest::active_document;
    return result;
}

// `:sy[ntax]`（最短は Vim と同じ `sy`）と引数 `on` `off` `?`・引数なし（ADR 0156 の追補・#829）。
// 構文の色は文書の値で設定に載せないので settings は空。問いの文言は今の値を知る controller が
// `syntax=on` / `syntax=off` に書き換える。ほかの引数（`enable` など）は E474、範囲の形は
// unsupported_argument。名前が当たらなければ値なし（ほかの命令として扱う）。
constexpr std::string_view syntax_name = "syntax";
constexpr std::size_t syntax_shortest = 2;

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
syntax_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    if (!abbreviates(text.substr(range, letters), syntax_name, syntax_shortest))
    {
        return std::nullopt;
    }
    if (range > 0)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    const std::string_view argument = trimmed(text.substr(range + letters));
    std::optional<bool> enabled;
    if (argument == "on")
    {
        enabled = true;
    }
    else if (argument == "off")
    {
        enabled = false;
    }
    else if (!argument.empty() && argument != "?")
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt,
                    DisplayText::parse(syntax_name).value()};
    result.syntax = ExSyntaxRequest{enabled};
    return result;
}

// `:preview` と引数 `full` `side` `off` `?`・引数なし（ADR 0155 の改訂 16・#1052）。Vim の
// `:prev[ious]`
// と重ならないよう省略は認めない（完全な名前だけ）。見せ方は窓の値で設定に載せないので settings
// は空。問いの文言は今の見せ方を知る controller が `preview=…` に書き換える。ほかの引数は
// E474、範囲の形は unsupported_argument。名前が当たらなければ値なし（ほかの命令として扱う）。
constexpr std::string_view preview_name = "preview";

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
preview_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    if (text.substr(range, letters) != preview_name)
    {
        return std::nullopt;
    }
    if (range > 0)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    const std::string_view argument = trimmed(text.substr(range + letters));
    ExPreviewRequest request = ExPreviewRequest::show;
    if (argument == "full")
    {
        request = ExPreviewRequest::full;
    }
    else if (argument == "side")
    {
        request = ExPreviewRequest::side;
    }
    else if (argument == "off")
    {
        request = ExPreviewRequest::off;
    }
    else if (!argument.empty() && argument != "?")
    {
        return std::unexpected(ExFailure::invalid_argument);
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt,
                    DisplayText::parse(preview_name).value()};
    result.preview = request;
    return result;
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
    const auto row =
        std::ranges::find_if(palette_names, [name](const ExPaletteName &entry)
                             { return abbreviates(name, entry.name, entry.shortest); });
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

constexpr std::array<ExDocumentName, 6> document_names{{
    {"write", 1, ExDocumentVerb::write, ExDocumentVerb::write},
    {"quit", 1, ExDocumentVerb::quit, ExDocumentVerb::quit_force},
    {"wq", 2, ExDocumentVerb::write_quit, ExDocumentVerb::write_quit},
    {"xit", 1, ExDocumentVerb::update_quit, ExDocumentVerb::update_quit},
    {"exit", 3, ExDocumentVerb::update_quit, ExDocumentVerb::update_quit},
    {"saveas", 3, ExDocumentVerb::save_as, ExDocumentVerb::save_as},
}};

[[nodiscard]] bool unsupported_filename_character(char value) noexcept
{
    return value == '%' || value == '#' || value == '$' || value == '~' || value == '*' ||
           value == '?' || value == '`' || value == '"' || value == '<' || value == '>' ||
           value == '!';
}

[[nodiscard]] std::expected<FilePath, ExFailure> document_path(std::string_view text)
{
    // ファイル名展開はまだしない。Windows の区切りは保ち、空白の escape だけ外す（ADR 0067）。
    if (text.starts_with('+') || std::ranges::any_of(text, unsupported_filename_character))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    std::string path;
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        if (text[index] == '\\' && index + 1 < text.size() && text[index + 1] == ' ')
        {
            ++index;
        }
        path += text[index];
    }
    auto parsed = FilePath::parse(path);
    if (!parsed)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return std::move(parsed).value();
}

// `!` と引数の両方を受けるのは `:w! <file>` と `:saveas! <file>` だけで、既存のファイルを置き換える
// 印 overwrite を立てる（Issue #738）。`:wq! file` `:x! file` は黙って `!` を捨てず
// unsupported_argument のまま（Issue #501）。
[[nodiscard]] bool overwrites(ExDocumentVerb verb) noexcept
{
    switch (verb)
    {
    case ExDocumentVerb::write:
    case ExDocumentVerb::save_as:
        return true;
    case ExDocumentVerb::quit:
    case ExDocumentVerb::quit_force:
    case ExDocumentVerb::write_quit:
    case ExDocumentVerb::update_quit:
        return false;
    }
    std::unreachable();
}

[[nodiscard]] std::expected<ExDocumentRequest, ExFailure>
document_request(ExDocumentVerb verb, std::string_view suffix, bool forced)
{
    const auto argument = trimmed(suffix);
    if (argument.empty())
    {
        if (verb == ExDocumentVerb::save_as)
        {
            return std::unexpected(ExFailure::unsupported_argument);
        }
        return ExDocumentRequest{verb, std::nullopt, false};
    }
    if ((forced && !overwrites(verb)) || !suffix.starts_with(' ') || verb == ExDocumentVerb::quit)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    auto path = document_path(argument);
    if (!path)
    {
        return std::unexpected(path.error());
    }
    return ExDocumentRequest{verb, std::move(path).value(), forced};
}

[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
document_command(std::string_view text)
{
    const std::size_t range = leading(text, is_digit);
    const std::size_t letters = leading(text.substr(range), is_letter);
    const auto name = text.substr(range, letters);
    const auto row =
        std::ranges::find_if(document_names, [name](const ExDocumentName &entry)
                             { return abbreviates(name, entry.name, entry.shortest); });
    if (row == document_names.end())
    {
        return std::nullopt;
    }
    auto suffix = text.substr(range + letters);
    const bool forced = suffix.starts_with('!');
    if (range > 0 || (forced && !row->forced.has_value()))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    const auto verb = forced ? row->forced.value_or(row->verb) : row->verb;
    auto request = document_request(verb, forced ? suffix.substr(1) : suffix, forced);
    if (!request)
    {
        return std::unexpected(request.error());
    }
    return ExResult{std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    DisplayText::parse(row->name).value(),
                    std::nullopt,
                    std::nullopt,
                    std::move(request).value()};
}

// 行番号だけの命令（ADR 0022 の追補・Issue #847）。全体が 10 進の数字だけか `$` だけのときに
// 当たり、ほかの形（`:3,5` `:+3` `:'a` `:3d`）は値なし（今までどおりほかの命令として扱う）。
// 数字は先頭の 0 を許し（`:007`）、size_t に収まらなければ最大に飽和する（Vim も最後の行）。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
line_command(std::string_view text)
{
    const bool last = text == "$";
    if (!last && (text.empty() || leading(text, is_digit) != text.size()))
    {
        return std::nullopt;
    }
    std::optional<std::size_t> line;
    if (!last)
    {
        std::size_t number = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
        line = parsed.ec == std::errc{} ? number : std::numeric_limits<std::size_t>::max();
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt, clipped_display_text(text).value()};
    result.line = ExLineRequest{line};
    return result;
}

// `:nohlsearch` と短縮形の `:noh` `:nohl` … は次の検索まで強調を止めるだけで、設定は変えない。
// 引数があれば値なし（ほかの命令として扱う）。名前と引数の切り出しは evaluate_ex と同じ最初の空白。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
nohlsearch_command(std::string_view text)
{
    const auto space = text.find(' ');
    const auto name = text.substr(0, space);
    const auto argument =
        space == std::string_view::npos ? std::string_view{} : trimmed(text.substr(space + 1));
    if (!abbreviates(name, nohlsearch_name, nohlsearch_shortest) || !argument.empty())
    {
        return std::nullopt;
    }
    return highlight_result(VimSearchHighlight::suspended);
}

// colorscheme と set のほかの命令の解析関数の表（Issue #1199・CPP-012・ADR 0022 の追補）。上から
// 見て最初に値を返した行が勝つ。`line_command` が先頭（数字だけ・`$` だけの行は名前を持たない）、
// `print_command`（`:ha[rdcopy]`）は `document_command` の前。行を足すときは解析関数を 1 つ書いて
// ここに 1 行足す（無名名前空間の関数を書き忘れると -Wunused-function で落ちる）。
using ExCommandParser =
    std::optional<std::expected<ExResult, ExEvaluationFailure>> (*)(std::string_view);
constexpr std::array<ExCommandParser, 10> command_parsers{
    line_command, nohlsearch_command, map_command,     syntax_command, preview_command,
    tab_command,  window_command,     palette_command, print_command,  document_command};

// 表のどの行にも当たらなければ unknown_command。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> remaining_command(std::string_view text)
{
    for (const ExCommandParser parser : command_parsers)
    {
        auto parsed = parser(text);
        if (parsed.has_value())
        {
            return std::move(parsed).value();
        }
    }
    return std::unexpected(ExFailure::unknown_command);
}

// `:s` の族（ADR 0168 の決定 1）。range は範囲の字面（無ければ今の行）。`:s` の族でなければ値なし。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
substitute_result(std::string_view text, std::optional<ExRange> range)
{
    auto parsed = substitute_command(text);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }
    if (!parsed.value())
    {
        return std::unexpected(parsed.value().error());
    }
    // 命令は入力行の上限（4096 バイト）まで長くなるので、知らせの 1 行に切る（置換は request の
    // 全文で流し、この文は表示にしか使わない・#1616）。evaluate_ex が制御文字と壊れた UTF-8 を
    // 先に断り、`:s` の族は空でないので、切った文は必ず受かる（不変条件・ARC-010）。
    ExResult result{std::nullopt, std::nullopt, std::nullopt, clipped_display_text(text).value()};
    result.substitute = std::move(parsed).value().value();
    result.substitute.value().range = std::move(range);
    return result;
}

// 範囲のない 1 命令（`colorscheme` `set` と表の命令）。範囲の後ろの命令もここを通す。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure>
unranged_command(std::string_view text, const EditorSettings &settings, const ExWindowState &window,
                 const ThemeCatalog &themes)
{
    if (auto substituted = substitute_result(text, std::nullopt))
    {
        return std::move(substituted).value();
    }
    const auto space = text.find(' ');
    const auto name = text.substr(0, space);
    const auto argument =
        space == std::string_view::npos ? std::string_view{} : trimmed(text.substr(space + 1));
    if (name == "colorscheme")
    {
        return colorscheme(argument, settings, window.system_appearance, themes);
    }
    if (name == "set")
    {
        return set_option(argument, settings, window);
    }
    return remaining_command(text);
}

// ------------------------------------------------ 範囲（ADR 0022 の追補 #1526）

// VISUAL の `:` が入力行の先に置く範囲（Vim の op_colon）。
constexpr std::string_view visual_range = "'<,'>";
// 回数の `N:` が置く範囲の頭（Vim の nv_colon の `.,.+(N-1)`）。
constexpr std::string_view relative_range = ".,.+";

// 行の先の範囲の長さ（範囲を読むのは parsed_ex_range の 1 本・ADR 0168 の決定 1）。
[[nodiscard]] std::size_t range_length(std::string_view text)
{
    return parsed_ex_range(text).second;
}

// `.`（0）と `.,.+N`（N）の N。ほかの形は値なし。大きすぎる数は size_t の最大に飽和する。
[[nodiscard]] std::optional<std::size_t> relative_lines(std::string_view text)
{
    if (text == ".")
    {
        return std::size_t{0};
    }
    if (!text.starts_with(relative_range))
    {
        return std::nullopt;
    }
    const auto digits = text.substr(relative_range.size());
    if (digits.empty() || leading(digits, is_digit) != digits.size())
    {
        return std::nullopt;
    }
    std::size_t below = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), below);
    return parsed.ec == std::errc{} ? below : std::numeric_limits<std::size_t>::max();
}

// 範囲だけの行。`'<,'>` は何もしない（Vim は VISUAL の `:` の確定でキャレットを選択の先の端に
// 残す・実測）。`.` と `.,.+N` はキャレットの行から N 行下へ。ほかの形はまだ受けない。
[[nodiscard]] std::expected<ExResult, ExEvaluationFailure> range_only(std::string_view text)
{
    const auto below = relative_lines(text);
    if (text != visual_range && !below.has_value())
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    ExResult result{std::nullopt, std::nullopt, std::nullopt, clipped_display_text(text).value()};
    result.relative_line = ExRelativeLineRequest{below};
    return result;
}

// Vim で範囲を取らない命令の答えか（`:set` 系・`:colorscheme`・`:noh`・`map` 系・`:syntax`）。
[[nodiscard]] bool takes_no_range(std::string_view name, const ExResult &result) noexcept
{
    return name == "set" || name == "colorscheme" || result.highlight.has_value() ||
           result.map.has_value() || result.syntax.has_value();
}

// 範囲つきの行（ADR 0022 の追補 #1526）。範囲が無いか数字だけ・`$` だけ（`:N`）なら値なし。
// 範囲の後ろの命令は範囲なしで評価し、失敗ならその失敗、範囲を取らない命令なら
// no_range_allowed、ほかは Nib がまだ範囲つきの形を持たないので unsupported_argument。
[[nodiscard]] std::optional<std::expected<ExResult, ExEvaluationFailure>>
ranged_command(std::string_view text, const EditorSettings &settings, const ExWindowState &window,
               const ThemeCatalog &themes)
{
    const std::size_t range = range_length(text);
    if (range == 0 || line_command(text).has_value())
    {
        return std::nullopt;
    }
    const auto command = trimmed(text.substr(range));
    if (command.empty())
    {
        return range_only(text);
    }
    if (auto substituted = substitute_result(command, parsed_ex_range(text).first))
    {
        return std::move(substituted).value();
    }
    const auto evaluated = unranged_command(command, settings, window, themes);
    if (!evaluated)
    {
        return std::unexpected(evaluated.error());
    }
    const auto name = command.substr(0, command.find(' '));
    return std::unexpected(takes_no_range(name, evaluated.value())
                               ? ExFailure::no_range_allowed
                               : ExFailure::unsupported_argument);
}
// 候補は命令の完全な形だけで、省略の形を並べない（`nohlsearch` は 1 つ・Issue #501）。
// 問いの形（`set clipboard?` `set explorer?` `set root?` `set lsp?`）は並べない（`?`
// は操作の一覧の印と重なる）。
constexpr std::array<std::string_view, 49> fixed_candidates{"colorscheme",
                                                            "set fontsize=",
                                                            guifont_command,
                                                            "set incsearch",
                                                            "set noincsearch",
                                                            "set hlsearch",
                                                            "set nohlsearch",
                                                            "set guide",
                                                            "set noguide",
                                                            "tabs",
                                                            "tabnew",
                                                            "tabnext",
                                                            "tabprevious",
                                                            "tabclose",
                                                            "set wrap",
                                                            "set nowrap",
                                                            "set display=grid",
                                                            "set display=text",
                                                            nohlsearch_name,
                                                            "only",
                                                            "set filetype=",
                                                            "set ft=",
                                                            "syntax on",
                                                            "syntax off",
                                                            "set clipboard=unnamed",
                                                            "set clipboard=",
                                                            "set explorer",
                                                            "set noexplorer",
                                                            "set root=",
                                                            "set root=.",
                                                            "set lsp",
                                                            "set nolsp",
                                                            "set ignorecase",
                                                            "set noignorecase",
                                                            "set smartcase",
                                                            "set nosmartcase",
                                                            "set relativenumber",
                                                            "set norelativenumber",
                                                            "set scrolloff=",
                                                            "set minimap",
                                                            "set nominimap",
                                                            "set language=ja",
                                                            "set language=en",
                                                            "set language=zh-hans",
                                                            "set language=system",
                                                            "preview full",
                                                            "preview side",
                                                            "preview off",
                                                            hardcopy_name};

[[nodiscard]] bool prefix_matches(std::optional<std::string_view> prefix, std::string_view stem,
                                  std::string_view suffix = {})
{
    if (!prefix.has_value())
    {
        return true;
    }
    const auto text = prefix.value();
    if (text.size() <= stem.size())
    {
        return stem.starts_with(text);
    }
    return text.starts_with(stem) && suffix.starts_with(text.substr(stem.size()));
}

// 引数の候補（テーマ名・字体名）の照合。命令名（stem）は区別し、引数は ASCII の大文字小文字を
// 無視する（#696・Vim 9.1 の colorscheme の補完と同じ）。
[[nodiscard]] bool argument_matches(std::optional<std::string_view> prefix, std::string_view stem,
                                    std::string_view argument)
{
    if (!prefix.has_value())
    {
        return true;
    }
    const auto text = prefix.value();
    if (text.size() <= stem.size())
    {
        return stem.starts_with(text);
    }
    return text.starts_with(stem) && ascii_prefix_ignoring_case(argument, text.substr(stem.size()));
}

void append_document_candidates(std::vector<std::string> &candidates,
                                std::optional<std::string_view> prefix)
{
    for (const ExDocumentName &name : document_names)
    {
        if (prefix_matches(prefix, name.name))
        {
            candidates.emplace_back(name.name);
        }
        // `!` で条件が変わる行（`quit!`）だけを候補にする。同じ条件の `write!` は並べない。
        if (name.forced.has_value() && name.forced.value() != name.verb &&
            prefix_matches(prefix, name.name, "!"))
        {
            candidates.emplace_back(std::string(name.name) + "!");
        }
    }
}

void append_theme_candidates(std::vector<std::string> &candidates, const ThemeCatalog &themes,
                             std::optional<std::string_view> prefix)
{
    if (prefix.has_value() && !prefix.value().starts_with("colorscheme "))
    {
        return;
    }
    for (const auto &name : themes.names())
    {
        if (argument_matches(prefix, "colorscheme ", name))
        {
            candidates.push_back("colorscheme " + name);
        }
    }
    if (argument_matches(prefix, "colorscheme ", "system"))
    {
        candidates.emplace_back("colorscheme system");
    }
}

// 字体の候補は字体の命令を打ち始めたときだけ（全件の一覧には出さない・D58）。
void append_font_candidates(std::vector<std::string> &candidates, const FontFamilies &fonts,
                            std::string_view prefix)
{
    if (!prefix.starts_with(guifont_command))
    {
        return;
    }
    for (const auto &name : fonts.names())
    {
        if (argument_matches(prefix, guifont_command, name))
        {
            candidates.push_back(std::string(guifont_command) + name);
        }
    }
}

// 言語の名前の候補は言語の命令の `=` まで打ったときだけ（全件の一覧には出さない・ADR 0154 の
// 決定 4・字体の候補と同じ形）。名前は language_name の表の順。
void append_language_names(std::vector<std::string> &candidates, std::string_view prefix,
                           const std::string &command)
{
    if (!prefix.starts_with(command))
    {
        return;
    }
    for (const auto name : all_language_names())
    {
        if (prefix_matches(prefix, command, name))
        {
            candidates.push_back(command + std::string(name));
        }
    }
}

void append_language_candidates(std::vector<std::string> &candidates, std::string_view prefix)
{
    for (const auto option : language_options)
    {
        append_language_names(candidates, prefix, "set " + std::string(option) + "=");
    }
}

[[nodiscard]] std::vector<std::string> matching_candidates(const ThemeCatalog &themes,
                                                           std::optional<std::string_view> prefix)
{
    std::vector<std::string> candidates;
    candidates.reserve(fixed_candidates.size());
    for (const auto name : fixed_candidates)
    {
        if (prefix_matches(prefix, name))
        {
            candidates.emplace_back(name);
        }
    }
    for (const ExPaletteName &name : palette_names)
    {
        if (prefix_matches(prefix, name.name))
        {
            candidates.emplace_back(name.name);
        }
    }
    for (const auto name : map_command_candidates())
    {
        if (prefix_matches(prefix, name))
        {
            candidates.emplace_back(name);
        }
    }
    append_document_candidates(candidates, prefix);
    append_theme_candidates(candidates, themes, prefix);
    return candidates;
}
} // namespace

std::expected<ExResult, ExEvaluationFailure> evaluate_ex(std::string_view text,
                                                         const EditorSettings &settings,
                                                         Appearance system_appearance,
                                                         const ThemeCatalog &themes)
{
    return evaluate_ex(text, settings, ExWindowState{system_appearance}, themes);
}

std::expected<ExResult, ExEvaluationFailure> evaluate_ex(std::string_view text,
                                                         const EditorSettings &settings,
                                                         const ExWindowState &window,
                                                         const ThemeCatalog &themes)
{
    // 入力行と同じ上限（#1568）。入力行はあふれた行を確定させないので、ここへ来る長い文は
    // 入力行を経ない呼び出し（試験・面の候補）だけ。
    if (text.size() > input_line_limit)
    {
        return std::unexpected(ExFailure::too_long);
    }
    if (!validate_utf8(text) || has_control_character(text))
    {
        return std::unexpected(ExFailure::invalid_text);
    }
    text = trimmed(text);
    // `|` は命令の区切りで Nib は続けて流さない。`:s` だけは区切りの間（パターンと置き換え）の `|`
    // を 文字として読む（Vim の ex_substitute・ADR 0168 の追補）。フラグの後ろの `|` は E488
    // の手前で断る。
    if (text.find('|') != std::string_view::npos &&
        !substitute_command(text.substr(range_length(text))).has_value())
    {
        return std::unexpected(ExFailure::unknown_command);
    }
    if (auto ranged = ranged_command(text, settings, window, themes))
    {
        return std::move(ranged).value();
    }
    return unranged_command(text, settings, window, themes);
}

std::vector<std::string> ex_command_candidates(const ThemeCatalog &themes)
{
    return matching_candidates(themes, std::nullopt);
}

std::vector<std::string> command_completions(std::string_view prefix, const ThemeCatalog &themes,
                                             const FontFamilies &fonts)
{
    auto candidates = matching_candidates(themes, prefix);
    append_font_candidates(candidates, fonts, prefix);
    append_language_candidates(candidates, prefix);
    return candidates;
}

DisplayText ex_failure_message(ExFailure failure)
{
    switch (failure)
    {
    case ExFailure::unknown_command:
        return DisplayText::parse("Unknown command").value();
    case ExFailure::unknown_option:
        return DisplayText::parse(
                   "Use set fontsize=, set guifont=, set (no)hlsearch or set (no)guide")
            .value();
    case ExFailure::unknown_theme:
        return DisplayText::parse("Unknown colorscheme (Tab lists names)").value();
    case ExFailure::invalid_font_size:
        return DisplayText::parse("Font size must be 8 to 40 pt").value();
    case ExFailure::invalid_font_family:
        return DisplayText::parse("Use set guifont=<name>:h<pt>").value();
    case ExFailure::invalid_text:
        return DisplayText::parse("Command must be a single line of UTF-8 text").value();
    case ExFailure::too_long:
        return DisplayText::parse("Command is limited to " + std::to_string(input_line_limit) +
                                  " bytes")
            .value();
    case ExFailure::unsupported_argument:
        return DisplayText::parse("Not supported").value();
    case ExFailure::invalid_argument:
        return DisplayText::parse("E474: Invalid argument").value();
    case ExFailure::no_range_allowed:
        return DisplayText::parse("E481: No range allowed").value();
    case ExFailure::letter_delimiter:
        return DisplayText::parse("E146: Regular expressions can't be delimited by letters")
            .value();
    case ExFailure::value_too_large:
        return DisplayText::parse("E1510: Value too large").value();
    case ExFailure::trailing_characters:
        return DisplayText::parse("E488: Trailing characters").value();
    }
    std::unreachable();
}
} // namespace nenenib::core
