#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文字コードを判別できなかったこと。期待される失敗なので結果型で返す（ARC-010 / CPP-005）。
enum class EncodingFailure : std::uint8_t
{
    undecodable
};
} // namespace nenenib::core
