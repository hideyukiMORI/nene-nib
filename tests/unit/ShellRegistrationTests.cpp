// Issue #1087 / ADR 0178。Windows のエクスプローラーへの登録。鍵と値は core の純関数
// shell_registration_plan の 1 本が文字列で作り（決定 2）、試験はレジストリに触れずに文字列で
// 確かめる。
#include "LanguageTable.hpp"
#include "Scopes.hpp"
#include "ShellRegistration.hpp"
#include "ShellRegistrationEntry.hpp"
#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::tests
{
namespace
{
namespace core = nenenib::core;

constexpr std::string_view exe = "C:\\Tools\\NeNe Nib\\NeNeNib.exe";

[[nodiscard]] core::ShellRegistrationInput input_of(std::vector<std::string> extensions)
{
    return core::ShellRegistrationInput{std::string(exe), "NeNe Nib で開く", "NeNe Nib",
                                        std::move(extensions)};
}

[[nodiscard]] bool holds(const core::ShellRegistrationPlan &plan, std::string_view key,
                         std::string_view name, std::string_view value)
{
    return std::ranges::find(plan, core::ShellRegistrationEntry{std::string(key), std::string(name),
                                                                std::string(value)}) != plan.end();
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
} // namespace

void verify_shell_registration_contracts()
{
    verify_plan_entries();
    verify_plan_extensions();
    verify_plan_rejections();
    verify_registered_extensions();
}
} // namespace nenenib::tests
