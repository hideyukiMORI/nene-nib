#pragma once

#include "Language.hpp"

#include <string_view>

namespace nenenib::core
{
// `didOpen` の `languageId` の唯一の写し（ADR 0161 の 2026-11-20 の追補・#1897）。LSP 3.17 の
// 仕様の表の名前（`cs` → `csharp`・`ps1` → `powershell`・`sh` → `shellscript`・`dosini` →
// `ini`・`text` → `plaintext`）で、表に無い言語（`toml` `cmake` `gdscript` `gdresource`）は
// language_name の名前のまま。`.tsx` と `.jsx` の文書（path の拡張子・大小を問わない）は
// `typescriptreact` と `javascriptreact`（Language の値は増やさない）。OS に触れない純関数。
[[nodiscard]] std::string_view lsp_language_id(Language language, std::string_view path) noexcept;
} // namespace nenenib::core
