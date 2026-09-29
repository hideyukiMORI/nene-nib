#include "VimClipboardText.hpp"

#include "VimRegister.hpp"
#include "VimRegisterKind.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nenenib::core
{
VimRegister vim_register_of_clipboard(std::string_view text)
{
    std::string folded;
    folded.reserve(text.size());
    bool after_carriage_return = false;
    for (const char unit : text)
    {
        // `\r\n` の `\r` は直前に積んだ 1 文字なので、`\n` を積む前に下ろす。
        if (unit == '\n' && after_carriage_return)
        {
            folded.pop_back();
        }
        folded.push_back(unit);
        after_carriage_return = unit == '\r';
    }
    const VimRegisterKind kind =
        folded.ends_with('\n') ? VimRegisterKind::lines : VimRegisterKind::characters;
    return VimRegister{std::move(folded), kind};
}
} // namespace nenenib::core
