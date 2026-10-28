#pragma once

#include "TextBuffer.hpp"
#include "TextEncoding.hpp"

namespace nenenib::application
{
// 読み・符号化の判定・検証・改行の索引まで揃った文書（ADR 0159 の決定 2）。application は
// text を move で採用するだけ。公開 aggregate（CPP-003）。
struct OpenedDocument
{
    core::TextBuffer text;
    core::TextEncoding encoding;
};
} // namespace nenenib::application
