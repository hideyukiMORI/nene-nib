#include "ModeLabel.hpp"

#include "UiMessage.hpp"
#include "UiTexts.hpp"

#include <utility>

namespace nenenib::core
{
namespace
{
[[nodiscard]] std::string_view vim_label(VimMode vim) noexcept
{
    switch (vim)
    {
    case VimMode::normal:
        return "NORMAL";
    case VimMode::insert:
        return "INSERT";
    case VimMode::visual:
        return "VISUAL";
    case VimMode::visual_line:
        return "VISUAL LINE";
    case VimMode::visual_block:
        return "VISUAL BLOCK";
    }
    std::unreachable();
}

// INSERT の `Ctrl-o` の命令を待つ間（Vim の showmode の `-- (insert) --` と `-- (insert) VISUAL
// --`・ ADR 0182 の決定 9）。INSERT のあいだは Vim と同じに INSERT のまま。
[[nodiscard]] std::string_view paused_label(VimMode vim) noexcept
{
    switch (vim)
    {
    case VimMode::normal:
        return "(insert)";
    case VimMode::insert:
        return "INSERT";
    case VimMode::visual:
        return "(insert) VISUAL";
    case VimMode::visual_line:
        return "(insert) VISUAL LINE";
    case VimMode::visual_block:
        return "(insert) VISUAL BLOCK";
    }
    std::unreachable();
}
} // namespace

std::string_view mode_label(EditMode mode, VimMode vim, bool insert_paused,
                            DisplayLanguage language) noexcept
{
    switch (mode)
    {
    case EditMode::ordinary:
        return ui_text(UiMessage::status_mode_basic, language);
    case EditMode::vim:
        break;
    }
    return insert_paused ? paused_label(vim) : vim_label(vim);
}
} // namespace nenenib::core
