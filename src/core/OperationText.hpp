#pragma once

#include "EditorOperation.hpp"

#include <string_view>

namespace nenenib::core
{
// 文字の表の 1 行（ADR 0078 の決定 5）。どれも UTF-8。reading はひらがなで、一覧の絞り込みにだけ
// 使う。
struct OperationText
{
    EditorOperation operation;
    std::string_view name;
    std::string_view reading;
    std::string_view description;
    // 未指定なら通常名を使う。操作一覧の名前・検索対象には加えない（ADR 0079）。
    std::string_view short_name{};
};
} // namespace nenenib::core
