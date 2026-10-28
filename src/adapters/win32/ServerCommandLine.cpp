#include "ServerCommandLine.hpp"

#include "Utf16.hpp"

#include <cstddef>
#include <string_view>

namespace nenenib::adapters::win32
{
namespace
{
bool plain(std::wstring_view argument) noexcept
{
    return !argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring_view::npos;
}

// `"` の前に続く `\` は倍にし、`"` そのものは `\"` にする。末尾の `\` は閉じの `"`
// の前なので倍にする。
void append_quoted(std::wstring &line, std::wstring_view argument)
{
    line.push_back(L'"');
    std::size_t slashes = 0;
    for (const wchar_t letter : argument)
    {
        if (letter == L'\\')
        {
            ++slashes;
            continue;
        }
        const std::size_t doubled = letter == L'"' ? slashes * 2 + 1 : slashes;
        line.append(doubled, L'\\');
        line.push_back(letter);
        slashes = 0;
    }
    line.append(slashes * 2, L'\\');
    line.push_back(L'"');
}

void append_argument(std::wstring &line, std::wstring_view argument)
{
    if (!line.empty())
    {
        line.push_back(L' ');
    }
    if (plain(argument))
    {
        line.append(argument);
        return;
    }
    append_quoted(line, argument);
}
} // namespace

std::expected<std::wstring, application::ServerFailure>
server_command_line(const std::wstring &executable, const std::vector<std::string> &arguments)
{
    std::wstring line;
    append_argument(line, executable);
    for (const auto &argument : arguments)
    {
        const auto wide = core::to_utf16(argument);
        if (!wide.has_value())
        {
            return std::unexpected(application::ServerFailure::cannot_start);
        }
        append_argument(line, wide.value());
    }
    return line;
}
} // namespace nenenib::adapters::win32
