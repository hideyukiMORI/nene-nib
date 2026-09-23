#pragma once

#include "Appearance.hpp"
#include "EditorController.hpp"
#include "EditorPorts.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ThemeCatalog.hpp"

#include <optional>
#include <utility>

namespace nenenib::tests
{
using nenenib::application::EditorController;

// ポートと controller をまとめて持つ足場。参照を握る controller より先にポートを宣言する。
class Editing final
{
  public:
    explicit Editing(SettingsReading reading = std::nullopt,
                     nenenib::core::ThemeCatalog themes = nenenib::core::ThemeCatalog::builtins())
        : settings_(std::move(reading)), themes_(std::move(themes)),
          controller_(nenenib::application::EditorPorts{appearance_, clipboard_, files_,
                                                        code_pages_, settings_, themes_})
    {
    }

    [[nodiscard]] ScriptedSettings &settings() noexcept
    {
        return settings_;
    }

    [[nodiscard]] EditorController &controller() noexcept
    {
        return controller_;
    }

    [[nodiscard]] ScriptedClipboard &clipboard() noexcept
    {
        return clipboard_;
    }

    [[nodiscard]] ScriptedAppearance &appearance() noexcept
    {
        return appearance_;
    }

    [[nodiscard]] ScriptedFiles &files() noexcept
    {
        return files_;
    }

    [[nodiscard]] ScriptedCodePages &code_pages() noexcept
    {
        return code_pages_;
    }

  private:
    ScriptedAppearance appearance_{Reading{Appearance::dark}};
    ScriptedClipboard clipboard_;
    ScriptedFiles files_;
    ScriptedCodePages code_pages_;
    ScriptedSettings settings_;
    ScriptedThemes themes_;
    EditorController controller_;
};
} // namespace nenenib::tests
