#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace Utils::Stats {

[[nodiscard]] inline auto percentile_index(std::size_t count,
                                           unsigned percent) noexcept -> std::size_t {
  if (count == 0U) {
    return 0U;
  }
  const std::size_t rank = ((count * percent) + 99U) / 100U;
  const std::size_t index = rank == 0U ? 0U : rank - 1U;
  return std::min(index, count - 1U);
}

template <typename T>
[[nodiscard]] auto percentile_of_sorted(std::span<const T> sorted,
                                        unsigned percent) noexcept -> T {
  if (sorted.empty()) {
    return T{};
  }
  return sorted[percentile_index(sorted.size(), percent)];
}

template <typename T>
[[nodiscard]] auto percentile(std::vector<T> samples, unsigned percent) -> T {
  if (samples.empty()) {
    return T{};
  }
  std::sort(samples.begin(), samples.end());
  return samples[percentile_index(samples.size(), percent)];
}

template <typename T>
[[nodiscard]] auto mean(std::span<const T> samples) noexcept -> double {
  if (samples.empty()) {
    return 0.0;
  }
  return std::accumulate(samples.begin(), samples.end(), 0.0) /
         static_cast<double>(samples.size());
}

template <typename T>
[[nodiscard]] auto mean(const std::vector<T>& samples) noexcept -> double {
  return mean(std::span<const T>(samples));
}

template <typename T>
[[nodiscard]] auto maximum(std::span<const T> samples) noexcept -> T {
  if (samples.empty()) {
    return T{};
  }
  return *std::max_element(samples.begin(), samples.end());
}

struct Distribution {
  std::size_t count{0};
  double average{0.0};
  double p50{0.0};
  double p95{0.0};
  double p99{0.0};
  double maximum{0.0};
};

[[nodiscard]] inline auto distribution_of(std::vector<double> samples) -> Distribution {
  Distribution out;
  if (samples.empty()) {
    return out;
  }
  out.count = samples.size();
  out.average = mean(std::span<const double>(samples));
  std::sort(samples.begin(), samples.end());
  const std::span<const double> sorted(samples);
  out.p50 = percentile_of_sorted(sorted, 50U);
  out.p95 = percentile_of_sorted(sorted, 95U);
  out.p99 = percentile_of_sorted(sorted, 99U);
  out.maximum = samples.back();
  return out;
}

template <std::size_t Capacity>
class SampleWindow {
public:
  static constexpr std::size_t k_capacity = Capacity;

  void push(double value) noexcept {
    m_samples[m_cursor] = value;
    m_cursor = (m_cursor + 1U) % Capacity;
    m_count = std::min<std::size_t>(m_count + 1U, Capacity);
    m_total += value;
    ++m_pushes;
    m_maximum = std::max(m_maximum, value);
  }

  void clear() noexcept {
    m_cursor = 0U;
    m_count = 0U;
    m_total = 0.0;
    m_pushes = 0U;
    m_maximum = 0.0;
  }

  [[nodiscard]] auto count() const noexcept -> std::size_t { return m_count; }
  [[nodiscard]] auto pushes() const noexcept -> std::uint64_t { return m_pushes; }
  [[nodiscard]] auto lifetime_average() const noexcept -> double {
    return m_pushes == 0U ? 0.0 : m_total / static_cast<double>(m_pushes);
  }
  [[nodiscard]] auto lifetime_maximum() const noexcept -> double { return m_maximum; }

  [[nodiscard]] auto distribution() const -> Distribution {
    Distribution out;
    if (m_count == 0U) {
      return out;
    }
    std::array<double, Capacity> sorted{};
    std::copy_n(m_samples.begin(), m_count, sorted.begin());
    const double sum = std::accumulate(
        sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(m_count), 0.0);
    std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(m_count));
    const std::span<const double> view(sorted.data(), m_count);
    out.count = m_count;
    out.average = sum / static_cast<double>(m_count);
    out.p50 = percentile_of_sorted(view, 50U);
    out.p95 = percentile_of_sorted(view, 95U);
    out.p99 = percentile_of_sorted(view, 99U);
    out.maximum = sorted[m_count - 1U];
    return out;
  }

private:
  std::array<double, Capacity> m_samples{};
  std::size_t m_cursor{0U};
  std::size_t m_count{0U};
  double m_total{0.0};
  std::uint64_t m_pushes{0U};
  double m_maximum{0.0};
};

} // namespace Utils::Stats
