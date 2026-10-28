#pragma once

#include "ServerFailure.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 本文 1 通の上限（16 MiB）と見出しの上限（8 KiB）。ADR 0160 の決定 3 と同じ桁。
inline constexpr std::size_t server_message_limit = std::size_t{16} * 1024 * 1024;
inline constexpr std::size_t server_header_limit = std::size_t{8} * 1024;

// 言語サーバーから受けたバイト列を繋ぎ、`Content-Length` の枠で本文に切る（ADR 0160 の決定 3）。
// core の lsp_frames（#1009）が main に入るまでの、adapter の中の最小の切り出し（2 本目の経路・
// #1022 で置き換える）。Content-Length だけを読み、ほかの見出しは無視する。見出しの誤り・
// 見出しの長さの超えは read_failed、本文の長さの超えは message_too_long で、
// 一度失敗したら以後は何も切り出さない（流れの区切りを失ったので）。
class ServerFramer final
{
  public:
    // 繋いだバイト列から切り出せた本文を順に返す。失敗の前に切り出せた本文も返す。
    [[nodiscard]] std::vector<std::string> feed(std::string_view bytes);
    [[nodiscard]] std::optional<application::ServerFailure> failure() const noexcept;

  private:
    std::string pending_;
    std::optional<application::ServerFailure> failure_;
};

// 本文 1 通に `Content-Length` の見出しを付けた枠（送る側）。
[[nodiscard]] std::string framed_message(std::string_view body);
} // namespace nenenib::adapters::win32
