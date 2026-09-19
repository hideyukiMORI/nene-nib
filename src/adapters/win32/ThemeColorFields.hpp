#pragma once

#include "ColorField.hpp"
#include "Palette.hpp"
#include "SyntaxPalette.hpp"

#include <array>

namespace nenenib::adapters::win32
{
inline constexpr std::array<ColorField<core::RgbColor, core::SyntaxPalette>, 16> body_color_fields{{
    {"body.foreground", &core::SyntaxPalette::foreground},
    {"body.background", &core::SyntaxPalette::background},
    {"body.cursor", &core::SyntaxPalette::cursor},
    {"body.selection", &core::SyntaxPalette::selection},
    {"body.current_line", &core::SyntaxPalette::current_line},
    {"body.line_number", &core::SyntaxPalette::line_number},
    {"body.comment", &core::SyntaxPalette::comment},
    {"body.keyword", &core::SyntaxPalette::keyword},
    {"body.string", &core::SyntaxPalette::string},
    {"body.number", &core::SyntaxPalette::number},
    {"body.type", &core::SyntaxPalette::type},
    {"body.function", &core::SyntaxPalette::function},
    {"body.constant", &core::SyntaxPalette::constant},
    {"body.operator", &core::SyntaxPalette::operators},
    {"body.error", &core::SyntaxPalette::error},
    {"body.warning", &core::SyntaxPalette::warning},
}};

inline constexpr std::array<ColorField<core::RgbColor, core::Palette>, 14> ui_rgb_fields{{
    {"ui.background", &core::Palette::background},
    {"ui.text", &core::Palette::text},
    {"ui.muted", &core::Palette::muted},
    {"ui.gutter", &core::Palette::gutter},
    {"ui.current_line", &core::Palette::current_line},
    {"ui.title_bar", &core::Palette::title_bar},
    {"ui.tab_active", &core::Palette::tab_active},
    {"ui.status", &core::Palette::status},
    {"ui.accent", &core::Palette::accent},
    {"ui.toggle", &core::Palette::toggle},
    {"ui.on_accent", &core::Palette::on_accent},
    {"ui.panel", &core::Palette::panel},
    {"ui.panel_border", &core::Palette::panel_border},
    {"ui.ime", &core::Palette::ime},
}};

inline constexpr std::array<ColorField<core::RgbaColor, core::Palette>, 2> ui_rgba_fields{{
    {"ui.selection", &core::Palette::selection},
    {"ui.search", &core::Palette::search},
}};
} // namespace nenenib::adapters::win32
