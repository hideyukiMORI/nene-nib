#pragma once

#include <cstdint>

namespace nenenib::application
{
// 読み込みの進み具合（ADR 0159 の決定 2）。adapter が塊ごとに 1 つ出す。read_bytes は読めた
// バイト数、total_bytes はファイルの大きさ。値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct DocumentReadProgress
{
    std::uint64_t ticket;
    std::uint64_t read_bytes;
    std::uint64_t total_bytes;
};
} // namespace nenenib::application
