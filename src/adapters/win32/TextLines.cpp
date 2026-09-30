#include "TextLines.hpp"

namespace nenenib::adapters::win32
{
std::vector<std::string_view> text_lines(std::string_view bytes)
{
    std::vector<std::string_view> lines;
    while (!bytes.empty())
    {
        const auto end = bytes.find('\n');
        auto line = bytes.substr(0, end);
        if (line.ends_with('\r'))
        {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        if (end == std::string_view::npos)
        {
            break;
        }
        bytes.remove_prefix(end + 1);
    }
    return lines;
}
} // namespace nenenib::adapters::win32
