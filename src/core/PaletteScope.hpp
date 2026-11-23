#pragma once

#include <cstdint>

namespace nenenib::core
{
// Ctrl+P の面が見せる出どころ（ADR 0060 の決定 2）。files は印のある候補の全部、tabs は開いて
// いるタブ、history は閉じたファイルの履歴、folder は今のファイルと同じフォルダ（ADR 0062 の
// 決定 11）、commands は Ex の設定のコマンド、operations は操作の一覧（ADR 0078 の決定 6）、
// project_hits はプロジェクトの全文検索の当たり（ADR 0167 の決定 1・入力は絞り込みではなく
// 検索語）、diagnostics は開いているタブの診断（#1939・ADR 0060 の表の追記・files には混ぜない）。
// 値が増えたら写す switch が落ちる（CPP-002）。
enum class PaletteScope : std::uint8_t
{
    files,
    tabs,
    bookmarks,
    history,
    folder,
    commands,
    operations,
    project_hits,
    diagnostics,
    // 言語サーバーの参照の答え（#1947・ADR 0191 の決定 4・files には混ぜない）。
    references,
    // 言語サーバーの記号の答え（#1955・ADR 0191 の決定 5・files には混ぜない）。
    symbols
};
} // namespace nenenib::core
