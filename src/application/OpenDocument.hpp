#pragma once

#include "FilePath.hpp"

namespace nenenib::application
{
// ファイルを開く。経路はダイアログか起動引数が作った検証済みの値（ADR 0010 の決定 8・11）。
struct OpenDocument
{
    core::FilePath path;
};
} // namespace nenenib::application
