#include "DateTimeText.hpp"

#include <cstddef>

namespace nenenib::core
{
namespace
{
// value を width 桁の 10 進で text の末尾に足す。足りない桁は 0 で埋め、溢れる上の桁は捨てない。
void append_padded(std::string &text, unsigned value, std::size_t width)
{
    std::string digits;
    do
    {
        digits.insert(digits.begin(), static_cast<char>('0' + (value % 10U)));
        value /= 10U;
    } while (value > 0U);
    if (digits.size() < width)
    {
        digits.insert(digits.begin(), width - digits.size(), '0');
    }
    text += digits;
}
} // namespace

std::string date_time_text(LocalDateTime value)
{
    std::string text;
    append_padded(text, value.hour, 2);
    text += ':';
    append_padded(text, value.minute, 2);
    text += ' ';
    append_padded(text, value.year, 4);
    text += '/';
    append_padded(text, value.month, 2);
    text += '/';
    append_padded(text, value.day, 2);
    return text;
}
} // namespace nenenib::core
