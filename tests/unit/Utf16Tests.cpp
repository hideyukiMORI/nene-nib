#include "ByteOrder.hpp"
#include "Scopes.hpp"
#include "TestSupport.hpp"
#include "TextFailure.hpp"
#include "Utf16.hpp"

#include <array>
#include <string>
#include <string_view>

namespace nenenib::tests
{
namespace
{
void verify_lengths()
{
    constexpr std::array<std::pair<std::string_view, std::size_t>, 8> cases{{
        {"", 0},
        {"abc", 3},
        {"©", 1},
        {"日本", 2},
        {"😀", 2},
        {"a©日😀", 5},
        {"\xef\xbf\xbf", 1},
        {"\xf0\x90\x80\x80", 2},
    }};
    for (const auto &[text, expected] : cases)
    {
        expect(core::utf16_length(text).value() == expected, "UTF-16 units match fixed values");
        expect(core::to_utf16(text).value().size() == expected,
               "the existing conversion uses the same scalar unit rule");
    }
    const std::string mixed = "a©日😀";
    for (std::size_t byte = 0; byte <= mixed.size(); ++byte)
    {
        const auto prefix = std::string_view{mixed}.substr(0, byte);
        const auto made = core::to_utf16(prefix);
        const auto measured = core::utf16_length(prefix);
        expect(made.has_value() == measured.has_value(),
               "every prefix has the same validation result as conversion");
        if (made && measured)
        {
            expect(measured.value() == made.value().size(), "boundary prefixes have equal units");
        }
        if (!made && !measured)
        {
            expect(measured.error() == made.error(), "partial prefixes keep the same failure");
        }
    }
}

void verify_failures_and_long_text()
{
    constexpr std::array<std::string_view, 7> invalid{
        "\x80", "\xc0\x80",     "\xe6\x97", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        "\xff", "a\xf0\x9f\x98"};
    for (const auto text : invalid)
    {
        const auto measured = core::utf16_length(text);
        expect(!measured && measured.error() == core::TextFailure::invalid_utf8,
               "measurement rejects malformed UTF-8 just as conversion does");
    }
    std::string long_text;
    for (std::size_t repeat = 0; repeat < 4096; ++repeat)
    {
        long_text += "a©日😀";
    }
    expect(core::utf16_length(long_text).value() == 4096 * std::size_t{5},
           "long text counts every supplementary scalar twice");
    expect(core::utf16_length(long_text).value() == core::to_utf16(long_text).value().size(),
           "long text measurement matches the existing conversion");
}

void verify_long_utf8_output()
{
    const std::wstring japanese(4096, L'\x65e5');
    const std::wstring ascii(8192, L'a');
    std::wstring supplementary;
    std::string japanese_bytes;
    std::string supplementary_bytes;
    for (std::size_t repeat = 0; repeat < 4096; ++repeat)
    {
        japanese_bytes += "\xe6\x97\xa5";
        supplementary += L"\xd83d\xdd8b";
        supplementary_bytes += "\xf0\x9f\x96\x8b";
    }
    const auto converted_japanese = core::to_utf8(japanese);
    const auto converted_ascii = core::to_utf8(ascii);
    const auto converted_supplementary = core::to_utf8(supplementary);
    expect(converted_japanese && converted_japanese.value() == japanese_bytes,
           "long Japanese UTF-16 converts to known three-byte scalars");
    expect(converted_ascii && converted_ascii.value() == std::string(8192, 'a'),
           "long ASCII UTF-16 converts to known single-byte scalars");
    expect(converted_supplementary && converted_supplementary.value() == supplementary_bytes,
           "long surrogate pairs convert to known four-byte scalars");
}
// UTF-16 のバイト列（BOM を除く）と UTF-8 の往復（ADR 0149 の決定 2・3）。値は手で書いた固定値。
void verify_utf16_bytes()
{
    const std::string utf8 = "a日\r\n😀";
    const std::string little("a\x00\xE5\x65\x0D\x00\x0A\x00\x3D\xD8\x00\xDE", 12);
    const std::string big("\x00"
                          "a\x65\xE5\x00\x0D\x00\x0A\xD8\x3D\xDE\x00",
                          12);
    expect(core::utf16_bytes_of_utf8(utf8, core::ByteOrder::little).value() == little,
           "UTF-8 becomes UTF-16 LE bytes with the low byte first");
    expect(core::utf16_bytes_of_utf8(utf8, core::ByteOrder::big).value() == big,
           "UTF-8 becomes UTF-16 BE bytes with the high byte first");
    expect(core::utf8_of_utf16_bytes(little, core::ByteOrder::little).value() == utf8,
           "UTF-16 LE bytes come back as the same UTF-8");
    expect(core::utf8_of_utf16_bytes(big, core::ByteOrder::big).value() == utf8,
           "UTF-16 BE bytes come back as the same UTF-8");
    expect(core::valid_utf16_bytes(little, core::ByteOrder::little) &&
               core::valid_utf16_bytes(big, core::ByteOrder::big),
           "the validator accepts both orders with a surrogate pair");
    const std::string high("\x00\xD8", 2);
    expect(!core::valid_utf16_bytes(high, core::ByteOrder::little) &&
               core::valid_utf16_bytes(high, core::ByteOrder::big),
           "the same two bytes are a lone surrogate in one order and a letter in the other");
    expect(core::utf8_of_utf16_bytes("", core::ByteOrder::little).value().empty() &&
               core::utf16_bytes_of_utf8("", core::ByteOrder::big).value().empty(),
           "empty is empty both ways");
    constexpr std::array<std::string_view, 4> broken{
        std::string_view("a", 1), std::string_view("\x3D\xD8", 2), std::string_view("\x00\xDE", 2),
        std::string_view("\x3D\xD8\x41\x00", 4)};
    for (const auto bytes : broken)
    {
        const auto converted = core::utf8_of_utf16_bytes(bytes, core::ByteOrder::little);
        expect(!converted && converted.error() == core::TextFailure::invalid_utf16 &&
                   !core::valid_utf16_bytes(bytes, core::ByteOrder::little),
               "odd bytes and unpaired surrogates are invalid UTF-16 both ways");
    }
    const auto rejected = core::utf16_bytes_of_utf8("\xFF", core::ByteOrder::little);
    expect(!rejected && rejected.error() == core::TextFailure::invalid_utf8,
           "broken UTF-8 is not written as UTF-16");
}
} // namespace

void verify_utf16_scope()
{
    verify_utf16_bytes();
    verify_utf16_conversions();
    verify_lengths();
    verify_failures_and_long_text();
    verify_long_utf8_output();
}
} // namespace nenenib::tests
