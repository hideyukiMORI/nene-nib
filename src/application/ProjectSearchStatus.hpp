#pragma once

#include <cstdint>

namespace nenenib::application
{
// 全文検索の面の今の状態（ADR 0167 の決定 2・5）。閉じた集合（CPP-002）。prompt は検索語が空
// （要求しない・案内の 1 行）、searching / complete / truncated / failed は要求の進み具合、
// unplaced は探す場所が無い（ルートが無く、文書が無題）、invalid は先頭が `/` の検索語を解析でき
// ない（どちらも要求しない・失敗の 1 行）。
enum class ProjectSearchStatus : std::uint8_t
{
    prompt,
    searching,
    complete,
    truncated,
    failed,
    unplaced,
    invalid
};
} // namespace nenenib::application
