// Issue #1625: 最後の砦（ADR 0137 の追補）を、本当に std::terminate を起こす子 process で通す。子は
// nib_last_resort_child（第 1 引数の経路）で、製品の合成ルートと同じ LastResortArming と
// arm_last_resort_thread を置く。置き場は temporary_test_directory（#1103）の中。子の終わりは
// 上限つきで待ち、止まったら子を殺して落とす（固まらないことを見る）。CTest の制限時間も付ける。
#include "AutosaveId.hpp"
#include "AutosaveRemnant.hpp"
#include "FilePath.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Win32AutosaveAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace adapters = nenenib::adapters::win32;
namespace application = nenenib::application;
namespace core = nenenib::core;

// 子が終わるまでの上限（固まった子を落とすため・時計は読まない）。
constexpr DWORD child_wait_milliseconds = 30'000;

std::size_t &failures()
{
    static std::size_t count = 0;
    return count;
}

std::size_t &checks()
{
    static std::size_t count = 0;
    return count;
}

void expect(bool condition, const char *what)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", what);
    }
}

std::wstring &child_path()
{
    static std::wstring path;
    return path;
}

// 子が書いたファイルの中身（無ければ値なし）。
std::optional<std::string> read_file(const std::wstring &path)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    std::string text(64, '\0');
    DWORD read = 0;
    const bool done =
        ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) != FALSE;
    static_cast<void>(CloseHandle(file));
    if (!done)
    {
        return std::nullopt;
    }
    text.resize(read);
    return text;
}

// 子を 1 回起こし、終了コードを返す。上限までに終わらなければ殺して値なし。
std::optional<DWORD> run_child(std::wstring_view mode, const std::wstring &folder)
{
    std::wstring line =
        L"\"" + child_path() + L"\" " + std::wstring(mode) + L" \"" + folder + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(child_path().c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &startup, &process) == FALSE)
    {
        return std::nullopt;
    }
    static_cast<void>(CloseHandle(process.hThread));
    std::optional<DWORD> code;
    if (WaitForSingleObject(process.hProcess, child_wait_milliseconds) == WAIT_OBJECT_0)
    {
        DWORD exit = 0;
        if (GetExitCodeProcess(process.hProcess, &exit) != FALSE)
        {
            code = exit;
        }
    }
    else
    {
        static_cast<void>(TerminateProcess(process.hProcess, 1));
        static_cast<void>(WaitForSingleObject(process.hProcess, INFINITE));
    }
    static_cast<void>(CloseHandle(process.hProcess));
    return code;
}

// 子の終わり方が「手続きを通って既定の終わり方で終わった」形か。終了コードの値は CRT の版で
// 変わる（abort の 3・__fastfail の 0xC0000409）ので書かず、0 と、子が手続きを通らずに戻ったときの
// 値（2 は使い方の誤り・4 は escape_noexcept から戻った）でないことだけを見る。
[[nodiscard]] bool ended_abnormally(const std::optional<DWORD> &code)
{
    return code.has_value() && code.value() != 0 && code.value() != 2 && code.value() != 4 &&
           code.value() != STILL_ACTIVE;
}

// 1 つの mode の走り。前の走りのファイルを消してから起こす。
struct Ran
{
    std::optional<DWORD> code;
    std::optional<std::string> backup;
};

Ran ran(std::wstring_view mode, const std::wstring &folder)
{
    const std::wstring backup = folder + L"\\backup.txt";
    static_cast<void>(DeleteFileW(backup.c_str()));
    const auto code = run_child(mode, folder);
    return Ran{code, read_file(backup)};
}

// (a) UI スレッドで noexcept を例外が抜ける: 退避の口を 1 回呼び、箱を出さずに既定の終わり方で
// 終わる（固まらない）。
void verify_ui_thread(const std::wstring &folder)
{
    const Ran result = ran(L"ui", folder);
    expect(ended_abnormally(result.code), "ui: the process ends abnormally without hanging");
    expect(result.backup == std::optional<std::string>{"backup"},
           "ui: the backup hook ran on the UI thread");
}

// (b) ワーカーのスレッドで投げる: 状態は UI
// スレッドの持ち物なので退避せず、既定の終わり方で終わる。
void verify_worker_thread(const std::wstring &folder)
{
    const Ran result = ran(L"worker", folder);
    expect(ended_abnormally(result.code), "worker: the process ends abnormally without hanging");
    expect(!result.backup.has_value(), "worker: no backup is written from the worker thread");
}

// (c) 退避の中でもう一度例外: 退避の口が投げれば手続きが受け、noexcept を抜ければ手続きに 2 度目に
// 入って退避せずに既定へ渡す。どちらも固まらない。
void verify_again(const std::wstring &folder)
{
    const Ran thrown = ran(L"again", folder);
    expect(ended_abnormally(thrown.code), "again: the process ends abnormally without hanging");
    expect(!thrown.backup.has_value(), "again: a throwing backup writes nothing");
    const Ran nested = ran(L"nested", folder);
    expect(ended_abnormally(nested.code), "nested: re-entering the procedure ends at once");
    expect(!nested.backup.has_value(), "nested: the second entry does not back up");
}

// (c) UI スレッドが mutex を握ったまま落ち、退避が同じ mutex を取りに行く（adapter のロックの
// 再入の形）。固まらずに終わる。MSVC STL の std::mutex は同じスレッドの 2 度目の lock
// で投げるので、 手続きが受けて退避は書かれない（2026-11-13 の Debug と Release で確かめた）。
void verify_lock(const std::wstring &folder)
{
    const Ran result = ran(L"lock", folder);
    expect(ended_abnormally(result.code), "lock: the process ends abnormally without hanging");
    expect(!result.backup.has_value(), "lock: a relocked mutex writes no backup");
}

// 本物の自動退避の adapter（子と同じ置き場の
// work/autosave）で、子が落ちる前に書いた退避を、次の起動と 同じ remnants → read で拾う。子の
// process は終わっているので残骸になる（ADR 0137 の決定 6）。
void verify_real_remnant()
{
    const Ran result = ran(L"real", L".");
    expect(ended_abnormally(result.code), "real: the process ends abnormally without hanging");
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32AutosaveAdapter autosave(worker, files,
                                            core::FilePath::parse("work/autosave").value());
    const auto listed = autosave.remnants().value_or(std::vector<application::AutosaveRemnant>{});
    expect(listed.size() == 1 && listed.front().id == application::AutosaveId{5} &&
               !listed.front().path.has_value() && listed.front().title.text() == "無題",
           "real: the next start lists the backup of the ended process as a remnant");
    if (listed.size() != 1)
    {
        return;
    }
    const auto read = autosave.read(listed.front());
    const auto bytes = read.value_or(std::vector<std::byte>{});
    std::string text;
    for (const std::byte value : bytes)
    {
        text.push_back(static_cast<char>(value));
    }
    expect(read.has_value() && text == "kept by the last resort",
           "real: the remnant reads back the bytes backed up before the end");
    autosave.forget(listed.front());
}
} // namespace

int wmain(int argc, wchar_t **argv)
{
    const std::span<wchar_t *const> arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 2)
    {
        std::fprintf(stderr, "usage: nib_last_resort_tests <nib_last_resort_child.exe>\n");
        return 2;
    }
    // 置き場へ cwd を移す前に、子の経路を絶対の経路にしておく。
    std::wstring absolute(MAX_PATH * 4, L'\0');
    const DWORD length = GetFullPathNameW(arguments[1], static_cast<DWORD>(absolute.size()),
                                          absolute.data(), nullptr);
    if (length == 0 || length >= absolute.size())
    {
        std::fprintf(stderr, "FAIL: the child path cannot be resolved\n");
        return 1;
    }
    absolute.resize(length);
    child_path() = absolute;
    const auto folder = nenenib::tests::temporary_test_directory(L"last-resort");
    if (!folder)
    {
        std::fprintf(stderr, "FAIL: no temporary folder (%u)\n", folder.error());
        return 1;
    }
    verify_ui_thread(folder.value());
    verify_worker_thread(folder.value());
    verify_again(folder.value());
    verify_lock(folder.value());
    verify_real_remnant();
    nenenib::tests::remove_temporary_test_directory(folder.value());
    std::printf("Last resort: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
