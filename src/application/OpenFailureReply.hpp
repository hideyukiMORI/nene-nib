#pragma once

#include <cstdint>

namespace nenenib::application
{
// 開けなかったことの告げ方（ADR 0060 の決定 7・ADR 0159 の決定 6）。呼び手ごとの今の規則で、
// 読みが裏で終わっても同じ告げ方を使うので要求に添えて覚える。閉じた集合（CPP-002）。
// dialog は Ctrl+O と起動引数（ADR 0010 の決定 9）。notice は一覧から選んだファイルの 1 行の
// 知らせ。notice_forgetting
// は履歴の候補で、無くなっていたファイル（not_found）を履歴から外す（D30）。
enum class OpenFailureReply : std::uint8_t
{
    dialog,
    notice,
    notice_forgetting
};
} // namespace nenenib::application
