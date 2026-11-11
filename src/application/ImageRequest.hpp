#pragma once

#include "FilePath.hpp"

#include <cstdint>

namespace nenenib::application
{
// 画像を読む要求（施主決定 D79・ADR 0155 の追補 53・#1381・ADR 0004 の不変の要求値）。ticket は
// controller が要求ごとに 1 つ進める券（0 は使わない）で、届いた結果がどの要求のものかを見分ける。
// path は core::resolve_image_reference が解いたローカルのファイルの経路（ドライブの絶対パス・
// UTF-8）。公開 aggregate（CPP-003）。
struct ImageRequest
{
    std::uint64_t ticket;
    core::FilePath path;
};
} // namespace nenenib::application
