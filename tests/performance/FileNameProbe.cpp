#include "FileNameProbe.hpp"

#include "FilePath.hpp"
#include "Milestone.hpp"

#include <array>
#include <string_view>

namespace nenenib::tests::performance
{
namespace
{
struct FileNameCase
{
    std::string_view prefix;
    std::string_view segment;
    std::size_t repetitions;
    std::string_view suffix;
    std::string_view name;
    std::size_t name_repetitions;
    std::size_t offset;
    std::uint64_t checksum;
};

constexpr std::array<FileNameCase, 7> cases{{
    {"C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\note.txt", "", 0U, "", "note.txt", 1U,
     48U, 958464U},
    {"C:\\", "segment\\", 128U, "note.txt", "note.txt", 1U, 1027U, 958464U},
    {"C:/", "資料\\階層/", 64U, "日誌🖋.txt", "日誌🖋.txt", 1U, 899U, 1474560U},
    {"note.txt", "", 0U, "", "note.txt", 1U, 0U, 958464U},
    {"", "a", 1024U, "", "a", 1024U, 0U, 4988928U},
    {"C:\\", "segment\\", 128U, "", "", 0U, 1027U, 0U},
    {"/", "", 0U, "", "", 0U, 1U, 0U},
}};

[[nodiscard]] const FileNameCase &file_name_case(FileNameWorkload workload)
{
    return cases.at(static_cast<std::size_t>(workload));
}

[[nodiscard]] std::string repeated(std::string_view text, std::size_t count)
{
    std::string result;
    for (std::size_t index = 0; index < count; ++index)
    {
        result += text;
    }
    return result;
}

[[nodiscard]] bool name_matches(const core::FilePath &path, std::string_view name,
                                std::string_view expected, std::size_t offset)
{
    return name == expected && name.data() == path.text().data() + offset;
}

[[nodiscard]] std::uint64_t name_checksum(std::string_view name)
{
    if (name.empty())
    {
        return 0U;
    }
    return name.size() + static_cast<unsigned char>(name.front()) +
           static_cast<unsigned char>(name.back());
}
} // namespace

std::string file_name_input(FileNameWorkload workload)
{
    const auto &sample = file_name_case(workload);
    return std::string(sample.prefix) + repeated(sample.segment, sample.repetitions) +
           std::string(sample.suffix);
}

std::expected<std::uint64_t, ProbeFailure> fixed_file_name(FileNameWorkload workload,
                                                           const std::string &input,
                                                           application::TimingPort &timing)
{
    const auto &sample = file_name_case(workload);
    const auto parsed = core::FilePath::parse(input);
    if (!parsed || input != file_name_input(workload))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    const auto &path = parsed.value();
    const auto expected = repeated(sample.name, sample.name_repetitions);
    const auto initial = path.file_name();
    if (path.text() != input || !name_matches(path, initial, expected, sample.offset))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    std::uint64_t checksum = 0;
    timing.mark(core::Milestone::probe_started);
    for (std::size_t iteration = 0; iteration < 4096U; ++iteration)
    {
        const auto name = path.file_name();
        checksum += name_checksum(name);
    }
    timing.mark(core::Milestone::probe_finished);
    if (checksum != sample.checksum || path.text() != input ||
        !name_matches(path, initial, expected, sample.offset) ||
        !name_matches(path, path.file_name(), expected, sample.offset))
    {
        return std::unexpected(ProbeFailure::wrong_result);
    }
    return checksum;
}
} // namespace nenenib::tests::performance
