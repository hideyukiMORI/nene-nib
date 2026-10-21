#pragma once

#include "Language.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace nenenib::core
{
// 言語の表は core のこの 1 か所（ADR 0154 の決定 2・3）。呼び出し元で拡張子を見ない。

// パスの最後の区切り（'\' か '/'）の後ろの名前から言語を引く。名前が `CMakeLists.txt` なら cmake、
// ほかは最後の '.' の後ろの拡張子を ASCII の大文字小文字を区別せずに表で引き、無ければ plain。
// 拡張子の無い名前・空のパスも plain（決定 8・中身や shebang は見ない）。
[[nodiscard]] Language language_of_path(std::string_view path) noexcept;
// Vim の filetype 名（plain は "text"）。
[[nodiscard]] std::string_view language_name(Language language) noexcept;
// language_name の逆引き。名前は小文字だけを受け、引けなければ空（決定 4）。
[[nodiscard]] std::optional<Language> language_of_name(std::string_view name) noexcept;
// 名前の表そのもの（Language の宣言の順）。補完の候補が並べる。
[[nodiscard]] std::span<const std::string_view> all_language_names() noexcept;
} // namespace nenenib::core
