#pragma once

#include "CommandEdit.hpp"
#include "HistoryWalk.hpp"
#include "InputText.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nenenib::core
{
// `:` と検索の入力行の履歴（#725・ADR 0022 決定 2 の追補）。窓に 1 つずつ・セッションの中だけ
// で、ファイルに書かない。足すのは Enter で確定した入力だけ（空と直前と同じは足さない）、上限を
// 越えたら古いものから捨てる。値は不変で、写しは列を共有する。
class InputHistory final
{
  public:
    static constexpr std::size_t limit = 100;

    [[nodiscard]] static InputHistory empty();
    [[nodiscard]] std::size_t size() const noexcept;
    // index 番目（古い方から 0・範囲の中であること）。
    [[nodiscard]] std::string_view at(std::size_t index) const;
    [[nodiscard]] InputHistory remembered(std::string_view entry) const;
    // 上下の 1 歩（history_previous / history_next）。walk が空なら typed から歩きを始める。
    // 前は origin の文字で始まる 1 つ前の履歴、次は 1 つ次で、次が無ければ origin に戻る（回り
    // 込まない）。動かない（前が無い・歩いていないときの次・ほかの編集）なら空。
    [[nodiscard]] std::optional<HistoryWalk> stepped(const InputText &typed,
                                                     const std::optional<HistoryWalk> &walk,
                                                     CommandEdit direction) const;
    // 歩きが見せる入力。履歴の位置なら全文とその末尾のキャレット、位置が空なら origin。
    [[nodiscard]] InputText shown(const HistoryWalk &walk) const;

  private:
    explicit InputHistory(std::shared_ptr<const std::vector<std::string>> entries);

    std::shared_ptr<const std::vector<std::string>> entries_;
};
} // namespace nenenib::core
