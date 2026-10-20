#pragma once

#include <string>

namespace nenenib::tests::preview
{
// nib_preview_capture の引数（ADR 0151 の決定 18）。寸法は DIP、字の大きさは pt。どれも単独で
// 妥当な値なので公開 aggregate。
struct CaptureOptions
{
    std::string input;
    std::string output;
    std::string theme;
    std::string font;
    float width;
    float height;
    float dpi;
    float points;
    float scroll;
};
} // namespace nenenib::tests::preview
