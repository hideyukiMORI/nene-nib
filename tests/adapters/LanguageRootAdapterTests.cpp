// Issue #1940（ADR 0161 の 2026-11-22 の追補・LSP 段 S3）: 言語サーバーの root を印で探す本物
// （Win32LanguageRootAdapter）を、temporary_test_directory（#1103）の中の本物のフォルダとワーカー
// 1 本で測る。印はファイルでもフォルダでもよく、最初に印のあるフォルダで止まり、止める合図の後は
// 見ない。経路は一時フォルダの中の相対の経路なので、上るのは置き場の中だけ（外の `.git`
// に届かない）。 時刻は読まない。待つのは合図の受け皿（セマフォ）だけで、止まらないときは CTest
// の制限時間で落ちる。
#include "FilePath.hpp"
#include "Language.hpp"
#include "LanguageRootAnswer.hpp"
#include "LanguageRootRequest.hpp"
#include "LanguageRootShelf.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Win32LanguageRootAdapter.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <optional>
#include <semaphore>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
namespace adapters = nenenib::adapters::win32;

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

void expect(bool condition, const char *message)
{
    ++checks();
    if (!condition)
    {
        ++failures();
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

core::FilePath path_of(std::string_view text)
{
    return core::FilePath::parse(text).value();
}

void make_folder(const wchar_t *name)
{
    expect(CreateDirectoryW(name, nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS,
           "a sample folder is made");
}

void make_file(const wchar_t *name)
{
    const HANDLE file =
        CreateFileW(name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a sample marker file is made");
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
}

// roots\.git（フォルダ）・roots\crate\Cargo.toml（ファイル）・roots\crate\src\deep の木。
void make_tree()
{
    make_folder(L"roots");
    make_folder(L"roots\\.git");
    make_folder(L"roots\\crate");
    make_folder(L"roots\\crate\\src");
    make_folder(L"roots\\crate\\src\\deep");
    make_file(L"roots\\crate\\Cargo.toml");
}

void verify_on_disk()
{
    const std::stop_source running;
    const auto rust = adapters::language_root_on_disk(path_of("roots\\crate\\src\\deep"),
                                                      core::Language::rust, running.get_token());
    expect(rust.has_value() && rust.value().text() == "roots\\crate",
           "a Rust file finds the folder of Cargo.toml (a marker file)");
    const auto plain = adapters::language_root_on_disk(path_of("roots\\crate\\src"),
                                                       core::Language::cs, running.get_token());
    expect(plain.has_value() && plain.value().text() == "roots",
           "a language with only .git finds the folder of .git (a marker folder)");
    std::stop_source stopped;
    stopped.request_stop();
    expect(!adapters::language_root_on_disk(path_of("roots\\crate\\src"), core::Language::rust,
                                            stopped.get_token())
                .has_value(),
           "after the stop request nothing is looked at");
}

// 今のフォルダ（一時フォルダ）の絶対の経路を UTF-8 で。取れなければ空。
std::string current_folder_utf8()
{
    std::wstring wide(MAX_PATH * 4, L'\0');
    const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(wide.size()), wide.data());
    if (length == 0 || length >= wide.size())
    {
        return {};
    }
    wide.resize(length);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string text(static_cast<std::size_t>(bytes > 0 ? bytes : 0), '\0');
    if (bytes > 0)
    {
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), text.data(),
                            bytes, nullptr, nullptr);
    }
    return text;
}

// `\\?\` の経路でも同じ印を見つける（#1944 のレビュー R1）。上る列の根で止まるかは core の純関数
// の試験（--language-root の verify_climb_floor）が UNC と `\\?\` の文字で確かめる。
void verify_long_prefix()
{
    const std::string here = current_folder_utf8();
    expect(!here.empty() && here.size() > 2 && here[1] == ':',
           "the temporary folder has a drive path");
    if (here.empty() || here.size() <= 2 || here[1] != ':')
    {
        return;
    }
    const std::string base = "\\\\?\\" + here + "\\roots";
    const std::stop_source running;
    const auto rust = adapters::language_root_on_disk(path_of(base + "\\crate\\src\\deep"),
                                                      core::Language::rust, running.get_token());
    expect(rust.has_value() && rust.value().text() == base + "\\crate",
           "a \\\\?\\ path finds the folder of Cargo.toml");
    const auto plain = adapters::language_root_on_disk(path_of(base + "\\crate\\src"),
                                                       core::Language::cs, running.get_token());
    expect(plain.has_value() && plain.value().text() == base,
           "a \\\\?\\ path finds the folder of .git");
}

void verify_on_worker()
{
    adapters::Win32Worker worker;
    adapters::Win32LanguageRootAdapter roots(worker);
    std::counting_semaphore<> arrived{0};
    roots.bind([&arrived] { arrived.release(); });
    roots.find(app::LanguageRootRequest{path_of("roots\\crate\\src"), core::Language::rust});
    arrived.acquire();
    const auto answers = roots.collect();
    const auto found = answers.empty() ? std::nullopt : answers.front().found;
    expect(answers.size() == 1 && answers.front().request.folder.text() == "roots\\crate\\src" &&
               found.value_or(path_of("none")).text() == "roots\\crate" && roots.collect().empty(),
           "the worker answers the request once with the found root and signals");
    adapters::LanguageRootShelf shelf;
    std::size_t signals = 0;
    shelf.bind([&signals] { ++signals; });
    shelf.deliver(app::LanguageRootAnswer{
        app::LanguageRootRequest{path_of("a"), core::Language::go}, std::nullopt});
    shelf.deliver(app::LanguageRootAnswer{
        app::LanguageRootRequest{path_of("b"), core::Language::go}, std::nullopt});
    expect(signals == 1 && shelf.take().size() == 2, "the shelf signals once when it fills");
    shelf.close();
    shelf.deliver(app::LanguageRootAnswer{
        app::LanguageRootRequest{path_of("c"), core::Language::go}, std::nullopt});
    expect(signals == 1 && shelf.take().empty(), "a closed shelf keeps nothing");
}
} // namespace

int main()
{
    const auto directory = nenenib::tests::temporary_test_directory(L"language-root");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    make_tree();
    verify_on_disk();
    verify_long_prefix();
    verify_on_worker();
    std::printf("Language root adapter: %zu checks, %zu failures\n", checks(), failures());
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return failures() == 0 ? 0 : 1;
}
