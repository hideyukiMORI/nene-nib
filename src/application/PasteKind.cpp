#include "PasteKind.hpp"

#include <utility>

namespace nenenib::application
{
PasteKind paste_kind_of(core::Language language, bool has_html) noexcept
{
    if (!has_html)
    {
        return PasteKind::text;
    }
    switch (language)
    {
    case core::Language::markdown:
        return PasteKind::markdown_from_html;
    case core::Language::html:
    case core::Language::javascript:
    case core::Language::typescript:
        return PasteKind::html_as_is;
    case core::Language::plain:
    case core::Language::c:
    case core::Language::cpp:
    case core::Language::python:
    case core::Language::json:
    case core::Language::yaml:
    case core::Language::toml:
    case core::Language::xml:
    case core::Language::css:
    case core::Language::powershell:
    case core::Language::shell:
    case core::Language::cmake:
        return PasteKind::text;
    }
    std::unreachable();
}
} // namespace nenenib::application
