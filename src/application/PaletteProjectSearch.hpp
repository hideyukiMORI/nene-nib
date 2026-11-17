#pragma once

#include "FilePath.hpp"
#include "ProjectSearchHit.hpp"
#include "ProjectSearchProgress.hpp"
#include "ProjectSearchStatus.hpp"
#include "VimPatternFailure.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nenenib::application
{
// Ctrl+P の面が全文検索を頼んだ控え（ADR 0167 の決定 1・4・5）。pattern は面の `!` の後ろの検索語
// で、面が `!` の出どころでなければ値なし（検索語が変わったかはこれと比べる）。ticket は今の要求の
// 券、live は券の要求がまだ終わっていない（入力が変わる・面が閉じると cancel する）。root は探して
// いるフォルダ（相対パスの起点）、hits は届いた順の当たり（多くて project_search_hit_limit・面の
// 当たりの候補の i 番目は hits の i 番目）。failure は invalid のときの解析の失敗。値だけを運ぶ公開
// aggregate（CPP-003）。
struct PaletteProjectSearch
{
    std::optional<std::string> pattern;
    std::uint64_t ticket = 0;
    bool live = false;
    std::optional<core::FilePath> root;
    std::vector<ProjectSearchHit> hits;
    ProjectSearchProgress progress{ProjectSearchPhase::searching, 0, 0};
    ProjectSearchStatus status = ProjectSearchStatus::prompt;
    std::optional<core::VimPatternFailure> failure;
};
} // namespace nenenib::application
