#include "LanguageServerLog.hpp"

#include "LanguageTable.hpp"
#include "ServerLogLine.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace nenenib::application
{
namespace
{
// 残りに持つ生のバイトの上限。core::server_log_line の上限を 1 バイト超える分まで持てば、切った行に
// 「…」が付く（それより後ろは改行まで捨てる）。
constexpr std::size_t partial_limit = core::server_log_line_bytes + 1;

[[nodiscard]] std::string_view kind_mark(LanguageServerLogKind kind) noexcept
{
    switch (kind)
    {
    case LanguageServerLogKind::output:
        return "stderr";
    case LanguageServerLogKind::error:
        return "error";
    case LanguageServerLogKind::warning:
        return "warning";
    case LanguageServerLogKind::info:
        return "info";
    case LanguageServerLogKind::log:
        return "log";
    }
    return "log";
}

// 行の終わりの CR を 1 つ落とす（CR LF の行）。
[[nodiscard]] std::string_view without_carriage_return(std::string_view line) noexcept
{
    return line.ends_with('\r') ? line.substr(0, line.size() - 1) : line;
}
} // namespace

void LanguageServerLog::add_output(LanguageServerSlot slot, core::Language language,
                                   std::string_view bytes)
{
    const std::size_t index = slot_index(slot);
    auto &partial = partial_.at(index);
    while (!bytes.empty())
    {
        const std::size_t newline = bytes.find('\n');
        const std::string_view piece = bytes.substr(0, newline);
        const std::size_t room = partial_limit - std::min(partial.size(), partial_limit);
        partial.append(piece.substr(0, std::min(room, piece.size())));
        if (newline == std::string_view::npos)
        {
            return;
        }
        push(index, language, LanguageServerLogKind::output, without_carriage_return(partial));
        partial.clear();
        bytes.remove_prefix(newline + 1);
    }
}

void LanguageServerLog::end_output(LanguageServerSlot slot, core::Language language)
{
    const std::size_t index = slot_index(slot);
    auto &partial = partial_.at(index);
    if (partial.empty())
    {
        return;
    }
    push(index, language, LanguageServerLogKind::output, without_carriage_return(partial));
    partial.clear();
}

void LanguageServerLog::add_message(LanguageServerSlot slot, core::Language language,
                                    LanguageServerLogKind kind, std::string_view text)
{
    const std::size_t index = slot_index(slot);
    while (!text.empty())
    {
        const std::size_t newline = text.find('\n');
        const std::string_view line = without_carriage_return(text.substr(0, newline));
        if (!line.empty())
        {
            push(index, language, kind, line);
        }
        if (newline == std::string_view::npos)
        {
            return;
        }
        text.remove_prefix(newline + 1);
    }
}

bool LanguageServerLog::empty() const noexcept
{
    return std::ranges::all_of(lines_, [](const auto &lines) { return lines.empty(); });
}

std::string LanguageServerLog::text() const
{
    std::string joined;
    std::size_t size = 0;
    for (std::size_t index = 0; index < lines_.size(); ++index)
    {
        size += bytes_.at(index) + lines_.at(index).size() * 2;
    }
    joined.reserve(size);
    for (const auto &lines : lines_)
    {
        for (const std::string &line : lines)
        {
            joined += line;
            joined += "\r\n";
        }
    }
    return joined;
}

// 1 行を置き、席の上限を超えた分を古い行から捨てる。
void LanguageServerLog::push(std::size_t index, core::Language language, LanguageServerLogKind kind,
                             std::string_view raw)
{
    std::string line = "[";
    line += core::language_name(language);
    line += "][";
    line += kind_mark(kind);
    line += "] ";
    line += core::server_log_line(raw);
    auto &lines = lines_.at(index);
    auto &bytes = bytes_.at(index);
    bytes += line.size();
    lines.push_back(std::move(line));
    while (lines.size() > language_server_log_lines || bytes > language_server_log_bytes)
    {
        bytes -= lines.front().size();
        lines.pop_front();
    }
}
} // namespace nenenib::application
