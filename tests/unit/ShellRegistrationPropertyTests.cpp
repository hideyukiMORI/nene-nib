// scope `--shell-registration-property` の性質の試験（#1289・ADR 0178 の決定 2・ADR 0042）。
// エクスプローラーへの登録の計画（ShellRegistration.hpp）に、固定 seed の決定的な生成器で作った材料
// （exe のパス・空白と多バイトを含む・空と `"` を含む失敗・拡張子の組は重複・空・`.` だけ・大小・
// 長い・記号・`\`）を渡し、計画の値だけを見て次を確かめる。実機のレジストリには触れない。
//   1. 計画の鍵は全部 Nib の木の中: 右クリックの項目 `Software\Classes\*\shell\NeNeNib`・アプリ
//      `Software\Classes\Applications\NeNeNib.exe`・ProgId `Software\Classes\NeNeNib.<…>`・
//      `Software\NeNeNib` のどれかの根か、その下。共有の鍵 `Software\RegisteredApplications` には
//      値 `NeNeNib` を 1 つだけ書き、外すときはその値だけ（removal が value）。
//   2. 「外す」の根（shell_registration_subtrees）は重複なく Nib
//   の木の根だけで、計画が書いた木だけを
//      指し、木ごと消す値の鍵はどれかの根の内にある。根の列は模型（右クリック・アプリ・拡張子ごとの
//      ProgId・`Software\NeNeNib`）と同じ。
//   3. 拡張子は模型（空を捨て、`.` を足し、ASCII の大文字だけを小文字に畳み、`.` だけを捨て、最初の
//      出現の順に重複を除く）と同じ列で、SupportedTypes と FileAssociations と ProgId に同じ順で
//      現れる。値の数は模型の数から決まり、コマンドとアイコンは引用した exe のパスを持つ。
//   4. 拡張子の列を 2 回繰り返しても、大小を入れ替えても計画は同じ。同じ入力で同じ計画。
// 回数は Debug（ASan）で scope 全体が 1 秒以内に終わる値。生成器は std::mt19937_64
// の剰余だけを使い、 分布の型・時刻・環境変数は使わない。
#include "Scopes.hpp"
#include "ShellRegistration.hpp"
#include "ShellRegistrationEntry.hpp"
#include "ShellRegistrationInput.hpp"
#include "ShellRegistrationPlan.hpp"
#include "ShellRegistrationPlanFailure.hpp"
#include "ShellRegistrationRemoval.hpp"
#include "TestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nenenib::tests
{
namespace
{
using core::ShellRegistrationEntry;
using core::ShellRegistrationInput;
using core::ShellRegistrationPlan;
using core::ShellRegistrationPlanFailure;
using core::ShellRegistrationRemoval;
using Random = std::mt19937_64;
using Strings = std::vector<std::string>;

constexpr std::size_t rounds = 1U;
constexpr std::size_t plan_inputs = 250U * rounds;
constexpr std::size_t maximum_extensions = 16U;
constexpr std::string_view verb_root = "Software\\Classes\\*\\shell\\NeNeNib";
constexpr std::string_view application_root = "Software\\Classes\\Applications\\NeNeNib.exe";
constexpr std::string_view prog_id_head = "Software\\Classes\\NeNeNib.";
constexpr std::string_view product_root = "Software\\NeNeNib";
constexpr std::string_view shared_key = "Software\\RegisteredApplications";

constexpr std::array<std::string_view, 24> extension_pieces{{
    "",  ".", "..", "txt", ".txt", "TXT", ".Md", "md",   "c++",      "h",   "Hpp",     ".tar.gz",
    " ", "*", "=",  "\\",  "a\\b", "\\x", ".\\", "日本", "\xC3\x89", "ps1", "NeNeNib", ".shell",
}};

constexpr std::array<std::string_view, 6> exe_paths{{
    "C:\\Program Files\\NeNe Nib\\NeNeNib.exe",
    "D:\\x\\NeNeNib.exe",
    "\\\\server\\share\\日本語\\NeNeNib.exe",
    "C:\\a b\\%1\\n.exe",
    "",
    "C:\\a\"b\\NeNeNib.exe",
}};

[[nodiscard]] std::size_t below(Random &random, std::size_t bound)
{
    return static_cast<std::size_t>(random() % bound);
}

// 表の断片か、印字できる ASCII の任意の並び（1〜300 バイト）。
[[nodiscard]] std::string random_extension(Random &random)
{
    if (below(random, 3U) != 0U)
    {
        return std::string(extension_pieces.at(below(random, extension_pieces.size())));
    }
    std::string text;
    const std::size_t length = 1U + below(random, below(random, 4U) == 0U ? 300U : 8U);
    for (std::size_t at = 0; at < length; ++at)
    {
        text.push_back(static_cast<char>(0x20U + below(random, 0x5FU)));
    }
    return text;
}

[[nodiscard]] ShellRegistrationInput random_input(Random &random)
{
    ShellRegistrationInput input{std::string(exe_paths.at(below(random, exe_paths.size()))),
                                 "NeNe Nib で開く",
                                 "NeNe Nib",
                                 {}};
    const std::size_t count = below(random, maximum_extensions + 1U);
    for (std::size_t index = 0; index < count; ++index)
    {
        input.extensions.push_back(random_extension(random));
    }
    return input;
}

// --- 模型 ---

[[nodiscard]] char lowered(char byte) noexcept
{
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : byte;
}

[[nodiscard]] Strings model_extensions(const Strings &given)
{
    Strings extensions;
    for (const std::string &extension : given)
    {
        if (extension.empty())
        {
            continue;
        }
        std::string text = extension.front() == '.' ? "" : ".";
        for (const char byte : extension)
        {
            text.push_back(lowered(byte));
        }
        if (text != "." && std::ranges::find(extensions, text) == extensions.end())
        {
            extensions.push_back(text);
        }
    }
    return extensions;
}

[[nodiscard]] bool within(std::string_view key, std::string_view root)
{
    return key == root ||
           (key.starts_with(root) && key.size() > root.size() && key[root.size()] == '\\');
}

// ProgId の根（`Software\Classes\NeNeNib.` と次の `\` の手前まで）。
[[nodiscard]] std::string prog_id_root(std::string_view bare)
{
    return std::string(prog_id_head) + std::string(bare.substr(0, bare.find('\\')));
}

[[nodiscard]] Strings model_roots(const Strings &extensions)
{
    Strings roots{std::string(verb_root), std::string(application_root)};
    for (const std::string &extension : extensions)
    {
        std::string root = prog_id_root(std::string_view(extension).substr(1));
        if (std::ranges::find(roots, root) == roots.end())
        {
            roots.push_back(std::move(root));
        }
    }
    roots.emplace_back(product_root);
    return roots;
}

// Nib の木の中か（右クリックの項目・アプリ・ProgId・製品の根の内）。ProgId は名前が `NeNeNib.` で
// 始まる鍵の木で、拡張子が `\` で始まると根は `NeNeNib.` そのもの（Nib だけが作る鍵）になる。
[[nodiscard]] bool own_key(std::string_view key)
{
    return within(key, verb_root) || within(key, application_root) || within(key, product_root) ||
           key.starts_with(prog_id_head);
}

[[nodiscard]] Strings value_names_under(const ShellRegistrationPlan &plan, std::string_view key)
{
    Strings names;
    for (const ShellRegistrationEntry &entry : plan)
    {
        if (entry.key_path == key)
        {
            names.push_back(entry.value_name);
        }
    }
    return names;
}

// --- 性質 ---

// 1 つの値が Nib の木の中か、共有の鍵の Nib の名前か（removal を足すと switch が落ちる・CPP-002）。
[[nodiscard]] std::string_view entry_violation(const ShellRegistrationEntry &entry)
{
    switch (entry.removal)
    {
    case ShellRegistrationRemoval::subtree:
        return own_key(entry.key_path) ? std::string_view{}
                                       : "a subtree value is outside the Nib trees";
    case ShellRegistrationRemoval::value:
        return entry.key_path == shared_key && entry.value_name == "NeNeNib"
                   ? std::string_view{}
                   : "a shared value is not the Nib name under RegisteredApplications";
    }
    std::unreachable();
}

[[nodiscard]] std::string_view keys_violation(const ShellRegistrationPlan &plan)
{
    for (const ShellRegistrationEntry &entry : plan)
    {
        const std::string_view reason = entry_violation(entry);
        if (!reason.empty())
        {
            return reason;
        }
    }
    const auto shared =
        std::ranges::count_if(plan, [](const ShellRegistrationEntry &entry)
                              { return entry.removal == ShellRegistrationRemoval::value; });
    return shared == 1 ? std::string_view{} : "the shared key is not written exactly once";
}

[[nodiscard]] std::string_view removal_violation(const ShellRegistrationPlan &plan,
                                                 const Strings &extensions)
{
    const Strings roots = core::shell_registration_subtrees(plan);
    if (roots != model_roots(extensions))
    {
        return "the removal roots differ from the model";
    }
    for (const std::string &root : roots)
    {
        const bool written =
            std::ranges::any_of(plan,
                                [&root](const ShellRegistrationEntry &entry)
                                {
                                    return entry.removal == ShellRegistrationRemoval::subtree &&
                                           within(entry.key_path, root);
                                });
        if (!own_key(root) || !written)
        {
            return "a removal root is not a tree the plan wrote";
        }
    }
    for (const ShellRegistrationEntry &entry : plan)
    {
        const bool covered = std::ranges::any_of(roots, [&entry](const std::string &root)
                                                 { return within(entry.key_path, root); });
        if (entry.removal == ShellRegistrationRemoval::subtree && !covered)
        {
            return "a subtree value is not under any removal root";
        }
    }
    return {};
}

// コマンドとアイコンの値は引用した exe のパスで始まる。
[[nodiscard]] bool launches_quoted(const ShellRegistrationPlan &plan, std::string_view exe_path)
{
    const std::string quoted = "\"" + std::string(exe_path) + "\"";
    return std::ranges::all_of(plan,
                               [&quoted](const ShellRegistrationEntry &entry)
                               {
                                   const bool launches = entry.key_path.ends_with("command") ||
                                                         entry.value_name == "Icon" ||
                                                         entry.key_path.ends_with("DefaultIcon");
                                   return !launches || entry.value.starts_with(quoted);
                               });
}

// 拡張子ごとの ProgId の鍵は既定値を 1 つだけ持つ。
[[nodiscard]] bool prog_ids_present(const ShellRegistrationPlan &plan, const Strings &extensions)
{
    return std::ranges::all_of(extensions,
                               [&plan](const std::string &extension)
                               {
                                   const std::string key =
                                       std::string(prog_id_head) + extension.substr(1);
                                   return value_names_under(plan, key) == Strings{""};
                               });
}

[[nodiscard]] std::string_view extensions_violation(const ShellRegistrationPlan &plan,
                                                    const ShellRegistrationInput &input,
                                                    const Strings &extensions)
{
    const std::string supported = std::string(application_root) + "\\SupportedTypes";
    const std::string associations = std::string(product_root) + "\\Capabilities\\FileAssociations";
    if (value_names_under(plan, supported) != extensions ||
        value_names_under(plan, associations) != extensions)
    {
        return "the extensions differ from the model";
    }
    if (!prog_ids_present(plan, extensions))
    {
        return "an extension lacks its ProgId";
    }
    if (!launches_quoted(plan, input.exe_path))
    {
        return "a command or icon does not quote the exe path";
    }
    return plan.size() == 8U + 5U * extensions.size() ? std::string_view{}
                                                      : "the plan has the wrong number of values";
}

[[nodiscard]] Strings swapped_case(const Strings &given)
{
    Strings result;
    for (const std::string &extension : given)
    {
        std::string text;
        for (const char byte : extension)
        {
            const bool lower = byte >= 'a' && byte <= 'z';
            text.push_back(lower ? static_cast<char>(byte - 'a' + 'A') : byte);
        }
        result.push_back(std::move(text));
    }
    return result;
}

[[nodiscard]] std::string_view stable_violation(const ShellRegistrationInput &input,
                                                const ShellRegistrationPlan &plan)
{
    ShellRegistrationInput doubled = input;
    doubled.extensions.insert(doubled.extensions.end(), input.extensions.begin(),
                              input.extensions.end());
    ShellRegistrationInput upper = input;
    upper.extensions = swapped_case(input.extensions);
    const auto again = core::shell_registration_plan(input);
    const auto twice = core::shell_registration_plan(doubled);
    const auto shouted = core::shell_registration_plan(upper);
    const bool same = again.has_value() && again.value() == plan && twice.has_value() &&
                      twice.value() == plan && shouted.has_value() && shouted.value() == plan;
    return same ? std::string_view{} : "the plan changes with repeats, case or a second call";
}

[[nodiscard]] std::string_view failure_violation(const ShellRegistrationInput &input)
{
    const auto plan = core::shell_registration_plan(input);
    if (input.exe_path.empty())
    {
        return !plan && plan.error() == ShellRegistrationPlanFailure::empty_path
                   ? std::string_view{}
                   : "an empty path is not refused";
    }
    return !plan && plan.error() == ShellRegistrationPlanFailure::quote_in_path
               ? std::string_view{}
               : "a quoted path is not refused";
}

[[nodiscard]] std::string_view plan_violation(const ShellRegistrationInput &input)
{
    if (input.exe_path.empty() || input.exe_path.find('"') != std::string::npos)
    {
        return failure_violation(input);
    }
    const auto plan = core::shell_registration_plan(input);
    if (!plan)
    {
        return "a valid input is refused";
    }
    const Strings extensions = model_extensions(input.extensions);
    for (const std::string_view reason :
         {keys_violation(plan.value()), removal_violation(plan.value(), extensions),
          extensions_violation(plan.value(), input, extensions),
          stable_violation(input, plan.value())})
    {
        if (!reason.empty())
        {
            return reason;
        }
    }
    return {};
}

void print_input(std::string_view reason, const ShellRegistrationInput &input)
{
    std::fprintf(stderr, "  %.*s: exe '%s' extensions", static_cast<int>(reason.size()),
                 reason.data(), input.exe_path.c_str());
    for (const std::string &extension : input.extensions)
    {
        std::fprintf(stderr, " '%s'", extension.c_str());
    }
    std::fprintf(stderr, "\n");
}

void verify_random_plans()
{
    Random random{12897U};
    std::size_t broken = 0;
    std::size_t planned = 0;
    for (std::size_t index = 0; index < plan_inputs; ++index)
    {
        const ShellRegistrationInput input = random_input(random);
        const std::string_view reason = plan_violation(input);
        planned += core::shell_registration_plan(input).has_value() ? 1U : 0U;
        if (!reason.empty() && broken++ == 0U)
        {
            print_input(reason, input);
        }
    }
    expect(broken == 0U, "every plan stays in the Nib trees and removes only what it wrote");
    expect(planned > plan_inputs / 2U && planned < plan_inputs,
           "the plan generator makes both plans and refusals");
}

// 製品の拡張子の表（言語の表と `.txt`）も同じ性質を持つ。
void verify_product_extensions()
{
    const Strings extensions = core::shell_registration_extensions();
    const ShellRegistrationInput input{"C:\\Program Files\\NeNe Nib\\NeNeNib.exe",
                                       "NeNe Nib で開く", "NeNe Nib", extensions};
    expect(plan_violation(input).empty() && model_extensions(extensions) == extensions &&
               std::ranges::find(extensions, ".txt") != extensions.end(),
           "the product extensions are normalized and keep every plan property");
}

void verify_generator_is_deterministic()
{
    Random left{12890U};
    Random right{12890U};
    bool same = true;
    for (std::size_t round = 0; round < 16U; ++round)
    {
        const ShellRegistrationInput first = random_input(left);
        const ShellRegistrationInput second = random_input(right);
        same = same && first.exe_path == second.exe_path && first.extensions == second.extensions;
    }
    expect(same, "the plan generator gives the same inputs for the same seed");
}
} // namespace

void verify_shell_registration_property_scope()
{
    verify_generator_is_deterministic();
    verify_product_extensions();
    verify_random_plans();
}
} // namespace nenenib::tests
