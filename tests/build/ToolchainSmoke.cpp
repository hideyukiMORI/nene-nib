// C++23 専用の構文だけで書く（ADR 0003）。
// clang-tidy が C++17 で動いたらここで落ちる＝それが検査である。
#include <array>
#include <cstddef>
#include <expected>

namespace
{

// 静的 operator()（P1169）
struct Doubler
{
    static constexpr int operator()(int value)
    {
        return value * 2;
    }
};

// deducing this（P0847）
class Counter
{
  public:
    constexpr explicit Counter(std::size_t amount) : amount_{amount} {}

    constexpr std::size_t doubled(this Counter self)
    {
        return self.amount_ * 2uz;
    }

  private:
    std::size_t amount_;
};

// std::expected（P0323）
constexpr std::expected<int, int> checked(int value)
{
    if (value < 0)
    {
        return std::unexpected(value);
    }
    return value;
}

consteval int only_at_compile_time()
{
    return 1;
}

// if consteval（P1938）
constexpr int chosen()
{
    if consteval
    {
        return only_at_compile_time();
    }
    return 0;
}

} // namespace

int main()
{
    // size_t リテラル 3uz（P0330）と auto(x) による decay-copy（P0849）
    constexpr std::array<int, 3uz> sample{1, 2, 3};
    constexpr auto taken = auto(sample);
    static_assert(taken[0uz] + taken[1uz] == taken[2uz]);
    static_assert(Doubler{}(2) == 4);
    static_assert(Counter{3uz}.doubled() == 6uz);
    static_assert(checked(4).value() == 4);
    static_assert(!checked(-1).has_value());
    static_assert(chosen() == 1);
    return taken[2uz] == 3 ? 0 : 1;
}
