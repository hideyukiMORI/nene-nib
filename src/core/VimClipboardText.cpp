#include "VimClipboardText.hpp"

#include "ClipboardText.hpp"
#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
VimRegister vim_register_of_clipboard(std::string_view text)
{
    // 畳む規則は clipboard_line_feeds の 1 本（ADR 0055 の決定 1）。ここは種類だけを決める。
    std::string folded = clipboard_line_feeds(text);
    const VimRegisterKind kind =
        folded.ends_with('\n') ? VimRegisterKind::lines : VimRegisterKind::characters;
    return VimRegister{std::move(folded), kind};
}
} // namespace nenenib::core
