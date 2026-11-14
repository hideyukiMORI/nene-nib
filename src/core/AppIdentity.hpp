#pragma once

#include <string>
#include <string_view>

namespace nenenib::core
{
// 製品の名乗り（施主決定 D90・#1677・ADR 0172 の追補）。名前・著作者・年・場所は
// この 1 か所の定数で、版は project(... VERSION) の 1 か所から CMake が
// AppIdentity.cpp にだけ渡す（app_version）。見せ方は 3 つで、どれもここを読む:
// F1 の操作「バージョン情報」と Vim の `:version` の 1 行の知らせ（app_about_line）・
// 起動画面の題の横の版と一覧の下の署名 2 行・exe の VERSIONINFO（rc は C++ を
// 読めないので文字で書き、CNF-007 が LegalCopyright と CompanyName を照合する）。
// 文は言語に依らない（訳さない）。年は時刻から作らない（ARC-007）。場所は文字として
// 見せるだけで、開かない（PRIVACY.md・アプリは Web のページを開かない）。
inline constexpr std::string_view app_name = "NeNe Nib";
inline constexpr std::string_view app_author = "Hideyuki Mori";
inline constexpr std::string_view app_copyright_year = "2026";
inline constexpr std::string_view app_homepage = "github.com/hideyukiMORI/nene-nib";

// 版（`1.0.0` の形・project(... VERSION) の値そのもの）。
[[nodiscard]] std::string_view app_version() noexcept;
// 署名の 1 行目「© 2026 Hideyuki Mori」。
[[nodiscard]] std::string app_copyright_line();
// 知らせの 1 行「NeNe Nib 1.0.0 · © 2026 Hideyuki Mori · github.com/hideyukiMORI/nene-nib」。
[[nodiscard]] std::string app_about_line();
} // namespace nenenib::core
