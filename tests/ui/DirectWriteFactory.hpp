#pragma once
#include <dwrite_2.h>
#include <wrl/client.h>

namespace nenenib::tests::ui
{
// 試験が持つ DirectWrite の factory（共有）。作れなければ nullptr。
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFactory2> directwrite_factory();
// 名前の family がこの機械の system collection にあるか（環境依存の検査の入口）。
[[nodiscard]] bool installed(IDWriteFactory2 &factory, const wchar_t *family);
} // namespace nenenib::tests::ui
