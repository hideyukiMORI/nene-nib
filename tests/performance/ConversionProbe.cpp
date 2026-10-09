#include "ConversionProbe.hpp"

#include "Milestone.hpp"
#include "Utf16.hpp"
#include "Win32CodePageAdapter.hpp"

namespace nenenib::tests::performance
{
namespace
{
[[nodiscard]] std::string repeated(std::string_view block, std::size_t count)
{
    std::string result;
    result.reserve(block.size() * count);
    for (std::size_t index = 0; index < count; ++index)
    {
        result += block;
    }
    return result;
}

[[nodiscard]] std::wstring units_of(std::string_view input)
{
    std::wstring units;
    units.reserve(input.size() / 2U);
    for (std::size_t at = 0; at < input.size(); at += 2U)
    {
        const unsigned int low = static_cast<unsigned char>(input[at]);
        const unsigned int high = static_cast<unsigned char>(input[at + 1U]);
        units.push_back(static_cast<wchar_t>(low | (high << 8U)));
    }
    return units;
}

[[nodiscard]] std::expected<std::uint64_t, ProbeFailure>
converted_units(const std::string &input, std::string_view expected,
                application::TimingPort &timing)
{
    if (input.size() != 16777216U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto units = units_of(input);
    if (units.size() != 8388608U)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    timing.mark(core::Milestone::probe_started);
    const auto result = core::to_utf8(units);
    timing.mark(core::Milestone::probe_finished);
    if (!result.has_value() || result.value() != expected)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(result.value());
}
} // namespace

std::string codepage_japanese_input()
{
    return repeated("\x93\xfa\x96\x7b", 4194304U);
}

std::string utf16_japanese_input()
{
    return repeated("\xe5\x65\x2c\x67", 4194304U);
}

std::string utf16_ascii_input()
{
    return repeated(std::string_view("a\0", 2U), 8388608U);
}

std::string utf16_supplementary_input()
{
    return repeated("\x3d\xd8\x8b\xdd", 4194304U);
}

std::expected<std::uint64_t, ProbeFailure> converted_codepage(const std::string &input,
                                                              application::TimingPort &timing)
{
    const std::string expected = repeated("日本", 4194304U);
    adapters::win32::Win32CodePageAdapter adapter;
    timing.mark(core::Milestone::probe_started);
    const auto result = adapter.to_utf8(input);
    timing.mark(core::Milestone::probe_finished);
    if (!result.has_value() || result.value() != expected)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum_of(result.value());
}

std::expected<std::uint64_t, ProbeFailure> converted_utf16_japanese(const std::string &input,
                                                                    application::TimingPort &timing)
{
    return converted_units(input, repeated("日本", 4194304U), timing);
}

std::expected<std::uint64_t, ProbeFailure> converted_utf16_ascii(const std::string &input,
                                                                 application::TimingPort &timing)
{
    return converted_units(input, std::string(8388608U, 'a'), timing);
}

std::expected<std::uint64_t, ProbeFailure>
converted_utf16_supplementary(const std::string &input, application::TimingPort &timing)
{
    return converted_units(input, repeated("🖋", 4194304U), timing);
}
} // namespace nenenib::tests::performance
