#include "ScopedProbe.hpp"

#include "BufferProbe.hpp"
#include "ConversionProbe.hpp"
#include "PaletteProbe.hpp"

#include <array>

namespace nenenib::tests::performance
{
namespace
{
using InputFactory = std::string (*)();
using Runner = std::expected<std::uint64_t, ProbeFailure> (*)(const std::string &,
                                                              application::TimingPort &);
constexpr std::array<InputFactory, 12> inputs{
    crlf_buffer_input,       lf_buffer_input,         position_buffer_input,
    position_buffer_input,   named_palette_input,     located_palette_input,
    narrowing_palette_input, narrowing_palette_input, codepage_japanese_input,
    utf16_japanese_input,    utf16_ascii_input,       utf16_supplementary_input};
constexpr std::array<Runner, 12> runners{queried_lines,         queried_lines,
                                         queried_long_position, queried_scattered_position,
                                         listed_palette,        listed_palette,
                                         narrowed_palette,      left_palette,
                                         converted_codepage,    converted_utf16_japanese,
                                         converted_utf16_ascii, converted_utf16_supplementary};
static_assert(static_cast<std::size_t>(ProbeWorkload::utf16_supplementary) -
                  static_cast<std::size_t>(ProbeWorkload::scattered_crlf_lines) + 1U ==
              inputs.size());

[[nodiscard]] std::size_t scoped_index(ProbeWorkload workload)
{
    return static_cast<std::size_t>(workload) -
           static_cast<std::size_t>(ProbeWorkload::scattered_crlf_lines);
}
} // namespace

std::string scoped_input_of(ProbeWorkload workload)
{
    return inputs.at(scoped_index(workload))();
}

std::expected<std::uint64_t, ProbeFailure> run_scoped_workload(ProbeWorkload workload,
                                                               const std::string &input,
                                                               application::TimingPort &timing)
{
    return runners.at(scoped_index(workload))(input, timing);
}
} // namespace nenenib::tests::performance
