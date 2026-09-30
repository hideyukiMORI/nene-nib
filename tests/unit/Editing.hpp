#pragma once

#include "Appearance.hpp"
#include "EditorController.hpp"
#include "EditorPorts.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedThemes.hpp"
#include "ThemeCatalog.hpp"

#include <functional>
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
          controller_(nenenib::application::EditorPorts{
              appearance_, clipboard_, files_, code_pages_, settings_, themes_, session_, history_})
    {
    }

    // 起動で前回のタブを戻す契約の口（ADR 0059 の決定 6）。一覧の読みと、controller が起動のときに
    // 読むファイルを仕込んでから controller を作る。
    Editing(SessionReading listed, const std::function<void(ScriptedFiles &)> &prepare)
        : session_(std::move(listed)), controller_(prepared_ports(prepare))
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

    [[nodiscard]] ScriptedSession &session() noexcept
    {
        return session_;
    }

    [[nodiscard]] ScriptedHistory &history() noexcept
    {
        return history_;
    }

    [[nodiscard]] ScriptedCodePages &code_pages() noexcept
    {
        return code_pages_;
    }

  private:
    // controller より先に宣言したポートは、この時点でもう出来ている。
    [[nodiscard]] nenenib::application::EditorPorts
    prepared_ports(const std::function<void(ScriptedFiles &)> &prepare)
    {
        prepare(files_);
        return nenenib::application::EditorPorts{appearance_, clipboard_, files_,   code_pages_,
                                                 settings_,   themes_,    session_, history_};
    }

    ScriptedAppearance appearance_{Reading{Appearance::dark}};
    ScriptedClipboard clipboard_;
    ScriptedFiles files_;
    ScriptedCodePages code_pages_;
    ScriptedSettings settings_;
    ScriptedThemes themes_;
    ScriptedSession session_;
    ScriptedHistory history_;
    EditorController controller_;
};
} // namespace nenenib::tests
