#include "ServerLogLine.hpp"

#include "Utf8.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nenenib::core
{
namespace
{
constexpr std::string_view replacement = "\xEF\xBF\xBD";
constexpr std::string_view ellipsis = "\xE2\x80\xA6";
constexpr unsigned char first_printable = 0x20;
constexpr unsigned char delete_character = 0x7F;
constexpr unsigned char tab_character = 0x09;
constexpr unsigned char escape_character = 0x1B;
constexpr unsigned char bell_character = 0x07;
// 制御文字の図形（U+2400〜U+241F は C0 の 1 対 1・U+2421 は DEL）の UTF-8。
constexpr std::string_view control_picture_lead = "\xE2\x90";
constexpr unsigned char control_picture_base = 0x80;
constexpr std::string_view delete_picture = "\xE2\x90\xA1";
// C1（U+0080〜U+009F）は C2 80〜C2 9F。
constexpr unsigned char c1_lead = 0xC2;
constexpr unsigned char c1_last = 0x9F;
// 行と段落の区切り（U+2028・U+2029 は E2 80 A8・A9）と双方向の制御（U+202A〜U+202E は
// E2 80 AA〜AE・U+2066〜U+2069 は E2 81 A6〜A9）。区切りと埋め込みは続いた 1 つの範囲で引く。
constexpr unsigned char bidi_lead = 0xE2;
constexpr unsigned char bidi_embed_middle = 0x80;
constexpr unsigned char bidi_embed_first = 0xA8;
constexpr unsigned char bidi_embed_last = 0xAE;
constexpr unsigned char bidi_isolate_middle = 0x81;
constexpr unsigned char bidi_isolate_first = 0xA6;
constexpr unsigned char bidi_isolate_last = 0xA9;
constexpr unsigned char continuation_bits = 0x3F;
constexpr unsigned char three_lead_bits = 0x0F;
constexpr unsigned int six_bits = 6;
constexpr unsigned int twelve_bits = 12;
constexpr unsigned int hex_bits = 4;
constexpr unsigned int hex_digits = 4;
constexpr unsigned int hex_mask = 0xF;
constexpr std::string_view hex = "0123456789ABCDEF";
// ESC の後ろの文字の範囲（ECMA-48）。CSI は ESC [、OSC は ESC ]。
constexpr unsigned char csi_introducer = '[';
constexpr unsigned char osc_introducer = ']';
constexpr unsigned char string_terminator = '\\';
constexpr unsigned char csi_parameter_first = 0x20;
constexpr unsigned char csi_parameter_last = 0x3F;
constexpr unsigned char csi_final_first = 0x40;
constexpr unsigned char csi_final_last = 0x7E;
// ESC と中間の字（0x20〜0x2F）の列と終わりの字（Fp・Fe・Fs の 0x30〜0x7E・ESC 7 や ESC c や
// ESC ( B）。
constexpr unsigned char escape_intermediate_first = 0x20;
constexpr unsigned char escape_intermediate_last = 0x2F;
constexpr unsigned char escape_final_first = 0x30;

[[nodiscard]] bool within(unsigned char byte, unsigned char first, unsigned char last) noexcept
{
    return byte >= first && byte <= last;
}

[[nodiscard]] unsigned char byte_at(std::string_view raw, std::size_t at) noexcept
{
    return static_cast<unsigned char>(raw[at]);
}

// 先頭のバイトが始める列の長さ（1〜4）。列を始めないバイト（継続・0xC0 0xC1・0xF5 以上）は 0。
[[nodiscard]] std::size_t sequence_length(unsigned char lead) noexcept
{
    constexpr unsigned char ascii_end = 0x80;
    constexpr unsigned char two_first = 0xC2;
    constexpr unsigned char three_first = 0xE0;
    constexpr unsigned char four_first = 0xF0;
    constexpr unsigned char four_last = 0xF4;
    if (lead < ascii_end)
    {
        return 1;
    }
    if (lead < two_first)
    {
        return 0;
    }
    if (lead < three_first)
    {
        return 2;
    }
    if (lead < four_first)
    {
        return 3;
    }
    return lead <= four_last ? 4 : 0;
}

// at から始まる 1 つの code point の長さ。正しい列でなければ 0（呼び手が 1 バイトを置き換える）。
[[nodiscard]] std::size_t valid_sequence(std::string_view raw, std::size_t at) noexcept
{
    const std::size_t length = sequence_length(byte_at(raw, at));
    if (length == 0 || raw.size() - at < length)
    {
        return 0;
    }
    return validate_utf8(raw.substr(at, length)).has_value() ? length : 0;
}

// CSI（ESC [ 引数と中間のバイト 終端のバイト）の長さ。終端が無ければ引数の終わりまで。
[[nodiscard]] std::size_t csi_length(std::string_view raw, std::size_t at) noexcept
{
    std::size_t end = at + 2;
    while (end < raw.size() && within(byte_at(raw, end), csi_parameter_first, csi_parameter_last))
    {
        ++end;
    }
    if (end < raw.size() && within(byte_at(raw, end), csi_final_first, csi_final_last))
    {
        ++end;
    }
    return end - at;
}

// OSC（ESC ] … BEL か ESC \）の長さ。終わりが無ければ行の終わりまで。
[[nodiscard]] std::size_t osc_length(std::string_view raw, std::size_t at) noexcept
{
    for (std::size_t end = at + 2; end < raw.size(); ++end)
    {
        if (byte_at(raw, end) == bell_character)
        {
            return end + 1 - at;
        }
        if (byte_at(raw, end) == escape_character && end + 1 < raw.size() &&
            byte_at(raw, end + 1) == string_terminator)
        {
            return end + 2 - at;
        }
    }
    return raw.size() - at;
}

// ESC と中間の字の列と終わりの字（ECMA-48 の nF・Fp・Fe・Fs）の長さ。中間の字の後ろに終わりの字が
// 無ければ中間の字の終わりまで（終わりの無い CSI と同じ）。中間の字も終わりの字も無ければ 0。
[[nodiscard]] std::size_t plain_escape_length(std::string_view raw, std::size_t at) noexcept
{
    std::size_t end = at + 1;
    while (end < raw.size() &&
           within(byte_at(raw, end), escape_intermediate_first, escape_intermediate_last))
    {
        ++end;
    }
    if (end < raw.size() && within(byte_at(raw, end), escape_final_first, csi_final_last))
    {
        ++end;
    }
    return end - at - 1 == 0 ? 0 : end - at;
}

// at の ESC が始める ANSI のエスケープ列の長さ（CSI・OSC・ESC
// と中間の字と終わりの字）。列でなければ 0（ESC は制御文字として見える形にする）。
[[nodiscard]] std::size_t escape_length(std::string_view raw, std::size_t at) noexcept
{
    if (at + 1 >= raw.size())
    {
        return 0;
    }
    const unsigned char kind = byte_at(raw, at + 1);
    if (kind == csi_introducer)
    {
        return csi_length(raw, at);
    }
    if (kind == osc_introducer)
    {
        return osc_length(raw, at);
    }
    return plain_escape_length(raw, at);
}

[[nodiscard]] bool control(unsigned char byte) noexcept
{
    return (byte < first_printable && byte != tab_character) || byte == delete_character;
}

// 正しい列 sequence が見えない code point（C1・行と段落の区切り・双方向の制御）ならその値、ほかは
// 0。
[[nodiscard]] std::uint32_t hidden_code_point(std::string_view sequence) noexcept
{
    if (sequence.size() == 2 && byte_at(sequence, 0) == c1_lead && byte_at(sequence, 1) <= c1_last)
    {
        return byte_at(sequence, 1);
    }
    if (sequence.size() != 3 || byte_at(sequence, 0) != bidi_lead)
    {
        return 0;
    }
    const unsigned char middle = byte_at(sequence, 1);
    const unsigned char last = byte_at(sequence, 2);
    const bool embedding =
        middle == bidi_embed_middle && within(last, bidi_embed_first, bidi_embed_last);
    const bool isolate =
        middle == bidi_isolate_middle && within(last, bidi_isolate_first, bidi_isolate_last);
    if (!embedding && !isolate)
    {
        return 0;
    }
    return (static_cast<std::uint32_t>(bidi_lead & three_lead_bits) << twelve_bits) |
           (static_cast<std::uint32_t>(middle & continuation_bits) << six_bits) |
           static_cast<std::uint32_t>(last & continuation_bits);
}

// 「<U+202E>」の形。
void append_code_point_mark(std::string &line, std::uint32_t code_point)
{
    std::array<char, hex_digits> digits{};
    for (std::size_t index = 0; index < hex_digits; ++index)
    {
        const unsigned int shift = hex_bits * static_cast<unsigned int>(hex_digits - 1 - index);
        digits.at(index) = hex.at((code_point >> shift) & hex_mask);
    }
    line.append("<U+");
    line.append(digits.data(), digits.size());
    line.append(">");
}

// 正しい列 1 つを見える形で足す。C0 と DEL は制御文字の図形、C1 と行と段落の区切りと双方向の制御は
// 「<U+XXXX>」。
void append_visible(std::string &line, std::string_view sequence)
{
    if (sequence.size() == 1 && control(byte_at(sequence, 0)))
    {
        const unsigned char byte = byte_at(sequence, 0);
        if (byte == delete_character)
        {
            line.append(delete_picture);
            return;
        }
        line.append(control_picture_lead);
        line.push_back(static_cast<char>(control_picture_base + byte));
        return;
    }
    const std::uint32_t hidden = hidden_code_point(sequence);
    if (hidden != 0)
    {
        append_code_point_mark(line, hidden);
        return;
    }
    line.append(sequence);
}

// 置き換えた行（上限で切る前）。
[[nodiscard]] std::string repaired(std::string_view raw)
{
    std::string line;
    line.reserve(raw.size());
    std::size_t at = 0;
    while (at < raw.size())
    {
        const std::size_t escape =
            byte_at(raw, at) == escape_character ? escape_length(raw, at) : 0;
        if (escape > 0)
        {
            at += escape;
            continue;
        }
        const std::size_t length = valid_sequence(raw, at);
        if (length == 0)
        {
            line.append(replacement);
            ++at;
            continue;
        }
        append_visible(line, raw.substr(at, length));
        at += length;
    }
    return line;
}
} // namespace

std::string server_log_line(std::string_view raw)
{
    std::string line = repaired(raw);
    if (line.size() <= server_log_line_bytes)
    {
        return line;
    }
    const std::size_t kept = server_log_line_bytes - ellipsis.size();
    line.resize(whole_code_points(std::string_view(line).substr(0, kept)));
    line.append(ellipsis);
    return line;
}
} // namespace nenenib::core
