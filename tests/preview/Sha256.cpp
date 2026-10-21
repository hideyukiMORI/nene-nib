#include "Sha256.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace nenenib::tests::preview
{
namespace
{
using Words = std::array<std::uint32_t, 8>;
using Schedule = std::array<std::uint32_t, 64>;

constexpr std::size_t block_bytes = 64;
constexpr std::size_t length_offset = 56;
constexpr unsigned bits_per_byte = 8;

constexpr std::array<std::uint32_t, 64> round_constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

constexpr Words initial_state{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                              0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};

[[nodiscard]] std::uint32_t byte_at(std::string_view bytes, std::size_t index) noexcept
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index]));
}

[[nodiscard]] Schedule schedule_of(std::string_view block) noexcept
{
    Schedule words{};
    for (std::size_t index = 0; index < 16; ++index)
    {
        const std::size_t at = index * 4;
        words[index] = (byte_at(block, at) << 24U) | (byte_at(block, at + 1) << 16U) |
                       (byte_at(block, at + 2) << 8U) | byte_at(block, at + 3);
    }
    for (std::size_t index = 16; index < words.size(); ++index)
    {
        const std::uint32_t low = words[index - 15];
        const std::uint32_t high = words[index - 2];
        const std::uint32_t small0 = std::rotr(low, 7) ^ std::rotr(low, 18) ^ (low >> 3U);
        const std::uint32_t small1 = std::rotr(high, 17) ^ std::rotr(high, 19) ^ (high >> 10U);
        words[index] = words[index - 16] + small0 + words[index - 7] + small1;
    }
    return words;
}

void compress(Words &state, std::string_view block) noexcept
{
    const Schedule words = schedule_of(block);
    Words work = state;
    for (std::size_t round = 0; round < words.size(); ++round)
    {
        const auto [a, b, c, d, e, f, g, h] = work;
        const std::uint32_t large1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t first = h + large1 + choose + round_constants[round] + words[round];
        const std::uint32_t large0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        work = Words{first + large0 + majority, a, b, c, d + first, e, f, g};
    }
    for (std::size_t index = 0; index < state.size(); ++index)
    {
        state[index] += work[index];
    }
}

// 末尾の 1 ビット・0 の詰め・ビット長（big endian 8 バイト）を足した最後の 1〜2 ブロック。
[[nodiscard]] std::string padded_tail(std::string_view bytes)
{
    std::string tail(bytes.substr(bytes.size() - (bytes.size() % block_bytes)));
    tail.push_back(static_cast<char>(0x80U));
    while (tail.size() % block_bytes != length_offset)
    {
        tail.push_back('\0');
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(bytes.size()) * bits_per_byte;
    for (unsigned shift = 64; shift > 0; shift -= bits_per_byte)
    {
        tail.push_back(static_cast<char>((bits >> (shift - bits_per_byte)) & 0xFFU));
    }
    return tail;
}
} // namespace

std::string sha256_hex(std::string_view bytes)
{
    Words state = initial_state;
    const std::size_t whole = bytes.size() - (bytes.size() % block_bytes);
    for (std::size_t at = 0; at < whole; at += block_bytes)
    {
        compress(state, bytes.substr(at, block_bytes));
    }
    const std::string tail = padded_tail(bytes);
    for (std::size_t at = 0; at < tail.size(); at += block_bytes)
    {
        compress(state, std::string_view(tail).substr(at, block_bytes));
    }
    std::string text;
    for (const std::uint32_t word : state)
    {
        text += std::format("{:08x}", word);
    }
    return text;
}
} // namespace nenenib::tests::preview
