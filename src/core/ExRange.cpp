#include "ExRange.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <system_error>

namespace nenenib::core
{
namespace
{
// 番地の足し算の飽和の端。行の数（size_t）より十分大きく、足しても溢れない。
constexpr std::int64_t offset_limit = std::int64_t{1} << 62;

[[nodiscard]] bool digit_at(std::string_view text, std::size_t at) noexcept
{
    return at < text.size() && text[at] >= '0' && text[at] <= '9';
}

[[nodiscard]] std::size_t digits_from(std::string_view text, std::size_t at) noexcept
{
    std::size_t end = at;
    while (digit_at(text, end))
    {
        ++end;
    }
    return end - at;
}

// 10 進の数字の列の値（大きすぎれば size_t の最大）。
[[nodiscard]] std::size_t number_of(std::string_view digits) noexcept
{
    std::size_t value = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return parsed.ec == std::errc{} ? value : std::numeric_limits<std::size_t>::max();
}

[[nodiscard]] std::int64_t saturated(std::int64_t sum, std::size_t amount, bool minus) noexcept
{
    const auto step = static_cast<std::int64_t>(
        amount > static_cast<std::size_t>(offset_limit) ? offset_limit : amount);
    const std::int64_t next = minus ? sum - step : sum + step;
    return next > offset_limit ? offset_limit : (next < -offset_limit ? -offset_limit : next);
}

// マークの名前の 1 文字（`'a` `'<`）。ASCII の印字できる文字だけを読む。
[[nodiscard]] bool is_mark_name(char letter) noexcept
{
    return letter > ' ' && letter <= '~';
}

// 基点の後ろの `+N` `-N` と数字の足し（Vim の get_address の後半）。読んだ長さを返す。
[[nodiscard]] std::size_t offsets_read(std::string_view text, std::size_t at, ExAddress &address)
{
    const std::size_t start = at;
    while (at < text.size())
    {
        const char next = text[at];
        if (next == '+' || next == '-')
        {
            const std::size_t digits = digits_from(text, at + 1);
            const std::size_t amount = digits == 0 ? 1 : number_of(text.substr(at + 1, digits));
            address.offset = saturated(address.offset, amount, next == '-');
            at += 1 + digits;
            continue;
        }
        const std::size_t digits = digits_from(text, at);
        if (digits == 0)
        {
            break;
        }
        address.offset = saturated(address.offset, number_of(text.substr(at, digits)), false);
        at += digits;
    }
    return at - start;
}

// 番地 1 つ（`%` は 2 つ）を range へ足し、長さを返す。基点も足しも無ければ current の 0 バイト。
[[nodiscard]] std::size_t address_read(std::string_view text, ExRange &range, bool semicolon)
{
    ExAddress address{ExAddressBase::current, 0, '\0', 0, semicolon};
    std::size_t at = 0;
    if (!text.empty() && (text.front() == '.' || text.front() == '$' || text.front() == '%'))
    {
        if (text.front() == '%')
        {
            range.addresses.push_back(ExAddress{ExAddressBase::number, 1, '\0', 0, semicolon});
        }
        address.base = text.front() == '.' ? ExAddressBase::current : ExAddressBase::last;
        at = 1;
    }
    else if (text.size() > 1 && text.front() == '\'' && is_mark_name(text[1]))
    {
        address.base = ExAddressBase::mark;
        address.mark = text[1];
        at = 2;
    }
    else if (digit_at(text, 0))
    {
        at = digits_from(text, 0);
        address.base = ExAddressBase::number;
        address.number = number_of(text.substr(0, at));
    }
    at += offsets_read(text, at, address);
    range.addresses.push_back(address);
    return at;
}
} // namespace

std::pair<ExRange, std::size_t> parsed_ex_range(std::string_view text)
{
    ExRange range;
    std::size_t at = 0;
    bool semicolon = false;
    while (true)
    {
        ExRange read;
        const std::size_t length = address_read(text.substr(at), read, semicolon);
        at += length;
        const bool separated = at < text.size() && (text[at] == ',' || text[at] == ';');
        if (length > 0 || separated || !range.addresses.empty())
        {
            range.addresses.insert(range.addresses.end(), read.addresses.begin(),
                                   read.addresses.end());
        }
        if (!separated)
        {
            break;
        }
        semicolon = text[at] == ';';
        ++at;
    }
    return {at == 0 ? ExRange{} : std::move(range), at};
}
} // namespace nenenib::core
