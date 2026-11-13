#include "ResolvedLanguage.hpp"

#include <utility>

namespace nenenib::core
{
DisplayLanguage resolved_language(DisplayLanguageChoice choice,
                                  std::optional<DisplayLanguage> observed) noexcept
{
    switch (choice)
    {
    case DisplayLanguageChoice::system:
        return observed.value_or(DisplayLanguage::english);
    case DisplayLanguageChoice::japanese:
        return DisplayLanguage::japanese;
    case DisplayLanguageChoice::english:
        return DisplayLanguage::english;
    case DisplayLanguageChoice::simplified_chinese:
        return DisplayLanguage::simplified_chinese;
    case DisplayLanguageChoice::korean:
    case DisplayLanguageChoice::spanish:
    case DisplayLanguageChoice::german:
        return DisplayLanguage::english;
    }
    std::unreachable();
}
} // namespace nenenib::core
