#pragma once

#include "ShellRegistrationRemoval.hpp"

#include <string>

namespace nenenib::core
{
// エクスプローラーへの登録で書く値の 1 つ（ADR 0178 の決定 2）。key_path は HKCU からの副鍵
// （`Software\Classes\…`）、value_name が空なら鍵の既定値（`(Default)`）、removal は外すときの
// 消し方。文字列はどれも UTF-8 で、書くのは src/adapters/win32 の ShellRegistrationPort の実装
// だけ（ARC-007）。
struct ShellRegistrationEntry
{
    std::string key_path;
    std::string value_name;
    std::string value;
    ShellRegistrationRemoval removal;
};

// 等値は非メンバで書く（CPP-003・公開 aggregate がメソッドを持つと lint が落ちる）。
[[nodiscard]] inline bool operator==(const ShellRegistrationEntry &left,
                                     const ShellRegistrationEntry &right) noexcept
{
    return left.key_path == right.key_path && left.value_name == right.value_name &&
           left.value == right.value && left.removal == right.removal;
}
} // namespace nenenib::core
