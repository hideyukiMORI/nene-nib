#pragma once

#include "LinkDestination.hpp"
#include "PreviewLink.hpp"

#include <cstddef>

namespace nenenib::application
{
// 開いているリンクの札（施主決定 D105・D106・#1869 の段 L2）。block は見せている結果
// （EditorState::shown_preview の document）の要素の番号、link はその要素の links の 1 つ（範囲は
// 要素の text のバイト位置・札はその下に出る）、destination は core::link_destination の 1 本が
// 決めた種類と見せる綴り。置くのは controller の ClickPreviewLink、消すのは入口の
// close_link_card_before と札の意図。EditorState が持ち、frame にもこの値で載る。公開 aggregate
// で、 比較は非メンバー（CPP-003）。
struct LinkCardView
{
    std::size_t block;
    core::PreviewLink link;
    core::LinkDestination destination;
};

[[nodiscard]] inline bool operator==(const LinkCardView &left, const LinkCardView &right)
{
    return left.block == right.block && left.link == right.link &&
           left.destination == right.destination;
}
} // namespace nenenib::application
