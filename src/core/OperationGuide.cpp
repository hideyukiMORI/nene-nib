#include "OperationGuide.hpp"

#include "OperationBindings.hpp"
#include "OperationTexts.hpp"
#include "UiTexts.hpp"

namespace nenenib::core
{
namespace
{
constexpr std::array<EditorOperation, 3> guide_operations{
    EditorOperation::list_files, EditorOperation::open_file, EditorOperation::list_operations};
} // namespace

GuideEntry operation_guide_entry(EditorOperation operation, EditMode mode, DisplayLanguage language)
{
    const auto &text = operation_text(operation);
    const auto name = ui_text(text.name, language);
    const auto short_name =
        text.short_name.has_value() ? ui_text(text.short_name.value(), language) : name;
    const auto body_name = operation == EditorOperation::list_operations ? short_name : name;
    const auto chord = shown_chord(operation, mode);
    return GuideEntry{chord.has_value() ? key_chord_label(chord.value()) : std::string{}, body_name,
                      short_name};
}

OperationGuide operation_guide(GuideContext context, EditMode mode)
{
    return operation_guide(context, mode, DisplayLanguage::japanese);
}

OperationGuide operation_guide(GuideContext context, EditMode mode, DisplayLanguage language)
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
            result.entries.at(index) =
                operation_guide_entry(guide_operations.at(index), mode, language);
        }
        return result;
    }
    return result;
}
} // namespace nenenib::core
