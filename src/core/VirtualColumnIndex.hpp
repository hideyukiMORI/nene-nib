#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VirtualColumnChunk.hpp"
#include "VirtualColumnCursor.hpp"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// 仮想桁の部分和（ADR 0140 の決定 4・ADR 0139 の決定 6）。改行の索引（ADR 0047）と同じく
// TextBuffer の隣に置く不変の値で、本文を約 64 KiB の塊に切り、塊ごとに桁・升・コードポイントの
// 数の要約を持つ。塊は作るとき 64 KiB で切り、編集で 128 KiB を越えた範囲は 128 KiB 以下の等しい
// 長さに切り直し、16 KiB を下回る範囲は次の塊と合わせる。
// 各文字の桁は ADR 0034 の表（column_advance の 1 本）だけを見る。本文は持たず、問い合わせのたびに
// 渡された TextBuffer から塊の 1 つだけを読む（plain の塊は読まない）。編集は replaced で、触れた
// 塊だけを読み直し、後ろの塊の始まりは要約から足す。
//
// for_grid は格子（ADR 0140）の升も持つ。2 升以上の表示（Tab を除く）が画面行の右端に収まらない
// ときは次の画面行へ送り、送りの空きも升に数える。升は columns に依るので、幅が変われば作り直す。
class VirtualColumnIndex final
{
  public:
    [[nodiscard]] static VirtualColumnIndex from_text(const TextBuffer &text);
    [[nodiscard]] static VirtualColumnIndex for_grid(const TextBuffer &text, std::size_t columns);

    // TextBuffer::replaced(begin, end, inserted) と同じ範囲の編集を写す。after は編集後の本文。
    [[nodiscard]] VirtualColumnIndex replaced(const TextBuffer &after, Offset begin, Offset end,
                                              std::string_view inserted) const;

    // 格子の 1 画面行の升の数。text の索引では 0。
    [[nodiscard]] std::size_t columns() const noexcept;
    [[nodiscard]] std::size_t chunk_count() const noexcept;
    // 塊の要約の列（本文の順）。切り直しと合わせの規則を外から確かめるための読む口。
    [[nodiscard]] std::span<const VirtualColumnChunk> chunks() const noexcept;
    // この値を作るときに文字を読み直した塊の数（from_text では全部、replaced では触れた塊と、
    // 送りの余りが変わって読み直した後ろの塊）。
    [[nodiscard]] std::size_t rebuilt_chunks() const noexcept;

    // at の手前までの累積（at は code point の先頭）。text は索引を作った本文。歩くのは at を含む
    // 塊の 1 つだけで、写すのは「塊の始まりか、塊の中で at より前の最後の行頭」から at まで。
    [[nodiscard]] VirtualColumnCursor cursor_at(const TextBuffer &text, Offset at) const;
    // 行頭から at の手前までの仮想桁の和（0 始まり）。
    [[nodiscard]] std::size_t virtual_column_at(const TextBuffer &text, Offset at) const;
    // 行頭から at の手前までのコードポイントの数（0 始まりの序数）。
    [[nodiscard]] std::size_t code_point_at(const TextBuffer &text, Offset at) const;
    // 行 line の中で、累積の仮想桁 column（0 始まり）を占める文字の先頭。幅 0 の文字は桁を
    // 占めない。行がそれより短ければ行の内容の終わり。
    [[nodiscard]] Offset byte_at_virtual_column(const TextBuffer &text, LineNumber line,
                                                std::size_t column) const;
    // 同じ逆引きを升で。送りの空きの升は、送られた文字（次の画面行の先頭）を返す。
    [[nodiscard]] Offset byte_at_cell(const TextBuffer &text, LineNumber line,
                                      std::size_t cell) const;
    // 同じ逆引きをコードポイントの序数（0 始まり）で。行がそれより短ければ行の内容の終わり。
    [[nodiscard]] Offset byte_at_code_point(const TextBuffer &text, LineNumber line,
                                            std::size_t ordinal) const;

  private:
    VirtualColumnIndex() = default;
    [[nodiscard]] static VirtualColumnIndex built(const TextBuffer &text, std::size_t columns);
    [[nodiscard]] std::size_t size_bytes() const noexcept;
    [[nodiscard]] std::size_t chunk_of(std::size_t at) const noexcept;
    // 最後の塊の終わりの累積（塊が無ければ 0）。
    [[nodiscard]] VirtualColumnCursor end_state() const noexcept;
    // 最後の塊までの '\n' の数（塊が無ければ 0）。
    [[nodiscard]] std::size_t end_line() const noexcept;
    // [begin, end) の本文を 128 KiB 以下の等しい長さに切って、最後の塊の後ろに足す。
    void append_cut(const TextBuffer &text, std::size_t begin, std::size_t end);
    // 古い塊 from から後ろを shift バイトずらして足し、始まりの累積を最後の塊から付け直す。
    void append_carried(const VirtualColumnIndex &old, std::size_t from, std::size_t shift,
                        const TextBuffer &text);
    [[nodiscard]] Offset byte_at(const TextBuffer &text, LineNumber line, std::size_t target,
                                 std::size_t VirtualColumnCursor::*measure) const;
    // bytes を state から歩き、measure（桁・升・序数）で target を占める文字のバイト位置を返す。
    // 見つからなければ bytes の長さ。
    [[nodiscard]] std::size_t scanned_until(std::string_view bytes, VirtualColumnCursor state,
                                            std::size_t target,
                                            std::size_t VirtualColumnCursor::*measure) const;

    std::vector<VirtualColumnChunk> chunks_;
    std::size_t columns_ = 0;
    std::size_t rebuilt_ = 0;
};
} // namespace nenenib::core
