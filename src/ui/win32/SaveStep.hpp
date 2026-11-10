#pragma once

#include "SaveAnswer.hpp"
#include "SaveQuestion.hpp"

#include <utility>

namespace nenenib::ui::win32
{
// 「保存しますか」の答えの後の一歩（ADR 0133 の追補 #1282）。
enum class SaveStep
{
    // 保存の道へ（保存したか、失敗・取消で未保存のままかは呼び手が確かめる）。
    save,
    // 未保存を捨てて閉じる側へ（EndSession の unsaved_discarded・ADR 0137 の決定 5）。
    close_discarding,
    // 閉じるが捨てたことにしない（自動退避を残し、次の起動で問う）。
    close_keeping,
    // 閉じない。
    stay,
};

// cancel は「キャンセルを押した」と「箱を出せなかった・知らない id」の両方。閉じる確認では
// 閉じない側、描画を失った後（取消が無く、描けない窓は残せない）では閉じるが捨てない側へ倒す。
[[nodiscard]] constexpr SaveStep save_step(SaveQuestion question, SaveAnswer answer) noexcept
{
    switch (answer)
    {
    case SaveAnswer::save:
        return SaveStep::save;
    case SaveAnswer::discard:
        return SaveStep::close_discarding;
    case SaveAnswer::cancel:
        switch (question)
        {
        case SaveQuestion::unsaved:
            return SaveStep::stay;
        case SaveQuestion::rendering_lost:
            return SaveStep::close_keeping;
        }
        std::unreachable();
    }
    std::unreachable();
}
} // namespace nenenib::ui::win32
