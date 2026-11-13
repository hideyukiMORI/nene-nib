// Issue #1625: 最後の砦の試験が起こす子（ADR 0137 の追補）。CTest には登録せず、親の試験
// nib_last_resort_tests が第 1 引数の経路から起こす。製品の合成ルートと同じ LastResortArming と
// arm_last_resort_thread を置き、本当に std::terminate を起こして手続きを通す。退避の口は替え玉で、
// 呼ばれたら置き場に backup.txt を書く（real だけは本物の自動退避の adapter で書く）。手続きは
// 退避の後に既定の終わり方（abort）へ渡すので、abort の箱と WER は wmain の頭で止める（製品は
// 止めない）。時計を読まない。
// 引数: <mode> <置き場>。置き場は親の cwd と同じ（子は cwd を受け継ぐ）。mode は
//   ui      UI スレッドで noexcept の関数を例外が抜ける（退避する）
//   worker  ワーカーのスレッドの仕事が投げる（Win32Worker::run は noexcept・退避しない）
//   again   退避の口が投げる（手続きが受けて既定へ渡す）
//   nested  退避の口の中で noexcept を例外が抜ける（手続きに 2 度目に入る・既定へ渡す）
//   lock    UI スレッドが mutex を握ったまま落ち、退避の口が同じ mutex を取りに行く
//   real    本物の Win32AutosaveAdapter（置き場の下の work/autosave）で 1 本を退避して落ちる
#include "AutosaveId.hpp"
#include "AutosaveOutcome.hpp"
#include "AutosaveSnapshot.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "LastResortArming.hpp"
#include "TextPosition.hpp"
#include "Win32AutosaveAdapter.hpp"
#include "Win32FileAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace adapters = nenenib::adapters::win32;
namespace application = nenenib::application;
namespace core = nenenib::core;

constexpr int usage_exit_code = 2;
// 手続きを通らずに戻ったとき（試験が落ちる値）。
constexpr int returned_exit_code = 4;
// real の退避の番号と本文（親が次の起動の残骸として読む）。
constexpr std::uint64_t real_id = 5;
constexpr std::string_view real_text = "kept by the last resort";

[[nodiscard]] std::wstring &place() noexcept
{
    static std::wstring folder;
    return folder;
}

[[nodiscard]] std::mutex &held() noexcept
{
    static std::mutex mutex;
    return mutex;
}

void write_file(std::wstring_view name, std::string_view text)
{
    const std::wstring path = place() + L"\\" + std::wstring(name);
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    static_cast<void>(
        WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr));
    static_cast<void>(CloseHandle(file));
}

[[noreturn]] void raise(const char *what)
{
    throw std::runtime_error(what);
}

// noexcept の境を例外が抜ける（製品の窓の手続きと Win32Worker::run と同じ形）。投げる関数は
// ポインタで受け、実行時に抜ける。
void escape_noexcept(void (*thrower)(const char *), const char *what) noexcept
{
    thrower(what);
}

// 子の振る舞い（引数の mode の名前の写し・名前は wmain の表で引く）。
enum class Mode : std::uint8_t
{
    ui,
    worker,
    again,
    nested,
    lock,
    real
};

constexpr std::array<std::pair<const wchar_t *, Mode>, 6> modes{{
    {L"ui", Mode::ui},
    {L"worker", Mode::worker},
    {L"again", Mode::again},
    {L"nested", Mode::nested},
    {L"lock", Mode::lock},
    {L"real", Mode::real},
}};

// 名前の照合は OS の順序なしの比較で行う（wstring_view の比較は UCRT の wmemcmp が argv の 2 バイト
// 境界の文字列を 8 バイトで読み、UBSan が止める）。
[[nodiscard]] std::optional<Mode> mode_of(const wchar_t *name)
{
    for (const auto &[text, mode] : modes)
    {
        if (CompareStringOrdinal(name, -1, text, -1, FALSE) == CSTR_EQUAL)
        {
            return mode;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool backed_up(Mode mode)
{
    switch (mode)
    {
    case Mode::again:
        raise("backup throws");
    case Mode::nested:
        escape_noexcept(&raise, "backup escapes noexcept");
        return false;
    case Mode::lock:
    {
        const std::scoped_lock lock(held());
        write_file(L"backup.txt", "backup");
        return true;
    }
    case Mode::ui:
    case Mode::worker:
    case Mode::real:
        break;
    }
    write_file(L"backup.txt", "backup");
    return true;
}

// 終わらない待ち（ワーカーの手続きが process を終えるまで）。止まれば親が上限で殺す。
void wait_forever()
{
    const HANDLE never = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    static_cast<void>(WaitForSingleObject(never, INFINITE));
}

[[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text)
{
    const auto view = std::as_bytes(std::span{text});
    return {view.begin(), view.end()};
}

// 本物の自動退避の adapter（製品と同じ worker・ファイルの port）で 1 本を退避して落ちる。退避は
// controller の back_up_before_defect と同じ store → flush → collect の順。
int run_real()
{
    adapters::Win32Worker worker;
    adapters::Win32FileAdapter files;
    adapters::Win32AutosaveAdapter autosave(worker, files,
                                            core::FilePath::parse("work/autosave").value());
    const auto backup = [&autosave]
    {
        autosave.store(application::AutosaveSnapshot{
            application::AutosaveId{real_id}, std::nullopt,
            core::DisplayText::parse("無題").value(), bytes_of(real_text),
            core::TextPosition{core::LineNumber{1}, core::Column{2}}});
        const bool flushed = autosave.flush();
        const auto outcomes = autosave.collect();
        return flushed &&
               std::ranges::none_of(outcomes, [](const application::AutosaveOutcome &done)
                                    { return done.failure.has_value(); });
    };
    const adapters::LastResortArming arming(backup);
    escape_noexcept(&raise, "ui throws after real edits");
    return returned_exit_code;
}

int run(Mode mode)
{
    if (mode == Mode::real)
    {
        return run_real();
    }
    const adapters::LastResortArming arming([mode] { return backed_up(mode); });
    if (mode == Mode::worker)
    {
        adapters::Win32Worker worker;
        worker.post(
            [](const std::stop_token &)
            {
                adapters::arm_last_resort_thread();
                raise("worker job throws");
            });
        wait_forever();
        return returned_exit_code;
    }
    if (mode == Mode::lock)
    {
        held().lock();
    }
    escape_noexcept(&raise, "ui throws");
    return returned_exit_code;
}
} // namespace

int wmain(int argc, wchar_t **argv)
{
    // 手続きは既定の終わり方（abort）へ渡す。Debug の CRT の「abort() has been called」の箱と WER
    // の記録で止まらない（親が上限で落とす）。製品には入れない（製品は WER を残す）。
    static_cast<void>(_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT));
    static_cast<void>(SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX));
    const std::span<wchar_t *const> arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 3)
    {
        return usage_exit_code;
    }
    const auto mode = mode_of(arguments[1]);
    if (!mode.has_value())
    {
        return usage_exit_code;
    }
    place() = arguments[2];
    return run(mode.value());
}
