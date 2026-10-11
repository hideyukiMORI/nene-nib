#include "FileDrop.hpp"
#include "WindowChecks.hpp"

#include <shlobj_core.h>

#include <bit>
#include <cstring>
#include <initializer_list>
#include <string>
#include <string_view>

namespace nenenib::tests::ui
{
namespace
{
// OS の DROPFILES と同じメモリを作る。窓も実ファイルも必要ない（ADR 0116）。
HDROP drop_of(std::initializer_list<std::wstring_view> names)
{
    std::wstring list;
    for (const auto name : names)
    {
        list.append(name);
        list.push_back(L'\0');
    }
    list.append(2, L'\0');
    const auto bytes = list.size() * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DROPFILES) + bytes);
    if (memory == nullptr)
    {
        return nullptr;
    }
    auto *storage = static_cast<char *>(GlobalLock(memory));
    if (storage == nullptr)
    {
        static_cast<void>(GlobalFree(memory));
        return nullptr;
    }
    const DROPFILES header{sizeof(DROPFILES), {0, 0}, FALSE, TRUE};
    std::memcpy(storage, &header, sizeof(header));
    std::memcpy(storage + sizeof(header), list.data(), bytes);
    static_cast<void>(GlobalUnlock(memory));
    return std::bit_cast<HDROP>(memory);
}

void verify_names()
{
    auto drop = drop_of({L"D:\\メモ 📄.txt"});
    expect(drop != nullptr, "a single-file drop is allocated");
    const auto single = nenenib::ui::win32::take_dropped_files(drop);
    expect(single.size() == 1 && single[0].has_value() &&
               single[0].value().text() == "D:\\メモ 📄.txt",
           "Japanese, spaces and supplementary characters reach the canonical UTF-8 path");
    expect(GlobalFlags(std::bit_cast<HGLOBAL>(drop)) == GMEM_INVALID_HANDLE,
           "the transfer memory is released before paths are returned");
    const std::wstring long_path = L"D:\\" + std::wstring(280, L'a') + L"\\file.txt";
    const auto multiple = nenenib::ui::win32::take_dropped_files(
        drop_of({L"D:\\first.txt", long_path, L"D:\\last.txt"}));
    expect(multiple.size() == 3 && multiple[0].has_value() && multiple[1].has_value() &&
               multiple[2].has_value(),
           "all files in a drop have validated paths");
    if (multiple.size() == 3 && multiple[0] && multiple[1] && multiple[2])
    {
        expect(multiple[0].value().text() == "D:\\first.txt" &&
                   multiple[2].value().text() == "D:\\last.txt",
               "OS path order is retained");
        expect(multiple[1].value().text() == "D:\\" + std::string(280, 'a') + "\\file.txt",
               "paths longer than MAX_PATH are not truncated");
    }
}

void verify_invalid_names()
{
    const std::wstring invalid_utf16{static_cast<wchar_t>(0xD800)};
    const auto drop = drop_of({L"D:\\bad\n.txt", invalid_utf16, L"D:\\good.txt"});
    const auto paths = nenenib::ui::win32::take_dropped_files(drop);
    expect(paths.size() == 3, "invalid names keep their place in the dropped list");
    if (paths.size() == 3)
    {
        expect(!paths[0] && paths[0].error() == application::FileFailure::unreadable,
               "path syntax errors use the existing unreadable failure");
        expect(!paths[1] && paths[1].error() == application::FileFailure::unreadable,
               "invalid UTF-16 uses the existing unreadable failure");
        expect(paths[2] && paths[2].value().text() == "D:\\good.txt",
               "a bad name does not discard subsequent valid names");
    }
    expect(GlobalFlags(std::bit_cast<HGLOBAL>(drop)) == GMEM_INVALID_HANDLE,
           "a drop containing errors also releases transfer memory");
}
} // namespace

void verify_file_drop()
{
    expect(nenenib::ui::win32::take_dropped_files(nullptr).empty(), "a null drop is empty");
    expect(nenenib::ui::win32::take_dropped_files(drop_of({})).empty(), "an empty drop is empty");
    verify_names();
    verify_invalid_names();
}
} // namespace nenenib::tests::ui
