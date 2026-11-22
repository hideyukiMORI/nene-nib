#pragma once

#include "Appearance.hpp"
#include "BodyTextLayouts.hpp"
#include "Distribution.hpp"
#include "EditorController.hpp"
#include "EditorPorts.hpp"
#include "ScriptedAppearance.hpp"
#include "ScriptedAutosave.hpp"
#include "ScriptedBell.hpp"
#include "ScriptedBookmarks.hpp"
#include "ScriptedClipboard.hpp"
#include "ScriptedClock.hpp"
#include "ScriptedCodePages.hpp"
#include "ScriptedDocumentReads.hpp"
#include "ScriptedFiles.hpp"
#include "ScriptedFolders.hpp"
#include "ScriptedFonts.hpp"
#include "ScriptedHistory.hpp"
#include "ScriptedImages.hpp"
#include "ScriptedLanguageRoots.hpp"
#include "ScriptedLanguageServer.hpp"
#include "ScriptedLinkOpen.hpp"
#include "ScriptedMarkdown.hpp"
#include "ScriptedPreview.hpp"
#include "ScriptedPrinter.hpp"
#include "ScriptedProjectSearch.hpp"
#include "ScriptedReveal.hpp"
#include "ScriptedSession.hpp"
#include "ScriptedSettings.hpp"
#include "ScriptedShellRegistration.hpp"
#include "ScriptedStartLists.hpp"
#include "ScriptedThemes.hpp"
#include "ScriptedUiLanguage.hpp"

namespace nenenib::tests::preview
{
// 本文のスクロールの probe（#1905）の足場。tests/unit の Editing と同じ替え玉のポートに、
// 文字組みだけ製品の BodyTextLayouts（DirectWrite）を結ぶ。controller と renderer の写しが同じ
// 1 個を借りる（合成ルートと同じ形・ADR 0114）。参照を握る controller より先にポートを宣言する。
class ScrollProbeEditing final
{
  public:
    ScrollProbeEditing() : controller_(ports(), core::Distribution::unpackaged) {}

    [[nodiscard]] application::EditorController &controller() noexcept
    {
        return controller_;
    }

    [[nodiscard]] ScriptedFiles &files() noexcept
    {
        return files_;
    }

    [[nodiscard]] ui::win32::BodyTextLayouts &layouts() noexcept
    {
        return layouts_;
    }

  private:
    [[nodiscard]] application::EditorPorts ports()
    {
        return application::EditorPorts{
            appearance_, clipboard_,       files_,         code_pages_,     settings_,
            themes_,     session_,         history_,       folders_,        bookmarks_,
            layouts_,    autosave_,        fonts_,         bell_,           markdown_,
            reads_,      language_server_, wall_clock_,    printer_,        shell_registration_,
            images_,     start_lists_,     ui_language_,   project_search_, previews_,
            reveal_,     links_,           language_roots_};
    }

    ScriptedAppearance appearance_{Reading{core::Appearance::dark}};
    ScriptedClipboard clipboard_;
    ScriptedFiles files_;
    ScriptedCodePages code_pages_;
    ScriptedSettings settings_;
    ScriptedThemes themes_;
    ScriptedSession session_;
    ScriptedHistory history_;
    ScriptedFolders folders_;
    ScriptedBookmarks bookmarks_;
    ui::win32::BodyTextLayouts layouts_;
    ScriptedAutosave autosave_;
    ScriptedFonts fonts_;
    ScriptedBell bell_;
    ScriptedMarkdown markdown_;
    ScriptedDocumentReads reads_{files_, code_pages_};
    ScriptedLanguageServer language_server_;
    ScriptedClock wall_clock_;
    ScriptedPrinter printer_;
    ScriptedShellRegistration shell_registration_;
    ScriptedImages images_;
    ScriptedStartLists start_lists_;
    ScriptedUiLanguage ui_language_;
    ScriptedProjectSearch project_search_;
    ScriptedPreview previews_;
    ScriptedReveal reveal_;
    ScriptedLinkOpen links_;
    ScriptedLanguageRoots language_roots_;
    application::EditorController controller_;
};
} // namespace nenenib::tests::preview
