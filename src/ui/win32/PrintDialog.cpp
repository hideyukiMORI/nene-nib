#include "PrintDialog.hpp"

#include "PrintPaperSizes.hpp"

#include <array>
#include <cderr.h>
#include <commdlg.h>
#include <memory>
#include <objbase.h>
#include <prntvpt.h>
#include <string>

namespace nenenib::ui::win32
{
namespace
{
// ダイアログが受けるページの範囲の数と、範囲に書けるページの上限（紙の大きさが決まる前なので
// ページの数はまだ分からない。範囲の外のページは描かない）。
constexpr DWORD range_capacity = 16;
constexpr DWORD highest_page = 0xFFFF;
// 印刷の print ticket の版（PTOpenProvider の dwVersion）。
constexpr DWORD ticket_version = 1;
using Global = std::unique_ptr<void, decltype(&GlobalFree)>;

[[nodiscard]] application::PrintFailure failure_of(DWORD error) noexcept
{
    return error == PDERR_NODEFAULTPRN || error == PDERR_NODEVICES
               ? application::PrintFailure::no_printer
               : application::PrintFailure::dialog_failed;
}

[[nodiscard]] std::wstring printer_name(HGLOBAL names)
{
    const auto *locked = static_cast<const DEVNAMES *>(GlobalLock(names));
    if (locked == nullptr)
    {
        return {};
    }
    // DEVNAMES の位置は先頭からの文字（wchar_t）の数。
    const auto *characters = static_cast<const wchar_t *>(static_cast<const void *>(locked));
    std::wstring name(characters + locked->wDeviceOffset);
    static_cast<void>(GlobalUnlock(names));
    return name;
}

// ダイアログの DEVMODE（部数・用紙・向きなど）を job の print ticket に写す。写せなければ空で、
// 印刷はプリンタの既定の設定になる。
[[nodiscard]] Microsoft::WRL::ComPtr<IStream> ticket_of(const std::wstring &printer, DEVMODEW &mode)
{
    HPTPROVIDER provider = nullptr;
    if (FAILED(PTOpenProvider(printer.c_str(), ticket_version, &provider)))
    {
        return {};
    }
    Microsoft::WRL::ComPtr<IStream> stream;
    const auto size = static_cast<ULONG>(mode.dmSize) + static_cast<ULONG>(mode.dmDriverExtra);
    const bool made =
        SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
        SUCCEEDED(PTConvertDevModeToPrintTicket(provider, size, &mode, kPTJobScope, stream.Get()));
    static_cast<void>(PTCloseProvider(provider));
    const LARGE_INTEGER origin{};
    if (!made || FAILED(stream->Seek(origin, STREAM_SEEK_SET, nullptr)))
    {
        return {};
    }
    return stream;
}

[[nodiscard]] PrintChoice choice_of(const PRINTDLGEXW &dialog,
                                    const std::array<PRINTPAGERANGE, range_capacity> &ranges)
{
    PrintChoice choice{printer_name(dialog.hDevNames), {}, PrintSetup{print_paper(DEVMODEW{}), {}}};
    auto *mode = static_cast<DEVMODEW *>(GlobalLock(dialog.hDevMode));
    if (mode != nullptr)
    {
        choice.setup.paper = print_paper(*mode);
        choice.ticket = ticket_of(choice.printer, *mode);
        static_cast<void>(GlobalUnlock(dialog.hDevMode));
    }
    if ((dialog.Flags & PD_PAGENUMS) != 0)
    {
        for (DWORD index = 0; index < dialog.nPageRanges && index < range_capacity; ++index)
        {
            choice.setup.ranges.push_back(
                PrintRange{ranges.at(index).nFromPage, ranges.at(index).nToPage});
        }
    }
    return choice;
}
} // namespace

std::expected<std::optional<PrintChoice>, application::PrintFailure> choose_printer(HWND owner)
{
    std::array<PRINTPAGERANGE, range_capacity> ranges{};
    PRINTDLGEXW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.Flags = PD_NOSELECTION | PD_NOCURRENTPAGE | PD_USEDEVMODECOPIESANDCOLLATE;
    dialog.nMaxPageRanges = range_capacity;
    dialog.lpPageRanges = ranges.data();
    dialog.nMinPage = 1;
    dialog.nMaxPage = highest_page;
    dialog.nCopies = 1;
    dialog.nStartPage = START_PAGE_GENERAL;
    const HRESULT shown = PrintDlgExW(&dialog);
    const Global modes(dialog.hDevMode, &GlobalFree);
    const Global names(dialog.hDevNames, &GlobalFree);
    if (FAILED(shown))
    {
        return std::unexpected(failure_of(CommDlgExtendedError()));
    }
    if (dialog.dwResultAction != PD_RESULT_PRINT)
    {
        return std::optional<PrintChoice>{};
    }
    return choice_of(dialog, ranges);
}
} // namespace nenenib::ui::win32
