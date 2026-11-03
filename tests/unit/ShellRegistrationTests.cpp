// Issue #1087 / ADR 0178。Windows のエクスプローラーへの登録。鍵と値は core の純関数
// shell_registration_plan の 1 本が文字列で作り（決定 2）、試験はレジストリに触れずに文字列で
// 確かめる。controller は 2 つの操作の意図のときだけ替え玉 ScriptedShellRegistration を呼ぶ
// （決定 3・4）。
#include "AutosaveTick.hpp"
#include "CancelCommand.hpp"
#include "Editing.hpp"
#include "InsertText.hpp"
#include "LanguageTable.hpp"
#include "OpenOperationList.hpp"
#include "RegisterShell.hpp"
#include "Scopes.hpp"
#include "ShellRegistration.hpp"
#include "ShellRegistrationEntry.hpp"
#include "ShellRegistrationFailure.hpp"
#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"
#include "ShellRegistrationRemoval.hpp"
#include "ShellRegistrationStatus.hpp"
#include "TestSupport.hpp"
#include "UnregisterShell.hpp"
#include "VimTestSupport.hpp"
#include "VisibleLines.hpp"
#include "WorkCompleted.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;
namespace app = nenenib::application;
using Failure = app::ShellRegistrationFailure;
using Status = app::ShellRegistrationStatus;

constexpr std::string_view windows_11_note = "（Windows 11 は「その他のオプションを表示」の中）";

constexpr std::string_view exe = "C:\\Tools\\NeNe Nib\\NeNeNib.exe";

[[nodiscard]] core::ShellRegistrationInput input_of(std::vector<std::string> extensions)
{
    return core::ShellRegistrationInput{std::string(exe), "NeNe Nib で開く", "NeNe Nib",
                                        std::move(extensions)};
}

[[nodiscard]] bool holds(const core::ShellRegistrationPlan &plan, std::string_view key,
                         std::string_view name, std::string_view value)
{
    return std::ranges::any_of(
        plan, [&](const core::ShellRegistrationEntry &entry)
        { return entry.key_path == key && entry.value_name == name && entry.value == value; });
}

// 決定 2: 右クリックの項目・「プログラムから開く」・ProgId・既定のアプリの鍵と値。
void verify_plan_entries()
{
    const auto planned = core::shell_registration_plan(input_of({".md"}));
    expect(planned.has_value(), "a plain exe path makes a plan");
    const auto &plan = planned.value();
    const std::string command = "\"C:\\Tools\\NeNe Nib\\NeNeNib.exe\" \"%1\"";
    const std::string icon = "\"C:\\Tools\\NeNe Nib\\NeNeNib.exe\",0";
    expect(holds(plan, "Software\\Classes\\*\\shell\\NeNeNib", "", "NeNe Nib で開く") &&
               holds(plan, "Software\\Classes\\*\\shell\\NeNeNib", "Icon", icon) &&
               holds(plan, "Software\\Classes\\*\\shell\\NeNeNib\\command", "", command),
           "the context menu verb has its name, the exe's icon and the quoted command");
    expect(
        holds(plan, "Software\\Classes\\Applications\\NeNeNib.exe", "FriendlyAppName",
              "NeNe Nib") &&
            holds(plan, "Software\\Classes\\Applications\\NeNeNib.exe\\shell\\open\\command", "",
                  command) &&
            holds(plan, "Software\\Classes\\Applications\\NeNeNib.exe\\SupportedTypes", ".md", ""),
        "the application key lists the friendly name, the command and the supported types");
    expect(holds(plan, "Software\\Classes\\NeNeNib.md", "", "md ファイル（NeNe Nib）") &&
               holds(plan, "Software\\Classes\\NeNeNib.md\\DefaultIcon", "", icon) &&
               holds(plan, "Software\\Classes\\NeNeNib.md\\shell\\open\\command", "", command),
           "each extension has a ProgId with a description, the exe's icon and the command");
    expect(
        holds(plan, "Software\\NeNeNib\\Capabilities", "ApplicationName", "NeNe Nib") &&
            holds(plan, "Software\\NeNeNib\\Capabilities", "ApplicationDescription", "NeNe Nib") &&
            holds(plan, "Software\\NeNeNib\\Capabilities\\FileAssociations", ".md", "NeNeNib.md") &&
            holds(plan, "Software\\RegisteredApplications", "NeNeNib",
                  "Software\\NeNeNib\\Capabilities"),
        "the capabilities associate each extension with its ProgId and are registered");
    expect(plan.size() == 13U && plan.front().key_path == "Software\\Classes\\*\\shell\\NeNeNib" &&
               plan.back().key_path == "Software\\RegisteredApplications",
           "the plan lists 13 values for one extension, from the verb to the registered "
           "application");
}

// 決定 2: 外すときは Nib の木の根ごと消し、共有の RegisteredApplications は値だけを消す。
void verify_plan_removals()
{
    const auto plan = core::shell_registration_plan(input_of({".md", ".txt"})).value();
    const auto shared =
        std::ranges::count_if(plan, [](const core::ShellRegistrationEntry &entry)
                              { return entry.removal == core::ShellRegistrationRemoval::value; });
    expect(shared == 1 && plan.back().removal == core::ShellRegistrationRemoval::value &&
               plan.back().key_path == "Software\\RegisteredApplications",
           "only the shared registered-applications value is removed by value");
    expect(core::shell_registration_subtrees(plan) ==
               std::vector<std::string>{"Software\\Classes\\*\\shell\\NeNeNib",
                                        "Software\\Classes\\Applications\\NeNeNib.exe",
                                        "Software\\Classes\\NeNeNib.md",
                                        "Software\\Classes\\NeNeNib.txt", "Software\\NeNeNib"},
           "the subtrees to remove are the verb, the application, each ProgId and the product key, "
           "once each in plan order");
}

// 決定 2: 拡張子は小文字・`.` 付き・重複なし（表の順）。空と `.` だけは捨てる。
void verify_plan_extensions()
{
    const auto plan =
        core::shell_registration_plan(input_of({"TXT", ".md", ".txt", "", ".", "Md"}));
    std::vector<std::string> supported;
    for (const auto &entry : plan.value())
    {
        if (entry.key_path == "Software\\Classes\\Applications\\NeNeNib.exe\\SupportedTypes")
        {
            supported.push_back(entry.value_name);
        }
    }
    expect(supported == std::vector<std::string>{".txt", ".md"},
           "extensions are lowercased, dotted and listed once in the given order");
    expect(holds(plan.value(), "Software\\Classes\\NeNeNib.txt", "", "txt ファイル（NeNe Nib）"),
           "the ProgId of a folded extension uses the lowercase name");
}

// 決定 2: パスの中の `"` は引用を壊すので拒み、空のパスも拒む（ARC-010）。
void verify_plan_rejections()
{
    auto quoted = input_of({".txt"});
    quoted.exe_path = "C:\\a\"b\\NeNeNib.exe";
    const auto rejected = core::shell_registration_plan(quoted);
    expect(!rejected.has_value() &&
               rejected.error() == core::ShellRegistrationPlanFailure::quote_in_path,
           "an exe path with a double quote is refused");
    auto empty = input_of({".txt"});
    empty.exe_path.clear();
    const auto missing = core::shell_registration_plan(empty);
    expect(!missing.has_value() &&
               missing.error() == core::ShellRegistrationPlanFailure::empty_path,
           "an empty exe path is refused");
}

// 決定 2: 登録する拡張子は文書の言語の表の拡張子の全部と `.txt`（別の表を書かない）。
void verify_registered_extensions()
{
    const auto extensions = core::shell_registration_extensions();
    const auto listed = core::listed_language_extensions();
    bool all_listed = extensions.size() == listed.size();
    for (std::size_t index = 0; all_listed && index < listed.size(); ++index)
    {
        all_listed = extensions[index] == "." + std::string(listed[index]);
    }
    expect(all_listed, "the registered extensions are the language table's, dotted, in order");
    expect(std::ranges::count(extensions, std::string(".txt")) == 1 &&
               std::ranges::count(extensions, std::string(".md")) == 1,
           "the registered extensions include .txt and .md once");
}
[[nodiscard]] std::string notice(const app::EditorFrame &frame)
{
    return frame.command_message.has_value() ? std::string(frame.command_message.value().text())
                                             : std::string("none");
}

[[nodiscard]] std::string with_note(std::string_view text)
{
    return std::string(text) + std::string(windows_11_note);
}

// 決定 3: 登録は exe のパスを読み、状態を確かめて 1 回書き、鳴らさない 1 行で知らせる。計画は
// 読んだパスと言語の表の拡張子から作る。
void verify_register()
{
    Editing editing;
    auto &controller = editing.controller();
    applied(controller, app::VisibleLines{3});
    applied(controller, app::InsertText{"ab"});
    auto &port = editing.shell_registration();
    expect(applied(controller, app::RegisterShell{}) == "ab" && port.paths() == 1 &&
               port.statuses() == 1 && port.applies() == 1 && port.removes() == 0 &&
               editing.bell().rings() == 0,
           "registering reads the exe path, checks the status and writes once without ringing");
    expect(notice(controller.frame()) == with_note("Windows の右クリックに登録しました"),
           "registering says so in one line with the Windows 11 note");
    const auto plan = port.last_plan().value_or(core::ShellRegistrationPlan{});
    expect(
        holds(plan, "Software\\Classes\\*\\shell\\NeNeNib\\command", "",
              "\"C:\\Tools\\NeNeNib\\NeNeNib.exe\" \"%1\"") &&
            holds(plan, "Software\\Classes\\*\\shell\\NeNeNib", "", "NeNe Nib で開く") &&
            holds(plan, "Software\\Classes\\Applications\\NeNeNib.exe\\SupportedTypes", ".txt",
                  "") &&
            holds(plan, "Software\\Classes\\Applications\\NeNeNib.exe\\SupportedTypes", ".md", ""),
        "the plan uses the port's exe path, the verb name and the language table's extensions");
    port.set_status(Status::registered_elsewhere);
    applied(controller, app::RegisterShell{});
    expect(port.applies() == 2 &&
               notice(controller.frame()) == with_note("以前の登録を置き換えました"),
           "a registration of an exe elsewhere is overwritten and said to be replaced");
}

// 決定 1・3: 失敗は値で受けて 1 行知らせる。パスを読めない・使えないときは書かない。
void verify_register_failures()
{
    Editing editing;
    auto &controller = editing.controller();
    auto &port = editing.shell_registration();
    port.fail_with(Failure::access_denied);
    applied(controller, app::RegisterShell{});
    expect(port.applies() == 1 &&
               notice(controller.frame()) == "登録できませんでした: アクセスが拒否されました" &&
               editing.bell().rings() == 0,
           "a refused write is said in one line without ringing");
    port.fail_with(std::nullopt);
    port.fail_path_with(Failure::io_error);
    applied(controller, app::RegisterShell{});
    expect(port.applies() == 1 && port.statuses() == 1 &&
               notice(controller.frame()) ==
                   "登録できませんでした: レジストリを読み書きできませんでした",
           "an unreadable exe path writes nothing");
    port.fail_path_with(std::nullopt);
    port.set_path("C:\\a\"b\\NeNeNib.exe");
    applied(controller, app::RegisterShell{});
    expect(port.applies() == 1 && port.statuses() == 1 &&
               notice(controller.frame()) == "登録できませんでした: exe のパスに \" があります",
           "an exe path with a double quote writes nothing");
}

// 決定 3: 外すのは登録があるときだけ。無ければ書かずに知らせる。
void verify_unregister()
{
    Editing editing;
    auto &controller = editing.controller();
    auto &port = editing.shell_registration();
    applied(controller, app::UnregisterShell{});
    expect(port.statuses() == 1 && port.removes() == 0 &&
               notice(controller.frame()) == "登録されていません",
           "unregistering without a registration writes nothing and says so");
    port.set_status(Status::registered);
    applied(controller, app::UnregisterShell{});
    expect(port.removes() == 1 && notice(controller.frame()) == "登録を外しました" &&
               editing.bell().rings() == 0,
           "unregistering removes the registration once and says so");
    port.set_status(Status::registered_elsewhere);
    port.fail_with(Failure::io_error);
    applied(controller, app::UnregisterShell{});
    expect(port.removes() == 2 &&
               notice(controller.frame()) ==
                   "登録を外せませんでした: レジストリを読み書きできませんでした",
           "a registration elsewhere is removed too, and a failed removal is said in one line");
}

// 決定 3・4: 操作は両モードで同じ。F1 の一覧を開く・打鍵の道は port を呼ばない。
void verify_vim_and_quiet()
{
    Editing editing;
    open_vim_document(editing, "ab\n");
    auto &controller = editing.controller();
    auto &port = editing.shell_registration();
    applied(controller, app::OpenOperationList{});
    applied(controller, app::CancelCommand{});
    vim_replay(controller, "ix<Esc>");
    applied(controller, app::AutosaveTick{});
    applied(controller, app::WorkCompleted{});
    expect(port.touches() == 0, "opening the operation list, typing, the autosave tick and the "
                                "work signal never touch the shell registration");
    applied(controller, app::RegisterShell{});
    expect(port.applies() == 1 && vim_body(controller.frame()) == "xab\n" &&
               notice(controller.frame()) == with_note("Windows の右クリックに登録しました"),
           "registering in Vim mode writes once and leaves the text alone");
}
} // namespace

void verify_shell_registration_contracts()
{
    verify_register();
    verify_register_failures();
    verify_unregister();
    verify_vim_and_quiet();
    verify_plan_entries();
    verify_plan_extensions();
    verify_plan_removals();
    verify_plan_rejections();
    verify_registered_extensions();
}
} // namespace nenenib::tests
