// scope `--json-property` の性質の試験（#994・ADR 0160 の決定 4・ADR 0042）。
// 固定 seed の決定的な生成器で作った値・バイト列・壊した本文を core の parse_json / write_json に
// 渡し、値は write → parse で同じ値に戻る（double は == で比べ、NaN と無限は作らない）・どんな
// バイト列も落ちずに値か閉じた失敗になる・読めた値は書き直しても同じ値・不正な UTF-8 を含む本文は
// invalid_utf8 で断る、を確かめる。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。50 倍の走りは rounds を手で 50 にして
// 回す（commit しない）。生成器は std::mt19937_64 の剰余だけを使い、分布の型は使わない。
#include "JsonFailure.hpp"
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "JsonWrite.hpp"
#include "Offset.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "Utf8.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::tests
{
namespace
{
using core::JsonArray;
using core::JsonFailure;
using core::JsonNull;
using core::JsonObject;
using core::JsonValue;
using Random = std::mt19937_64;
using Parsed = std::expected<JsonValue, JsonFailure>;
using MakeValue = JsonValue (*)(Random &, std::size_t);

constexpr std::size_t rounds = 1U;
constexpr std::size_t value_cases = 300U * rounds;
constexpr std::size_t byte_cases = 400U * rounds;
constexpr std::size_t mutation_cases = 400U * rounds;
constexpr std::size_t utf8_cases = 150U * rounds;
constexpr std::size_t deepest = 8U;
constexpr std::size_t widest = 5U;
constexpr std::int64_t exact_limit = (std::int64_t{1} << 53) - 1;

// 文字列に入れるコードポイント（制御文字・escape の要る文字・非 ASCII・代理対になる文字）。
constexpr std::array<char32_t, 20> code_points{{
    U'a', U'Z', U'0',   U' ',   U'"',   U'\\',  U'/',   0x00,    0x01,    0x1F,
    0x7F, 0xE9, 0x3042, 0x65E5, 0xD7FF, 0xE000, 0xFFFF, 0x1F600, 0x10000, 0x10FFFF,
}};

// ランダムなバイト列と壊しに使う JSON らしいバイト。
constexpr std::string_view json_bytes =
    "{}[],:\"\\/ \t\r\n0123456789-+.eEtrufalsn\x01\x7f\xc3\xa9\xff";

// 正しい UTF-8 の境目に差し込むと本文が不正な UTF-8 になるバイト列。
constexpr std::array<std::string_view, 7> broken_utf8{{
    "\xFF",
    "\x80",
    "\xC0\xAF",
    "\xE3\x81",
    "\xED\xA0\x80",
    "\xF4\x90\x80\x80",
    "\xF0\x9F\x98",
}};

[[nodiscard]] std::size_t below(Random &random, std::size_t limit)
{
    return static_cast<std::size_t>(random() % limit);
}

[[nodiscard]] JsonValue random_value(Random &random, std::size_t depth);

[[nodiscard]] std::string random_text(Random &random)
{
    std::string text;
    const std::size_t length = below(random, 8U);
    for (std::size_t index = 0; index < length; ++index)
    {
        core::append_utf8(text, code_points[below(random, code_points.size())]);
    }
    return text;
}

// 有限の double（ビット列から作る・NaN と無限は作り直す）。
[[nodiscard]] double finite_double(Random &random)
{
    while (true)
    {
        const auto value = std::bit_cast<double>(random());
        if (value - value == 0.0)
        {
            return value;
        }
    }
}

[[nodiscard]] JsonValue null_value(Random & /*random*/, std::size_t /*depth*/)
{
    return JsonValue{JsonNull{}};
}

[[nodiscard]] JsonValue bool_value(Random &random, std::size_t /*depth*/)
{
    return JsonValue{below(random, 2U) == 0};
}

[[nodiscard]] JsonValue number_value(Random &random, std::size_t /*depth*/)
{
    const std::size_t kind = below(random, 3U);
    if (kind == 0)
    {
        const auto span = static_cast<std::uint64_t>(exact_limit) * 2U + 1U;
        return JsonValue{
            core::json_integer(static_cast<std::int64_t>(random() % span) - exact_limit)};
    }
    if (kind == 1)
    {
        return JsonValue{core::json_number(finite_double(random))};
    }
    return JsonValue{core::json_number(static_cast<double>(below(random, 20001U)) / 100.0 - 100.0)};
}

[[nodiscard]] JsonValue string_value(Random &random, std::size_t /*depth*/)
{
    return JsonValue{random_text(random)};
}

[[nodiscard]] JsonValue array_value(Random &random, std::size_t depth)
{
    JsonArray items;
    const std::size_t count = below(random, widest);
    for (std::size_t index = 0; index < count; ++index)
    {
        items.push_back(random_value(random, depth + 1));
    }
    return JsonValue{std::move(items)};
}

[[nodiscard]] JsonValue object_value(Random &random, std::size_t depth)
{
    JsonObject members;
    const std::size_t count = below(random, widest);
    for (std::size_t index = 0; index < count; ++index)
    {
        // 鍵の重複も作る（短い鍵の集合から引く）。並びを保つので往復は同じ値。
        std::string key = below(random, 3U) == 0 ? std::string{"k"} : random_text(random);
        members.emplace_back(std::move(key), random_value(random, depth + 1));
    }
    return JsonValue{std::move(members)};
}

// 葉の 4 種を先に並べ、深さの上限では葉だけから引く。
constexpr std::array<MakeValue, 6> makers{{
    null_value,
    bool_value,
    number_value,
    string_value,
    array_value,
    object_value,
}};

JsonValue random_value(Random &random, std::size_t depth)
{
    const std::size_t kinds = depth >= deepest ? 4U : makers.size();
    return makers[below(random, kinds)](random, depth);
}

// 読めた値は書いて読み直しても同じ値で、書いた本文も同じ（write は parse の不動点）。
[[nodiscard]] bool stable(const JsonValue &value)
{
    const std::string written = core::write_json(value);
    const Parsed again = core::parse_json(written);
    return again.has_value() && again.value() == value &&
           core::write_json(again.value()) == written;
}

void verify_values_round_trip()
{
    Random random{99401U};
    bool all = true;
    for (std::size_t index = 0; index < value_cases; ++index)
    {
        all = stable(random_value(random, 0)) && all;
    }
    expect(all, "random values survive write and parse unchanged");
}

[[nodiscard]] std::string random_bytes(Random &random)
{
    std::string bytes;
    const std::size_t length = below(random, 24U);
    for (std::size_t index = 0; index < length; ++index)
    {
        const bool any = below(random, 4U) == 0;
        bytes.push_back(any ? static_cast<char>(random() & 0xFFU)
                            : json_bytes[below(random, json_bytes.size())]);
    }
    return bytes;
}

// どんなバイト列も値か閉じた失敗になり、値なら書き直しても同じ値。
[[nodiscard]] bool parses_or_refuses(std::string_view text)
{
    const Parsed parsed = core::parse_json(text);
    return !parsed.has_value() || stable(parsed.value());
}

void verify_random_bytes()
{
    Random random{99402U};
    bool all = true;
    for (std::size_t index = 0; index < byte_cases; ++index)
    {
        all = parses_or_refuses(random_bytes(random)) && all;
    }
    expect(all, "random bytes parse to a stable value or a closed failure");
}

// 1〜3 回の挿入・削除・置き換えで正しい本文を壊す。
[[nodiscard]] std::string mutated(Random &random, std::string text)
{
    const std::size_t edits = 1U + below(random, 3U);
    for (std::size_t edit = 0; edit < edits; ++edit)
    {
        const std::size_t at = below(random, text.size() + 1U);
        const char byte = json_bytes[below(random, json_bytes.size())];
        const std::size_t kind = below(random, 3U);
        if (kind == 0 || at == text.size())
        {
            text.insert(at, 1U, byte);
            continue;
        }
        if (kind == 1)
        {
            text.erase(at, 1U);
            continue;
        }
        text[at] = byte;
    }
    return text;
}

void verify_mutations()
{
    Random random{99403U};
    bool all = true;
    for (std::size_t index = 0; index < mutation_cases; ++index)
    {
        const std::string written = core::write_json(random_value(random, 0));
        all = parses_or_refuses(mutated(random, written)) && all;
        all = parses_or_refuses(written.substr(0, below(random, written.size() + 1U))) && all;
    }
    expect(all, "broken JSON texts parse to a stable value or a closed failure");
}

// 書いた本文のコードポイントの境目に、不正な UTF-8 の列を差し込む。
[[nodiscard]] std::string with_broken_utf8(Random &random, std::string text)
{
    std::size_t at = below(random, text.size() + 1U);
    while (!core::is_boundary(text, core::Offset{at}))
    {
        ++at;
    }
    text.insert(at, broken_utf8[below(random, broken_utf8.size())]);
    return text;
}

void verify_invalid_utf8()
{
    Random random{99404U};
    bool all = true;
    for (std::size_t index = 0; index < utf8_cases; ++index)
    {
        const std::string written = core::write_json(JsonValue{random_text(random)});
        const Parsed parsed = core::parse_json(with_broken_utf8(random, written));
        all = !parsed.has_value() && parsed.error() == JsonFailure::invalid_utf8 && all;
    }
    expect(all, "strings with invalid UTF-8 are refused as invalid_utf8");
}

void verify_generators_are_deterministic()
{
    Random first{99405U};
    Random second{99405U};
    const std::string left = core::write_json(random_value(first, 0));
    const std::string right = core::write_json(random_value(second, 0));
    expect(left == right, "the value generator is deterministic for a fixed seed");
    expect(random_bytes(first) == random_bytes(second),
           "the byte generator is deterministic for a fixed seed");
}
} // namespace

void verify_json_property_scope()
{
    verify_generators_are_deterministic();
    verify_values_round_trip();
    verify_random_bytes();
    verify_mutations();
    verify_invalid_utf8();
}
} // namespace nenenib::tests
