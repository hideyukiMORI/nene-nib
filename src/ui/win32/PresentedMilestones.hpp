#pragma once

#include "TimingPort.hpp"

namespace nenenib::ui::win32
{
// present が返った frame の節目を打つ 1 か所（ADR 0011 の決定 1・ADR 0138 の決定 2・#546）。
// frame_presented は毎回打つ。body_presented は本文を描いた（display_pending でない）最初の
// frame で 1 回だけ打つ。問いの無い文書では最初の frame で 2 つを続けて打ち、問いのある文書では
// 答えの後の最初の frame で打つ。device lost の後の frame では打ち直さない。
class PresentedMilestones
{
  public:
    // display_pending は present した frame の EditorFrame::display_pending（本文を描いていない）。
    void presented(bool display_pending, application::TimingPort &timing) noexcept;

  private:
    bool body_presented_ = false;
};
} // namespace nenenib::ui::win32
