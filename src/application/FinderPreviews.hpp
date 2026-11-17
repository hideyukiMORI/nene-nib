#pragma once

#include "PreviewKey.hpp"
#include "PreviewResult.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace nenenib::application
{
// 窓の保持の数（ADR 0189 の決定 4）。戻ったときは port を呼ばずに出す。
inline constexpr std::size_t finder_preview_held = 8;

// 探して見る面のプレビューの読みの状態（ADR 0189 の決定 4・段 P2）。券は要求のたびに 1 つ進め、
// live は今の要求の券（無ければ 0）と鍵。held は届いた窓（古い順・最大 finder_preview_held）。面を
// 閉じると全部捨てる。値だけを運ぶ公開 aggregate（CPP-003）。
struct FinderPreviews
{
    std::uint64_t tickets = 0;
    std::uint64_t live = 0;
    std::optional<PreviewKey> waiting;
    std::vector<std::pair<PreviewKey, PreviewResult>> held;
};
} // namespace nenenib::application
