#pragma once

#include "CaretMotionClock.hpp"
#include "CaretPoint.hpp"
#include "CaretSparkTrack.hpp"

#include <cstdint>
#include <optional>

namespace nenenib::ui::win32
{
// 打ったときの粒の frame ごとの覚え（ADR 0192 の追補・#1979）。描く側（Direct2DRenderer）が 1 つ持
// ち、frame の入口の begin・キャレットを描く所の see・粒を描く所と frame の出口の advance で進め
// る。粒の姿と覚えの進め方は core の純関数（CaretSparks）が決め、ここはこの frame で粒を出すかとキ
// ャレットの中心が見えたかだけを持つ。キャレットの動き（CaretMotionFollower）とは独立で、動きがオフ
// でも粒だけ出せる。
class CaretSparksFollower final
{
  public:
    // 窓が描く前に毎回置く。allowed は Windows の「アニメーション効果」、floor は窓が最後に追った粒
    // の時計の番号（作り直した描く側でも controller に残った古い進みと番号が当たらない・
    // CaretMotionFollower と同じ形・#1956 の R2）。
    void allow(bool allowed, std::uint32_t floor) noexcept;
    // frame の入口。wanted は設定の caret_sparks、composing は IME の変換中、typed は frame の打鍵
    // の通し番号、progress は controller が frame に載せた最後の合図の粒の時計。オフの frame は覚え
    // を空にし（次にオンになった最初の frame は番号を覚えるだけ）、アニメーション効果がオフか変換中
    // の frame は生きている粒を捨てて番号だけを覚える。
    void begin(bool wanted, bool composing, std::uint32_t typed,
               core::CaretMotionClock progress) noexcept;
    // この frame でキャレットの中心を知りたいか（オフの道はこの真偽 1 つを読むだけ）。
    [[nodiscard]] bool on() const noexcept;
    // この frame の操作中のキャレットの中心（物理画素）。
    void see(core::CaretPoint center) noexcept;
    // この frame の覚えを 1 回だけ進めて返す（2 回目からは同じ値）。粒を描く所と出口が呼ぶ。
    const core::CaretSparkTrack &advance() noexcept;
    // 描く粒があるか（advance の後に読む）。
    [[nodiscard]] bool paints() const noexcept;
    // frame の出口（粒を描かなかった frame でも覚えを進める）。
    void end() noexcept;
    // 生きている粒を捨てる（窓が見えなくなったとき）。
    void settle() noexcept;
    // 粒が生きていれば粒の時計の番号、無ければ値なし（窓は描いた後に読み、生きている間だけ合図を送
    // る・全部消えたら合図が止まる）。
    [[nodiscard]] std::optional<std::uint32_t> generation() const noexcept;

  private:
    core::CaretSparkTrack track_{};
    bool allowed_ = true;
    bool wanted_ = false;
    bool paused_ = false;
    bool advanced_ = false;
    std::uint32_t typed_ = 0;
    core::CaretMotionClock progress_{};
    std::optional<core::CaretPoint> seen_ = std::nullopt;
};
} // namespace nenenib::ui::win32
