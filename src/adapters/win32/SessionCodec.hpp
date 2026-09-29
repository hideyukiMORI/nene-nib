#pragma once

#include "Session.hpp"
#include "SessionFailure.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// session.v1 の上限（ADR 0059 の決定 2）。越えた一覧は書かず、読むときも全体を拒む。
inline constexpr std::size_t maximum_session_tabs = 256;
inline constexpr std::size_t maximum_session_bytes = 1024U * 1024U;

// 形式（UTF-8・書きは LF・読みは CRLF と BOM も受ける）:
//   version=1
//   active=<tabs の中の位置>
//   <カーソルの行>,<桁>,<画面の先頭の行>,<使った順の順位>,<パス>   ← 1 行 1 タブ・帯の順
// パスは行の最後の欄で行末まで読む（パスに `,` や `=` があってもよい）。数は 10 進で、行・桁・
// 先頭の行は 1 始まり。壊れたものは一部だけ返さず全体を拒む。
[[nodiscard]] std::expected<application::Session, application::SessionFailure>
decode_session(std::string_view bytes);
[[nodiscard]] std::expected<std::string, application::SessionFailure>
encode_session(const application::Session &session);
} // namespace nenenib::adapters::win32
