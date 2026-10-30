#pragma once

#include "PrintSetup.hpp"

#include <objidl.h>
#include <string>
#include <wrl/client.h>

namespace nenenib::ui::win32
{
// 印刷のダイアログで使う人が「印刷」を押した答え（ADR 0177 の決定 3）。printer はプリンタの名前、
// ticket はダイアログの DEVMODE を写した job の print ticket（写せなければ空でプリンタの既定）。
struct PrintChoice
{
    std::wstring printer;
    Microsoft::WRL::ComPtr<IStream> ticket;
    PrintSetup setup;
};
} // namespace nenenib::ui::win32
