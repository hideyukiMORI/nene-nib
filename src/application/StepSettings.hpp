#pragma once

namespace nenenib::application
{
// 設定の面の焦点を見えている行の中で 1 つ動かす（Tab・上下の鍵・回り込む・追補 2）。使う人の操作
// なので keeps_tab_walk にも begin_intent の表にも書かない。
struct StepSettings
{
    bool down;
};
} // namespace nenenib::application
