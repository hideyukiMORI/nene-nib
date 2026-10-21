#pragma once

#include "AutosaveId.hpp"
#include "Language.hpp"
#include "LexState.hpp"
#include "LineNumber.hpp"

#include <cstddef>

namespace nenenib::application
{
// 枠ごとの構文の状態の覚え（#816・ADR 0156 の決定 9）。文書（tab）の改訂 revision を言語 language
// で走らせたとき、行 line の行頭に入る字句の状態が state。controller が枠ごとに 1 つ持ち、
// (tab, revision, language) が同じで line が見えている最初の行の手前 4,096 行以内なら、遡りの
// 200 行の代わりにここから続ける。EditorState には入れない（値型で写しが多い）。
// どのメンバーも単独で妥当な値なので公開 aggregate。
struct SyntaxStateCache
{
    AutosaveId tab;
    std::size_t revision;
    core::Language language;
    core::LineNumber line;
    core::LexState state;
};
} // namespace nenenib::application
