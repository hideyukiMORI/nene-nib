#pragma once

#include "DisplayLanguage.hpp"

#include <utility>

namespace nenenib::ui::win32
{
// 画面の文字（帯・ステータスバー・面・起動画面・エクスプローラー）の IDWriteTextFormat の locale
// （ADR 0123 の追補 2026-11-13 の決定 9・#1019）。漢字の字形の fallback が表示言語に合う（簡体字が
// 日本語の字形で出ない）。日本語は今までと同じ空の名前（OS の既定）のままにして、今の見た目と
// 字体の fallback を変えない。本文・プレビュー・印刷の書式はここを読まない（文書の言語は表示言語と
// 無関係）。
[[nodiscard]] constexpr const wchar_t *ui_locale_name(core::DisplayLanguage language) noexcept
{
    switch (language)
    {
    case core::DisplayLanguage::japanese:
        return L"";
    case core::DisplayLanguage::english:
        return L"en-us";
    case core::DisplayLanguage::simplified_chinese:
        return L"zh-cn";
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
