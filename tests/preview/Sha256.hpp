#pragma once

#include <string>
#include <string_view>

namespace nenenib::tests::preview
{
// FIPS 180-4 の SHA-256 を小文字の 16 進 64 桁で返す（#818 の probe が入力の同一性を記録する）。
// preview_tests の使える platform library に bcrypt が無いので、試験の中で計算する。
[[nodiscard]] std::string sha256_hex(std::string_view bytes);
} // namespace nenenib::tests::preview
