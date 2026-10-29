#include "VimNumber.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
// 窓の 1 バイト（0〜255）。行の終わりは end_of_line（Vim の NUL）。
using Read = std::expected<int, VimNumberMiss>;
using Reach = std::expected<std::size_t, VimNumberMiss>;
using Found = std::expected<VimNumberSpan, VimNumberMiss>;
using Holds = std::expected<bool, VimNumberMiss>;
using Classifies = bool (*)(int) noexcept;

constexpr int end_of_line = -1;
constexpr std::uint64_t largest = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t decimal_base = 10;
constexpr std::uint64_t hexadecimal_base = 16;
constexpr std::uint64_t binary_base = 2;
constexpr std::size_t prefix_length = 2;
constexpr int letter_case_bit = 0x20;
constexpr int byte_mask = 0xFF;

[[nodiscard]] Read byte_at(const VimNumberWindow &window, std::size_t at) noexcept
{
    if (at < window.bytes.size())
    {
        return static_cast<int>(static_cast<unsigned char>(window.bytes[at])) & byte_mask;
    }
    if (window.to_line_end)
    {
        return end_of_line;
    }
    return std::unexpected(VimNumberMiss::window_short);
}

// Vim の `col > 0`（行の中で手前に字があるか）。窓の始まりが行頭でなければ窓が足りない。
[[nodiscard]] Holds has_before(const VimNumberWindow &window, std::size_t at) noexcept
{
    if (at > 0)
    {
        return true;
    }
    if (window.from_line_start)
    {
        return false;
    }
    return std::unexpected(VimNumberMiss::window_short);
}

[[nodiscard]] bool is_digit(int value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool is_binary_digit(int value) noexcept
{
    return value == '0' || value == '1';
}

[[nodiscard]] bool is_hexadecimal_digit(int value) noexcept
{
    const int lower = value | letter_case_bit;
    return is_digit(value) || (lower >= 'a' && lower <= 'f');
}

[[nodiscard]] bool is_letter(int value) noexcept
{
    const int lower = value | letter_case_bit;
    return lower >= 'a' && lower <= 'z';
}

[[nodiscard]] bool is_upper(int value) noexcept
{
    return value >= 'A' && value <= 'Z';
}

[[nodiscard]] bool same_letter(int value, char lower) noexcept
{
    return (value | letter_case_bit) == lower;
}

// 手前を確かめた結果のまま `at` に留まる（窓が足りなければその印）。
[[nodiscard]] Reach stayed(const Holds &before, std::size_t at)
{
    if (!before)
    {
        return std::unexpected(before.error());
    }
    return at;
}

// `while (col > 0 && matches(ptr[col])) --col`。
[[nodiscard]] Reach scanned_back(const VimNumberWindow &window, std::size_t at, Classifies matches)
{
    while (true)
    {
        const Read value = byte_at(window, at);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (!matches(value.value()))
        {
            return at;
        }
        const Holds before = has_before(window, at);
        if (!before || !before.value())
        {
            return stayed(before, at);
        }
        --at;
    }
}

// `at` が `0x`（`lower` が 'b' なら `0b`）の x / b の上で、次が桁か（Vim の do_addsub の条件）。
[[nodiscard]] Holds on_prefix(const VimNumberWindow &window, std::size_t at, char lower,
                              Classifies digit)
{
    const Read value = byte_at(window, at);
    if (!value || !same_letter(value.value(), lower))
    {
        return value.transform([](int) { return false; });
    }
    const Holds before = has_before(window, at);
    if (!before || !before.value() || byte_at(window, at - 1) != '0')
    {
        return before.transform([](bool) { return false; });
    }
    return byte_at(window, at + 1).transform([digit](int next) { return digit(next); });
}

[[nodiscard]] Holds on_any_prefix(const VimNumberWindow &window, std::size_t at)
{
    return on_prefix(window, at, 'x', is_hexadecimal_digit)
        .and_then(
            [&window, at](bool hexadecimal)
            { return hexadecimal ? Holds{true} : on_prefix(window, at, 'b', is_binary_digit); });
}

// キャレットから右へ最初の 10 進の桁（無ければ行の終わり）まで進む。
[[nodiscard]] Reach first_digit(const VimNumberWindow &window)
{
    std::size_t at = window.at;
    while (true)
    {
        const Read value = byte_at(window, at);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (value.value() == end_of_line || is_digit(value.value()))
        {
            return at;
        }
        ++at;
    }
}

// 手前が 10 進の桁の間だけ戻る（`while (col > 0 && isdigit(ptr[col - 1])) --col`）。
[[nodiscard]] Reach digits_back(const VimNumberWindow &window, std::size_t at)
{
    while (true)
    {
        const Holds before = has_before(window, at);
        if (!before || !before.value() || !is_digit(byte_at(window, at - 1).value_or(end_of_line)))
        {
            return stayed(before, at);
        }
        --at;
    }
}

// キャレットから右へ最初の 10 進の桁まで進み、その数の頭まで戻る（Vim の do_addsub の else）。
[[nodiscard]] Reach forward_start(const VimNumberWindow &window)
{
    return first_digit(window).and_then([&window](std::size_t at)
                                        { return digits_back(window, at); });
}

// キャレットが 16 進・2 進の数の中か `0x` `0b` の上なら、その `0` の位置（Vim の 2 段の後ろ戻り）。
[[nodiscard]] std::expected<std::optional<std::size_t>, VimNumberMiss>
prefixed_start(const VimNumberWindow &window)
{
    const Reach binary = scanned_back(window, window.at, is_binary_digit);
    const Reach back = binary.and_then([&window](std::size_t at)
                                       { return scanned_back(window, at, is_hexadecimal_digit); });
    const Holds hexadecimal = back.and_then(
        [&window](std::size_t at) { return on_prefix(window, at, 'x', is_hexadecimal_digit); });
    if (!hexadecimal)
    {
        return std::unexpected(hexadecimal.error());
    }
    const Reach start = hexadecimal.value() ? back : scanned_back(window, window.at, is_digit);
    return start.and_then(
        [&window](std::size_t at)
        {
            return on_any_prefix(window, at)
                .transform(
                    [at](bool prefixed)
                    { return prefixed ? std::optional<std::size_t>{at - 1} : std::nullopt; });
        });
}

[[nodiscard]] Reach caret_start(const VimNumberWindow &window)
{
    return prefixed_start(window).and_then(
        [&window](std::optional<std::size_t> start)
        { return start.has_value() ? Reach{start.value()} : forward_start(window); });
}

[[nodiscard]] std::uint64_t base_of(VimNumberForm form) noexcept
{
    switch (form)
    {
    case VimNumberForm::decimal:
        return decimal_base;
    case VimNumberForm::hexadecimal:
        return hexadecimal_base;
    case VimNumberForm::binary:
        return binary_base;
    }
    std::unreachable();
}

[[nodiscard]] bool is_digit_of(VimNumberForm form, int value) noexcept
{
    switch (form)
    {
    case VimNumberForm::decimal:
        return is_digit(value);
    case VimNumberForm::hexadecimal:
        return is_hexadecimal_digit(value);
    case VimNumberForm::binary:
        return is_binary_digit(value);
    }
    std::unreachable();
}

[[nodiscard]] std::uint64_t digit_value(int value) noexcept
{
    if (is_digit(value))
    {
        return static_cast<std::uint64_t>(value - '0');
    }
    return static_cast<std::uint64_t>((value | letter_case_bit) - 'a') + decimal_base;
}

// 1 桁を足す。溢れたら最大値に貼り付く（Vim の vim_str2nr）。10 進だけは最大値ちょうどまで入る。
void accumulate(VimNumberSpan &span, std::uint64_t digit) noexcept
{
    const std::uint64_t base = base_of(span.form);
    const bool fits = span.form == VimNumberForm::decimal
                          ? span.magnitude < largest / base ||
                                (span.magnitude == largest / base && digit <= largest % base)
                          : span.magnitude <= largest / base;
    if (!fits)
    {
        span.magnitude = largest;
        span.overflow = true;
        return;
    }
    span.magnitude = span.magnitude * base + digit;
}

// `0x` `0b` の接頭辞（Vim の vim_str2nr の頭。maxlen は 0 なら上限なし）。
[[nodiscard]] std::expected<VimNumberForm, VimNumberMiss> form_at(const VimNumberWindow &window,
                                                                  std::size_t at, std::size_t limit)
{
    const Read second = byte_at(window, at + 1);
    const Read third = second.and_then([&](int) { return byte_at(window, at + 2); });
    if (!third)
    {
        return std::unexpected(third.error());
    }
    const bool room = limit == 0 || limit > prefix_length;
    if (same_letter(second.value(), 'x') && is_hexadecimal_digit(third.value()) && room)
    {
        return VimNumberForm::hexadecimal;
    }
    if (same_letter(second.value(), 'b') && is_binary_digit(third.value()) && room)
    {
        return VimNumberForm::binary;
    }
    return VimNumberForm::decimal;
}

// 桁の頭 `at` の数の形。`0` で始まり次が `8` `9` でなければ接頭辞を見る（Vim の vim_str2nr）。
[[nodiscard]] std::expected<VimNumberForm, VimNumberMiss>
leading_form(const VimNumberWindow &window, std::size_t at, std::size_t limit)
{
    const Read second = byte_at(window, at + 1);
    if (!second)
    {
        return std::unexpected(second.error());
    }
    const bool zero = byte_at(window, at) == '0';
    if (zero && second.value() != '8' && second.value() != '9' && (limit == 0 || limit > 1))
    {
        return form_at(window, at, limit);
    }
    return VimNumberForm::decimal;
}

// 桁の頭 `at` から数を読む（Vim の vim_str2nr）。`limit` は読む字数の上限（0 は上限なし）。
[[nodiscard]] Found parsed(const VimNumberWindow &window, std::size_t at, std::size_t limit)
{
    VimNumberSpan span{at, at, 0, VimNumberForm::decimal, false, false, false, false, true};
    const auto form = leading_form(window, at, limit);
    if (!form)
    {
        return std::unexpected(form.error());
    }
    span.form = form.value();
    std::size_t read = span.form == VimNumberForm::decimal ? 0 : prefix_length;
    while (true)
    {
        const Read value = byte_at(window, at + read);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (!is_digit_of(span.form, value.value()))
        {
            break;
        }
        accumulate(span, digit_value(value.value()));
        ++read;
        if (read == limit)
        {
            break;
        }
    }
    span.end = at + read;
    return span;
}

// 範囲の最後の英字の大小（Vim の hexupper）と、接頭辞の x / b の大小。
[[nodiscard]] VimNumberSpan with_cases(const VimNumberWindow &window, VimNumberSpan span) noexcept
{
    for (std::size_t at = span.begin; at < span.end; ++at)
    {
        const int value = static_cast<unsigned char>(window.bytes[at]);
        if (is_letter(value))
        {
            span.upper_digits = is_upper(value);
        }
    }
    if (span.form != VimNumberForm::decimal)
    {
        span.upper_prefix = is_upper(static_cast<unsigned char>(window.bytes[span.begin + 1]));
    }
    return span;
}

// 頭の桁 `at` からの数。`-` が負号になるのは `signed_from` より右にあるときで、16 進と 2 進は
// 負号を持たない（Vim は `-` を残して正の数として増減する）。
[[nodiscard]] Found number_at(const VimNumberWindow &window, std::size_t at, std::size_t limit,
                              Holds minus_allowed)
{
    const Read first = byte_at(window, at);
    if (!first)
    {
        return std::unexpected(first.error());
    }
    if (!is_digit(first.value()))
    {
        return std::unexpected(VimNumberMiss::absent);
    }
    if (!minus_allowed)
    {
        return std::unexpected(minus_allowed.error());
    }
    const bool minus = minus_allowed.value() && byte_at(window, at - 1) == '-';
    return parsed(window, at, limit)
        .transform(
            [&window, minus, &first](VimNumberSpan span)
            {
                span.zero_led = first.value() == '0';
                span.negative = minus && span.form == VimNumberForm::decimal;
                span.begin -= static_cast<std::size_t>(span.negative);
                return with_cases(window, span);
            });
}

[[nodiscard]] std::string digits_of(std::uint64_t value, VimNumberForm form, bool upper)
{
    const char *const symbols = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    const std::uint64_t base = base_of(form);
    std::string digits;
    // 2 進の 0 は桁が空（Vim は先頭の 0 を飛ばす数え方で 1 桁も書かない）。10 進と 16 進は "0"。
    if (value == 0 && form != VimNumberForm::binary)
    {
        return "0";
    }
    while (value != 0)
    {
        digits.insert(digits.begin(), symbols[value % base]);
        value /= base;
    }
    return digits;
}

// 増減した値と符号（Vim の do_addsub）。10 進は 64 bit の符号なしで回り込むと負号が反転する。
[[nodiscard]] std::pair<std::uint64_t, bool> added(const VimNumberSpan &span, std::uint64_t amount,
                                                   bool subtract) noexcept
{
    const bool down = subtract != span.negative;
    const std::uint64_t old = span.magnitude;
    std::uint64_t value = old;
    if (!span.overflow)
    {
        value = down ? old - amount : old + amount;
    }
    bool negative = span.negative;
    if (span.form != VimNumberForm::decimal)
    {
        return {value, negative};
    }
    if (down && value > old)
    {
        value = 0 - value;
        negative = !negative;
    }
    if (!down && value < old)
    {
        value = ~value;
        negative = !negative;
    }
    return {value, negative && value != 0};
}
} // namespace

std::expected<VimNumberSpan, VimNumberMiss> vim_find_number(const VimNumberWindow &window) noexcept
{
    const Read under = byte_at(window, window.at);
    if (!under || under.value() == end_of_line)
    {
        return std::unexpected(under ? VimNumberMiss::absent : under.error());
    }
    return caret_start(window).and_then(
        [&window](std::size_t at) { return number_at(window, at, 0, has_before(window, at)); });
}

std::expected<VimNumberSpan, VimNumberMiss>
vim_find_selected_number(const VimNumberWindow &window, std::size_t length, bool bounded) noexcept
{
    std::size_t at = window.at;
    std::size_t left = length;
    while (left > 0)
    {
        const Read value = byte_at(window, at);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        if (value.value() == end_of_line || is_digit(value.value()))
        {
            break;
        }
        ++at;
        --left;
    }
    if (left == 0)
    {
        return std::unexpected(VimNumberMiss::absent);
    }
    return number_at(window, at, bounded ? left : 0, Holds{at > window.at});
}

std::string vim_incremented_text(const VimNumberSpan &span, std::uint64_t amount, bool subtract)
{
    const auto [value, negative] = added(span, amount, subtract);
    const std::size_t sign = span.negative ? 1 : 0;
    const std::size_t prefix = span.form == VimNumberForm::decimal ? 0 : prefix_length;
    const std::size_t width = span.end - span.begin - sign - prefix;
    const std::string digits = digits_of(value, span.form, span.upper_digits);
    std::string text = negative ? "-" : "";
    switch (span.form)
    {
    case VimNumberForm::decimal:
        break;
    case VimNumberForm::hexadecimal:
        text += span.upper_prefix ? "0X" : "0x";
        break;
    case VimNumberForm::binary:
        text += span.upper_prefix ? "0B" : "0b";
        break;
    }
    if (span.zero_led && digits.size() < width)
    {
        text.append(width - digits.size(), '0');
    }
    return text + digits;
}
} // namespace nenenib::core
