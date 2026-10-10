#include "StatusItemsProbe.hpp"

#include "Milestone.hpp"
#include "StatusItems.hpp"

#include <array>
#include <limits>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
struct StatusCase
{
    core::TextPosition caret;
    core::TextEncoding encoding;
    core::LineEnding ending;
    std::string_view input;
    std::array<std::string_view, 3> labels;
    std::array<std::size_t, 3> code_points;
    std::uint64_t checksum;
};

static_assert(std::numeric_limits<std::size_t>::digits == 64);
constexpr auto maximum_position = std::numeric_limits<std::size_t>::max();
constexpr std::array<StatusCase, 6> cases{{
    {{core::LineNumber{1U}, core::Column{1U}},
     core::TextEncoding::utf8,
     core::LineEnding::crlf,
     "UTF-8|CRLF|1|1",
     {"行 1, 桁 1", "UTF-8", "CRLF"},
     {8U, 5U, 4U},
     38912U},
    {{core::LineNumber{9U}, core::Column{24U}},
     core::TextEncoding::utf8_bom,
     core::LineEnding::lf,
     "UTF-8 BOM|LF|9|24",
     {"行 9, 桁 24", "UTF-8 BOM", "LF"},
     {9U, 9U, 2U},
     45056U},
    {{core::LineNumber{123U}, core::Column{456U}},
     core::TextEncoding::shift_jis,
     core::LineEnding::crlf,
     "Shift_JIS|CRLF|123|456",
     {"行 123, 桁 456", "Shift_JIS", "CRLF"},
     {12U, 9U, 4U},
     55296U},
    {{core::LineNumber{maximum_position}, core::Column{maximum_position}},
     core::TextEncoding::utf8,
     core::LineEnding::lf,
     "UTF-8|LF|18446744073709551615|18446744073709551615",
     {"行 18446744073709551615, 桁 18446744073709551615", "UTF-8", "LF"},
     {46U, 5U, 2U},
     112640U},
    {{core::LineNumber{123456U}, core::Column{654321U}},
     core::TextEncoding::utf8_bom,
     core::LineEnding::crlf,
     "UTF-8 BOM|CRLF|123456|654321",
     {"行 123456, 桁 654321", "UTF-8 BOM", "CRLF"},
     {18U, 9U, 4U},
     67584U},
    {{core::LineNumber{1U}, core::Column{1U}},
     core::TextEncoding::shift_jis,
     core::LineEnding::lf,
     "Shift_JIS|LF|1|1",
     {"行 1, 桁 1", "Shift_JIS", "LF"},
     {8U, 9U, 2U},
     43008U},
}};

[[nodiscard]] const StatusCase &status_case(StatusItemsWorkload workload)
{
    return cases.at(static_cast<std::size_t>(workload));
}

[[nodiscard]] bool
status_matches(const std::array<core::DisplayText, core::status_item_count> &items,
               const StatusCase &expected)
{
    for (std::size_t index = 0; index < items.size(); ++index)
    {
        if (items.at(index).text() != expected.labels.at(index) ||
            items.at(index).code_point_count() != expected.code_points.at(index))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::uint64_t
status_checksum(const std::array<core::DisplayText, core::status_item_count> &items)
{
    std::uint64_t checksum = 0;
    for (const auto &item : items)
    {
        checksum += item.text().size() + item.code_point_count();
    }
    return checksum;
}
} // namespace

std::string status_items_input(StatusItemsWorkload workload)
{
    return std::string(status_case(workload).input);
}

std::expected<std::uint64_t, ProbeFailure> fixed_status_items(StatusItemsWorkload workload,
                                                              const std::string &input,
                                                              application::TimingPort &timing)
{
    const auto &sample = status_case(workload);
    if (input != sample.input)
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto initial = core::status_items_for(sample.caret, sample.encoding, sample.ending);
    if (!status_matches(initial, sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t iteration = 0; iteration < 1024U; ++iteration)
    {
        const auto items = core::status_items_for(sample.caret, sample.encoding, sample.ending);
        checksum += status_checksum(items);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || !status_matches(initial, sample) ||
        !status_matches(core::status_items_for(sample.caret, sample.encoding, sample.ending),
                        sample))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
