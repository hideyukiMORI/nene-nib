#pragma once

#include <cstdint>

namespace nenenib::core
{
// 文書の言語（ADR 0154 の決定 1）。閉じた選択肢で、文書ごとの値は application が持つ（DisplayForm
// と同じ所有）。設定（settings.v4）とセッション（session.v2）には入れない。宣言の順は
// LanguageTable.cpp の名前の表の順で、値を足すとその表の static_assert が落ちる（決定 8）。
enum class Language : std::uint8_t
{
    plain,
    markdown,
    html,
    c,
    cpp,
    python,
    json,
    javascript,
    typescript,
    yaml,
    toml,
    xml,
    css,
    powershell,
    shell,
    cmake,
    gdscript,
    dosini,
    gdresource,
    cs,
    rust,
    go,
    php,
    sql,
    lua,
    dockerfile
};
} // namespace nenenib::core
