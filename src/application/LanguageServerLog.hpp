#pragma once

#include "Language.hpp"
#include "LanguageServerLogKind.hpp"
#include "LanguageServerSlot.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <string>
#include <string_view>

namespace nenenib::application
{
// 席ごとの記録の上限（ADR 0160 の 2026-11-22 の追補・LSP 段 S8）。行の数かバイトの数のどちらかを
// 超えたら、その席の古い行から捨てる（行は core::server_log_line で 2 KiB までに切ってある）。
inline constexpr std::size_t language_server_log_lines = 512;
inline constexpr std::size_t language_server_log_bytes = std::size_t{256} * 1024;

// 言語サーバーの記録（追補）。席ごとに小さな輪のバッファを持ち、行の頭に席の言語と出どころの印
// （`[cpp][stderr] `）を付けて置く。時刻は付けない（application は時計を持たない・ARC-007）。
// 標準エラーは塊で届くので、改行の前の残りを席ごとに持ち（行の上限を超えた分は改行まで捨てる）、
// 子が終わるか止めるとき（end_output）に 1 行として閉じる。CR LF の CR は落とす。サーバーの文の
// 直し（不正な UTF-8・制御文字・長い行）は core::server_log_line の 1 本。controller は
// WorkCompleted の collect の中でだけ足す（打鍵の道は触れない・lsp がオフなら何も動かない）。
class LanguageServerLog final
{
  public:
    // 標準エラーのバイト列を足す。改行で閉じた行だけを記録に置く。
    void add_output(LanguageServerSlot slot, core::Language language, std::string_view bytes);
    // 席の標準エラーの残り（改行の無い最後の行）があれば 1 行として閉じる。
    void end_output(LanguageServerSlot slot, core::Language language);
    // 通知の文を足す（改行で行に分け、空の行は置かない）。
    void add_message(LanguageServerSlot slot, core::Language language, LanguageServerLogKind kind,
                     std::string_view text);
    [[nodiscard]] bool empty() const noexcept;
    // 全部の席の記録を席の順・届いた順に CR LF で繋いだ写し（最後の行も CR LF で終わる）。
    [[nodiscard]] std::string text() const;

  private:
    void push(std::size_t index, core::Language language, LanguageServerLogKind kind,
              std::string_view raw);

    std::array<std::deque<std::string>, language_server_slot_limit> lines_{};
    std::array<std::size_t, language_server_slot_limit> bytes_{};
    std::array<std::string, language_server_slot_limit> partial_{};
};
} // namespace nenenib::application
