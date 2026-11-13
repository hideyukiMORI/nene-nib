#pragma once

#include "DisplayLanguage.hpp"
#include "GuideEntry.hpp"
#include "StartEntry.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace nenenib::application
{
// 起動画面の表示値（ADR 0172 の決定 2・5）。entries は選べる行の列（core::start_entries の順）、
// selected はその中の選んだ行（最初は値なし・追補 2026-11-11 の 5）。lists_read は履歴と
// ブックマークを読み終えたか（偽なら entries は「はじめる」の 3 行だけで、「まだ履歴がありません」
// も出さない）。keys は鍵の案内の節（1 列）。
struct StartScreenView
{
    std::vector<core::StartEntry> entries;
    std::optional<std::size_t> selected;
    bool lists_read = false;
    std::array<core::GuideEntry, 3> keys{};
    // 画面の文の言語（frame の language と同じ値・ADR 0123 の追補 2026-11-13）。
    core::DisplayLanguage language = core::DisplayLanguage::japanese;
};
} // namespace nenenib::application
