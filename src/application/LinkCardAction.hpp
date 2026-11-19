#pragma once

#include <cstdint>

namespace nenenib::application
{
// 開いている札への働きかけ（施主決定 D106・#1869 の段 L2）。open は Enter（refused は何もしない）、
// close は Esc と札の外の押し、copy は Ctrl+C。primary と secondary は札の右と左のボタンで、
// 何をするかは controller が種類から決める（primary は refused なら写す・ほかは開く、secondary
// は閉じる）。
enum class LinkCardAction : std::uint8_t
{
    open,
    close,
    copy,
    primary,
    secondary
};
} // namespace nenenib::application
