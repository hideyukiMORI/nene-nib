#include "CommandChoice.hpp"

#include "CommandMatch.hpp"
#include "ExResult.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] char lower_ascii(char letter) noexcept
{
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter + ('a' - 'A')) : letter;
}

[[nodiscard]] std::optional<std::size_t> match_score(std::string_view query,
                                                     std::string_view command)
{
    std::size_t at = 0;
    std::size_t score = command.size();
    for (const char letter : query)
    {
        if (letter == ' ')
        {
            continue;
        }
        const auto found = command.find(lower_ascii(letter), at);
        if (found == std::string_view::npos)
        {
            return std::nullopt;
        }
        score += (found - at) * (at == 0 ? 4 : 1);
        at = found + 1;
    }
    return score;
}

[[nodiscard]] CommandChoice choice_of(std::string command)
{
    const auto label = DisplayText::parse(command).value();
    const auto kind = command == "set fontsize=" || command == "set guifont="
                          ? CommandChoiceKind::fill
                          : CommandChoiceKind::execute;
    return CommandChoice{label, std::move(command), kind};
}

[[nodiscard]] std::string lowered(std::string_view text)
{
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), lower_ascii);
    return lower;
}

// 出どころの印が見せる出どころ（ADR 0060 の決定 3）。印が増えたらここが落ちる（CPP-002）。
[[nodiscard]] PaletteScope scope_of(PaletteOrigin origin) noexcept
{
    switch (origin)
    {
    case PaletteOrigin::tab:
        return PaletteScope::tabs;
    }
    std::unreachable();
}

// 出どころで絞るのはこの 1 つ（ADR 0060 の決定 3）。files は印のある候補の全部、commands の候補は
// 列に無い（Ex のコマンドは palette_choices が作る）。
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
    case PaletteScope::tabs:
    case PaletteScope::commands:
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
    return lowered(located);
}

[[nodiscard]] std::optional<std::size_t> listed_score(std::string_view query,
                                                      const CommandChoice &choice)
{
    const auto named = match_score(query, lowered(choice.label.text()));
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

// 点の小さい順、同点は列の順。std::stable_sort は一時の領域を std::nothrow で取るので core の外へ
// シンボルが出る（ARC-003）。列の位置を添えた並べ替えで同じ順にする。
[[nodiscard]] std::vector<CommandChoice> in_score_order(std::vector<CommandMatch> matches)
{
    std::vector<std::size_t> order;
    order.reserve(matches.size());
    for (std::size_t index = 0; index < matches.size(); ++index)
    {
        order.push_back(index);
    }
    std::ranges::sort(order,
                      [&matches](std::size_t left, std::size_t right)
                      {
                          const std::size_t left_score = matches.at(left).score;
                          const std::size_t right_score = matches.at(right).score;
                          return left_score != right_score ? left_score < right_score
                                                           : left < right;
                      });
    std::vector<CommandChoice> choices;
    choices.reserve(order.size());
    for (const std::size_t index : order)
    {
        choices.push_back(std::move(matches.at(index).choice));
    }
    return choices;
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

std::string_view palette_origin_label(PaletteOrigin origin) noexcept
{
    switch (origin)
    {
    case PaletteOrigin::tab:
        return "開いているタブ";
    }
    std::unreachable();
}

std::vector<CommandChoice> listed_choices(const std::vector<CommandChoice> &entries,
                                          PaletteScope scope, std::string_view query)
{
    std::vector<CommandMatch> matches;
    for (const CommandChoice &entry : entries)
    {
        if (!in_scope(entry.origin, scope))
        {
            continue;
        }
        const auto score =
            query.empty() ? std::optional<std::size_t>{0} : listed_score(query, entry);
        if (score.has_value())
        {
            matches.push_back(CommandMatch{entry, score.value()});
        }
    }
    return in_score_order(std::move(matches));
}
} // namespace nenenib::core
