#include "ServerCommand.hpp"

#include <string>
#include <utility>
#include <vector>

namespace nenenib::application
{
namespace
{
[[nodiscard]] constexpr bool blank(char character) noexcept
{
    return character == ' ' || character == '\t';
}

[[nodiscard]] std::string_view skip_blanks(std::string_view rest) noexcept
{
    while (!rest.empty() && blank(rest.front()))
    {
        rest.remove_prefix(1);
    }
    return rest;
}

// rest の先頭の 1 語を word に読み、残りを返す。`"` の間の空白は区切らない。閉じない `"` は
// 値なし。
[[nodiscard]] std::optional<std::string_view> read_word(std::string_view rest, std::string &word)
{
    bool quoted = false;
    while (!rest.empty() && (quoted || !blank(rest.front())))
    {
        const char character = rest.front();
        rest.remove_prefix(1);
        if (character == '"')
        {
            quoted = !quoted;
            continue;
        }
        word.push_back(character);
    }
    if (quoted)
    {
        return std::nullopt;
    }
    return rest;
}

// 命令の 1 行を語に分ける。閉じない `"` は値なし。
[[nodiscard]] std::optional<std::vector<std::string>> command_words(std::string_view command)
{
    std::vector<std::string> words;
    std::string_view rest = skip_blanks(command);
    while (!rest.empty())
    {
        std::string word;
        const auto after = read_word(rest, word);
        if (!after.has_value())
        {
            return std::nullopt;
        }
        words.push_back(std::move(word));
        rest = skip_blanks(after.value());
    }
    return words;
}
} // namespace

std::optional<ServerLaunch> server_launch_of(std::string_view command, const core::FilePath &root)
{
    auto words = command_words(command);
    if (!words.has_value() || words.value().empty())
    {
        return std::nullopt;
    }
    auto executable = core::FilePath::parse(words.value().front());
    if (!executable)
    {
        return std::nullopt;
    }
    std::vector<std::string> arguments(std::make_move_iterator(words.value().begin() + 1),
                                       std::make_move_iterator(words.value().end()));
    return ServerLaunch{std::move(executable).value(), std::move(arguments), root};
}
} // namespace nenenib::application
