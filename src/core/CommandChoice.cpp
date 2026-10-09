#include "CommandChoice.hpp"

#include "CommandMatch.hpp"
#include "ExResult.hpp"
#include "KeyChord.hpp"
#include "Offset.hpp"
#include "OperationBindings.hpp"
#include "OperationText.hpp"
#include "OperationTexts.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] char32_t lower_code_point(char32_t value) noexcept
{
    return value >= U'A' && value <= U'Z' ? value + (U'a' - U'A') : value;
}

// from から先で wanted のコードポイントが始まる境目（ADR 0061 の決定 6）。境目ごとに進むので、
// 別の文字の継続バイトの並びには当たらない。
[[nodiscard]] std::optional<Offset> code_point_from(std::string_view command, Offset from,
                                                    char32_t wanted) noexcept
{
    for (Offset at = from; at.value < command.size(); at = next_code_point(command, at))
    {
        if (lower_code_point(code_point_at(command, at)) == wanted)
        {
            return at;
        }
    }
    return std::nullopt;
}

// query のコードポイントを順に候補の中から探す部分列の照合。空白は飛ばし、ASCII は大文字と
// 小文字を区別しない。点は候補の長さと飛ばした量の和で、最初の文字までは 4 倍（単位はバイト）。
[[nodiscard]] std::optional<std::size_t> match_score(std::string_view query,
                                                     std::string_view command)
{
    Offset at{0};
    std::size_t score = command.size();
    for (Offset letter{0}; letter.value < query.size(); letter = next_code_point(query, letter))
    {
        const char32_t wanted = lower_code_point(code_point_at(query, letter));
        if (wanted == U' ')
        {
            continue;
        }
        const auto found = code_point_from(command, at, wanted);
        if (!found.has_value())
        {
            return std::nullopt;
        }
        score += (found.value().value - at.value) * (at.value == 0 ? 4 : 1);
        at = next_code_point(command, found.value());
    }
    return score;
}

[[nodiscard]] CommandChoice choice_of(std::string command)
{
    const auto label = DisplayText::parse(command).value();
    const auto kind = command == "set fontsize=" || command == "set guifont="
                          ? CommandChoiceKind::fill
                          : CommandChoiceKind::execute;
    return CommandChoice{label,        std::move(command), kind, std::nullopt,
                         std::nullopt, std::nullopt,       {}};
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

// 「場所＋名前」の文字（場所・区切り 1 文字・名前）。場所の無い候補は無し。
[[nodiscard]] std::optional<std::string> located_name(const CommandChoice &choice)
{
    if (!choice.detail.has_value())
    {
        return std::nullopt;
    }
    std::string located(choice.detail.value().text());
    located += '\\';
    located += choice.label.text();
    return located;
}

[[nodiscard]] std::optional<std::size_t> listed_score(std::string_view query,
                                                      const CommandChoice &choice)
{
    const auto named = match_score(query, choice.label.text());
    if (named.has_value())
    {
        return named;
    }
    const auto located = located_name(choice);
    if (!located.has_value())
    {
        return std::nullopt;
    }
    const auto score = match_score(query, located.value());
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
template <std::ranges::input_range Positions>
[[nodiscard]] std::vector<std::size_t> scored_positions(const std::vector<CommandChoice> &entries,
                                                        PaletteScope scope, std::string_view query,
                                                        const Positions &positions)
{
    std::vector<ScoredPosition> scored;
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
[[nodiscard]] CommandChoice operation_choice_of(const OperationText &text, EditMode mode)
{
    const auto chord = shown_chord(text.operation, mode);
    return CommandChoice{DisplayText::parse(text.name).value(),
                         {},
                         CommandChoiceKind::operate,
                         DisplayText::parse(text.description).value(),
                         std::nullopt,
                         text.operation,
                         chord.has_value() ? key_chord_label(chord.value()) : std::string{}};
}

// 名前・読み・鍵の表示名のうち一番良い点（ADR 0078 の決定 6）。どれにも当たらなければ無し。
[[nodiscard]] std::optional<std::size_t>
operation_score(std::string_view query, const CommandChoice &choice, std::string_view reading)
{
    std::optional<std::size_t> best;
    const std::array<std::string_view, 3> fields{choice.label.text(), reading, choice.key};
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
    return left.choice.command < right.choice.command;
}
} // namespace

std::vector<CommandChoice> palette_choices(const CommandLine &input)
{
    auto query = input.text();
    if (query.starts_with(':'))
    {
        query.remove_prefix(1);
    }
    // 値を直接打った場合も候補の実行文字列になり、同じEx評価が妥当性を決める。
    if (query.starts_with("set fontsize=") || query.starts_with("set guifont="))
    {
        return {choice_of(std::string(query))};
    }
    std::vector<CommandMatch> matches;
    for (auto command : ex_command_candidates(input.catalog()))
    {
        const auto score = match_score(query, command);
        if (score.has_value())
        {
            matches.push_back(CommandMatch{choice_of(std::move(command)), score.value()});
        }
    }
    std::sort(matches.begin(), matches.end(), before);
    std::vector<CommandChoice> choices;
    for (auto &match : matches)
    {
        choices.push_back(std::move(match.choice));
    }
    if (choices.empty() && query.starts_with("colorscheme "))
    {
        return {choice_of(std::string(query))};
    }
    return choices;
}

std::vector<CommandChoice> operation_choices(std::string_view query, EditMode mode)
{
    std::vector<CommandChoice> available;
    std::vector<ScoredPosition> scored;
    for (const OperationText &text : operation_texts)
    {
        if (!operation_available(text.operation, mode))
        {
            continue;
        }
        CommandChoice choice = operation_choice_of(text, mode);
        const auto score = query.empty() ? std::optional<std::size_t>{0}
                                         : operation_score(query, choice, text.reading);
        if (score.has_value())
        {
            scored.emplace_back(available.size(), score.value());
            available.push_back(std::move(choice));
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

std::string_view palette_origin_label(PaletteOrigin origin) noexcept
{
    switch (origin)
    {
    case PaletteOrigin::tab:
        return "開いているタブ";
    case PaletteOrigin::bookmarked_tab:
        return "タブ・ブックマーク";
    case PaletteOrigin::bookmark:
        return "ブックマーク";
    case PaletteOrigin::history:
        return "履歴";
    case PaletteOrigin::folder:
        return "同じフォルダ";
    }
    std::unreachable();
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
