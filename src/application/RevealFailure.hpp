#pragma once

#include <cstdint>

namespace nenenib::application
{
// エクスプローラーで場所を開けなかった理由（ADR 0078 の追補 #1790）。not_found はファイルが無い
// （消された・名前が変わった）、failed はほかの全部（シェルの失敗・パスを渡せない）。
enum class RevealFailure : std::uint8_t
{
    not_found,
    failed
};
} // namespace nenenib::application
