#pragma once

#include "CommandChoiceKind.hpp"
#include "CommandLine.hpp"
#include "DisplayText.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
struct CommandChoice
{
    DisplayText label;
    std::string command;
    CommandChoiceKind kind;
    // 題名の後ろに muted で添える補足（ADR 0057 の決定 7）。タブの一覧ではファイルのあるフォルダ
    // （無題は無し）、Ex の候補では無し。
    std::optional<DisplayText> detail = std::nullopt;
};

[[nodiscard]] std::vector<CommandChoice> palette_choices(const CommandLine &input);
// タブの一覧の候補（ADR 0057 の決定 7）。tabs は帯の順の 1 タブ 1 行。query が空なら帯の順の
// まま、あれば題名に対する match_score（大文字と小文字を区別しない部分列）で絞って点数の順に
// 並べる（同点は帯の順）。
[[nodiscard]] std::vector<CommandChoice> tab_list_choices(const std::vector<CommandChoice> &tabs,
                                                          std::string_view query);
} // namespace nenenib::core
