#pragma once

#include "CaretSparkTrack.hpp"
#include "CaretSparks.hpp"
#include "CaretSparksPaint.hpp"

#include <cstddef>
#include <d2d1.h>

namespace nenenib::ui::win32
{
// 打ったときの粒（ADR 0192 の追補・#1979）。本文の字形の上に重ねる層で、生きている束の粒を 1 つずつ
// 小さな四角で塗る（粒の位置・大きさ・不透明度・色は core の caret_spark_at の 1 本）。Target は
// ID2D1RenderTarget の同じ名前のメソッドを持つ描画先（試験はメモリの bitmap）。色は呼び手が palette
// のトークンから渡す（ui に色のリテラルを書かない・ADR 0008）。brush の色は書き換えるので、呼び手は
// 次に塗る前に色を置き直す（renderer の塗りはどれも色を置いてから塗る）。
class CaretSparksPainter final
{
  public:
    template <typename Target>
    static void paint(Target &target, ID2D1SolidColorBrush &brush,
                      const core::CaretSparkTrack &track, const CaretSparksPaint &picture)
    {
        // 束と粒を 1 本の番号で歩く（束 = 番号 / 粒の数・粒 = 番号 % 粒の数）。
        for (std::size_t place = 0; place < track.count * core::caret_spark_count; ++place)
        {
            const auto &shot = track.bursts.at(place / core::caret_spark_count);
            const auto spark = core::caret_spark_at(shot.serial, place % core::caret_spark_count,
                                                    track.now - shot.born);
            if (!spark.has_value())
            {
                continue;
            }
            const auto &seen = spark.value();
            D2D1_COLOR_F color = seen.tinted ? picture.tint : picture.head;
            color.a *= seen.opacity;
            brush.SetColor(color);
            const float half = seen.size * picture.scale / 2.0F;
            const float x = shot.origin.x + (seen.x * picture.scale);
            const float y = shot.origin.y + (seen.y * picture.scale);
            target.FillRectangle(D2D1::RectF(x - half, y - half, x + half, y + half), &brush);
        }
    }
};
} // namespace nenenib::ui::win32
