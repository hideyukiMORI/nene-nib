#pragma once

#include "ImageFailure.hpp"
#include "ImagePixels.hpp"

#include <cstdint>
#include <expected>
#include <memory>

namespace nenenib::application
{
// 画像を読んだ結果（施主決定 D79・ADR 0155 の追補 53・#1381）。ticket は要求の券で、controller は待
// っている券の結果だけを使う。成功は復号した画素（null を返さない）、失敗は閉じた理由。値だけを運ぶ
// 公開 aggregate（CPP-003・ADR 0004）。
struct ImageOutcome
{
    std::uint64_t ticket;
    std::expected<std::shared_ptr<const ImagePixels>, ImageFailure> result;
};
} // namespace nenenib::application
