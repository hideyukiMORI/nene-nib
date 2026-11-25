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

// at から続く空白と Tab の数（Vim の skipwhite）。
[[nodiscard]] std::size_t blanks_from(std::string_view text, std::size_t at) noexcept
{
    std::size_t end = at;
    while (end < text.size() && (text[end] == ' ' || text[end] == '\t'))
    {
        ++end;
    }
    return end - at;
}

// at が足し（`+` `-` か数字）の始まりか。
[[nodiscard]] bool offset_start(std::string_view text, std::size_t at) noexcept
{
    return at < text.size() && (text[at] == '+' || text[at] == '-' || digit_at(text, at));
}

// マークの名前の 1 文字（`'a` `'<`）。ASCII の印字できる文字だけを読む。
[[nodiscard]] bool is_mark_name(char letter) noexcept
{
    return letter > ' ' && letter <= '~';
}

// 基点の後ろの `+N` `-N` と数字の足し（Vim の get_address の後半）。読んだ長さを返す。足しの前の
// 空白と Tab は Vim と同じく読み飛ばす（`:1 2` は 3 行目・`:1 +2d`・#2019）。足しの続かない空白は
// 読まない（命令の前の空白として残す）。
[[nodiscard]] std::size_t offsets_read(std::string_view text, std::size_t at, ExAddress &address)
{
    const std::size_t start = at;
    while (offset_start(text, at + blanks_from(text, at)))
    {
        at += blanks_from(text, at);
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
        address.offset = saturated(address.offset, number_of(text.substr(at, digits)), false);
        at += digits;
    }
    return at - start;
}

// 空白で始まらない番地 1 つ（`%` は 2 つ）を range へ足し、長さを返す。基点も足しも無ければ current
// の 0 バイト。
[[nodiscard]] std::size_t based_address_read(std::string_view text, ExRange &range, bool semicolon)
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

// 番地 1 つを range へ足し、長さを返す。基点の前の空白と Tab は Vim の get_address と同じく
// 読み飛ばす（`:1, 3d`・#2019）。基点も足しも無ければ前の空白も読まない（0 バイト）。
[[nodiscard]] std::size_t address_read(std::string_view whole, ExRange &range, bool semicolon)
{
    const std::size_t lead = blanks_from(whole, 0);
    const std::size_t read = based_address_read(whole.substr(lead), range, semicolon);
    return read == 0 ? 0 : lead + read;
}

// at の後ろの区切り（`,` `;`）の位置。区切りの前の空白と Tab も Vim と同じく読み飛ばす
// （`:1 ,3d`・#2019）。区切りが無ければ text の長さ。
[[nodiscard]] std::size_t separator_at(std::string_view text, std::size_t at) noexcept
{
    const std::size_t ahead = at + blanks_from(text, at);
    return ahead < text.size() && (text[ahead] == ',' || text[ahead] == ';') ? ahead : text.size();
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
        const std::size_t ahead = separator_at(text, at);
        const bool separated = ahead < text.size();
        at = separated ? ahead : at;
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
