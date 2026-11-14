#include "ExLineCommand.hpp"

#include "ExRange.hpp"
#include "ExSubstituteCommand.hpp"

#include <charconv>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr std::string_view delete_name = "delete";
constexpr std::string_view yank_name = "yank";
constexpr std::string_view global_name = "global";
constexpr std::string_view vglobal_name = "vglobal";
// 書けるレジスタの名前のうち英字の外のもの（Vim の valid_yank_reg の writing・`"` は注釈・
// 数字は回数）。
constexpr std::string_view register_marks = "-_+*";

[[nodiscard]] bool is_letter(char letter) noexcept
{
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z');
}

[[nodiscard]] bool is_digit(char letter) noexcept
{
    return letter >= '0' && letter <= '9';
}

// letter が set の 1 文字か（STL の検索は core の外へ memchr を出すので歩く・ARC-003）。
[[nodiscard]] bool one_of(std::string_view set, char letter) noexcept
{
    for (const char entry : set)
    {
        if (entry == letter)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string_view skipped_white(std::string_view text) noexcept
{
    std::size_t first = 0;
    while (first < text.size() && (text[first] == ' ' || text[first] == '\t'))
    {
        ++first;
    }
    return text.substr(first);
}

[[nodiscard]] std::size_t letters_of(std::string_view text) noexcept
{
    std::size_t length = 0;
    while (length < text.size() && is_letter(text[length]))
    {
        ++length;
    }
    return length;
}

// name が full の省略（1 文字以上の頭）か。
[[nodiscard]] bool abbreviates(std::string_view full, std::string_view name) noexcept
{
    return !name.empty() && full.starts_with(name);
}

// `:dl` `:dp` `:dell` `:deletep` …（Vim の find_ex_command の特例・`:d` に表示のフラグが付く）。
[[nodiscard]] bool flagged_delete(std::string_view name) noexcept
{
    return name.size() >= 2 && (name.back() == 'l' || name.back() == 'p') &&
           abbreviates(delete_name, name.substr(0, name.size() - 1));
}

// 回数（先頭の数字・大きすぎれば size_t の最大）と、その長さ。
[[nodiscard]] std::pair<std::optional<std::size_t>, std::size_t>
count_read(std::string_view text) noexcept
{
    std::size_t digits = 0;
    while (digits < text.size() && is_digit(text[digits]))
    {
        ++digits;
    }
    if (digits == 0)
    {
        return {std::nullopt, 0};
    }
    std::size_t count = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + digits, count);
    return {parsed.ec == std::errc{} ? count : std::numeric_limits<std::size_t>::max(), digits};
}

// レジスタの名前（Vim の do_one_cmd の EX_REGSTR）。数字は回数なので読まない。読んだ名前と残り。
[[nodiscard]] std::expected<std::pair<std::optional<char>, std::string_view>, ExFailure>
register_read(std::string_view text) noexcept
{
    if (text.empty() || is_digit(text.front()))
    {
        return std::pair{std::optional<char>{}, text};
    }
    const char name = text.front();
    if (name == '#')
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    if (!is_letter(name) && !one_of(register_marks, name))
    {
        return std::pair{std::optional<char>{}, text};
    }
    return std::pair{std::optional<char>{name}, skipped_white(text.substr(1))};
}

// レジスタと回数の後ろ（Vim の do_one_cmd の separate_nextcmd）。空と注釈（`"`）は受け、`|` の後ろの
// 命令は Nib ではまだ流さない。
[[nodiscard]] std::optional<ExFailure> trailing_failure(std::string_view text) noexcept
{
    if (text.empty() || text.front() == '"')
    {
        return std::nullopt;
    }
    return text.front() == '|' ? ExFailure::unsupported_argument : ExFailure::trailing_characters;
}

// `:d` `:y` の引数（名前と `!` の後ろ）。
[[nodiscard]] std::expected<ExLinesRequest, ExFailure> lines_argument(std::string_view text,
                                                                      ExLineVerb verb)
{
    if (text.starts_with('!'))
    {
        return std::unexpected(ExFailure::no_bang_allowed);
    }
    const auto named = register_read(skipped_white(text));
    if (!named)
    {
        return std::unexpected(named.error());
    }
    ExLinesRequest request{std::nullopt, verb, named.value().first, std::nullopt};
    const std::string_view rest = named.value().second;
    const auto [count, digits] = count_read(rest);
    request.count = count;
    const auto failure = trailing_failure(skipped_white(rest.substr(digits)));
    if (failure.has_value())
    {
        return std::unexpected(failure.value());
    }
    return request;
}

// 当たった行に流す命令の頭の `:` と空白（Vim の do_one_cmd が飛ばす）を落とす。
[[nodiscard]] std::string_view command_head(std::string_view text) noexcept
{
    std::size_t first = 0;
    while (first < text.size() && (text[first] == ':' || text[first] == ' ' || text[first] == '\t'))
    {
        ++first;
    }
    return text.substr(first);
}

// `:s` の族のうち `:g` の中で受ける形か。範囲の回数（行ごとに範囲を広げる）と `n`（数えるだけ）は
// 段 3a ではまだ受けない。
[[nodiscard]] bool global_substitute(const ExSubstituteRequest &request) noexcept
{
    return !request.count.has_value() && !one_of(request.flags, 'n');
}

// 当たった行に流す命令（Vim の global_exe_one）。段 3a が受けるのは範囲の無い `:d` と `:s` の族。
[[nodiscard]] std::expected<ExGlobalCommand, ExFailure> global_command_of(std::string_view text)
{
    const std::string_view command = command_head(text);
    if (command.empty() || parsed_ex_range(command).second > 0)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    if (auto substitute = substitute_command(command))
    {
        if (!substitute.value())
        {
            return std::unexpected(substitute.value().error());
        }
        if (!global_substitute(substitute.value().value()))
        {
            return std::unexpected(ExFailure::unsupported_argument);
        }
        return ExGlobalCommand{std::move(substitute).value().value()};
    }
    auto lines = lines_command(command);
    if (!lines.has_value())
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    if (!lines.value())
    {
        return std::unexpected(lines.value().error());
    }
    if (lines.value().value().verb != ExLineVerb::delete_lines)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return ExGlobalCommand{std::move(lines).value().value()};
}

// `:g` の引数（名前と `!` の後ろ・先頭の空白は落とした）。
[[nodiscard]] std::expected<ExGlobalRequest, ExFailure> global_argument(std::string_view text,
                                                                        bool inverted)
{
    if (text.empty())
    {
        return std::unexpected(ExFailure::global_pattern_missing);
    }
    const char delimiter = text.front();
    if (is_letter(delimiter))
    {
        return std::unexpected(ExFailure::letter_delimiter);
    }
    const auto byte = static_cast<unsigned char>(delimiter);
    constexpr unsigned char ascii_limit = 0x80;
    // `\/` `\?` `\&`（前の検索・前の置換のパターン）と、注釈・命令の区切りと紛れる文字は段 3a では
    // まだ受けない。
    if (delimiter == '\\' || delimiter == '"' || delimiter == '|' || byte >= ascii_limit)
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    text.remove_prefix(1);
    const std::size_t end = ex_pattern_end(text, delimiter);
    std::string pattern(text.substr(0, end));
    auto command = global_command_of(end < text.size() ? text.substr(end + 1) : std::string_view{});
    if (!command)
    {
        return std::unexpected(command.error());
    }
    return ExGlobalRequest{std::nullopt, inverted, delimiter, std::move(pattern),
                           std::move(command).value()};
}
} // namespace

std::optional<std::expected<ExLinesRequest, ExFailure>> lines_command(std::string_view text)
{
    const std::size_t length = letters_of(text);
    const std::string_view name = text.substr(0, length);
    if (abbreviates(delete_name, name))
    {
        return lines_argument(text.substr(length), ExLineVerb::delete_lines);
    }
    if (abbreviates(yank_name, name))
    {
        return lines_argument(text.substr(length), ExLineVerb::yank_lines);
    }
    if (flagged_delete(name))
    {
        return std::unexpected(ExFailure::unsupported_argument);
    }
    return std::nullopt;
}

std::optional<std::expected<ExGlobalRequest, ExFailure>> global_command(std::string_view text)
{
    const std::size_t length = letters_of(text);
    const std::string_view name = text.substr(0, length);
    const bool global = abbreviates(global_name, name);
    if (!global && !(name.starts_with('v') && abbreviates(vglobal_name, name)))
    {
        return std::nullopt;
    }
    std::string_view rest = text.substr(length);
    const bool bang = rest.starts_with('!');
    if (bang && !global)
    {
        return std::unexpected(ExFailure::no_bang_allowed);
    }
    rest.remove_prefix(bang ? 1 : 0);
    return global_argument(skipped_white(rest), !global || bang);
}
} // namespace nenenib::core
