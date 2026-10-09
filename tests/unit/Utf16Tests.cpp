#include "Scopes.hpp"
#include "TestSupport.hpp"
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
} // namespace

void verify_utf16_scope()
{
    verify_utf16_conversions();
    verify_lengths();
    verify_failures_and_long_text();
    verify_long_utf8_output();
}
} // namespace nenenib::tests
