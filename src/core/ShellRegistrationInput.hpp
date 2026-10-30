#pragma once

#include <string>
#include <vector>

namespace nenenib::core
{
// 登録の計画の材料（ADR 0178 の決定 2）。exe_path は自分の exe の完全なパス、display_name は
// 右クリックの項目の文、friendly_app_name は「プログラムから開く」と「既定のアプリ」に出る名前、
// extensions は登録する拡張子（`.` の有無と大文字小文字と重複は計画が畳む）。どれも UTF-8。
struct ShellRegistrationInput
{
    std::string exe_path;
    std::string display_name;
    std::string friendly_app_name;
    std::vector<std::string> extensions;
};
} // namespace nenenib::core
