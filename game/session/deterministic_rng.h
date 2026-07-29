#pragma once

#include <cstdint>

namespace Game::Session {

constexpr auto splitmix64(std::uint64_t& state) -> std::uint64_t {
  state += 0x9E3779B97F4A7C15ULL;
  std::uint64_t z = state;
  z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31U);
}

class DeterministicRng {
public:
  explicit DeterministicRng(std::uint64_t seed = 0x5EED5EED5EED5EEDULL) {
    reseed(seed);
  }

  void reseed(std::uint64_t seed) {
    m_seed = seed;
    std::uint64_t mix = seed;
    for (auto& word : m_state) {
      word = splitmix64(mix);
    }
    m_draws = 0;
  }

  [[nodiscard]] auto seed() const -> std::uint64_t { return m_seed; }

  [[nodiscard]] auto draw_count() const -> std::uint64_t { return m_draws; }

  auto next_u64() -> std::uint64_t {
    const std::uint64_t result = rotl(m_state[1] * 5ULL, 7U) * 9ULL;
    const std::uint64_t t = m_state[1] << 17U;

    m_state[2] ^= m_state[0];
    m_state[3] ^= m_state[1];
    m_state[1] ^= m_state[2];
    m_state[0] ^= m_state[3];
    m_state[2] ^= t;
    m_state[3] = rotl(m_state[3], 45U);

    ++m_draws;
    return result;
  }

  auto next_float() -> float {
    return static_cast<float>(static_cast<double>(next_u64() >> 11U) * 0x1.0p-53);
  }

  auto next_float_in(float min_value, float max_value) -> float {
    return min_value + (max_value - min_value) * next_float();
  }

  auto next_int_in(int min_value, int max_value) -> int {
    if (max_value <= min_value) {
      return min_value;
    }
    const auto span = static_cast<std::uint64_t>(max_value - min_value) + 1ULL;
    return min_value + static_cast<int>(next_u64() % span);
  }

  auto next_bool(float probability) -> bool { return next_float() < probability; }

  void restore(std::uint64_t seed, std::uint64_t draw_count) {
    reseed(seed);
    for (std::uint64_t i = 0; i < draw_count; ++i) {
      next_u64();
    }
  }

private:
  static constexpr auto rotl(std::uint64_t value, unsigned bits) -> std::uint64_t {
    return (value << bits) | (value >> (64U - bits));
  }

  std::uint64_t m_seed = 0;
  std::uint64_t m_state[4]{};
  std::uint64_t m_draws = 0;
};

} // namespace Game::Session
