#pragma once

#include "FilePath.hpp"
#include "ImageFailure.hpp"
#include "ImagePixels.hpp"
#include "PreviewImageStatus.hpp"

#include <cstdint>
#include <memory>
#include <optional>

namespace nenenib::application
{
// プレビューの 1 つの画像のファイル（ADR 0155 の追補 50・#1381）。同じ経路を指す画像の節は 1 つの
// entry を共有する。ticket は requested の間に待っている券（ほかは最後に使った券）、pixels は
// loaded の間だけ持つ画素、failure は failed の理由、loaded_order は読めた順の番号（捨てる順に使
// う）。欄は単独で妥当な値なので公開 aggregate（CPP-003）。
struct PreviewImageEntry
{
    core::FilePath path;
    PreviewImageStatus status = PreviewImageStatus::waiting;
    std::uint64_t ticket = 0;
    std::shared_ptr<const ImagePixels> pixels = nullptr;
    std::optional<ImageFailure> failure = std::nullopt;
    std::uint64_t loaded_order = 0;
};
} // namespace nenenib::application
