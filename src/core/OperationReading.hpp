#pragma once

#include "EditorOperation.hpp"

#include <string_view>

namespace nenenib::core
{
// 操作の読みの表の 1 行（ADR 0123 の追補 2026-11-13 の決定 6）。reading はひらがなの UTF-8 で、
// 表示言語が日本語のときだけ F1 の一覧の絞り込みに使う。
struct OperationReading
{
    EditorOperation operation;
    std::string_view reading;
};
} // namespace nenenib::core
