#include "Win32Printer.hpp"

namespace nenenib::ui::win32
{
std::expected<application::PrintOutcome, application::PrintFailure>
Win32Printer::print(application::PrintJob job)
{
    // N6b が job の本文と字体で XPS の print control に描く。いまは受け取って捨てる。
    static_cast<void>(job);
    return std::unexpected(application::PrintFailure::no_printer);
}
} // namespace nenenib::ui::win32
