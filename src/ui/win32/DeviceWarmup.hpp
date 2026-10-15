#pragma once

#include "WarmedDevice.hpp"

#include <windows.h>

#include <memory>
#include <optional>

namespace nenenib::ui::win32
{
// D3D11 の device と DXGI の factory を worker の仕事の中で先に作り、UI スレッドへ 1 回だけ
// 渡す箱（ADR 0131 の決定 1）。合成ルートが worker より前に宣言して所有する。
// make() は結果を置いてから event を立て、take() は event を待ってから読むので、
// 書きと読みの順は event が決める（<atomic> は使わない・CNF-009）。スレッドは持たない。
class DeviceWarmup final
{
  public:
    DeviceWarmup();
    ~DeviceWarmup() = default;
    DeviceWarmup(const DeviceWarmup &) = delete;
    DeviceWarmup(DeviceWarmup &&) = delete;
    DeviceWarmup &operator=(const DeviceWarmup &) = delete;
    DeviceWarmup &operator=(DeviceWarmup &&) = delete;

    // worker の仕事の中で 1 回だけ呼ぶ。stop が立っていれば作らず、event だけ立てる。
    // stop の印は bool で受ける（<stop_token> は adapters/win32 の外に置かない・CPP-013）。
    void make(bool stop_requested);
    // make() を載せた後に UI スレッドから呼ぶ。make() の終わりを待ち、作れていれば 1 回だけ渡す。
    // 2 回目以降と、作れなかったときは空（呼び手は今日どおり自分で作る・決定 3）。
    [[nodiscard]] std::optional<WarmedDevice> take();

  private:
    using EventHandle = std::unique_ptr<void, decltype(&::CloseHandle)>;

    EventHandle ready_;
    HRESULT result_ = E_FAIL;
    WarmedDevice warmed_;
    bool taken_ = false;
};
} // namespace nenenib::ui::win32
