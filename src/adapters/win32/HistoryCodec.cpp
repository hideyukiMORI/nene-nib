#include "HistoryCodec.hpp"

#include "AbsolutePath.hpp"
#include "TextLines.hpp"
#include "Utf8.hpp"

#include <utility>
#include <vector>

namespace nenenib::adapters::win32
{
namespace
{
using Failure = application::FileHistoryFailure;
using application::FileHistory;
using Lines = std::vector<std::string_view>;
} // namespace

std::expected<FileHistory, Failure> decode_history(std::string_view bytes)
{
    if (bytes.starts_with("\xEF\xBB\xBF"))
    {
        bytes.remove_prefix(3);
    }
    const Lines lines = text_lines(bytes);
    const std::string_view head = lines.empty() ? std::string_view{} : lines.front();
    if (!head.starts_with("version="))
    {
        return std::unexpected(Failure::malformed);
    }
    if (head != "version=1")
    {
        return std::unexpected(Failure::unsupported_version);
    }
    if (!core::validate_utf8(bytes))
    {
        return std::unexpected(Failure::malformed);
    }
    if (lines.size() - 1 > maximum_history_lines)
    {
        return std::unexpected(Failure::too_large);
    }
    FileHistory history;
    history.files.reserve(lines.size() - 1);
    for (std::size_t index = 1; index < lines.size(); ++index)
    {
        const std::string_view line = lines.at(index);
        auto path = core::FilePath::parse(line);
        if (!path || !rooted_path_text(line))
        {
            return std::unexpected(Failure::malformed);
        }
        history.files.push_back(std::move(path).value());
    }
    return history;
}

std::expected<std::string, Failure> encode_history(const FileHistory &history)
{
    if (history.files.size() > maximum_history_lines)
    {
        return std::unexpected(Failure::too_large);
    }
    std::string bytes = "version=1\n";
    for (const core::FilePath &path : history.files)
    {
        bytes += path.text();
        bytes += '\n';
        if (bytes.size() > maximum_history_bytes)
        {
            return std::unexpected(Failure::too_large);
        }
    }
    return bytes;
}
} // namespace nenenib::adapters::win32
