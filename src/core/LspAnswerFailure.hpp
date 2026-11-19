#pragma once

#include <cstdint>

namespace nenenib::core
{
// 補完と定義の答えの読み（lsp_completion_items・lsp_locations）が断る値の閉じた一覧（ADR 0190 の
// 決定 4）。1 件ずつの不正（label の無い項目・uri や range の欠けた場所）は断らずにその 1 件を
// 捨てる。
enum class LspAnswerFailure : std::uint8_t
{
    // result が null でも、その要求の答えの形（配列・object）でもない
    invalid_result
};
} // namespace nenenib::core
