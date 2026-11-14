#include "CommandChoice.hpp"

#include "Appearance.hpp"
#include "AsciiCase.hpp"
#include "CommandMatch.hpp"
#include "DisplayLanguageNames.hpp"
#include "EditorSettings.hpp"
#include "ExResult.hpp"
#include "KeyChord.hpp"
#include "OperationBindings.hpp"
#include "OperationReadings.hpp"
#include "OperationText.hpp"
#include "OperationTexts.hpp"
#include "SubsequenceScore.hpp"
#include "ThemeCatalog.hpp"
#include "UiTexts.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 照合の核は SubsequenceScore の 1 つ（ADR 0091 の決定 2・#912 の速道もその中）。
[[nodiscard]] std::optional<std::size_t> match_score(std::string_view query,
                                                     std::span<const std::string_view> command)
{
    return subsequence_score(query, command);
}

[[nodiscard]] std::optional<std::size_t> match_score(std::string_view query,
                                                     std::string_view command)
{
    const std::array<std::string_view, 1> parts{command};
    return match_score(query, std::span{parts});
}

// 短い名前も別テーマの部分列に負けない。利用者テーマも同じ候補で扱う（ADR 0117）。
// 入力が候補の先頭と一致する（ASCII の大文字小文字を無視・空白も字）候補も点 0 で、
// 表の順（Ex の Tab 補完と同じ・テーマは名前順）に並ぶ（#712）。空の入力は長さの点のまま。
[[nodiscard]] std::optional<std::size_t> command_score(std::string_view query,
                                                       std::string_view command)
{
    constexpr std::string_view prefix = "colorscheme ";
    if (command.starts_with(prefix) && command.size() - prefix.size() == query.size() &&
        ascii_prefix_ignoring_case(command.substr(prefix.size()), query))
    {
        return 0;
    }
    if (!query.empty() && ascii_prefix_ignoring_case(command, query))
    {
        return 0;
    }
    return match_score(query, command);
}

// 設定を変えるか表示するだけの答えか（#1527・`set` の族・`colorscheme`・`syntax`）。文書の保存と
// 終了・タブ・本文の置換・map・枠・面・印刷・行の移動・プレビューの要求を持つ答えは偽。答えの欄を
// 構造化束縛で全部名指しするので、ExResult に欄が増えるとここがコンパイルで落ち、どちらの側かを
// 決め直す（CPP-002 と同じ形・命令の名前の表は持たない）。途中で止まった複数の引数の行も偽。
[[nodiscard]] bool settings_only(const ExResult &result) noexcept
{
    [[maybe_unused]] const auto &[settings, highlight, incsearch, message, tab, palette, document,
                                  display, window, language, syntax, line, clipboard, root, lsp,
                                  map, format, preview, print, rechecks_window, relative_line,
                                  substitute, display_language, wrapscan, stopped] = result;
    return !tab.has_value() && !palette.has_value() && !document.has_value() &&
           !window.has_value() && !line.has_value() && !map.has_value() && !preview.has_value() &&
           !print.has_value() && !relative_line.has_value() && !substitute.has_value() &&
           !stopped.has_value();
}

// 面が打った行を候補として出すか（#1527）。Ex の入力行と同じ評価が受け、答えが設定の族だけのとき。
// 評価は既定の設定で行い、決めるのは候補に出すかと値の無い候補の動きだけ（確定は今の設定で
// evaluate_command が評価し直す）。答えの種類は設定の値に依らない。候補の動きを決めるのはこの 1
// 本で、形ごとの表を別に持たない。
[[nodiscard]] bool offered(std::string_view command)
{
    const auto result = evaluate_ex(command, default_editor_settings(), Appearance::dark);
    return result.has_value() && settings_only(result.value());
}

[[nodiscard]] CommandChoice choice_of(std::string command)
{
    // 打った値（`set guifont=…`）の候補は入力行の上限まで長くなるので、名前の欄は 1 行に切る
    // （実行する命令は command の全文・#1568）。
    auto label = clipped_display_text(command).value();
    // 値の無い候補（`set fontsize=` `set guifont=` `set scrolloff=` `set ft=`）のうち、空の値を
    // Ex が受けないものは、選ぶと入力欄をその文字に置き換えて面を開いたままにする（#1527）。空の
    // 値に意味がある `set clipboard=` `set root=` はそのまま実行する。
    const auto kind = command.ends_with('=') && !offered(command) ? CommandChoiceKind::fill
                                                                  : CommandChoiceKind::execute;
    return CommandChoice{std::move(label), std::move(command), kind, std::nullopt,
                         std::nullopt,     std::nullopt,       {}};
}

// 打った値を先頭に、Ex の補完と同じ字体の候補を後ろに（D58）。字体の一覧が無ければ打った値だけ。
[[nodiscard]] std::vector<CommandChoice> font_choices(std::string_view query,
                                                      const CommandLine &input)
{
    std::vector<CommandChoice> choices{choice_of(std::string(query))};
    for (auto &command : command_completions(query, input.catalog(), input.fonts()))
    {
        if (command != query)
        {
            choices.push_back(choice_of(std::move(command)));
        }
    }
    return choices;
}

// `set language=<名前>` の候補に添える説明（ADR 0123 の追補 2026-11-13 の決定 8）。言語の自称か
// 「Windows に合わせる」の表の文。ほかの候補は無し。
[[nodiscard]] std::optional<DisplayText> language_detail(std::string_view command,
                                                         DisplayLanguage language)
{
    constexpr std::string_view stem = "set language=";
    if (!command.starts_with(stem))
    {
        return std::nullopt;
    }
    const auto choice = display_language_choice_named(command.substr(stem.size()));
    if (!choice.has_value())
    {
        return std::nullopt;
    }
    return DisplayText::parse(display_language_choice_detail(choice.value(), language)).value();
}

// 出どころの印が見せる出どころ（ADR 0060 の決定 3）。印が増えたらここが落ちる（CPP-002）。
[[nodiscard]] PaletteScope scope_of(PaletteOrigin origin) noexcept
{
    switch (origin)
    {
    case PaletteOrigin::tab:
    case PaletteOrigin::bookmarked_tab:
        return PaletteScope::tabs;
    case PaletteOrigin::bookmark:
        return PaletteScope::bookmarks;
    case PaletteOrigin::history:
        return PaletteScope::history;
    case PaletteOrigin::folder:
        return PaletteScope::folder;
    case PaletteOrigin::document_format:
        // 保存の形の面は記号で開く出どころではない（ADR 0010 の追補 #1049）。その面の列はこの印の
        // 候補だけなので、空の入力（files）が全部を見せる。
        return PaletteScope::files;
    }
    std::unreachable();
}

// 出どころで絞るのはこの 1 つ（ADR 0060 の決定 3）。files は印のある候補の全部、commands と
// operations の候補は列に無い（Ex のコマンドは palette_choices、操作は operation_choices が作る）。
[[nodiscard]] bool in_scope(const std::optional<PaletteOrigin> &origin, PaletteScope scope) noexcept
{
    if (!origin.has_value())
    {
        return false;
    }
    switch (scope)
    {
    case PaletteScope::files:
        return true;
    case PaletteScope::bookmarks:
        return origin.value() == PaletteOrigin::bookmark ||
               origin.value() == PaletteOrigin::bookmarked_tab;
    case PaletteScope::tabs:
    case PaletteScope::history:
    case PaletteScope::folder:
    case PaletteScope::commands:
    case PaletteScope::operations:
        return scope_of(origin.value()) == scope;
    }
    std::unreachable();
}

// 名前だけに当たった候補より後ろに置く罰点（ADR 0060 の決定 4）。名前の点は名前の長さと飛ばし量の
// 和で、DisplayText の上限（256 バイト）の数倍にしかならないので、この大きさなら必ず後ろになる。
constexpr std::size_t location_only_penalty = std::numeric_limits<std::size_t>::max() / 2;

// 名前を先に照合し、不一致のときだけ場所・区切り・名前を借りて同じ点を付ける。
[[nodiscard]] std::optional<std::size_t> listed_score(std::string_view query,
                                                      const CommandChoice &choice)
{
    const auto named = match_score(query, choice.label.text());
    if (named.has_value())
    {
        return named;
    }
    if (!choice.detail.has_value())
    {
        return std::nullopt;
    }
    const std::array<std::string_view, 3> located{choice.detail.value().text(), "\\",
                                                  choice.label.text()};
    const auto score = match_score(query, std::span{located});
    if (!score.has_value())
    {
        return std::nullopt;
    }
    return location_only_penalty + score.value();
}

// 列の中の位置と点の組（first が位置・second が点）。
using ScoredPosition = std::pair<std::size_t, std::size_t>;

// 点の小さい順、同点は列の順。std::stable_sort は一時の領域を std::nothrow で取るので core の外へ
// シンボルが出る（ARC-003）。列の位置を添えた並べ替えで同じ順にする。
[[nodiscard]] std::vector<std::size_t> in_score_order(std::vector<ScoredPosition> scored)
{
    std::ranges::sort(scored,
                      [](const ScoredPosition &left, const ScoredPosition &right)
                      {
                          return left.second != right.second ? left.second < right.second
                                                             : left.first < right.first;
                      });
    std::vector<std::size_t> positions;
    positions.reserve(scored.size());
    for (const ScoredPosition &entry : scored)
    {
        positions.push_back(entry.first);
    }
    return positions;
}

// 全件と前の結果は対象位置だけが違い、出どころ・採点・同点の順はこの一つで決める（ADR 0091）。
template <std::ranges::sized_range Positions>
[[nodiscard]] std::vector<std::size_t> scored_positions(const std::vector<CommandChoice> &entries,
                                                        PaletteScope scope, std::string_view query,
                                                        const Positions &positions)
{
    std::vector<ScoredPosition> scored;
    scored.reserve(std::ranges::size(positions));
    for (const std::size_t position : positions)
    {
        const CommandChoice &entry = entries.at(position);
        if (!in_scope(entry.origin, scope))
        {
            continue;
        }
        const auto score =
            query.empty() ? std::optional<std::size_t>{0} : listed_score(query, entry);
        if (score.has_value())
        {
            scored.emplace_back(position, score.value());
        }
    }
    return in_score_order(std::move(scored));
}

// 操作の一覧の 1 行（ADR 0078 の決定 6）。鍵はモードで見せる鍵の表示名、無ければ空。
[[nodiscard]] CommandChoice operation_choice_of(const OperationText &text, std::string key,
                                                DisplayLanguage language)
{
    return CommandChoice{DisplayText::parse(ui_text(text.name, language)).value(),
                         {},
                         CommandChoiceKind::operate,
                         DisplayText::parse(ui_text(text.description, language)).value(),
                         std::nullopt,
                         text.operation,
                         std::move(key)};
}

// 2 つ目の照合の欄（ADR 0123 の追補 2026-11-13 の決定 6）。日本語は読み（ひらがな）、英語と
// 中国語は短い名前（無ければ空・何にも当たらない）。読みの拼音は持たない。
[[nodiscard]] std::string_view second_field(const OperationText &text,
                                            DisplayLanguage language) noexcept
{
    switch (language)
    {
    case DisplayLanguage::japanese:
        return operation_reading(text.operation);
    case DisplayLanguage::english:
    case DisplayLanguage::simplified_chinese:
        break;
    }
    return text.short_name.has_value() ? ui_text(text.short_name.value(), language)
                                       : std::string_view{};
}

// 名前・読み（日本語）か短い名前（英語・中国語）・鍵の表示名のうち一番良い点（ADR 0078 の決定
// 6）。どれにも当たらなければ無し。
[[nodiscard]] std::optional<std::size_t> operation_score(std::string_view query,
                                                         const OperationText &text,
                                                         std::string_view key,
                                                         DisplayLanguage language)
{
    std::optional<std::size_t> best;
    const std::array<std::string_view, 3> fields{ui_text(text.name, language),
                                                 second_field(text, language), key};
    for (const std::string_view field : fields)
    {
        const auto score = match_score(query, field);
        if (score.has_value() && (!best.has_value() || score.value() < best.value()))
        {
            best = score;
        }
    }
    return best;
}

[[nodiscard]] bool before(const CommandMatch &left, const CommandMatch &right)
{
    if (left.score != right.score)
    {
        return left.score < right.score;
    }
    // 同点は Ex の補完と同じ名前順（#696）。
    return ascii_folded_before(left.choice.command, right.choice.command);
}

// 点 0 の候補は表の順のまま先に並べ、ほかの候補だけを点と名前で並べる（#712）。
[[nodiscard]] std::vector<CommandChoice> ranked_choices(std::string_view query,
                                                        const ThemeCatalog &themes)
{
    std::vector<CommandChoice> choices;
    std::vector<CommandMatch> matches;
    for (auto &command : ex_command_candidates(themes))
    {
        const auto score = command_score(query, command);
        if (score.has_value() && score.value() == 0)
        {
            choices.push_back(choice_of(std::move(command)));
        }
        else if (score.has_value())
        {
            matches.push_back(CommandMatch{choice_of(std::move(command)), score.value()});
        }
    }
    std::sort(matches.begin(), matches.end(), before);
    for (auto &match : matches)
    {
        choices.push_back(std::move(match.choice));
    }
    return choices;
}

// 打った引数つきの行（`set so=2` `set ft=python` `setl ic` `se cb=unnamed`）を先頭の候補にするか
// （#1527）。Ex が受けて答えが設定の族だけの行（offered）で、同じ文字の候補が列に無いときだけ。
// ファイルを書く・閉じる・本文を変える・map・タブの行（`w! <file>` `%s/a b/x/g` `map x y`）と
// 受けない行（`set so=abc`）は候補にせず、面は今までどおり候補の列だけ（無ければ何も起きない・
// 受けない行でも当たる候補があればそちら・`set display=g` は `set display=grid`）。引数の無い行
// （`w` `tabn`）も今までどおり候補の列だけ。
[[nodiscard]] bool typed_command(std::string_view query, const std::vector<CommandChoice> &choices)
{
    const auto space = query.find(' ');
    if (space == std::string_view::npos || space == 0 || space + 1 == query.size() ||
        std::ranges::any_of(choices, [query](const CommandChoice &choice)
                            { return choice.command == query; }))
    {
        return false;
    }
    return offered(query);
}
} // namespace

bool completes_font_family(std::string_view input)
{
    if (input.starts_with(':'))
    {
        input.remove_prefix(1);
    }
    return input.starts_with(guifont_command);
}

std::vector<CommandChoice> palette_choices(const CommandLine &input)
{
    auto query = input.text();
    if (query.starts_with(':'))
    {
        query.remove_prefix(1);
    }
    // 値を直接打った場合も候補の実行文字列になり、同じEx評価が妥当性を決める。
    if (query.starts_with("set fontsize="))
    {
        return {choice_of(std::string(query))};
    }
    if (query.starts_with(guifont_command))
    {
        return font_choices(query, input);
    }
    auto choices = ranked_choices(query, input.catalog());
    if (choices.empty() && query.starts_with("colorscheme "))
    {
        return {choice_of(std::string(query))};
    }
    if (typed_command(query, choices))
    {
        choices.insert(choices.begin(), choice_of(std::string(query)));
    }
    for (auto &choice : choices)
    {
        choice.detail = language_detail(choice.command, input.language());
    }
    return choices;
}

std::vector<CommandChoice> operation_choices(std::string_view query, EditMode mode,
                                             DisplayLanguage language)
{
    std::vector<CommandChoice> available;
    std::vector<ScoredPosition> scored;
    for (const OperationText &text : operation_texts)
    {
        if (!operation_available(text.operation, mode))
        {
            continue;
        }
        const auto chord = shown_chord(text.operation, mode);
        auto key = chord.has_value() ? key_chord_label(chord.value()) : std::string{};
        const auto score = query.empty() ? std::optional<std::size_t>{0}
                                         : operation_score(query, text, key, language);
        if (score.has_value())
        {
            scored.emplace_back(available.size(), score.value());
            available.push_back(operation_choice_of(text, std::move(key), language));
        }
    }
    std::vector<CommandChoice> choices;
    choices.reserve(available.size());
    for (const std::size_t position : in_score_order(std::move(scored)))
    {
        choices.push_back(std::move(available.at(position)));
    }
    return choices;
}

namespace
{
// 出どころの印の補足の文の名前（ADR 0060 の決定 3）。印が増えたらここが落ちる（CPP-002）。
[[nodiscard]] UiMessage palette_origin_message(PaletteOrigin origin) noexcept
{
    switch (origin)
    {
    case PaletteOrigin::tab:
        return UiMessage::palette_origin_tab;
    case PaletteOrigin::bookmarked_tab:
        return UiMessage::palette_origin_bookmarked_tab;
    case PaletteOrigin::bookmark:
        return UiMessage::label_bookmarks;
    case PaletteOrigin::history:
        return UiMessage::label_history;
    case PaletteOrigin::folder:
        return UiMessage::palette_origin_folder;
    case PaletteOrigin::document_format:
        return UiMessage::palette_origin_document_format;
    }
    std::unreachable();
}
} // namespace

std::string_view palette_origin_label(PaletteOrigin origin, DisplayLanguage language) noexcept
{
    return ui_text(palette_origin_message(origin), language);
}

std::vector<std::size_t> listed_positions(const std::vector<CommandChoice> &entries,
                                          PaletteScope scope, std::string_view query)
{
    return scored_positions(entries, scope, query,
                            std::views::iota(std::size_t{0}, entries.size()));
}

std::vector<std::size_t> listed_positions(const std::vector<CommandChoice> &entries,
                                          PaletteScope scope, std::string_view query,
                                          std::span<const std::size_t> positions)
{
    return scored_positions(entries, scope, query, positions);
}
} // namespace nenenib::core
