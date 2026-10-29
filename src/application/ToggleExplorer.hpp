#pragma once

namespace nenenib::application
{
// エクスプローラーを出す・消す（ADR 0162 の決定 4・D66）。隠れていれば出して焦点を置き、出ていて
// 焦点があれば消し、焦点が無ければ焦点だけを置く。出す・消すは設定 explorer を書く。使う人の操作。
struct ToggleExplorer
{
};
} // namespace nenenib::application
