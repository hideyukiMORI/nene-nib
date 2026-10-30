#include "ShellRegistration.hpp"

#include "AsciiCase.hpp"
#include "LanguageTable.hpp"

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::core
{
namespace
{
// 右クリックの項目・アプリ・ProgId・既定のアプリの鍵（ADR 0178 の決定 2）。どれも HKCU からの副鍵。
constexpr std::string_view verb_key = "Software\\Classes\\*\\shell\\NeNeNib";
constexpr std::string_view application_key = "Software\\Classes\\Applications\\NeNeNib.exe";
constexpr std::string_view prog_id_prefix = "NeNeNib.";
constexpr std::string_view classes_key = "Software\\Classes\\";
constexpr std::string_view product_key = "Software\\NeNeNib";
constexpr std::string_view capabilities_key = "Software\\NeNeNib\\Capabilities";
constexpr std::string_view registered_applications_key = "Software\\RegisteredApplications";
constexpr std::string_view registered_application_name = "NeNeNib";
constexpr std::string_view command_suffix = "\\shell\\open\\command";
// ProgId の説明の後ろ（「md ファイル（NeNe Nib）」）。
constexpr std::string_view prog_id_description_suffix = " ファイル（NeNe Nib）";

[[nodiscard]] std::string joined(std::string_view head, std::string_view tail)
{
    std::string text(head);
    text += tail;
    return text;
}

// 引用した exe のパスと `%1`（空白を含むパスのため両方を `"` で囲む）。
[[nodiscard]] std::string command_of(std::string_view exe_path)
{
    std::string text = "\"";
    text += exe_path;
    text += "\" \"%1\"";
    return text;
}

// アイコンは exe の最初のアイコン（`"<exe>",0`）。
[[nodiscard]] std::string icon_of(std::string_view exe_path)
{
    std::string text = "\"";
    text += exe_path;
    text += "\",0";
    return text;
}

// 小文字・`.` 付きの拡張子に畳む。空なら空のまま返し、呼び出し元が捨てる。
[[nodiscard]] std::string normalized_extension(std::string_view extension)
{
    std::string text;
    if (extension.empty())
    {
        return text;
    }
    if (extension.front() != '.')
    {
        text += '.';
    }
    for (const char byte : extension)
    {
        text += static_cast<char>(ascii_folded(byte));
    }
    return text;
}

// 拡張子を表の順に小文字・`.` 付き・重複なしで並べる。`.` だけの拡張子は捨てる。
[[nodiscard]] std::vector<std::string> normalized_extensions(const std::vector<std::string> &given)
{
    std::vector<std::string> extensions;
    for (const auto &extension : given)
    {
        auto text = normalized_extension(extension);
        if (text.size() > 1 && std::ranges::find(extensions, text) == extensions.end())
        {
            extensions.push_back(std::move(text));
        }
    }
    return extensions;
}

// 右クリックの項目と「プログラムから開く」の鍵（決定 2 の 1 つ目と 2 つ目）。
void add_verb_and_application(ShellRegistrationPlan &plan, const ShellRegistrationInput &input,
                              const std::vector<std::string> &extensions)
{
    const auto command = command_of(input.exe_path);
    plan.push_back(
        {std::string(verb_key), "", input.display_name, ShellRegistrationRemoval::subtree});
    plan.push_back({std::string(verb_key), "Icon", icon_of(input.exe_path),
                    ShellRegistrationRemoval::subtree});
    plan.push_back({joined(verb_key, "\\command"), "", command, ShellRegistrationRemoval::subtree});
    plan.push_back({std::string(application_key), "FriendlyAppName", input.friendly_app_name,
                    ShellRegistrationRemoval::subtree});
    plan.push_back(
        {joined(application_key, command_suffix), "", command, ShellRegistrationRemoval::subtree});
    const auto supported = joined(application_key, "\\SupportedTypes");
    for (const auto &extension : extensions)
    {
        plan.push_back({supported, extension, "", ShellRegistrationRemoval::subtree});
    }
}

// 拡張子ごとの ProgId（決定 2 の 3 つ目）。ProgId の名前は `NeNeNib.` に `.` を除いた拡張子。
void add_prog_ids(ShellRegistrationPlan &plan, const ShellRegistrationInput &input,
                  const std::vector<std::string> &extensions)
{
    const auto command = command_of(input.exe_path);
    const auto icon = icon_of(input.exe_path);
    for (const auto &extension : extensions)
    {
        const std::string_view bare = std::string_view(extension).substr(1);
        const auto key = joined(classes_key, joined(prog_id_prefix, bare));
        plan.push_back(
            {key, "", joined(bare, prog_id_description_suffix), ShellRegistrationRemoval::subtree});
        plan.push_back({joined(key, "\\DefaultIcon"), "", icon, ShellRegistrationRemoval::subtree});
        plan.push_back(
            {joined(key, command_suffix), "", command, ShellRegistrationRemoval::subtree});
    }
}

// 設定アプリの「既定のアプリ」（決定 2 の 4 つ目）。
void add_capabilities(ShellRegistrationPlan &plan, const ShellRegistrationInput &input,
                      const std::vector<std::string> &extensions)
{
    plan.push_back({std::string(capabilities_key), "ApplicationName", input.friendly_app_name,
                    ShellRegistrationRemoval::subtree});
    plan.push_back({std::string(capabilities_key), "ApplicationDescription",
                    input.friendly_app_name, ShellRegistrationRemoval::subtree});
    const auto associations = joined(capabilities_key, "\\FileAssociations");
    for (const auto &extension : extensions)
    {
        plan.push_back({associations, extension,
                        joined(prog_id_prefix, std::string_view(extension).substr(1)),
                        ShellRegistrationRemoval::subtree});
    }
    plan.push_back({std::string(registered_applications_key),
                    std::string(registered_application_name), std::string(capabilities_key),
                    ShellRegistrationRemoval::value});
}
// key_path が属する Nib の木の根。右クリックの項目とアプリはその鍵、`Software\NeNeNib` の下は
// `Software\NeNeNib`、ほかの `Software\Classes\` の下（ProgId）は次の 1 段。
[[nodiscard]] std::string subtree_root_of(std::string_view key_path)
{
    for (const auto root : {verb_key, application_key, product_key})
    {
        if (key_path == root || key_path.starts_with(joined(root, "\\")))
        {
            return std::string(root);
        }
    }
    if (key_path.starts_with(classes_key))
    {
        const auto end = key_path.find('\\', classes_key.size());
        return std::string(key_path.substr(0, end));
    }
    return std::string(key_path);
}
// 外すときに木ごと消す値か（値を足すとこの switch がコンパイルで落ちる・CPP-002）。
[[nodiscard]] bool removes_subtree(ShellRegistrationRemoval removal) noexcept
{
    switch (removal)
    {
    case ShellRegistrationRemoval::subtree:
        return true;
    case ShellRegistrationRemoval::value:
        return false;
    }
    std::unreachable();
}
} // namespace

std::expected<ShellRegistrationPlan, ShellRegistrationPlanFailure>
shell_registration_plan(const ShellRegistrationInput &input)
{
    if (input.exe_path.empty())
    {
        return std::unexpected(ShellRegistrationPlanFailure::empty_path);
    }
    if (input.exe_path.find('"') != std::string::npos)
    {
        return std::unexpected(ShellRegistrationPlanFailure::quote_in_path);
    }
    const auto extensions = normalized_extensions(input.extensions);
    ShellRegistrationPlan plan;
    add_verb_and_application(plan, input, extensions);
    add_prog_ids(plan, input, extensions);
    add_capabilities(plan, input, extensions);
    return plan;
}

std::vector<std::string> shell_registration_subtrees(const ShellRegistrationPlan &plan)
{
    std::vector<std::string> roots;
    for (const auto &entry : plan)
    {
        if (!removes_subtree(entry.removal))
        {
            continue;
        }
        auto root = subtree_root_of(entry.key_path);
        if (std::ranges::find(roots, root) == roots.end())
        {
            roots.push_back(std::move(root));
        }
    }
    return roots;
}

std::vector<std::string> shell_registration_extensions()
{
    std::vector<std::string> given;
    for (const auto extension : listed_language_extensions())
    {
        given.emplace_back(extension);
    }
    given.emplace_back("txt");
    return normalized_extensions(given);
}
} // namespace nenenib::core
