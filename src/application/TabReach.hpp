#pragma once

#include <cstdint>

namespace nenenib::application
{
// まだ読んでいないタブへ届いたか（ADR 0059 の決定 5・ADR 0159 の追補 1）。reached は読めた
// （または読み込み済み）、reading は裏の読みを頼んだ（タブは帯に残り、揃ったらそのタブへ
// 切り替える）、dropped は読めずに帯から外して 1 行知らせた。閉じた enum（CPP-002）。
enum class TabReach : std::uint8_t
{
    reached,
    reading,
    dropped
};
} // namespace nenenib::application
