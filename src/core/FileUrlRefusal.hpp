#pragma once

#include <cstdint>

namespace nenenib::core
{
// file_url_path が断る値の閉じた一覧（#1825 のレビュー M1 / R1 / R2）。
enum class FileUrlRefusal : std::uint8_t
{
    // host がある（image_reference は localhost のほかの host を全部断る）
    network,
    // host が device の名前空間（`.` `?`）になる形（`\\.\` `\\?\`）
    device,
    // ドライブの絶対パスでも UNC でもない・host や share の形が崩れている
    malformed,
    // 根（ドライブの根か `\\host\share`）より上へ出る `..`（language_server だけが断る）
    above_root
};
} // namespace nenenib::core
