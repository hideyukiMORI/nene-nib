#include "VimEndOfLine.hpp"

namespace nenenib::application
{
VimEndOfLine vim_end_of_line_decided(const core::TextBuffer &text)
{
    return VimEndOfLine{text.final_line_break_bytes() > 0, std::nullopt};
}

bool vim_end_of_line_up(const VimEndOfLine &flag, const core::EditHistory &history)
{
    if (!flag.opened)
    {
        return false;
    }
    if (!flag.lowered.has_value())
    {
        return true;
    }
    const core::EditHistory &lowered = flag.lowered.value();
    const std::size_t at = lowered.position();
    return at == 0 || history.position() < at || !history.shares_unit(lowered, at - 1);
}
} // namespace nenenib::application
