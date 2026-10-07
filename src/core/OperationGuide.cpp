#include "OperationGuide.hpp"

#include "OperationBindings.hpp"
#include "OperationTexts.hpp"

namespace nenenib::core
{
namespace
{
constexpr std::array<EditorOperation, 3> guide_operations{
    EditorOperation::list_files, EditorOperation::open_file, EditorOperation::list_operations};

[[nodiscard]] GuideEntry guide_entry(EditorOperation operation, EditMode mode)
{
    const auto &text = operation_text(operation);
    const auto short_name = text.short_name.empty() ? text.name : text.short_name;
    const auto body_name = operation == EditorOperation::list_operations ? short_name : text.name;
    const auto chord = shown_chord(operation, mode);
    return GuideEntry{chord.has_value() ? key_chord_label(chord.value()) : std::string{}, body_name,
                      short_name};
}
} // namespace

OperationGuide operation_guide(GuideContext context, EditMode mode)
{
    OperationGuide result{context, {}};
    switch (context)
    {
    case GuideContext::hidden:
        return result;
    case GuideContext::untouched_untitled:
    case GuideContext::other:
        for (std::size_t index = 0; index < guide_operations.size(); ++index)
        {
            result.entries.at(index) = guide_entry(guide_operations.at(index), mode);
        }
        return result;
    }
    return result;
}
} // namespace nenenib::core
