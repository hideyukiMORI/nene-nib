#include "VimEndOfLine.hpp"

namespace nenenib::application
{
VimEndOfLine vim_end_of_line_decided(const core::TextBuffer &text, std::uint64_t entry)
{
    return VimEndOfLine{text.final_line_break_bytes() > 0, entry};
}
} // namespace nenenib::application
