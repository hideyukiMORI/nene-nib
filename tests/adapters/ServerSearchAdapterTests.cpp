// Issue #1945（LSP 段 S4）: 言語サーバーの探し（find_language_servers と
// Win32LanguageServerSearch）を、 作った一時のフォルダを置き場にして確かめる（本物の PATH
// に依らない）。一時フォルダは
// temporary_test_directory（#1103）の中に作る。時刻は読まず、待つのは合図の受け皿（セマフォ）だけで、
// 止まらないときは CTest の制限時間で落ちる。`--system`
// はこの機械の置き場（system_search_places）で 表の候補を探して見つかった物を出す道具で、CTest
// に載せない（何も起こさず、窓も出さない）。
#include "FoundLanguageServer.hpp"
#include "KnownLanguageServers.hpp"
#include "LanguageServerProbe.hpp"
#include "ServerSearch.hpp"
#include "ServerSearchPlaces.hpp"
#include "TemporaryTestDirectory.hpp"
#include "Utf16.hpp"
#include "Win32LanguageServerSearch.hpp"
#include "Win32Worker.hpp"

#include <windows.h>

#include <algorithm>
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
namespace adapters = nenenib::adapters::win32;
namespace app = nenenib::application;
namespace core = nenenib::core;
using Arrivals = std::counting_semaphore<>;

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

// 置き場 root の下の相対パス。
[[nodiscard]] std::wstring under(const std::wstring &root, std::wstring_view rest)
{
    return root + L"\\" + std::wstring{rest};
}

void make_folder(const std::wstring &path)
{
    expect(CreateDirectoryW(path.c_str(), nullptr) != 0, "a test folder is created");
}

void make_file(const std::wstring &path, std::string_view bytes)
{
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(file != INVALID_HANDLE_VALUE, "a test file is created");
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD written = 0;
    expect(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0,
           "a test file is written");
    CloseHandle(file);
}

[[nodiscard]] std::string utf8_of(const std::wstring &wide)
{
    return core::to_utf8(wide).value_or(std::string{});
}

// 作業フォルダ（一時フォルダ）の絶対パス。
[[nodiscard]] std::wstring current_folder()
{
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = GetCurrentDirectoryW(static_cast<DWORD>(path.size()), path.data());
    path.resize(length);
    return path;
}

// 置き場: bin（clangd.exe・gopls.cmd・lua-language-server.exe という名前のフォルダ）・tools
// （rust-analyzer.exe）・node（node.exe）・npm（pyright・外を指す包み・スクリプトの無い包み）。
struct Places
{
    std::wstring root;
    adapters::ServerSearchPlaces places;
};

[[nodiscard]] Places made_places()
{
    const std::wstring root = current_folder();
    for (const std::wstring_view folder :
         {L"bin", L"tools", L"node", L"npm", L"npm\\node_modules", L"npm\\node_modules\\pyright",
          L"npm\\node_modules\\pyright\\dist", L"npm\\node_modules\\evil",
          L"npm\\node_modules\\hollow", L"bin\\lua-language-server.exe"})
    {
        make_folder(under(root, folder));
    }
    make_file(under(root, L"bin\\clangd.exe"), "MZ");
    make_file(under(root, L"bin\\gopls.cmd"), "@echo off");
    make_file(under(root, L"tools\\rust-analyzer.exe"), "MZ");
    make_file(under(root, L"node\\node.exe"), "MZ");
    make_file(under(root, L"npm\\node_modules\\pyright\\package.json"),
              R"({"name":"pyright","bin":{"pyright-langserver":"./dist/langserver.index.js"}})");
    make_file(under(root, L"npm\\node_modules\\pyright\\dist\\langserver.index.js"), "//");
    make_file(under(root, L"npm\\node_modules\\evil\\package.json"),
              R"({"name":"evil","bin":{"evil":"../pyright/dist/langserver.index.js"}})");
    make_file(under(root, L"npm\\node_modules\\hollow\\package.json"),
              R"({"name":"hollow","bin":{"hollow":"cli.js"}})");
    return Places{root, adapters::ServerSearchPlaces{
                            {under(root, L"bin"), under(root, L"tools"), under(root, L"node")},
                            {under(root, L"npm")}}};
}

[[nodiscard]] std::vector<app::LanguageServerProbe> sample_probes()
{
    return {{"clangd", ""},
            {"rust-analyzer", ""},
            {"gopls", ""},
            {"lua-language-server", ""},
            {"pyright-langserver", "pyright"},
            {"evil", "evil"},
            {"hollow", "hollow"},
            {"x", "../npm"},
            {"node", ""}};
}

// 見つかった物の実行ファイルとスクリプトのパス（無ければ空）。
[[nodiscard]] std::string executable_of(const std::optional<app::FoundLanguageServer> &found)
{
    return found.has_value() ? std::string{found.value().executable.text()} : std::string{};
}

[[nodiscard]] std::string script_of(const std::optional<app::FoundLanguageServer> &found)
{
    if (!found.has_value())
    {
        return {};
    }
    const auto &script = found.value().script;
    return script.has_value() ? std::string{script.value().text()} : std::string{};
}

// 1 回の探し（ワーカーなし）。
void verify_found(const Places &made)
{
    const auto found = adapters::find_language_servers(sample_probes(), made.places, {});
    expect(found.size() == sample_probes().size(), "one result per candidate in the same order");
    expect(executable_of(found.at(0)) == utf8_of(under(made.root, L"bin\\clangd.exe")) &&
               script_of(found.at(0)).empty(),
           "an .exe in the first folder is found by its absolute path");
    expect(executable_of(found.at(1)) == utf8_of(under(made.root, L"tools\\rust-analyzer.exe")),
           "a later folder of the places is searched too");
    expect(!found.at(2).has_value() && !found.at(3).has_value(),
           "a .cmd and a folder named like the server are not found");
    expect(
        executable_of(found.at(4)) == utf8_of(under(made.root, L"node\\node.exe")) &&
            script_of(found.at(4)) ==
                utf8_of(under(made.root, L"npm\\node_modules\\pyright\\dist\\langserver.index.js")),
        "an npm server is node.exe and the script its package.json names");
    expect(!found.at(5).has_value() && !found.at(6).has_value() && !found.at(7).has_value(),
           "a bin outside its package, a missing script and a bad package name are not found");
    auto no_node = made.places;
    no_node.folders = {under(made.root, L"bin")};
    const auto without = adapters::find_language_servers(sample_probes(), no_node, {});
    expect(without.at(0).has_value() && !without.at(4).has_value(),
           "without node.exe in the folders an npm server is not found");
    expect(adapters::find_language_servers(sample_probes(), adapters::ServerSearchPlaces{}, {})
                   .at(0)
                   .has_value() == false,
           "no folders means nothing is found (the current folder is never searched)");
}

void verify_path_folders()
{
    const auto folders =
        adapters::absolute_path_folders(LR"(C:\a;"C:\b c";relative;;  D:\d  ;\\server\share;.)");
    expect(folders == std::vector<std::wstring>{LR"(C:\a)", LR"(C:\b c)", LR"(D:\d)",
                                                LR"(\\server\share)"},
           "PATH keeps only absolute folders, unquoted and trimmed");
    // UNC の段（`\\?\` も）は探さない。この機械の C: はネットワークのドライブではない。
    const auto local = adapters::local_places(
        {LR"(C:\a)", LR"(\\server\share)", LR"(\\?\C:\b)", LR"(relative)", LR"(C:\c)"});
    expect(local == std::vector<std::wstring>{LR"(C:\a)", LR"(C:\c)"},
           "UNC folders and relative folders are dropped and local drive folders are kept");
}

// 止める合図が来ていれば、在る物も探さずに「見つからない」で埋める（数と順は同じ・レビュー R1）。
void verify_stopped(const Places &made)
{
    std::stop_source source;
    source.request_stop();
    const auto found =
        adapters::find_language_servers(sample_probes(), made.places, source.get_token());
    expect(found.size() == sample_probes().size() &&
               std::ranges::none_of(found, [](const auto &one) { return one.has_value(); }),
           "a stop request leaves every candidate unsearched");
}

// ワーカー 1 本で探して、合図の後に 1 回だけ受け取る。
void verify_worker(const Places &made)
{
    adapters::Win32Worker worker;
    Arrivals arrived(0);
    adapters::Win32LanguageServerSearch search(worker, [&made] { return made.places; });
    search.bind([&arrived] { arrived.release(); });
    expect(!search.collect().has_value() && !worker.started(),
           "nothing is searched and the worker sleeps before the request");
    search.search(sample_probes());
    arrived.acquire();
    const auto found = search.collect();
    expect(found.has_value() && found.value().size() == sample_probes().size() &&
               found.value().at(0).has_value() && found.value().at(4).has_value(),
           "one request searches on the worker and delivers once");
    expect(!search.collect().has_value(), "the result is taken once");
}

// この機械の置き場で表の候補を探して出す（`--system`・CTest に載せない）。
int print_system()
{
    std::vector<app::LanguageServerProbe> probes;
    for (const auto &row : core::known_language_servers())
    {
        for (const auto &candidate : row.candidates)
        {
            probes.push_back(app::LanguageServerProbe{std::string{candidate.program},
                                                      std::string{candidate.package}});
        }
    }
    const auto places = adapters::system_search_places();
    std::printf("%zu folders, %zu npm roots\n", places.folders.size(), places.npm_roots.size());
    const auto found = adapters::find_language_servers(probes, places, {});
    for (std::size_t index = 0; index < probes.size(); ++index)
    {
        const std::string executable = executable_of(found.at(index));
        const std::string script = script_of(found.at(index));
        std::printf("%-30s %s %s\n", probes.at(index).program.c_str(),
                    executable.empty() ? "-" : executable.c_str(), script.c_str());
    }
    return 0;
}

int run_tests()
{
    const auto made = made_places();
    verify_found(made);
    verify_path_folders();
    verify_stopped(made);
    verify_worker(made);
    std::fprintf(stdout, "%zu checks, %zu failures\n", checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--system")
    {
        return print_system();
    }
    const auto directory = nenenib::tests::temporary_test_directory(L"server-search");
    if (!directory.has_value())
    {
        std::fprintf(stderr, "FAIL: the temporary test folder is not made (error %lu)\n",
                     static_cast<unsigned long>(directory.error()));
        return 1;
    }
    const int result = run_tests();
    nenenib::tests::remove_temporary_test_directory(directory.value());
    return result;
}
