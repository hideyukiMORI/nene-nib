#pragma once

#include "AutosaveForm.hpp"
#include "AutosaveId.hpp"
#include "DisplayText.hpp"
#include "FilePath.hpp"
#include "TextPosition.hpp"

#include <optional>
#include <string>

namespace nenenib::application
{
// 前回のプロセスが残した退避の 1 本（ADR 0137 の決定 6）。location は置き場の印で、中身は
// adapters が決める不透明な文字列（application は読まずに read / forget へ返すだけ）。caret は
// 一覧に書いたキャレットの行と桁で、戻したタブのキャレットになる。form は一覧に書いた文書の形
// （追補 #1688）で、あれば本文を UTF-8 として読んで形を置き、無ければ（前の版の一覧）開くのと
// 同じ判別で読む。値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveRemnant
{
    std::string location;
    AutosaveId id;
    std::optional<core::FilePath> path;
    core::DisplayText title;
    core::TextPosition caret;
    std::optional<AutosaveForm> form = std::nullopt;
};
} // namespace nenenib::application
