#include "StartScreen.hpp"

#include "OperationGuide.hpp"

#include <algorithm>

namespace nenenib::core
{
namespace
{
void append_paths(std::span<const FilePath> paths, StartEntryKind kind,
                  std::vector<StartEntry> &entries)
{
    const auto shown = std::min(paths.size(), start_section_limit);
    for (const FilePath &path : paths.first(shown))
    {
        entries.push_back(StartEntry{kind, path});
    }
}
} // namespace

std::vector<StartEntry> start_entries(std::span<const FilePath> recent,
                                      std::span<const FilePath> bookmarks)
{
    std::vector<StartEntry> entries;
    append_paths(recent, StartEntryKind::recent, entries);
    append_paths(bookmarks, StartEntryKind::bookmark, entries);
    entries.push_back(StartEntry{StartEntryKind::new_file, std::nullopt});
    entries.push_back(StartEntry{StartEntryKind::open_file, std::nullopt});
    entries.push_back(StartEntry{StartEntryKind::open_folder, std::nullopt});
    return entries;
}

std::array<GuideEntry, 3> start_key_entries(EditMode mode, DisplayLanguage language)
{
    return {operation_guide_entry(EditorOperation::list_files, mode, language),
            operation_guide_entry(EditorOperation::list_operations, mode, language),
            operation_guide_entry(EditorOperation::toggle_explorer, mode, language)};
}

std::array<GuideEntry, 3> start_key_entries(EditMode mode)
{
    return start_key_entries(mode, DisplayLanguage::japanese);
}

std::optional<std::size_t> start_selection_stepped(std::optional<std::size_t> selected,
                                                   std::size_t count, StartStep step) noexcept
{
    if (count == 0)
    {
        return std::nullopt;
    }
    switch (step)
    {
    case StartStep::next:
        return selected.has_value() ? (selected.value() + 1) % count : 0;
    case StartStep::previous:
        return selected.has_value() ? (selected.value() + count - 1) % count : count - 1;
    }
    return std::nullopt;
}
} // namespace nenenib::core
