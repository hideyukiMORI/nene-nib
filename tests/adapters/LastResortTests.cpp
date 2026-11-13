// Issue #1625: 最後の砦（ADR 0137 の追補）を、本当に std::terminate を起こす子 process で通す。子は
// nib_last_resort_child（第 1 引数の経路）で、製品の合成ルートと同じ LastResortArming と
// arm_last_resort_thread を置く。置き場は temporary_test_directory（#1103）の中。子の終わりは
// 上限つきで待ち、止まったら子を殺して落とす（固まらないことを見る）。CTest の制限時間も付ける。
#include "LastResortArming.hpp"
#include "TemporaryTestDirectory.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace
{
namespace adapters = nenenib::adapters::win32;

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

// 1 つの mode の走り。前の走りのファイルを消してから起こす。
struct Ran
{
    std::optional<DWORD> code;
    std::optional<std::string> backup;
    std::optional<std::string> notice;
};

Ran ran(std::wstring_view mode, const std::wstring &folder)
{
    const std::wstring backup = folder + L"\\backup.txt";
    const std::wstring notice = folder + L"\\notice.txt";
    static_cast<void>(DeleteFileW(backup.c_str()));
    static_cast<void>(DeleteFileW(notice.c_str()));
    const auto code = run_child(mode, folder);
    return Ran{code, read_file(backup), read_file(notice)};
}

[[nodiscard]] bool ended_by_last_resort(const Ran &result)
{
    return result.code.has_value() && result.code.value() == adapters::last_resort_exit_code;
}

// (a) UI スレッドで noexcept を例外が抜ける: 退避の口を 1 回呼び、箱の替え玉に「退避した」を渡し、
// 決めた終了コードで終わる。
void verify_ui_thread(const std::wstring &folder)
{
    const Ran result = ran(L"ui", folder);
    expect(ended_by_last_resort(result), "ui: the process ends with last_resort_exit_code");
    expect(result.backup == std::optional<std::string>{"backup"},
           "ui: the backup hook ran on the UI thread");
    expect(result.notice == std::optional<std::string>{"1"},
           "ui: the notice hook was told the backup succeeded (no box in the test)");
}

// (b) ワーカーのスレッドで投げる: 状態は UI スレッドの持ち物なので退避せず、知らせず、決めた終了
// コードで終わる（arm_last_resort_thread が無ければ abort で終わる）。
void verify_worker_thread(const std::wstring &folder)
{
    const Ran result = ran(L"worker", folder);
    expect(ended_by_last_resort(result), "worker: the process ends with last_resort_exit_code");
    expect(!result.backup.has_value(), "worker: no backup is written from the worker thread");
    expect(!result.notice.has_value(), "worker: no notice from the worker thread");
}

// (c) 退避の中でもう一度例外: 退避の口が投げれば手続きが受けて「退避できなかった」と知らせる。
// noexcept を抜ければ手続きに 2 度目に入り、何もせずにすぐ終える。どちらも固まらない。
void verify_again(const std::wstring &folder)
{
    const Ran thrown = ran(L"again", folder);
    expect(ended_by_last_resort(thrown), "again: the process ends with last_resort_exit_code");
    expect(!thrown.backup.has_value() && thrown.notice == std::optional<std::string>{"0"},
           "again: a throwing backup is reported as not backed up");
    const Ran nested = ran(L"nested", folder);
    expect(ended_by_last_resort(nested), "nested: re-entering the procedure ends at once");
    expect(!nested.backup.has_value() && !nested.notice.has_value(),
           "nested: the second entry neither backs up nor notifies");
}

// (c) UI スレッドが mutex を握ったまま落ち、退避が同じ mutex を取りに行く（adapter のロックの
// 再入の形）。固まらずに決めた終了コードで終わる。MSVC STL の std::mutex は同じスレッドの 2 度目の
// lock で投げるので、手続きが受けて「退避できなかった」になる（2026-11-13 の Debug で確かめた）。
void verify_lock(const std::wstring &folder)
{
    const Ran result = ran(L"lock", folder);
    expect(ended_by_last_resort(result), "lock: the process ends with last_resort_exit_code");
    expect(!result.backup.has_value() && result.notice == std::optional<std::string>{"0"},
           "lock: a relocked mutex is reported as not backed up, without hanging");
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
    nenenib::tests::remove_temporary_test_directory(folder.value());
    std::printf("Last resort: %zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
