#include "GutterLabels.hpp"

#include <string>

namespace nenenib::core
{
GutterLabel gutter_label(LineNumber line, LineNumber caret, bool relative)
{
    if (!relative)
    {
        return GutterLabel{std::to_string(line.value), GutterAlignment::trailing};
    }
    if (line == caret)
    {
        return GutterLabel{std::to_string(line.value), GutterAlignment::leading};
    }
    const auto distance = line < caret ? caret.value - line.value : line.value - caret.value;
    return GutterLabel{std::to_string(distance), GutterAlignment::trailing};
}
} // namespace nenenib::core
