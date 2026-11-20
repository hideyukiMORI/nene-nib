#pragma once

#include <cstddef>
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
    dockerfile,
    java,
    kotlin,
    dart,
    zig,
    terraform,
    make,
    batch,
    ruby,
    swift,
    scala,
    haskell,
    elixir,
    r,
    perl,
    vue,
    svelte
};

// Language の値か。値を足すとこの switch がコンパイルで落ち（CPP-002）、足した後は下の数え上げが
// 1 つ増えて、言語ごとの表（LanguageTable.cpp の名前・Lexer.cpp の字句の関数）に行が無ければ
// 表の static_assert が落ちる（OperationTexts.cpp と同じ形）。
[[nodiscard]] consteval bool is_language(Language language) noexcept
{
    switch (language)
    {
    case Language::plain:
    case Language::markdown:
    case Language::html:
    case Language::c:
    case Language::cpp:
    case Language::python:
    case Language::json:
    case Language::javascript:
    case Language::typescript:
    case Language::yaml:
    case Language::toml:
    case Language::xml:
    case Language::css:
    case Language::powershell:
    case Language::shell:
    case Language::cmake:
    case Language::gdscript:
    case Language::dosini:
    case Language::gdresource:
    case Language::cs:
    case Language::rust:
    case Language::go:
    case Language::php:
    case Language::sql:
    case Language::lua:
    case Language::dockerfile:
    case Language::java:
    case Language::kotlin:
    case Language::dart:
    case Language::zig:
    case Language::terraform:
    case Language::make:
    case Language::batch:
    case Language::ruby:
    case Language::swift:
    case Language::scala:
    case Language::haskell:
    case Language::elixir:
    case Language::r:
    case Language::perl:
    case Language::vue:
    case Language::svelte:
        return true;
    }
    return false;
}

// 値は 0 から連続して並ぶので、最初に値でなくなる所までが値の数。
[[nodiscard]] consteval std::size_t language_count() noexcept
{
    std::size_t count = 0;
    while (is_language(static_cast<Language>(count)))
    {
        ++count;
    }
    return count;
}
} // namespace nenenib::core
