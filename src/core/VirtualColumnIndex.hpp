#pragma once

#include "LineNumber.hpp"
#include "Offset.hpp"
#include "VirtualColumnChunk.hpp"
#include "VirtualColumnCursor.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace nenenib::core
{
class TextBuffer;

// 仮想桁の部分和（ADR 0140 の決定 4・ADR 0139 の決定 6）。改行の索引（ADR 0047）と同じく
// TextBuffer の隣に置く不変の値で、本文を約 64 KiB の塊に切り、塊ごとに桁の要約を持つ。
// 各文字の桁は ADR 0034 の表（display_width・Tab は次の 8 の倍数まで）の 1 本だけを見る。
// 本文は持たず、問い合わせのたびに渡された TextBuffer から塊の 1 つだけを読む（plain の塊は
// 読まない）。編集は replaced で、触れた塊だけを読み直し、後ろの塊の始まりは要約から足す。
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
    // この値を作るときに文字を読み直した塊の数（from_text では全部、replaced では触れた塊と、
    // 送りの余りが変わって読み直した後ろの塊）。
    [[nodiscard]] std::size_t rebuilt_chunks() const noexcept;

    // at の手前までの累積（at は code point の先頭）。text は索引を作った本文。
    [[nodiscard]] VirtualColumnCursor cursor_at(const TextBuffer &text, Offset at) const;
    // 行頭から at の手前までの仮想桁の和（0 始まり）。
    [[nodiscard]] std::size_t virtual_column_at(const TextBuffer &text, Offset at) const;
    // 行 line の中で、累積の仮想桁 column（0 始まり）を占める文字の先頭。幅 0 の文字は桁を
    // 占めない。行がそれより短ければ行の内容の終わり。
    [[nodiscard]] Offset byte_at_virtual_column(const TextBuffer &text, LineNumber line,
                                                std::size_t column) const;
    // 同じ逆引きを升で。送りの空きの升は、送られた文字（次の画面行の先頭）を返す。
    [[nodiscard]] Offset byte_at_cell(const TextBuffer &text, LineNumber line,
                                      std::size_t cell) const;

  private:
    VirtualColumnIndex() = default;
    [[nodiscard]] static VirtualColumnIndex built(const TextBuffer &text, std::size_t columns);
    [[nodiscard]] std::size_t size_bytes() const noexcept;
    [[nodiscard]] std::size_t chunk_of(std::size_t at) const noexcept;
    // [begin, end) の本文を約 64 KiB に切って、state から始まる塊として足す。足した後の累積を返す。
    VirtualColumnCursor append_cut(const TextBuffer &text, std::size_t begin, std::size_t end,
                                   VirtualColumnCursor state);
    // 古い塊 from から後ろを shift バイトずらして足し、始まりの累積を state から付け直す。
    void append_carried(const VirtualColumnIndex &old, std::size_t from, std::size_t shift,
                        const TextBuffer &text);
    [[nodiscard]] Offset byte_at(const TextBuffer &text, LineNumber line, std::size_t target,
                                 bool by_cell) const;
    // bytes を state から歩き、target の桁（by_cell なら升）を占める文字のバイト位置を返す。
    // 見つからなければ bytes の長さ。
    [[nodiscard]] std::size_t scanned_until(std::string_view bytes, VirtualColumnCursor state,
                                            std::size_t target, bool by_cell) const;

    std::vector<VirtualColumnChunk> chunks_;
    std::size_t columns_ = 0;
    std::size_t rebuilt_ = 0;
};
} // namespace nenenib::core
