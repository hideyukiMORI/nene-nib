#pragma once

#include "DocumentReadOutcome.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace nenenib::adapters::win32
{
// 文書の塊の読みと UI スレッドが共有する棚（ADR 0159 の決定 3・5・9・FolderShelf と同じ形）。
// 走っている券の集まり・届いた結果のたまり・「届いた」の合図を 1 つのロックで守る。塊の仕事が
// shared_ptr で持つので、adapter が先に壊れても残る。取り消した券は走っている集まりから外れ、
// その券の結果はたまりからも消えて以後は置かれない（取消の後に Finished は届かない）。合図は
// たまりが空から 1 つ以上になったときだけ、ロックの外で呼ぶ。合図を受ける前に届いた結果は、
// bind のときに 1 回知らせる（起動の引数の読みは窓より先に頼まれる）。
class DocumentReadShelf final
{
  public:
    DocumentReadShelf() = default;

    void bind(std::function<void()> arrived);
    // 裏で読み始める券を走っている集まりに入れる。
    void start(std::uint64_t ticket);
    // 券を走っている集まりから外し、たまりのその券の結果を消す。
    void cancel(std::uint64_t ticket);
    // 以後の結果を受けず、合図も呼ばない（adapter が壊れるとき）。走っている塊は次の塊で止まる。
    void close();
    // 券の読みを続けてよいか（閉じておらず、取り消されていない）。
    [[nodiscard]] bool wanted(std::uint64_t ticket) const;
    // 進み具合を置く。走っていない券は置かない。
    void progress(application::DocumentReadProgress progress);
    // 終わりを置き、券を走っている集まりから外す。走っていない券は置かない。
    void finish(application::DocumentReadFinished finished);
    [[nodiscard]] std::vector<application::DocumentReadOutcome> take();

  private:
    [[nodiscard]] bool running(std::uint64_t ticket) const;
    // ロックを持って結果を積み、空から 1 つ以上になったら合図を返す（呼ぶのはロックの外）。
    [[nodiscard]] std::function<void()> stored(application::DocumentReadOutcome outcome);

    mutable std::mutex mutex_;
    std::vector<std::uint64_t> running_;
    std::vector<application::DocumentReadOutcome> outcomes_;
    std::function<void()> arrived_;
    bool closed_ = false;
};
} // namespace nenenib::adapters::win32
