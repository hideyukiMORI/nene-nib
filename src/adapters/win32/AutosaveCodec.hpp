#pragma once

#include "AutosaveEntry.hpp"
#include "AutosaveFailure.hpp"
#include "AutosaveId.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::adapters::win32
{
// 一覧の上限。越えた一覧は書かず、読むときも全体を拒む（session.v2 と同じ値）。
inline constexpr std::size_t maximum_autosave_entries = 256;
inline constexpr std::size_t maximum_autosave_list_bytes = 1024U * 1024U;
// 本文を読む上限は開く上限と同じ core::maximum_document_bytes（ADR 0137 の限界・#792 で正本を
// core の 1 か所にした）。

// 本文のファイル名 `<番号>.body`。
[[nodiscard]] std::string autosave_body_name(application::AutosaveId id);

// 形式（UTF-8・書きは LF・読みは CRLF と BOM も受ける・session.v2 と同じ行の道具）:
//   version=1
//   entry=<番号>,<キャレットの行>,<桁>,<本文のファイル名>,<パス（無題なら空）>
//   title=<題名>
// entry と title の 2 行で 1 タブ。パスは行の最後の欄で行末まで読む。行と桁は 1 始まり。
// 本文のファイル名は番号から作る名前と一致しなければ拒む。壊れたものは一部だけ返さず全体を拒む。
[[nodiscard]] std::expected<std::vector<AutosaveEntry>, application::AutosaveFailure>
decode_autosave_entries(std::string_view bytes);
[[nodiscard]] std::expected<std::string, application::AutosaveFailure>
encode_autosave_entries(const std::vector<AutosaveEntry> &entries);
} // namespace nenenib::adapters::win32
