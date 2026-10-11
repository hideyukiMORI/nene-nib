#pragma once
namespace nenenib::tests::performance
{
enum class OperationChoiceWorkload
{
    reading_ordinary,
    miss_ordinary,
    all_ordinary,
    all_vim,
    space_ordinary,
    key_vim,
    name_ordinary,
    close_key_vim,
    description_miss,
    short_name_miss
};
} // namespace nenenib::tests::performance
