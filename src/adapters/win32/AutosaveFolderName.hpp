#pragma once

#include <cstdint>
#include <optional>

namespace nenenib::adapters::win32
{
// 置き場の 1 フォルダの名前の読み（#1634）。今の形は `<pid の 16 進>-<作成時刻の 16 進>`。
// created が無いのは、前の版が書いた `<pid の 16 進>` だけの形。
// 値だけを運ぶ公開 aggregate（CPP-003）。
struct AutosaveFolderName
{
    std::uint32_t pid = 0;
    std::optional<std::uint64_t> created;
};
} // namespace nenenib::adapters::win32
