// scope `--detected-text-property` の性質の試験（#898・ADR 0010 / 0036 / 0149・#734・ADR 0042）。
// ファイルを開く最初の入口 DetectedText::from_bytes に、固定 seed の決定的な生成器（ランダムな
// バイト列・正しい UTF-8・UTF-8 BOM・Shift_JIS・UTF-16 LE / BE・UTF-32 の BOM・それらの変異・
// 巨大な 1 行）で作った入力を渡し、値か閉じた失敗で返る・値の本文は妥当な UTF-8・改行の形が本文
// と矛盾しない・同じ入力で同じ結果、を確かめる。正しい入力は controller で開いて同じ形で保存し、
// 書いたバイト列が読んだバイト列と同じになる（encoded_parts の往復）ことを確かめる。Shift_JIS の
// 変換は CodePagePort の向こうなので、往復は替え玉 ScriptedCodePages で本文の受け渡しだけを見る。
// BOM の無い列の判定の決め方（UTF-8 が勝つ・ほかは CP932 の形）そのものは不変量にしない。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "CodePageFailure.hpp"
#include "DetectedText.hpp"
#include "Editing.hpp"
#include "EditorFrame.hpp"
#include "EncodingFailure.hpp"
#include "FileFailure.hpp"
#include "LineEnding.hpp"
#include "OpenDocument.hpp"
#include "SaveDocument.hpp"
#include "SaveState.hpp"
#include "Scopes.hpp"
#include "SealedBytes.hpp"
#include "TestSupport.hpp"
#include "TextBuffer.hpp"
#include "TextEncoding.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using application::FileFailure;
using application::OpenDocument;
using application::SaveDocument;
using core::ByteOrder;
using core::DetectedText;
using core::EncodingFailure;
using core::LineEnding;
using core::SealedBytes;
using core::TextBuffer;
using core::TextEncoding;
using Random = std::mt19937_64;
using Detected = std::expected<DetectedText, EncodingFailure>;
using Make = std::string (*)(Random &);
using Check = std::string_view (*)(std::string_view);

constexpr std::size_t rounds = 1U;
constexpr std::size_t family_inputs = 150U * rounds;
constexpr std::size_t round_trip_inputs = 12U * rounds;
constexpr std::size_t giant_inputs = 2U * rounds;
constexpr std::size_t short_input_limit = 384U;
constexpr std::size_t code_point_limit = 96U;
constexpr std::size_t giant_line_bytes = std::size_t{128U} * 1024U;
// 替え玉の CP932 が返す本文（往復で同じ本文が符号化へ渡るかを見る）。
constexpr std::string_view scripted_japanese = "日本語\r\nの本文";

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] char byte_of(std::size_t value)
{
    return static_cast<char>(static_cast<unsigned char>(value));
}

[[nodiscard]] char raw_byte(Random &random)
{
    return byte_of(below(random, 256U));
}

[[nodiscard]] char32_t within(Random &random, char32_t first, char32_t last)
{
    return first +
           static_cast<char32_t>(below(random, static_cast<std::size_t>(last - first) + 1U));
}

// --- 入力の族 ---

// 結合文字・ZWJ・異体字セレクタ・絵文字・制御文字（NUL・CR だけ・C1 の NEL）。
constexpr std::array<char32_t, 10> special_code_points{
    {0x0301U, 0x200DU, 0xFE0FU, 0x1F600U, 0x1F1EFU, 0x0000U, 0x0009U, 0x000DU, 0x0085U, 0xFEFFU}};

// スカラー値の範囲（サロゲートは含めない）。1〜4 バイトの UTF-8・改行・かな・C0 制御文字。
constexpr std::array<std::pair<char32_t, char32_t>, 8> scalar_ranges{{{0x20U, 0x7EU},
                                                                      {0x0AU, 0x0AU},
                                                                      {0x80U, 0x7FFU},
                                                                      {0x800U, 0xD7FFU},
                                                                      {0xE000U, 0xFFFDU},
                                                                      {0x10000U, 0x10FFFFU},
                                                                      {0x3041U, 0x3096U},
                                                                      {0x00U, 0x1FU}}};

// スカラー値を 1 つ選ぶ。範囲の 1 つから、ときどき特別な文字から。
[[nodiscard]] char32_t scalar(Random &random)
{
    const std::size_t kind = below(random, scalar_ranges.size() + 1U);
    if (kind == scalar_ranges.size())
    {
        return special_code_points.at(below(random, special_code_points.size()));
    }
    const auto [first, last] = scalar_ranges.at(kind);
    return within(random, first, last);
}

[[nodiscard]] std::u32string scalars(Random &random)
{
    const std::size_t length = below(random, code_point_limit);
    std::u32string text;
    for (std::size_t index = 0; index < length; ++index)
    {
        const char32_t chosen = scalar(random);
        // CRLF の文書も作る（改行の直前に CR を置く）。
        if (chosen == U'\n' && below(random, 2U) == 0U)
        {
            text.push_back(U'\r');
        }
        text.push_back(chosen);
    }
    return text;
}

// 製品の写しを使わずに手で符号化する（往復の期待値を製品から独立に作る）。
void append_utf8(std::string &out, char32_t code_point)
{
    const auto value = static_cast<std::uint32_t>(code_point);
    if (value < 0x80U)
    {
        out.push_back(byte_of(value));
        return;
    }
    if (value < 0x800U)
    {
        out.push_back(byte_of(0xC0U | (value >> 6U)));
        out.push_back(byte_of(0x80U | (value & 0x3FU)));
        return;
    }
    if (value < 0x10000U)
    {
        out.push_back(byte_of(0xE0U | (value >> 12U)));
        out.push_back(byte_of(0x80U | ((value >> 6U) & 0x3FU)));
        out.push_back(byte_of(0x80U | (value & 0x3FU)));
        return;
    }
    out.push_back(byte_of(0xF0U | (value >> 18U)));
    out.push_back(byte_of(0x80U | ((value >> 12U) & 0x3FU)));
    out.push_back(byte_of(0x80U | ((value >> 6U) & 0x3FU)));
    out.push_back(byte_of(0x80U | (value & 0x3FU)));
}

void append_unit(std::string &out, std::uint32_t unit, ByteOrder order)
{
    const char high = byte_of(unit >> 8U);
    const char low = byte_of(unit & 0xFFU);
    out.push_back(order == ByteOrder::little ? low : high);
    out.push_back(order == ByteOrder::little ? high : low);
}

void append_utf16(std::string &out, char32_t code_point, ByteOrder order)
{
    const auto value = static_cast<std::uint32_t>(code_point);
    if (value < 0x10000U)
    {
        append_unit(out, value, order);
        return;
    }
    const std::uint32_t offset = value - 0x10000U;
    append_unit(out, 0xD800U | (offset >> 10U), order);
    append_unit(out, 0xDC00U | (offset & 0x3FFU), order);
}

[[nodiscard]] std::string utf8_of(const std::u32string &text)
{
    std::string out;
    for (const char32_t code_point : text)
    {
        append_utf8(out, code_point);
    }
    return out;
}

[[nodiscard]] std::string utf16_of(const std::u32string &text, ByteOrder order)
{
    std::string out(order == ByteOrder::little ? core::utf16_le_byte_order_mark()
                                               : core::utf16_be_byte_order_mark());
    for (const char32_t code_point : text)
    {
        append_utf16(out, code_point, order);
    }
    return out;
}

std::string utf8_sample(Random &random)
{
    return utf8_of(scalars(random));
}

std::string bom_sample(Random &random)
{
    return std::string(core::byte_order_mark()) + utf8_of(scalars(random));
}

std::string utf16_le_sample(Random &random)
{
    return utf16_of(scalars(random), ByteOrder::little);
}

std::string utf16_be_sample(Random &random)
{
    return utf16_of(scalars(random), ByteOrder::big);
}

// JIS X 0208 の 2 バイトの範囲（先頭 0x81〜0x9F / 0xE0〜0xEF・2 バイト目 0x40〜0x7E /
// 0x80〜0xFC）。
void append_cp932_pair(std::string &out, Random &random, bool low_lead)
{
    const std::size_t lead = low_lead || below(random, 2U) == 0U ? 0x81U + below(random, 0x1FU)
                                                                 : 0xE0U + below(random, 0x10U);
    const std::size_t trail =
        below(random, 2U) == 0U ? 0x40U + below(random, 0x3FU) : 0x80U + below(random, 0x7DU);
    out.push_back(byte_of(lead));
    out.push_back(byte_of(trail));
}

void append_cp932_unit(std::string &out, Random &random)
{
    const std::size_t kind = below(random, 4U);
    if (kind == 0U)
    {
        out.push_back(byte_of(0x20U + below(random, 0x5FU)));
        return;
    }
    if (kind == 1U)
    {
        out.append(below(random, 2U) == 0U ? "\r\n" : "\n");
        return;
    }
    if (kind == 2U)
    {
        out.push_back(byte_of(0xA1U + below(random, 0x3FU)));
        return;
    }
    append_cp932_pair(out, random, false);
}

// 正しい CP932 の形。先頭の 2 バイト文字の先頭バイトを 0x81〜0x9F にして、UTF-8 として
// 正しくなる偶然を除く（そのバイトは UTF-8 の続きのバイトで、列の先頭に来られない）。
std::string sjis_sample(Random &random)
{
    std::string out;
    const std::size_t ascii = below(random, 8U);
    for (std::size_t index = 0; index < ascii; ++index)
    {
        out.push_back(byte_of(0x20U + below(random, 0x5FU)));
    }
    append_cp932_pair(out, random, true);
    const std::size_t units = below(random, code_point_limit);
    for (std::size_t index = 0; index < units; ++index)
    {
        append_cp932_unit(out, random);
    }
    return out;
}

std::string random_sample(Random &random)
{
    const std::size_t length = below(random, short_input_limit);
    std::string out;
    for (std::size_t index = 0; index < length; ++index)
    {
        out.push_back(raw_byte(random));
    }
    return out;
}

std::string utf32_sample(Random &random)
{
    const std::string_view mark = below(random, 2U) == 0U ? core::utf32_le_byte_order_mark()
                                                          : core::utf32_be_byte_order_mark();
    return std::string(mark) + random_sample(random);
}

constexpr std::array<Make, 7> samples{{utf8_sample, bom_sample, sjis_sample, utf16_le_sample,
                                       utf16_be_sample, utf32_sample, random_sample}};

[[nodiscard]] std::string bom_fragment(Random &random)
{
    constexpr std::array<std::string_view, 5> marks{
        {core::byte_order_mark(), core::utf16_le_byte_order_mark(),
         core::utf16_be_byte_order_mark(), core::utf32_le_byte_order_mark(),
         core::utf32_be_byte_order_mark()}};
    const std::string_view mark = marks.at(below(random, marks.size()));
    return std::string(mark.substr(0U, 1U + below(random, mark.size())));
}

// 変異 1 回: 切り詰め・1 バイトの置換・NUL の挿入・BOM の切れ端を前に・先頭の 1 バイトを消す。
void mutate(std::string &input, Random &random)
{
    const std::size_t at = below(random, input.size() + 1U);
    const std::size_t kind = below(random, 5U);
    if (kind == 0U)
    {
        input.resize(at);
        return;
    }
    if (kind == 1U && at < input.size())
    {
        input.at(at) = raw_byte(random);
        return;
    }
    if (kind == 2U)
    {
        input.insert(at, 1U, '\0');
        return;
    }
    if (kind == 3U)
    {
        input.insert(0U, bom_fragment(random));
        return;
    }
    input.erase(0U, 1U);
}

std::string mutated_sample(Random &random)
{
    std::string input = samples.at(below(random, samples.size()))(random);
    const std::size_t times = 1U + below(random, 3U);
    for (std::size_t index = 0; index < times; ++index)
    {
        mutate(input, random);
    }
    return input;
}

// 巨大な 1 行（改行の無い 128 KiB 前後）。UTF-8・UTF-16 LE・CP932 の 3 つの形。
std::string giant_sample(Random &random)
{
    std::u32string line;
    std::string cp932(1U, '\x93');
    cp932.push_back('\xFA');
    while (line.size() * 3U < giant_line_bytes)
    {
        const char32_t chosen = scalar(random);
        line.push_back(chosen == U'\n' ? U'a' : chosen);
        append_cp932_pair(cp932, random, false);
    }
    const std::size_t kind = below(random, 3U);
    if (kind == 0U)
    {
        return utf8_of(line);
    }
    return kind == 1U ? utf16_of(line, ByteOrder::little) : cp932;
}

// --- 不変量 ---

[[nodiscard]] bool starts_utf32(std::string_view input)
{
    return input.starts_with(core::utf32_le_byte_order_mark()) ||
           input.starts_with(core::utf32_be_byte_order_mark());
}

[[nodiscard]] bool starts_utf16(std::string_view input)
{
    return input.starts_with(core::utf16_le_byte_order_mark()) ||
           input.starts_with(core::utf16_be_byte_order_mark());
}

[[nodiscard]] bool valid_utf8(std::string_view text)
{
    return core::validate_utf8(text).has_value();
}

[[nodiscard]] bool same_result(const Detected &left, const Detected &right)
{
    if (left.has_value() != right.has_value())
    {
        return false;
    }
    if (!left.has_value())
    {
        return left.error() == right.error();
    }
    return left.value().encoding() == right.value().encoding() &&
           left.value().bytes() == right.value().bytes();
}

[[nodiscard]] SealedBytes sealed_of(std::string_view source)
{
    return SealedBytes::filled(source.size(),
                               [source](std::span<char> target) noexcept
                               {
                                   std::copy_n(source.data(), target.size(), target.data());
                                   return target.size();
                               });
}

// 開けない列は UTF-32 の BOM でも、BOM の後ろの正しい UTF-8 でもない。
[[nodiscard]] std::string_view undecodable_violation(std::string_view input)
{
    if (starts_utf32(input))
    {
        return "a UTF-32 BOM was not named as UTF-32";
    }
    return !starts_utf16(input) && valid_utf8(core::without_byte_order_mark(input))
               ? "valid UTF-8 was refused"
               : "";
}

// 閉じた失敗は入力の形と矛盾しない。
[[nodiscard]] std::string_view failure_violation(std::string_view input, EncodingFailure failure)
{
    switch (failure)
    {
    case EncodingFailure::unsupported_utf32:
        return starts_utf32(input) ? "" : "unsupported_utf32 without a UTF-32 BOM";
    case EncodingFailure::odd_utf16_length:
        return !starts_utf32(input) && starts_utf16(input) && input.size() % 2U != 0U
                   ? ""
                   : "odd_utf16_length without a UTF-16 BOM and an odd body";
    case EncodingFailure::undecodable:
        return undecodable_violation(input);
    }
    return "an unknown failure";
}

[[nodiscard]] std::string_view utf16_violation(std::string_view input, std::string_view body,
                                               std::string_view mark, ByteOrder order)
{
    if (!input.starts_with(mark) || starts_utf32(input))
    {
        return "UTF-16 was named without its BOM";
    }
    if (!valid_utf8(body))
    {
        return "the UTF-16 body is not valid UTF-8";
    }
    const auto back = core::utf16_bytes_of_utf8(body, order);
    return back.has_value() && back.value() == input.substr(mark.size())
               ? ""
               : "the UTF-16 body does not encode back to the bytes read";
}

// 判定した文字コードと持っている列が、読んだ列と矛盾しない。
[[nodiscard]] std::string_view encoding_violation(std::string_view input, const DetectedText &text)
{
    const std::string_view bytes = text.bytes();
    switch (text.encoding())
    {
    case TextEncoding::utf8:
        return bytes == input && valid_utf8(bytes) && !input.starts_with(core::byte_order_mark())
                   ? ""
                   : "UTF-8 is not the valid bytes read without a BOM";
    case TextEncoding::utf8_bom:
        return bytes == input && input.starts_with(core::byte_order_mark()) &&
                       valid_utf8(core::without_byte_order_mark(bytes))
                   ? ""
                   : "UTF-8 BOM is not the bytes read behind a BOM";
    case TextEncoding::shift_jis:
        return bytes == input && !valid_utf8(input) && !starts_utf16(input) &&
                       !starts_utf32(input) && !input.starts_with(core::byte_order_mark())
                   ? ""
                   : "Shift_JIS is not the unmarked bytes read that are not UTF-8";
    case TextEncoding::utf16_le:
        return utf16_violation(input, bytes, core::utf16_le_byte_order_mark(), ByteOrder::little);
    case TextEncoding::utf16_be:
        return utf16_violation(input, bytes, core::utf16_be_byte_order_mark(), ByteOrder::big);
    }
    return "an unknown encoding";
}

// 改行の形の判定（ADR 0010 の決定 4）が本文と矛盾しない。
[[nodiscard]] bool ending_agrees(std::string_view body, LineEnding ending)
{
    const std::size_t first = body.find('\n');
    const bool crlf_first =
        first != std::string_view::npos && first > 0U && body[first - 1U] == '\r';
    switch (ending)
    {
    case LineEnding::crlf:
        return first == std::string_view::npos || crlf_first;
    case LineEnding::lf:
        return first != std::string_view::npos && !crlf_first;
    }
    return false;
}

// 本文へ渡すと、BOM を除いた列がそのまま本文になる。CP932 は本文へ直には渡らない。
[[nodiscard]] std::string_view buffer_violation(std::string_view input)
{
    auto detected = DetectedText::from_bytes(input);
    const TextEncoding encoding = detected.value().encoding();
    const std::string body(encoding == TextEncoding::utf8_bom
                               ? core::without_byte_order_mark(detected.value().bytes())
                               : detected.value().bytes());
    const auto buffer = TextBuffer::from_utf8(std::move(detected).value());
    if (encoding == TextEncoding::shift_jis)
    {
        return buffer.has_value() ? "CP932 bytes became a body without the code page" : "";
    }
    if (!buffer.has_value() || buffer.value().text() != body)
    {
        return "the body is not the detected text";
    }
    return ending_agrees(body, buffer.value().line_ending())
               ? ""
               : "the line ending contradicts the body";
}

// どの入力でも守ること。破れた性質の名前で、守られていれば空。
std::string_view general_violation(std::string_view input)
{
    const Detected first = DetectedText::from_bytes(input);
    if (!same_result(first, DetectedText::from_bytes(input)))
    {
        return "the same bytes gave a different result";
    }
    if (!same_result(first, DetectedText::from_sealed(sealed_of(input))))
    {
        return "from_sealed differs from from_bytes";
    }
    if (!first.has_value())
    {
        return failure_violation(input, first.error());
    }
    const std::string_view named = encoding_violation(input, first.value());
    return named.empty() ? buffer_violation(input) : named;
}

[[nodiscard]] std::string_view named_violation(std::string_view input, TextEncoding expected)
{
    const std::string_view general = general_violation(input);
    if (!general.empty())
    {
        return general;
    }
    const Detected detected = DetectedText::from_bytes(input);
    return detected.has_value() && detected.value().encoding() == expected
               ? ""
               : "a valid sample was not named as its encoding";
}

std::string_view utf8_violation(std::string_view input)
{
    // U+FEFF で始まる UTF-8 は BOM 付きと読む（ラベルの規則・決定 3）。
    return named_violation(input, input.starts_with(core::byte_order_mark())
                                      ? TextEncoding::utf8_bom
                                      : TextEncoding::utf8);
}

std::string_view bom_violation(std::string_view input)
{
    return named_violation(input, TextEncoding::utf8_bom);
}

std::string_view sjis_violation(std::string_view input)
{
    return named_violation(input, TextEncoding::shift_jis);
}

std::string_view utf16_be_violation(std::string_view input)
{
    return named_violation(input, TextEncoding::utf16_be);
}

std::string_view utf32_violation_of(std::string_view input)
{
    const Detected detected = DetectedText::from_bytes(input);
    return !detected.has_value() && detected.error() == EncodingFailure::unsupported_utf32
               ? ""
               : "a UTF-32 BOM was not refused as UTF-32";
}

// 先頭の文字が NUL の UTF-16 LE は FF FE 00 00 で始まり、UTF-32 LE の BOM として断る（#734）。
std::string_view utf16_le_violation(std::string_view input)
{
    if (!input.starts_with(core::utf32_le_byte_order_mark()))
    {
        return named_violation(input, TextEncoding::utf16_le);
    }
    const std::string_view general = general_violation(input);
    return !general.empty() ? general : utf32_violation_of(input);
}

std::string_view utf32_violation(std::string_view input)
{
    const std::string_view general = general_violation(input);
    return !general.empty() ? general : utf32_violation_of(input);
}

// 開いて同じ形で保存すると、書いたバイト列が読んだバイト列と同じ（encoded_parts の往復）。
// CP932 は替え玉の答えなので、本文が変換へそのまま渡ることだけを見る。
std::string_view round_trip_violation(std::string_view input)
{
    // 先頭の文字が NUL の UTF-16 LE は UTF-32 として断る（utf16_le_violation が見る・#734）。
    if (starts_utf32(input))
    {
        return "";
    }
    const Detected detected = DetectedText::from_bytes(input);
    if (!detected.has_value())
    {
        return "a valid sample was refused";
    }
    const TextEncoding encoding = detected.value().encoding();
    Editing editing;
    editing.files().hold(std::string(input));
    editing.code_pages().decode_to(std::string(scripted_japanese));
    editing.code_pages().encode_to(std::string(input));
    auto &controller = editing.controller();
    const auto opened = controller.apply_frame(OpenDocument{sample_path()});
    if (opened.document.encoding != encoding || opened.document.last_failure.has_value())
    {
        return "the opened document lost its encoding";
    }
    const auto saved = controller.apply_frame(SaveDocument{sample_path(), encoding});
    if (saved.document.save_state != core::SaveState::saved || editing.files().written() != input)
    {
        return "the written bytes differ from the bytes read";
    }
    return encoding != TextEncoding::shift_jis ||
                   editing.code_pages().encoded_from() == scripted_japanese
               ? ""
               : "the CP932 body did not reach the code page unchanged";
}

// --- 走らせる ---

// 破れた入力を 16 進で出す（160 バイトまで）。
void print_bytes(std::string_view input)
{
    const std::size_t shown = std::min<std::size_t>(input.size(), 160U);
    for (std::size_t index = 0; index < shown; ++index)
    {
        std::fprintf(stderr, " %02x",
                     static_cast<unsigned>(static_cast<unsigned char>(input[index])));
    }
    std::fprintf(stderr, "\n");
}

// 大きさ chunk の 1 か所を消して、同じ理由で破れたままなら残す。
[[nodiscard]] bool shrunk(std::string &input, std::size_t chunk, Check check,
                          std::string_view reason)
{
    for (std::size_t at = 0; at + chunk <= input.size(); at += chunk)
    {
        std::string candidate = input;
        candidate.erase(at, chunk);
        if (check(candidate) == reason)
        {
            input = std::move(candidate);
            return true;
        }
    }
    return false;
}

// 同じ理由で破れる小さな入力へ貪欲に縮める（回帰 case の手がかり）。
[[nodiscard]] std::string minimized(std::string input, Check check)
{
    const std::string_view reason = check(input);
    std::size_t chunk = std::max<std::size_t>(input.size() / 2U, 1U);
    while (chunk > 0U && !input.empty())
    {
        chunk = shrunk(input, chunk, check, reason) ? std::min(chunk, input.size()) : chunk / 2U;
    }
    return input;
}

void report(const char *family, const std::string &input, Check check)
{
    const std::string_view reason = check(input);
    const std::string smallest = minimized(input, check);
    std::fprintf(stderr, "  %s: %.*s (%zu bytes, minimized to %zu):", family,
                 static_cast<int>(reason.size()), reason.data(), input.size(), smallest.size());
    print_bytes(smallest);
}

// 生成した入力のうち性質を破ったものの数。最初の 1 つは縮めて 16 進で出す。
[[nodiscard]] std::size_t broken_inputs(const char *description, std::uint64_t seed, Make make,
                                        Check check)
{
    Random random{seed};
    const std::size_t count = make == giant_sample            ? giant_inputs
                              : check == round_trip_violation ? round_trip_inputs
                                                              : family_inputs;
    std::size_t broken = 0;
    for (std::size_t index = 0; index < count; ++index)
    {
        const std::string input = make(random);
        if (!check(input).empty() && broken++ == 0U)
        {
            report(description, input, check);
        }
    }
    return broken;
}

void verify_family(const char *description, std::uint64_t seed, Make make, Check check)
{
    expect(broken_inputs(description, seed, make, check) == 0U, description);
}

// 見本の境目（BOM の切れ端・サロゲートの片割れ・2 バイト目の欠け・UTF-32 の BOM）。
void verify_fixed_edges()
{
    constexpr std::array<std::string_view, 12> edges{
        {std::string_view{}, "\xEF", "\xEF\xBB", "\xFF", "\xFE",
         std::string_view{"\xFF\xFE\x00", 3}, std::string_view{"\x00\x00\xFE", 3},
         "\xFF\xFE\x3D\xD8", "\xFE\xFF\xDE\x00", "\x93", "a\x93", std::string_view{"\x93\x00", 2}}};
    bool held = true;
    for (const std::string_view edge : edges)
    {
        held = held && general_violation(edge).empty();
    }
    expect(held, "the fixed edge samples keep every property");
    expect(DetectedText::from_bytes(std::string_view{"\xFF\xFE\x00", 3}).error() ==
               EncodingFailure::odd_utf16_length,
           "a cut UTF-32 LE BOM is an odd UTF-16 LE body");
    expect(DetectedText::from_bytes("\xFF\xFE\x3D\xD8").error() == EncodingFailure::undecodable,
           "a lone high surrogate behind a UTF-16 BOM is undecodable");
    // 50 倍の走りで見つかった形（製品の破れではない）: 先頭の文字が NUL の UTF-16 LE は
    // FF FE 00 00 で始まり、UTF-32 LE の BOM として断る（#734 の決め方のまま）。
    expect(DetectedText::from_bytes(std::string_view{"\xFF\xFE\x00\x00\x61\x00", 6}).error() ==
               EncodingFailure::unsupported_utf32,
           "UTF-16 LE whose first character is NUL reads as a UTF-32 LE BOM");
}

// CP932 にできない本文は書かずに閉じた失敗で返る（ADR 0010 の決定 2 の範囲）。
void verify_unencodable_cp932()
{
    Editing editing;
    editing.files().hold(std::string("\x93\xFA\x96\x7B"));
    editing.code_pages().decode_to(std::string(scripted_japanese));
    editing.code_pages().encode_to(std::unexpected(application::CodePageFailure::unencodable));
    auto &controller = editing.controller();
    static_cast<void>(controller.apply_frame(OpenDocument{sample_path()}));
    const auto saved = controller.apply_frame(SaveDocument{sample_path(), TextEncoding::shift_jis});
    expect(saved.document.last_failure.has_value() &&
               saved.document.last_failure.value() == FileFailure::unencodable &&
               editing.files().written().empty(),
           "a CP932 body the code page cannot hold is unencodable and nothing is written");
}

// 生成器そのものが決定的（同じ seed で同じ列）。
void verify_generators_are_deterministic()
{
    Random left{8981U};
    Random right{8981U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && mutated_sample(left) == mutated_sample(right) &&
               sjis_sample(left) == sjis_sample(right);
    }
    expect(same, "the generators give the same inputs for the same seed");
}

void verify_detection_properties()
{
    verify_family("from_bytes holds on random bytes", 8901U, random_sample, general_violation);
    verify_family("valid UTF-8 is UTF-8", 8902U, utf8_sample, utf8_violation);
    verify_family("UTF-8 behind a BOM is UTF-8 BOM", 8903U, bom_sample, bom_violation);
    verify_family("JIS X 0208 pairs are Shift_JIS", 8904U, sjis_sample, sjis_violation);
    verify_family("UTF-16 LE with surrogate pairs is UTF-16 LE", 8905U, utf16_le_sample,
                  utf16_le_violation);
    verify_family("UTF-16 BE with surrogate pairs is UTF-16 BE", 8906U, utf16_be_sample,
                  utf16_be_violation);
    verify_family("a UTF-32 BOM is refused as UTF-32", 8907U, utf32_sample, utf32_violation);
    verify_family("from_bytes holds on mutated samples", 8908U, mutated_sample, general_violation);
    verify_family("from_bytes holds on a giant line", 8909U, giant_sample, general_violation);
}

void verify_round_trips()
{
    verify_family("UTF-8 opens and saves back to the same bytes", 8911U, utf8_sample,
                  round_trip_violation);
    verify_family("UTF-8 BOM opens and saves back to the same bytes", 8912U, bom_sample,
                  round_trip_violation);
    verify_family("UTF-16 LE opens and saves back to the same bytes", 8913U, utf16_le_sample,
                  round_trip_violation);
    verify_family("UTF-16 BE opens and saves back to the same bytes", 8914U, utf16_be_sample,
                  round_trip_violation);
    verify_family("Shift_JIS hands the body to the code page both ways", 8915U, sjis_sample,
                  round_trip_violation);
    verify_unencodable_cp932();
}
} // namespace

void verify_detected_text_property_scope()
{
    verify_generators_are_deterministic();
    verify_fixed_edges();
    verify_detection_properties();
    verify_round_trips();
}
} // namespace nenenib::tests
