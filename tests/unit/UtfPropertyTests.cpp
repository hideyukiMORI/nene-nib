// scope `--utf-property` の性質の試験（#1289・ARC-001 / CPP-014・ADR 0042）。UTF-8 の検証と走査
// （Utf8.hpp）と UTF-8 ↔ UTF-16 の変換（Utf16.hpp）に、遅くてよい素朴な模型を当てて次を確かめる。
//   1. 代理の範囲を除く 0〜0x10FFFF の全 code point で、append_utf8 が模型の符号化と同じバイト列を
//      作り、全部を並べた本文の validate_utf8 / code_point_count / lenient_code_point_count が
//      code point の数を返し、next_code_point / previous_code_point / is_boundary / code_point_at
//      が 模型の境目と値をたどり、to_utf16 / utf16_length / to_utf8 / UTF-16 のバイト列（LE と
//      BE）が 模型の UTF-16 と往復する。
//   2. 固定 seed の乱れたバイト列（正しい列・孤立した代理・過長・範囲外・切れた末尾・孤立した継続
//      バイト・F5 以降）で、validate_utf8 の受理と数が Unicode の表 3-7 の模型と一致し、どこで
//      切って Utf8Validation に渡しても一括の検証と同じ答えになる。
//   3. 同じ乱れたバイト列の全位置で、next_code_point は「後ろで最初の継続バイトでない位置か末尾」、
//      previous_code_point は「前で最後の継続バイトでない位置か先頭」で、端以外では必ず動く。
//      lenient_code_point_count は区間を分けて数えた和が全体と同じで、正しい UTF-8 では
//      code_point_count と同じ。
//   4. 固定 seed の UTF-16 の列（孤立した上位・下位の代理・正しい対）で、to_utf8 と
//      valid_utf16_bytes と utf8_of_utf16_bytes の受理が模型と一致し、受理した値は往復する。
// 既定実行（契約 verify_utf_property_contracts）は 1 の族を境の前後 32 と 61 刻みの値に間引き、
// 全 code point は selector の `--utf-property` が通す（Debug で約 5 秒・既定実行の 1
// 秒に収まらない）。 ほかの族の回数は Debug（ASan）で 1 秒に収まる値。生成器は std::mt19937_64
// の剰余だけを使い、 分布の型・時刻・環境変数は使わない。
#include "ByteOrder.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextFailure.hpp"
#include "Utf16.hpp"
#include "Utf8.hpp"
#include "Utf8Validation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::ByteOrder;
using core::Offset;
using core::TextFailure;
using Random = std::mt19937_64;

constexpr std::size_t rounds = 1U;
constexpr std::size_t byte_inputs = 1500U * rounds;
constexpr std::size_t unit_inputs = 1500U * rounds;
constexpr std::size_t maximum_pieces = 24U;
constexpr char32_t last_code_point = 0x10FFFF;
constexpr char32_t first_surrogate = 0xD800;
constexpr char32_t last_surrogate = 0xDFFF;

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

[[nodiscard]] bool surrogate(char32_t value) noexcept
{
    return value >= first_surrogate && value <= last_surrogate;
}

[[nodiscard]] unsigned char byte_of(std::string_view text, std::size_t index) noexcept
{
    return static_cast<unsigned char>(text[index]);
}

[[nodiscard]] bool continuation(unsigned char byte) noexcept
{
    return byte >= 0x80U && byte <= 0xBFU;
}

// --- 模型（製品の走査とは別の書き方で、遅くてよい）---

// 値の範囲ごとにバイトを直に並べる UTF-8 の符号化（全 code point で 1 回ずつ呼ぶので一時の文字列を
// 作らずに末尾へ足す）。
void append_model_utf8(std::string &text, char32_t value)
{
    const auto at = [value](unsigned shift, unsigned mask, unsigned marker)
    { return static_cast<char>(marker | ((static_cast<unsigned>(value) >> shift) & mask)); };
    if (value <= 0x7FU)
    {
        text.push_back(static_cast<char>(value));
        return;
    }
    if (value <= 0x7FFU)
    {
        text.push_back(at(6U, 0x1FU, 0xC0U));
        text.push_back(at(0U, 0x3FU, 0x80U));
        return;
    }
    if (value <= 0xFFFFU)
    {
        text.push_back(at(12U, 0x0FU, 0xE0U));
        text.push_back(at(6U, 0x3FU, 0x80U));
        text.push_back(at(0U, 0x3FU, 0x80U));
        return;
    }
    text.push_back(at(18U, 0x07U, 0xF0U));
    text.push_back(at(12U, 0x3FU, 0x80U));
    text.push_back(at(6U, 0x3FU, 0x80U));
    text.push_back(at(0U, 0x3FU, 0x80U));
}

void append_model_utf16(std::wstring &text, char32_t value)
{
    if (value <= 0xFFFFU)
    {
        text.push_back(static_cast<wchar_t>(value));
        return;
    }
    const char32_t rest = value - 0x10000U;
    text.push_back(static_cast<wchar_t>(0xD800U + (rest >> 10U)));
    text.push_back(static_cast<wchar_t>(0xDC00U + (rest & 0x3FFU)));
}

// Unicode の表 3-7（整形式の UTF-8 のバイト列）の行。先頭バイトの最初と最後・2 バイト目の最初と
// 最後・長さ（CPP-011 で型を増やさず配列で持つ）。
using WellFormedRow = std::array<unsigned, 5>;

constexpr std::array<WellFormedRow, 9> well_formed{{
    {0x00U, 0x7FU, 0x00U, 0x00U, 1U},
    {0xC2U, 0xDFU, 0x80U, 0xBFU, 2U},
    {0xE0U, 0xE0U, 0xA0U, 0xBFU, 3U},
    {0xE1U, 0xECU, 0x80U, 0xBFU, 3U},
    {0xEDU, 0xEDU, 0x80U, 0x9FU, 3U},
    {0xEEU, 0xEFU, 0x80U, 0xBFU, 3U},
    {0xF0U, 0xF0U, 0x90U, 0xBFU, 4U},
    {0xF1U, 0xF3U, 0x80U, 0xBFU, 4U},
    {0xF4U, 0xF4U, 0x80U, 0x8FU, 4U},
}};

// 表の行 row に当たる先頭バイトから at で始まる列が、その行の形に収まるか。
[[nodiscard]] bool fits_row(std::string_view text, std::size_t at, const WellFormedRow &row)
{
    const std::size_t length = row[4];
    if (at + length > text.size())
    {
        return false;
    }
    bool fits =
        length == 1U || (byte_of(text, at + 1U) >= row[2] && byte_of(text, at + 1U) <= row[3]);
    for (std::size_t offset = 2U; offset < length; ++offset)
    {
        fits = fits && continuation(byte_of(text, at + offset));
    }
    return fits;
}

// at から始まる整形式の列の長さ。整形式でなければ値なし。
[[nodiscard]] std::optional<std::size_t> model_sequence(std::string_view text, std::size_t at)
{
    const unsigned lead = byte_of(text, at);
    const auto row = std::ranges::find_if(well_formed, [lead](const WellFormedRow &candidate)
                                          { return lead >= candidate[0] && lead <= candidate[1]; });
    if (row == well_formed.end() || !fits_row(text, at, *row))
    {
        return std::nullopt;
    }
    return std::size_t{(*row)[4]};
}

// 整形式なら code point の数、そうでなければ値なし。
[[nodiscard]] std::optional<std::size_t> model_count(std::string_view text)
{
    std::size_t count = 0;
    for (std::size_t at = 0; at < text.size(); ++count)
    {
        const auto length = model_sequence(text, at);
        if (!length.has_value())
        {
            return std::nullopt;
        }
        at += length.value();
    }
    return count;
}

[[nodiscard]] std::size_t model_next(std::string_view text, std::size_t at)
{
    if (at >= text.size())
    {
        return text.size();
    }
    std::size_t index = at + 1U;
    while (index < text.size() && continuation(byte_of(text, index)))
    {
        ++index;
    }
    return index;
}

[[nodiscard]] std::size_t model_previous(std::string_view text, std::size_t at)
{
    std::size_t index = at > text.size() ? text.size() : at;
    while (index > 0U)
    {
        --index;
        if (!continuation(byte_of(text, index)))
        {
            return index;
        }
    }
    return 0U;
}

[[nodiscard]] bool high_unit(wchar_t unit) noexcept
{
    return unit >= 0xD800U && unit <= 0xDBFFU;
}

[[nodiscard]] bool low_unit(wchar_t unit) noexcept
{
    return unit >= 0xDC00U && unit <= 0xDFFFU;
}

// 対になっていない代理が無いか（UTF-16 の模型）。上位の代理は直後の下位の代理と 2 単位で進む。
[[nodiscard]] bool model_paired(std::wstring_view units)
{
    std::size_t at = 0;
    while (at < units.size())
    {
        const wchar_t unit = units[at];
        const bool pair = high_unit(unit) && at + 1U < units.size() && low_unit(units[at + 1U]);
        if (!pair && (high_unit(unit) || low_unit(unit)))
        {
            return false;
        }
        at += pair ? 2U : 1U;
    }
    return true;
}

[[nodiscard]] std::string bytes_of(std::wstring_view units, ByteOrder order)
{
    std::string bytes;
    bytes.reserve(units.size() * 2U);
    for (const wchar_t unit : units)
    {
        const auto value = static_cast<unsigned>(static_cast<std::uint16_t>(unit));
        const auto high = static_cast<char>(value >> 8U);
        const auto low = static_cast<char>(value & 0xFFU);
        bytes.push_back(order == ByteOrder::little ? low : high);
        bytes.push_back(order == ByteOrder::little ? high : low);
    }
    return bytes;
}

void print_bytes(const char *family, std::string_view text)
{
    std::fprintf(stderr, "  %s (%zu bytes):", family, text.size());
    for (std::size_t index = 0; index < text.size() && index < 96U; ++index)
    {
        std::fprintf(stderr, " %02x", static_cast<unsigned>(byte_of(text, index)));
    }
    std::fprintf(stderr, "\n");
}

// --- 1. 全 code point ---

struct AllCodePoints
{
    std::string utf8;
    std::wstring utf16;
    std::vector<std::size_t> starts;
    std::vector<char32_t> values;
};

// 間引いた走りで必ず通す値（符号化の長さと代理の範囲の境の前後 32）。
constexpr std::array<char32_t, 9> edges{
    {0x7FU, 0x80U, 0x7FFU, 0x800U, 0xD7FFU, 0xE000U, 0xFFFFU, 0x10000U, 0x10FFFFU}};
constexpr char32_t edge_reach = 32U;
constexpr char32_t sample_stride = 61U;

// 既定実行（間引き）で通す値か。境の前後と 61 刻み。
[[nodiscard]] bool sampled(char32_t value)
{
    return value % sample_stride == 0U ||
           std::ranges::any_of(
               edges, [value](char32_t edge)
               { return value + edge_reach >= edge && value <= edge + edge_reach; });
}

// 模型の本文と製品の append_utf8 の本文を別々に組み立てて、最後に 1 回だけ比べる。every が偽なら
// sampled の値だけを並べる。
[[nodiscard]] AllCodePoints all_code_points(bool every)
{
    constexpr std::size_t scalars = 0x110000U - 0x800U;
    AllCodePoints all;
    all.utf8.reserve(scalars * 4U);
    all.utf16.reserve(scalars * 2U);
    all.starts.reserve(scalars);
    all.values.reserve(scalars);
    std::string product;
    product.reserve(scalars * 4U);
    for (char32_t value = 0; value <= last_code_point; ++value)
    {
        if (surrogate(value) || (!every && !sampled(value)))
        {
            continue;
        }
        all.starts.push_back(all.utf8.size());
        all.values.push_back(value);
        append_model_utf8(all.utf8, value);
        core::append_utf8(product, value);
        append_model_utf16(all.utf16, value);
    }
    expect(product == all.utf8, "append_utf8 matches the model at every code point");
    return all;
}

void verify_counts(const AllCodePoints &all)
{
    const std::string_view text = all.utf8;
    const auto validated = core::validate_utf8(text);
    expect(validated.has_value() && validated.value() == all.values.size(),
           "validate_utf8 accepts every code point and counts them");
    expect(core::code_point_count(text) == all.values.size(),
           "code_point_count counts every code point");
    expect(core::lenient_code_point_count(text, Offset{0}, Offset{text.size()}) ==
               all.values.size(),
           "lenient_code_point_count counts every code point");
}

// index 番目の code point の [start, end)
// を先頭から歩き、内側は境目でないことを模型と比べる（内側から の next / previous
// は乱れたバイト列の族 3 が全位置で見る）。
[[nodiscard]] bool walks_one(const AllCodePoints &all, std::size_t index)
{
    const std::string_view text = all.utf8;
    const std::size_t start = all.starts[index];
    const std::size_t end = index + 1U < all.starts.size() ? all.starts[index + 1U] : text.size();
    bool walks = core::next_code_point(text, Offset{start}).value == end &&
                 core::previous_code_point(text, Offset{end}).value == start &&
                 core::code_point_at(text, Offset{start}) == all.values[index] &&
                 core::is_boundary(text, Offset{start});
    for (std::size_t at = start + 1U; at < end; ++at)
    {
        walks = walks && !core::is_boundary(text, Offset{at});
    }
    return walks;
}

void verify_walks(const AllCodePoints &all)
{
    std::size_t broken = 0;
    for (std::size_t index = 0; index < all.starts.size(); ++index)
    {
        if (!walks_one(all, index) && broken++ == 0U)
        {
            std::fprintf(stderr, "  the walk breaks at U+%06X\n",
                         static_cast<unsigned>(all.values[index]));
        }
    }
    expect(broken == 0U, "the code point walk follows the model boundaries and values");
}

void verify_conversions(const AllCodePoints &all)
{
    const auto utf16 = core::to_utf16(all.utf8);
    expect(utf16.has_value() && utf16.value() == all.utf16, "to_utf16 matches the model");
    const auto length = core::utf16_length(all.utf8);
    expect(length.has_value() && length.value() == all.utf16.size(),
           "utf16_length matches the model");
    const auto back = core::to_utf8(all.utf16);
    expect(back.has_value() && back.value() == all.utf8, "to_utf8 returns every code point");
    // UTF-16 のバイト列（LE / BE）の口は to_utf16 / to_utf8 に並びを写すだけなので、全 code point
    // では LE だけを見る（BE と孤立した代理は族 4 が見る・Debug の所要を抑える）。
    const std::string bytes = bytes_of(all.utf16, ByteOrder::little);
    const auto read = core::utf8_of_utf16_bytes(bytes, ByteOrder::little);
    expect(core::valid_utf16_bytes(bytes, ByteOrder::little) && read.has_value() &&
               read.value() == all.utf8,
           "the UTF-16 bytes of every code point read back");
}

void verify_code_points(bool every)
{
    const AllCodePoints all = all_code_points(every);
    expect(!every || all.values.size() == 0x110000U - 0x800U, "every scalar value is generated");
    verify_counts(all);
    verify_walks(all);
    verify_conversions(all);
}

// --- 2・3. 乱れたバイト列 ---

// 正しい列・孤立した代理・過長・範囲外・切れた末尾・孤立した継続・使えない先頭バイト。
constexpr std::array<std::string_view, 22> pieces{{
    "a",
    "\n",
    "\xC3\xA9",
    "\xE3\x81\x82",
    "\xF0\x9F\x98\x80",
    "\xF4\x8F\xBF\xBF",
    "\xED\x9F\xBF",
    "\xEE\x80\x80",
    "\xED\xA0\x80",
    "\xED\xBF\xBF",
    "\xC0\xAF",
    "\xC1\xBF",
    "\xE0\x80\xAF",
    "\xE0\x9F\xBF",
    "\xF0\x80\x80\xAF",
    "\xF0\x8F\xBF\xBF",
    "\xF4\x90\x80\x80",
    "\xF5\x80\x80\x80",
    "\xE3\x81",
    "\xF0\x9F\x98",
    "\x80",
    "\xFF",
}};

void any_byte(Random &random, std::string &text)
{
    text.push_back(static_cast<char>(below(random, 256U)));
}

// 先頭バイトと継続バイトの帯（0x80〜0xFF）に寄せる。
void high_byte(Random &random, std::string &text)
{
    text.push_back(static_cast<char>(0x80U + below(random, 128U)));
}

// 代理の範囲も含む 0〜0x10FFFF の値を模型で符号化する（代理は CESU 風の不正な 3 バイト）。
void any_value(Random &random, std::string &text)
{
    append_model_utf8(text, static_cast<char32_t>(below(random, 0x110000U)));
}

void any_piece(Random &random, std::string &text)
{
    text += pieces.at(below(random, pieces.size()));
}

using AddBytes = void (*)(Random &, std::string &);
constexpr std::array<AddBytes, 4> byte_makers{{any_byte, high_byte, any_value, any_piece}};

[[nodiscard]] std::string random_bytes(Random &random)
{
    std::string text;
    const std::size_t count = below(random, maximum_pieces + 1U);
    for (std::size_t piece = 0; piece < count; ++piece)
    {
        byte_makers.at(below(random, byte_makers.size()))(random, text);
    }
    return text;
}

[[nodiscard]] std::expected<std::size_t, TextFailure> split_validation(Random &random,
                                                                       std::string_view text)
{
    auto progress = core::Utf8Validation::empty();
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t size = below(random, text.size() - at + 1U);
        const auto next = progress.with_bytes(text.substr(at, size));
        if (!next)
        {
            return std::unexpected(next.error());
        }
        progress = next.value();
        at += size;
    }
    return progress.finish();
}

// 一括の検証が模型と同じか（受理と数）。受理しない理由は invalid_utf8 だけ。
[[nodiscard]] bool validation_matches(std::string_view text)
{
    const auto validated = core::validate_utf8(text);
    const auto model = model_count(text);
    if (!model.has_value())
    {
        return !validated.has_value() && validated.error() == TextFailure::invalid_utf8;
    }
    return validated.has_value() && validated.value() == model.value();
}

void verify_validation_family()
{
    Random random{12891U};
    std::size_t broken = 0;
    std::size_t accepted = 0;
    for (std::size_t input = 0; input < byte_inputs; ++input)
    {
        const std::string text = random_bytes(random);
        const auto whole = core::validate_utf8(text);
        accepted += whole.has_value() ? 1U : 0U;
        const bool same = validation_matches(text) && split_validation(random, text) == whole;
        if (!same && broken++ == 0U)
        {
            print_bytes("validation differs from the model or the split", text);
        }
    }
    expect(broken == 0U, "validate_utf8 matches the model and any split of the bytes");
    // 生成器が受理と拒否の両方を作っているか（片側だけなら命題が空回りする）。
    expect(accepted > byte_inputs / 20U && accepted < byte_inputs - byte_inputs / 20U,
           "the byte generator makes both valid and invalid inputs");
}

[[nodiscard]] bool walk_matches(std::string_view text)
{
    bool same = true;
    for (std::size_t at = 0; at <= text.size() + 2U; ++at)
    {
        const std::size_t next = core::next_code_point(text, Offset{at}).value;
        const std::size_t previous = core::previous_code_point(text, Offset{at}).value;
        const bool moves = (at >= text.size() || next > at) && (at == 0U || previous < at);
        const bool boundary = core::is_boundary(text, Offset{at}) ==
                              (at >= text.size() || !continuation(byte_of(text, at)));
        same = same && moves && boundary && next == model_next(text, at) &&
               previous == model_previous(text, at);
    }
    return same;
}

[[nodiscard]] bool lenient_matches(Random &random, std::string_view text)
{
    const std::size_t whole = core::lenient_code_point_count(text, Offset{0}, Offset{text.size()});
    std::size_t sum = 0;
    std::size_t at = 0;
    while (at < text.size())
    {
        const std::size_t size = 1U + below(random, text.size() - at);
        sum += core::lenient_code_point_count(text, Offset{at}, Offset{at + size});
        at += size;
    }
    const bool valid = core::validate_utf8(text).has_value();
    return sum == whole && (!valid || whole == core::code_point_count(text)) &&
           whole <= text.size() && (text.empty() || whole > 0U);
}

void verify_walk_family()
{
    Random random{12892U};
    std::size_t broken = 0;
    for (std::size_t input = 0; input < byte_inputs; ++input)
    {
        const std::string text = random_bytes(random);
        if ((!walk_matches(text) || !lenient_matches(random, text)) && broken++ == 0U)
        {
            print_bytes("the walk or the lenient count differs from the model", text);
        }
    }
    expect(broken == 0U, "the code point walk always moves and follows the model on any bytes");
}

// --- 4. UTF-16 の列 ---

void high_surrogate(Random &random, std::wstring &units)
{
    units.push_back(static_cast<wchar_t>(0xD800U + below(random, 0x400U)));
}

void low_surrogate(Random &random, std::wstring &units)
{
    units.push_back(static_cast<wchar_t>(0xDC00U + below(random, 0x400U)));
}

void supplementary(Random &random, std::wstring &units)
{
    append_model_utf16(units, static_cast<char32_t>(0x10000U + below(random, 0x100000U)));
}

void any_unit(Random &random, std::wstring &units)
{
    units.push_back(static_cast<wchar_t>(below(random, 0x10000U)));
}

using AddUnits = void (*)(Random &, std::wstring &);
constexpr std::array<AddUnits, 4> unit_makers{
    {high_surrogate, low_surrogate, supplementary, any_unit}};

[[nodiscard]] std::wstring random_units(Random &random)
{
    std::wstring units;
    const std::size_t count = below(random, maximum_pieces + 1U);
    for (std::size_t unit = 0; unit < count; ++unit)
    {
        unit_makers.at(below(random, unit_makers.size()))(random, units);
    }
    return units;
}

[[nodiscard]] bool units_match(std::wstring_view units)
{
    const bool paired = model_paired(units);
    const auto utf8 = core::to_utf8(units);
    if (utf8.has_value() != paired || (!paired && utf8.error() != TextFailure::invalid_utf16))
    {
        return false;
    }
    bool same = true;
    for (const ByteOrder order : {ByteOrder::little, ByteOrder::big})
    {
        const std::string bytes = bytes_of(units, order);
        const auto read = core::utf8_of_utf16_bytes(bytes, order);
        same = same && core::valid_utf16_bytes(bytes, order) == paired &&
               read.has_value() == paired && (!paired || read.value() == utf8.value());
        // 奇数バイトはどの並びでも断る。
        same = same && !core::valid_utf16_bytes(bytes + "a", order);
    }
    if (!paired)
    {
        return same;
    }
    const auto again = core::to_utf16(utf8.value());
    return same && core::validate_utf8(utf8.value()).has_value() && again.has_value() &&
           again.value() == units;
}

void print_units(std::wstring_view units)
{
    std::fprintf(stderr, "  UTF-16 differs from the model (%zu units):", units.size());
    for (const wchar_t unit : units)
    {
        std::fprintf(stderr, " %04x", static_cast<unsigned>(unit));
    }
    std::fprintf(stderr, "\n");
}

void verify_units_family()
{
    Random random{12893U};
    std::size_t broken = 0;
    std::size_t paired = 0;
    for (std::size_t input = 0; input < unit_inputs; ++input)
    {
        const std::wstring units = random_units(random);
        paired += model_paired(units) ? 1U : 0U;
        if (!units_match(units) && broken++ == 0U)
        {
            print_units(units);
        }
    }
    expect(broken == 0U, "the UTF-16 conversions match the pairing model and round trip");
    expect(paired > unit_inputs / 20U && paired < unit_inputs - unit_inputs / 20U,
           "the UTF-16 generator makes both paired and unpaired inputs");
}

void verify_generators_are_deterministic()
{
    Random left{12899U};
    Random right{12899U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        same = same && random_bytes(left) == random_bytes(right) &&
               random_units(left) == random_units(right);
    }
    expect(same, "the UTF generators give the same inputs for the same seed");
}
} // namespace

// 既定実行の契約: 全 code point の族は境の前後と 61 刻みに間引く（Debug で 1 秒に収める）。
void verify_utf_property_contracts()
{
    verify_generators_are_deterministic();
    verify_code_points(false);
    verify_validation_family();
    verify_walk_family();
    verify_units_family();
}

// selector の `--utf-property`: 代理の範囲を除く全 code point を通す。
void verify_utf_property_scope()
{
    verify_generators_are_deterministic();
    verify_code_points(true);
    verify_validation_family();
    verify_walk_family();
    verify_units_family();
}
} // namespace nenenib::tests
