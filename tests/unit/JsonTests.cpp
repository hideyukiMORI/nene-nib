// scope `--json` の契約（#994・ADR 0160 の決定 4・ADR 0042）。core の parse_json / write_json に
// RFC 8259 の例・値ごとの読みと書き・失敗の閉じた一覧・入れ子の深さの上限を当てる。
#include "JsonFailure.hpp"
#include "JsonNull.hpp"
#include "JsonNumber.hpp"
#include "JsonParse.hpp"
#include "JsonValue.hpp"
#include "JsonWrite.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace nenenib::tests
{
namespace
{
using core::JsonArray;
using core::JsonFailure;
using core::JsonNull;
using core::JsonNumber;
using core::JsonObject;
using core::JsonValue;
using Parsed = std::expected<JsonValue, JsonFailure>;
// 本文と、読んで書き直した最小の形。
using Rewritten = std::pair<std::string_view, std::string_view>;
using Refused = std::pair<std::string_view, JsonFailure>;

// RFC 8259 の 13 節の例（Image の object と、座標の配列）。
constexpr std::string_view rfc_object = R"({
      "Image": {
          "Width":  800,
          "Height": 600,
          "Title":  "View from 15th Floor",
          "Thumbnail": {
              "Url":    "http://www.example.com/image/481989943",
              "Height": 125,
              "Width":  100
          },
          "Animated" : false,
          "IDs": [116, 943, 234, 38793]
        }
    })";

constexpr std::string_view rfc_array = R"([
        {
           "precision": "zip",
           "Latitude":  37.7668,
           "Longitude": -122.3959,
           "Address":   "",
           "City":      "SAN FRANCISCO",
           "State":     "CA",
           "Zip":       "94107",
           "Country":   "US"
        },
        {
           "precision": "zip",
           "Latitude":  37.371991,
           "Longitude": -122.026020,
           "Address":   "",
           "City":      "SUNNYVALE",
           "State":     "CA",
           "Zip":       "94085",
           "Country":   "US"
        }
    ])";

// 読んで最小の形に書き直した本文（RFC の 13 節の小さな例・escape・数の形・空白）。
constexpr std::array<Rewritten, 26> rewritten{{
    {R"("Hello world!")", R"("Hello world!")"},
    {"42", "42"},
    {"true", "true"},
    {" false ", "false"},
    {"\t\r\n null \n", "null"},
    {"-0", "0"},
    {"-17", "-17"},
    {"1.5", "1.5"},
    {"1.5e3", "1500"},
    {"1E-2", "0.01"},
    {"0.1", "0.1"},
    {"1e20", "1e+20"},
    {"9007199254740993", "9007199254740992"},
    {"[]", "[]"},
    {"{}", "{}"},
    {"[ 1 , [ 2 , { } ] ]", "[1,[2,{}]]"},
    {R"({ "a" : 1 , "b" : [ true , null ] })", R"({"a":1,"b":[true,null]})"},
    {R"("é")", "\"\xC3\xA9\""},
    {R"("😀")", "\"\xF0\x9F\x98\x80\""},
    {R"("\"\\\/\b\f\n\r\t")", R"("\"\\/\b\f\n\r\t")"},
    {R"("\u0001\u001f\u007f")", "\"\\u0001\\u001f\x7f\""},
    {R"("\u0000")", R"("\u0000")"},
    {"\"日本語\"", "\"日本語\""},
    {R"({"a":1,"a":2})", R"({"a":1,"a":2})"},
    {R"(["a",{"b":"c"}])", R"(["a",{"b":"c"}])"},
    {"123456789012", "123456789012"},
}};

// 断る本文と閉じた失敗（JsonFailure.hpp の一覧のどれにも 1 つ以上）。
constexpr std::array<Refused, 30> refused{{
    {"", JsonFailure::unexpected_end},
    {"   ", JsonFailure::unexpected_end},
    {"[", JsonFailure::unexpected_end},
    {"[1,2", JsonFailure::unexpected_end},
    {R"({"a":1)", JsonFailure::unexpected_end},
    {R"("abc)", JsonFailure::unexpected_end},
    {R"("\)", JsonFailure::unexpected_end},
    {R"("\u12)", JsonFailure::unexpected_end},
    {"[1,]", JsonFailure::unexpected_character},
    {R"({"a":1,})", JsonFailure::unexpected_character},
    {R"({"a" 1})", JsonFailure::unexpected_character},
    {"{a:1}", JsonFailure::unexpected_character},
    {"[1 2]", JsonFailure::unexpected_character},
    {"+1", JsonFailure::unexpected_character},
    {".5", JsonFailure::unexpected_character},
    {"1 2", JsonFailure::trailing_characters},
    {"truex", JsonFailure::trailing_characters},
    {"tru", JsonFailure::invalid_literal},
    {"nul", JsonFailure::invalid_literal},
    {"01", JsonFailure::invalid_number},
    {"1.", JsonFailure::invalid_number},
    {"-", JsonFailure::invalid_number},
    {"1e", JsonFailure::invalid_number},
    {"1e400", JsonFailure::number_out_of_range},
    {R"("\x")", JsonFailure::invalid_escape},
    {R"("\u12G4")", JsonFailure::invalid_unicode_escape},
    {R"("\ud800")", JsonFailure::unpaired_surrogate},
    {R"("\udc00")", JsonFailure::unpaired_surrogate},
    {"\"a\tb\"", JsonFailure::control_character},
    {"\"\xFF\"", JsonFailure::invalid_utf8},
}};

// 上の表に入れない残りの失敗（桁落ち・代理対の 2 つ目の誤り・UTF-8 で書いた代理）。
constexpr std::array<Refused, 5> refused_more{{
    {"1e-400", JsonFailure::number_out_of_range},
    {R"("\ud800A")", JsonFailure::unpaired_surrogate},
    {R"("\ud800x")", JsonFailure::unpaired_surrogate},
    {"\"\xED\xA0\x80\"", JsonFailure::invalid_utf8},
    {"\"\xC0\x80\"", JsonFailure::invalid_utf8},
}};

[[nodiscard]] const JsonValue &member(const JsonValue &value, std::string_view key)
{
    static const JsonValue missing{JsonNull{}};
    const auto *object = std::get_if<JsonObject>(&value.value);
    if (object == nullptr)
    {
        return missing;
    }
    const auto found = core::json_member(*object, key);
    return found.has_value() ? found.value().get() : missing;
}

[[nodiscard]] std::optional<std::int64_t> integer_of(const JsonValue &value)
{
    const auto *number = std::get_if<JsonNumber>(&value.value);
    return number == nullptr ? std::nullopt : number->integer;
}

void verify_rfc_examples()
{
    const Parsed object = core::parse_json(rfc_object);
    expect(object.has_value(), "the RFC 8259 object example parses");
    const JsonValue root = object.value_or(JsonValue{JsonNull{}});
    const JsonValue &image = member(root, "Image");
    expect(integer_of(member(image, "Width")) == std::optional<std::int64_t>{800},
           "the RFC example's Width is the integer 800");
    expect(member(image, "Animated") == JsonValue{false}, "the RFC example's Animated is false");
    expect(member(member(image, "Thumbnail"), "Url") ==
               JsonValue{std::string{"http://www.example.com/image/481989943"}},
           "the RFC example's nested Url is read");
    const auto *ids = std::get_if<JsonArray>(&member(image, "IDs").value);
    expect(ids != nullptr && ids->size() == 4, "the RFC example's IDs has 4 items");
    const Parsed array = core::parse_json(rfc_array);
    expect(array.has_value(), "the RFC 8259 array example parses");
    const JsonValue list = array.value_or(JsonValue{JsonNull{}});
    const auto *places = std::get_if<JsonArray>(&list.value);
    expect(places != nullptr && places->size() == 2, "the RFC array example has 2 objects");
    for (const std::string_view text : {rfc_object, rfc_array})
    {
        const Parsed parsed = core::parse_json(text);
        const std::string written = core::write_json(parsed.value_or(JsonValue{JsonNull{}}));
        expect(core::parse_json(written) == parsed, "the RFC examples survive write and parse");
    }
}

void verify_rewritten()
{
    for (const auto &[text, minimal] : rewritten)
    {
        const Parsed parsed = core::parse_json(text);
        expect(parsed.has_value(), "the JSON text parses");
        if (!parsed.has_value())
        {
            continue;
        }
        expect(core::write_json(parsed.value()) == minimal, "the value is written minimally");
        expect(core::parse_json(minimal) == parsed, "the minimal form reads back the same value");
    }
}

void verify_refused()
{
    for (const auto &table :
         {std::span<const Refused>{refused}, std::span<const Refused>{refused_more}})
    {
        for (const auto &[text, failure] : table)
        {
            const Parsed parsed = core::parse_json(text);
            expect(!parsed.has_value() && parsed.error() == failure,
                   "an invalid JSON text is refused with its closed failure");
        }
    }
}

void verify_numbers()
{
    const Parsed big = core::parse_json("9007199254740992");
    expect(big.has_value() &&
               integer_of(big.value()) == std::optional<std::int64_t>{9007199254740992},
           "2^53 still carries the integer");
    const Parsed beyond = core::parse_json("18014398509481984");
    expect(beyond.has_value() && !integer_of(beyond.value()).has_value(),
           "integers beyond 2^53 keep only the double");
    const Parsed whole = core::parse_json("2.0");
    expect(whole.has_value() && integer_of(whole.value()) == std::optional<std::int64_t>{2},
           "an integral fraction also carries the integer");
    expect(!integer_of(core::parse_json("2.5").value_or(JsonValue{JsonNull{}})).has_value(),
           "a fraction carries no integer");
    for (const double edge :
         {std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min(),
          std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()})
    {
        const JsonValue number{core::json_number(edge)};
        expect(core::parse_json(core::write_json(number)) == Parsed{number},
               "the smallest subnormal, the smallest normal and the largest doubles round-trip");
    }
    constexpr double infinity = std::numeric_limits<double>::infinity();
    expect(core::write_json(JsonValue{core::json_number(infinity)}) == "null",
           "infinity is written as null");
    expect(core::write_json(JsonValue{core::json_number(-infinity)}) == "null",
           "negative infinity is written as null");
    expect(core::write_json(
               JsonValue{core::json_number(std::numeric_limits<double>::quiet_NaN())}) == "null",
           "NaN is written as null");
    expect(core::write_json(JsonValue{core::json_integer(-9007199254740993)}) ==
               "-9007199254740992",
           "an integer beyond 2^53 is written through its double");
}

void verify_members()
{
    const Parsed doubled = core::parse_json(R"({"a":1,"b":2,"a":3})");
    expect(integer_of(member(doubled.value_or(JsonValue{JsonNull{}}), "a")) ==
               std::optional<std::int64_t>{3},
           "a duplicated key reads its last value");
    expect(member(doubled.value_or(JsonValue{JsonNull{}}), "c") == JsonValue{JsonNull{}},
           "a missing key has no value");
    JsonObject ordered;
    ordered.emplace_back("z", JsonValue{true});
    ordered.emplace_back("a", JsonValue{std::string{"x\"y"}});
    expect(core::write_json(JsonValue{std::move(ordered)}) == R"({"z":true,"a":"x\"y"})",
           "an object is written in its own order");
}

[[nodiscard]] std::string nested(std::size_t depth, std::string_view open, std::string_view close)
{
    std::string text;
    for (std::size_t level = 0; level < depth; ++level)
    {
        text.append(open);
    }
    text.append("0");
    for (std::size_t level = 0; level < depth; ++level)
    {
        text.append(close);
    }
    return text;
}

void verify_depth()
{
    const std::size_t limit = core::json_depth_limit;
    expect(core::parse_json(nested(limit, "[", "]")).has_value(),
           "arrays nested to the limit parse");
    const Parsed deeper = core::parse_json(nested(limit + 1, "[", "]"));
    expect(!deeper.has_value() && deeper.error() == JsonFailure::too_deep,
           "arrays nested past the limit are too deep");
    expect(core::parse_json(nested(limit, R"({"a":)", "}")).has_value(),
           "objects nested to the limit parse");
    const Parsed objects = core::parse_json(nested(limit + 1, R"({"a":)", "}"));
    expect(!objects.has_value() && objects.error() == JsonFailure::too_deep,
           "objects nested past the limit are too deep");
    const Parsed unclosed = core::parse_json(std::string(10000, '['));
    expect(!unclosed.has_value() && unclosed.error() == JsonFailure::too_deep,
           "10000 open brackets stop at the depth limit");
}
} // namespace

void verify_json_scope()
{
    verify_rfc_examples();
    verify_rewritten();
    verify_refused();
    verify_numbers();
    verify_members();
    verify_depth();
}
} // namespace nenenib::tests
