#pragma once

#include "DisplayText.hpp"
#include "FontSize.hpp"
#include "Language.hpp"
#include "LineEnding.hpp"

#include <memory>
#include <string>

namespace nenenib::application
{
// 印刷の 1 回の依頼（ADR 0177 の決定 2）。text はアクティブな文書の本文の連続 1 本で、印刷の意図の
// ときだけ写す。line_ending は本文の改行の形、title はヘッダの左に出すファイル名（無題は「無題」・
// 未保存の印は付けない）、language は構文の色の言語、family と size は本文と同じ字体と大きさ、
// gutter は行番号の欄を描くか。
struct PrintJob
{
    std::shared_ptr<const std::string> text;
    core::LineEnding line_ending;
    core::DisplayText title;
    core::Language language;
    core::DisplayText family;
    core::FontSize size;
    bool gutter;
};
} // namespace nenenib::application
