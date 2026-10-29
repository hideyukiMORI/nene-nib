#include "LiteralPattern.hpp"

namespace nenenib::core
{
std::string literal_pattern(std::string_view text, VimSearchDirection direction)
{
    constexpr std::string_view special = "\\/.*[~^$";
    const bool backward = direction == VimSearchDirection::backward;
    std::string escaped;
    escaped.reserve(text.size());
    for (const char byte : text)
    {
        if (special.contains(byte) || (backward && byte == '?'))
        {
            escaped.push_back('\\');
        }
        escaped.push_back(byte);
    }
    return escaped;
}
} // namespace nenenib::core
