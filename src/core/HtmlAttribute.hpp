#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nenenib::core
{
// 開きタグの名前の後ろ（`href="a" class=b`）から属性 1 つの値を引く（ADR 0157 の決定 6）。
// 名前は大文字小文字を区別しない（name は小文字で渡す）。値は引用符を外して実体参照を解く。
// 無ければ値なし。
[[nodiscard]] std::optional<std::string> html_attribute(std::string_view attributes,
                                                        std::string_view name);

// 文字の中の実体参照（`&amp;` `&#39;` `&#x41;` など）を解く。解けない綴りは綴りのまま残す。
// 名前つきの表は Markdown の実体の表（markdown_entity_text）の 1 つを使う。
[[nodiscard]] std::string html_decoded(std::string_view text);
} // namespace nenenib::core
