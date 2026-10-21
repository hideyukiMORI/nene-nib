#pragma once

#include <optional>
#include <string_view>

namespace nenenib::adapters::win32
{
// クリップボードの "HTML Format"（CF_HTML）のバイト列から fragment を切り出す（ADR 0157 の
// 決定 4）。先頭の見出しは `Version:` `StartHTML:` `EndHTML:` `StartFragment:` `EndFragment:`
// の行（CRLF）で、`StartFragment` / `EndFragment` は UTF-8 のバイト位置（文字位置ではない）。
// 見出しは最初の `<` より前だけを読む。見出しが無い・数でない・範囲が逆か外れていれば値なし。
// OS に触れない純関数で、試験はバイト列を渡して確かめる（実機のクリップボードを触らない）。
[[nodiscard]] std::optional<std::string_view> html_fragment_of(std::string_view clipboard_bytes);
} // namespace nenenib::adapters::win32
