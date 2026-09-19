#include "CommandChoice.hpp"

#include "CommandMatch.hpp"
#include "ExResult.hpp"

#include <algorithm>
#include <optional>
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
} // namespace nenenib::core
