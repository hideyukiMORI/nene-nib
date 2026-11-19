#pragma once

#include "LinkOpenFailure.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace nenenib::adapters::win32
{
// リンクを OS の既定の道に渡す呼び出しの前後（#1869 の段 L2）。入口の守りと UTF-16 への写しは
// 純関数で、試験は本物のブラウザ・メールを開かずにここを試す。

// シェルに渡す綴り（UTF-16）。宛先の種類を決めるのは core::link_destination の 1 本で、ここは守りの
// 2 重として最小の 2 つだけを見る: scheme（ASCII の大小を畳む）が http / https / mailto で、綴りの
// 全部が ASCII の URI の字（英数字と RFC 3986 の記号と `%`）。違えば refused。
[[nodiscard]] std::expected<std::wstring, application::LinkOpenFailure>
link_open_target(std::string_view uri);

// 本物のシェルを呼ぶ（ShellExecuteExW・verb は open）。呼んだスレッドで STA を作って壊す。
// ワーカーの仕事の中だけが呼ぶ（UI スレッドを止めない）。失敗は failed。
[[nodiscard]] std::expected<void, application::LinkOpenFailure>
open_link_in_shell(const std::wstring &uri);
} // namespace nenenib::adapters::win32
