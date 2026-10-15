#pragma once

#include "WarmedDevice.hpp"

#include <windows.h>

#include <memory>
#include <optional>

namespace nenenib::ui::win32
{
// D3D11 の device と DXGI の factory を worker の仕事の中で先に作り、UI スレッドへ 1 回だけ
// 渡す箱（ADR 0131 の決定 1）。合成ルートが worker より前に宣言して所有する。
// 渡すのは 2 段で、先に WARP（仮の device・event A）、次に本物（HARDWARE・event B）を置く
// （ADR 0132 の決定 1）。make() は結果を置いてから event を立て、
// take は event を待ってから読むので、書きと読みの順は event が決める
// （<atomic> は使わない・CNF-009）。スレッドは持たない。
class DeviceWarmup final
{
  public:
    DeviceWarmup();
    ~DeviceWarmup() = default;
    DeviceWarmup(const DeviceWarmup &) = delete;
    DeviceWarmup(DeviceWarmup &&) = delete;
    DeviceWarmup &operator=(const DeviceWarmup &) = delete;
    DeviceWarmup &operator=(DeviceWarmup &&) = delete;

    // worker の仕事の中で 1 回だけ呼ぶ。WARP を作って A を立て、
    // 次に HARDWARE（失敗なら 2 つ目の WARP）と factory を作って B を立てる。
    // WARP を先に作るのは、HARDWARE の生成中に始めた WARP がその終わりまで
    // 待たされるため（ADR 0132 の決定 1）。stop が立っていれば作らず、event だけ立てる。
    // stop の印は bool で受ける（<stop_token> は adapters/win32 の外に置かない・CPP-013）。
    void make(bool stop_requested);
    // A を待ち、WARP の device を 1 回だけ渡す（factory は空）。
    // 2 回目以降と、作れなかったときは空。
    [[nodiscard]] std::optional<WarmedDevice> take_provisional();
    // B を待ち、本物の device を 1 回だけ渡す。2 回目以降と、作れなかったときは空
    // （呼び手は今日どおり自分で作る・ADR 0131 の決定 3）。
    [[nodiscard]] std::optional<WarmedDevice> take();
    // B が立っていれば真。待たない。箱が event を持てなかったときも真（待つものが無い）。
    [[nodiscard]] bool ready() const;
    // B の HANDLE。合成ルートのメッセージループが、仮の device で描いている間だけ待つ（決定 4）。
    [[nodiscard]] HANDLE event() const;

  private:
    using EventHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;

    EventHandle provisional_ready_;
    EventHandle ready_;
    HRESULT provisional_result_ = E_FAIL;
    HRESULT result_ = E_FAIL;
    WarmedDevice provisional_;
    WarmedDevice warmed_;
    bool provisional_taken_ = false;
    bool taken_ = false;
};
} // namespace nenenib::ui::win32
