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
    case core::Language::gdscript:
    case core::Language::dosini:
    case core::Language::gdresource:
    case core::Language::cs:
    case core::Language::rust:
    case core::Language::go:
    case core::Language::php:
    case core::Language::sql:
    case core::Language::lua:
    case core::Language::dockerfile:
    case core::Language::java:
    case core::Language::kotlin:
    case core::Language::dart:
    case core::Language::zig:
    case core::Language::terraform:
    case core::Language::make:
    case core::Language::batch:
    case core::Language::ruby:
    case core::Language::swift:
    case core::Language::scala:
    case core::Language::haskell:
    case core::Language::elixir:
    case core::Language::r:
    case core::Language::perl:
    case core::Language::vue:
    case core::Language::svelte:
        return PasteKind::text;
    }
    std::unreachable();
}
} // namespace nenenib::application
