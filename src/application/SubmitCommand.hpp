#pragma once

namespace nenenib::application
{
// 入力行の確定（Enter）。all は Ctrl+Enter の旗で、通常モードの「置換後: 」でだけ全件の置換に
// なる（ADR 0174 の決定 3・7）。ほかの入力行では Ctrl+Enter は今までどおり何もしない。
struct SubmitCommand
{
    bool all = false;
};
} // namespace nenenib::application
