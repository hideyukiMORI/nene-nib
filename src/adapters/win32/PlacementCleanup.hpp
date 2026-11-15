#pragma once

#include "FileWriteMode.hpp"

#include <cstdint>

namespace nenenib::adapters::win32
{
// 一時ファイルを保存先へ置けなかった後の始末（ADR 0135 の追補・#1689）。backup の名前を渡さない
// ReplaceFileW の失敗の番号ごとに、元のファイルと一時ファイルが Microsoft の文書でどこにあるかで
// 決める。新しいバイト列がディスクのどこにも無くなる始末はしない。
enum class PlacementCleanup : std::uint8_t
{
    // 元のファイルは元の名前のまま無事。一時ファイルを消す（1175・ほかの番号・新規保存）。
    remove_temporary,
    // 元のファイルはもう無く、新しいバイト列は一時ファイルの名前で残る。保存先の名前へ
    // 置き直す（1176 = ERROR_UNABLE_TO_MOVE_REPLACEMENT）。
    move_temporary,
    // 元のファイルは別の名前で残り、一時ファイルが元の属性を継いで残る。消さずに残す
    // （1177 = ERROR_UNABLE_TO_MOVE_REPLACEMENT_2・置き直しも失敗した 1176）。
    keep_temporary,
};

// 置けなかった操作の GetLastError の番号と保存のモードから始末を決める純関数。新規保存
// （create_new）は ReplaceFileW を使わないので、いつも remove_temporary。
[[nodiscard]] PlacementCleanup placement_cleanup(std::uint32_t error,
                                                 application::FileWriteMode mode) noexcept;
} // namespace nenenib::adapters::win32
