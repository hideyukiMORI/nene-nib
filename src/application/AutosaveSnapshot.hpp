#pragma once

#include "AutosaveId.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "TextPosition.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 退避する 1 タブの写し（ADR 0137 の決定 4）。bytes は保存と同じ符号化で作った本文で、戻すときは
// 開くのと同じ復号で読む。path は無題なら無し。caret は行と桁（前回のタブの一覧と同じ形）。
// 値だけを運ぶ公開 aggregate（CPP-003・ADR 0004）。
struct AutosaveSnapshot
{
    AutosaveId id;
    std::optional<core::FilePath> path;
    core::DisplayText title;
    std::vector<std::byte> bytes;
    core::TextPosition caret;
};
} // namespace nenenib::application
