#include "CompletionEntries.hpp"

#include "CompletionCandidates.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace nenenib::core
{
namespace
{
constexpr char ascii_case_gap = 'a' - 'A';

[[nodiscard]] constexpr char folded(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte + ascii_case_gap) : byte;
}

[[nodiscard]] bool starts_folded(std::string_view text, std::string_view prefix) noexcept
{
    return text.size() >= prefix.size() &&
           std::ranges::equal(text.substr(0, prefix.size()), prefix,
                              [](char left, char right) { return folded(left) == folded(right); });
}

[[nodiscard]] CompletionEntry entry_of(const LspCompletionItem &item)
{
    return CompletionEntry{item.label,       item.detail,     item.filter_text,
                           item.insert_text, item.edit_range, CompletionSource::language_server};
}

// 語 word が LSP の候補のどれかと同じ綴りか。
[[nodiscard]] bool spelled(std::span<const CompletionEntry> server, std::string_view word) noexcept
{
    return std::ranges::any_of(server, [word](const CompletionEntry &entry)
                               { return entry.label == word || entry.insert == word; });
}
} // namespace

std::vector<CompletionEntry> completion_entries_of(const LspCompletionList &list)
{
    std::vector<std::size_t> order(list.items.size());
    for (std::size_t index = 0; index < order.size(); ++index)
    {
        order[index] = index;
    }
    std::ranges::sort(order,
                      [&list](std::size_t left, std::size_t right)
                      {
                          const int compared =
                              list.items[left].sort_text.compare(list.items[right].sort_text);
                          return compared != 0 ? compared < 0 : left < right;
                      });
    std::vector<CompletionEntry> entries;
    entries.reserve(order.size());
    for (const std::size_t index : order)
    {
        entries.push_back(entry_of(list.items[index]));
    }
    return entries;
}

std::vector<CompletionEntry> completion_entries(std::span<const CompletionEntry> server,
                                                std::span<const CompletionCandidate> words)
{
    std::vector<CompletionEntry> entries(server.begin(), server.end());
    entries.reserve(server.size() + words.size());
    for (const CompletionCandidate &word : words)
    {
        if (!spelled(server, word.word))
        {
            entries.push_back(
                CompletionEntry{word.word, {}, word.word, word.word, std::nullopt, word.source});
        }
    }
    return entries;
}

std::vector<std::size_t> completion_matches(std::span<const CompletionEntry> entries,
                                            std::string_view prefix)
{
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        if (starts_folded(entries[index].filter, prefix))
        {
            matches.push_back(index);
        }
    }
    return matches;
}

bool completion_prefix_kept(std::string_view typed) noexcept
{
    return completion_prefix_start(typed, typed.size()) == 0;
}

std::string_view completion_source_mark(CompletionSource source) noexcept
{
    switch (source)
    {
    case CompletionSource::buffer:
        return ".";
    case CompletionSource::other_buffers:
        return "b";
    case CompletionSource::language_server:
        return "o";
    }
    std::unreachable();
}
} // namespace nenenib::core
