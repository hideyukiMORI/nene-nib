#include "LineEndingConversion.hpp"

#include <cstddef>

namespace nenenib::core
{
namespace
{
// 1 行ぶん（行を終える `\n` の手前まで）を写す。直前の `\r` は改行の一部として形に合わせる。
void append_line(std::string &converted, std::string_view line, LineEnding to)
{
    const bool carried = line.ends_with('\r');
    switch (to)
    {
    case LineEnding::crlf:
        converted.append(line);
        converted.append(carried ? "\n" : "\r\n");
        return;
    case LineEnding::lf:
        converted.append(carried ? line.substr(0, line.size() - 1) : line);
        converted.push_back('\n');
        return;
    }
}
} // namespace

std::string line_endings_converted(std::string_view text, LineEnding to)
{
    std::string converted;
    converted.reserve(text.size() + (to == LineEnding::crlf ? text.size() / 8 : 0));
    std::size_t from = 0;
    for (std::size_t at = text.find('\n'); at != std::string_view::npos;
         at = text.find('\n', at + 1))
    {
        append_line(converted, text.substr(from, at - from), to);
        from = at + 1;
    }
    converted.append(text.substr(from));
    return converted;
}
} // namespace nenenib::core
