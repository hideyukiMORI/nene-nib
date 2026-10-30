#pragma once

#include <cstdint>

namespace nenenib::core
{
// 登録を外すときの値の消し方（ADR 0178 の決定 2）。subtree は Nib だけが持つ木（右クリックの項目・
// アプリ・ProgId・`Software\NeNeNib`）の中の値で、外すときは木の根ごと消す（根は
// shell_registration_subtrees が引く）。value は共有の鍵（`Software\RegisteredApplications`）の中の
// 値で、外すときはその値だけを消す。
enum class ShellRegistrationRemoval : std::uint8_t
{
    subtree,
    value
};
} // namespace nenenib::core
