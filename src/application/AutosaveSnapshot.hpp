#pragma once

#include "AutosaveForm.hpp"
#include "AutosaveId.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "TextPosition.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 退避する 1 タブの写し（ADR 0137 の決定 4・追補 #1688）。bytes は本文の UTF-8（BOM なし・本文の
// 改行の形のまま・保存と同じ encoded_parts で作る）で、form が文書の文字コードと改行の形を運ぶ。
// path は無題なら無し。caret は行と桁（前回のタブの一覧と同じ形）。
// 値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct AutosaveSnapshot
{
    AutosaveId id;
    std::optional<core::FilePath> path;
    core::DisplayText title;
    std::vector<std::byte> bytes;
    core::TextPosition caret;
    AutosaveForm form;
};
} // namespace nenenib::application
