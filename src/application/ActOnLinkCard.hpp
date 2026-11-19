#pragma once

#include "LinkCardAction.hpp"

namespace nenenib::application
{
// 開いている札の鍵とボタン（施主決定 D106・#1869 の段 L2）。札が無ければ何もしない。使う人の
// 操作なので keeps_tab_walk にも begin_intent の表にも書かない。
struct ActOnLinkCard
{
    LinkCardAction action;
};
} // namespace nenenib::application
